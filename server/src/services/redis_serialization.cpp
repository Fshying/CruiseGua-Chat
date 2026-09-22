//
// Copyright (c) 2023-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "services/redis_serialization.hpp"

#include <boost/describe/class.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/serialize.hpp>
#include <boost/json/value_from.hpp>
#include <boost/json/value_to.hpp>
#include <boost/redis/resp3/type.hpp>

#include <string>

#include "error.hpp"
#include "timestamp.hpp"

namespace resp3 = boost::redis::resp3;
using namespace chat;
using boost::system::error_code;
using boost::system::result;

namespace {

// 存储在 Redis 中的消息。注意它不含 ID，
// 因为消息 ID 就是 stream 的 ID。
struct redis_wire_message
{
    std::string_view content;
    std::int64_t timestamp;
    std::int64_t user_id;
};
BOOST_DESCRIBE_STRUCT(redis_wire_message, (), (content, timestamp, user_id))

}  // namespace

static message to_message(const redis_wire_message& from, std::string id)
{
    return chat::message{
        std::move(id),
        std::string(from.content),
        parse_timestamp(from.timestamp),
        from.user_id,
    };
}

result<std::vector<message_batch>> chat::parse_room_history_batch(node_span nodes)
{
    std::vector<message_batch> res;
    error_code ec;

    // 我们需要一个单遍（one-pass）解析器。每个响应的格式如下：
    // MessageEntry 组成的列表：
    //    MessageEntry[0]：string（id）
    //    MessageEntry[1]：list<string>（键值对；个数总是偶数）
    // 由于操作这些节点相当繁琐，我们只用一个名为 "payload" 的键，
    // 其单个值中包含一个 JSON
    // 本函数能够解析多个批量返回的响应
    enum state_t
    {
        wants_level0_list,
        wants_level0_or_entry_list,
        wants_id,
        wants_attr_list,
        wants_key,
        wants_value
    };

    struct parser_data_t
    {
        state_t state{wants_level0_list};
        const std::string* id{};
    } data;

    for (const auto& node : nodes)
    {
        if (data.state == wants_level0_list)
        {
            // 顶层列表，表示一个新响应开始
            if (node.data_type != resp3::type::array)
                CHAT_RETURN_ERROR(errc::redis_parse_error)
            if (node.depth != 0u)
                CHAT_RETURN_ERROR(errc::redis_parse_error)
            res.emplace_back();
            data.state = wants_level0_or_entry_list;
        }
        else if (data.state == wants_level0_or_entry_list)
        {
            // 这里要么是新响应，要么是当前响应中的新消息
            if (node.data_type != resp3::type::array)
                CHAT_RETURN_ERROR(errc::redis_parse_error)
            if (node.depth == 0u)
            {
                // 新响应
                res.emplace_back();
                data.state = wants_level0_or_entry_list;
            }
            else if (node.depth == 1u)
            {
                // 新消息
                if (node.aggregate_size != 2u)
                    CHAT_RETURN_ERROR(errc::redis_parse_error)
                data.state = wants_id;
            }
            else
            {
                CHAT_RETURN_ERROR(errc::redis_parse_error)
            }
        }
        else if (data.state == wants_id)
        {
            // 正在等待 stream 的 ID 字段
            if (node.data_type != resp3::type::blob_string)
                CHAT_RETURN_ERROR(errc::redis_parse_error)
            if (node.depth != 2u)
                CHAT_RETURN_ERROR(errc::redis_parse_error)
            data.id = &node.value;
            data.state = wants_attr_list;
        }
        else if (data.state == wants_attr_list)
        {
            // 正在等待 stream 记录的属性列表
            if (node.data_type != resp3::type::array)
                CHAT_RETURN_ERROR(errc::redis_parse_error)
            if (node.depth != 2u)
                CHAT_RETURN_ERROR(errc::redis_parse_error)
            if (node.aggregate_size != 2u)  // 单个键值对，序列化为 JSON
                CHAT_RETURN_ERROR(errc::redis_parse_error)
            data.state = wants_key;
        }
        else if (data.state == wants_key)
        {
            // 我们在属性列表中等待键。我们的消息
            // 只有一个名为 "payload" 的键
            if (node.data_type != resp3::type::blob_string)
                CHAT_RETURN_ERROR(errc::redis_parse_error)
            if (node.depth != 3u)
                CHAT_RETURN_ERROR(errc::redis_parse_error)
            if (node.value != "payload")
                CHAT_RETURN_ERROR(errc::redis_parse_error)
            data.state = wants_value;
        }
        else if (data.state == wants_value)
        {
            // 我们在属性列表中等待值。它包含
            // 一个 JSON payload，里面是消息内容
            if (node.data_type != resp3::type::blob_string)
                CHAT_RETURN_ERROR(errc::redis_parse_error)
            if (node.depth != 3u)
                CHAT_RETURN_ERROR(errc::redis_parse_error)

            // 解析 payload
            auto jv = boost::json::parse(node.value, ec);
            if (ec)
                CHAT_RETURN_ERROR(ec)
            auto msg = boost::json::try_value_to<redis_wire_message>(jv);
            if (msg.has_error())
                CHAT_RETURN_ERROR(msg.error())
            res.back().messages.push_back(to_message(msg.value(), *data.id));

            // 重置解析器状态
            data.state = wants_level0_or_entry_list;
            data.id = nullptr;
        }
    }

    // 响应已损坏：消息或响应还没有结束
    if (data.state != wants_level0_or_entry_list && data.state != wants_level0_list)
        CHAT_RETURN_ERROR(errc::redis_parse_error)

    return res;
}

result<std::vector<std::string>> chat::parse_batch_xadd_response(node_span nodes)
{
    // 预分配内存
    std::vector<std::string> res;
    res.reserve(nodes.size());

    for (const auto& node : nodes)
    {
        // 校验响应的结构是否符合预期
        if (node.depth != 0u)
            CHAT_RETURN_ERROR(errc::redis_parse_error)
        else if (node.data_type != resp3::type::blob_string)
            CHAT_RETURN_ERROR(errc::redis_parse_error)

        // 加入结果
        res.push_back(node.value);
    }

    return res;
}

std::string chat::serialize_redis_message(const message& msg)
{
    // 构造线格式消息
    redis_wire_message redis_msg{msg.content, serialize_timestamp(msg.timestamp), msg.user_id};

    // 序列化为 JSON
    return boost::json::serialize(boost::json::value_from(redis_msg));
}
