//
// Copyright (c) 2026 Fshying (Fshying@users.noreply.github.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "services/redis_serialization.hpp"

#include <boost/json/parse.hpp>
#include <boost/redis/resp3/node.hpp>
#include <boost/redis/resp3/type.hpp>
#include <boost/redis/response.hpp>
#include <boost/system/error_code.hpp>
#include <boost/test/unit_test.hpp>

#include <chrono>

#include "business_types.hpp"
#include "error.hpp"

namespace resp3 = boost::redis::resp3;
using namespace chat;
using boost::system::error_code;

BOOST_AUTO_TEST_SUITE(redis_serialization)

// 创建一个 array 类型的节点
static resp3::node array_node(std::size_t size, std::size_t depth)
{
    return {resp3::type::array, size, depth, ""};
}

// 创建一个 string 类型的节点
static resp3::node string_node(std::size_t depth, std::string content)
{
    return {
        resp3::type::blob_string,
        0,
        depth,
        std::move(content),
    };
}

BOOST_AUTO_TEST_CASE(parse_room_history_success)
{
    // 输入数据
    std::vector<resp3::node> nodes{
        // 第 1 个房间的顶层节点
        array_node(2, 0),
        // 第一条消息
        array_node(2, 1),
        string_node(2, "100-1"),
        array_node(2, 2),
        string_node(3, "payload"),
        string_node(3, R"%({"user_id":11,"content":"Test message 2","timestamp":1691666793896})%"),
        // 第二条消息
        array_node(2, 1),
        string_node(2, "90-1"),
        array_node(2, 2),
        string_node(3, "payload"),
        string_node(3, R"%({"user_id":12,"content":"Test message 3","timestamp":1691666793897})%"),

        // 第 2 个房间的顶层节点；该房间没有消息
        array_node(0, 0),

        // 第 3 个房间的顶层节点
        array_node(1, 0),
        // 第一条消息
        array_node(2, 1),
        string_node(2, "150-1"),
        array_node(2, 2),
        string_node(3, "payload"),
        string_node(3, R"%({"user_id":11,"content":"msg7","timestamp":1691666793898})%"),
    };

    // 调用函数
    auto res = parse_room_history_batch(nodes);
    const auto& val = res.value();

    // 校验结果
    BOOST_TEST(val.size() == 3u);
    BOOST_TEST(val[0].messages.size() == 2u);
    BOOST_TEST(val[1].messages.size() == 0u);
    BOOST_TEST(val[2].messages.size() == 1u);

    BOOST_TEST(val[0].messages[0].id == "100-1");
    BOOST_TEST(val[0].messages[0].user_id == 11);
    BOOST_TEST(serialize_timestamp(val[0].messages[0].timestamp) == 1691666793896);
    BOOST_TEST(val[0].messages[0].content == "Test message 2");

    BOOST_TEST(val[0].messages[1].id == "90-1");
    BOOST_TEST(val[0].messages[1].user_id == 12);
    BOOST_TEST(serialize_timestamp(val[0].messages[1].timestamp) == 1691666793897);
    BOOST_TEST(val[0].messages[1].content == "Test message 3");

    BOOST_TEST(val[2].messages[0].id == "150-1");
    BOOST_TEST(val[2].messages[0].user_id == 11);
    BOOST_TEST(serialize_timestamp(val[2].messages[0].timestamp) == 1691666793898);
    BOOST_TEST(val[2].messages[0].content == "msg7");
}

BOOST_AUTO_TEST_CASE(parse_room_history_empty)
{
    // 输入数据
    std::vector<resp3::node> nodes{};

    // 调用函数
    auto res = parse_room_history_batch(nodes);
    const auto& val = res.value();

    // 校验结果
    BOOST_TEST(val.size() == 0u);
}

BOOST_AUTO_TEST_CASE(parse_room_history_error)
{
    // 输入数据
    std::vector<resp3::node> nodes{
        // 顶层节点
        array_node(1, 0),
        // 第一条消息
        array_node(2, 1),
        string_node(2, "100-1"),
        array_node(2, 2),
        array_node(0, 0),  // 这个顶层节点不该出现在这里
        string_node(3, "payload"),
        string_node(
            3,
            R"%({"user":{"id":"user1","username":"username1"},"content":"Test message 2","timestamp":1691666793896})%"
        ),
    };

    // 调用函数
    auto res = parse_room_history_batch(nodes);
    BOOST_TEST(res.error() == error_code(errc::redis_parse_error));
}

BOOST_AUTO_TEST_CASE(parse_string_list_success)
{
    // 输入数据
    std::vector<resp3::node> nodes{string_node(0, "s1"), string_node(0, "s2"), string_node(0, "mykey")};

    // 调用函数
    auto res = parse_batch_xadd_response(nodes);
    auto& val = res.value();

    // 校验结果
    BOOST_TEST(val == std::vector<std::string>({"s1", "s2", "mykey"}));
}

BOOST_AUTO_TEST_CASE(parse_string_list_empty)
{
    // 输入数据
    std::vector<resp3::node> nodes{};

    // 调用函数
    auto res = parse_batch_xadd_response(nodes);
    auto& val = res.value();

    // 校验结果
    BOOST_TEST(val.size() == 0u);
}

BOOST_AUTO_TEST_CASE(parse_string_list_error)
{
    // 输入数据
    std::vector<resp3::node> nodes{
        string_node(0, "s1"),
        array_node(1, 0),  // 这里不该出现这个节点
        string_node(0, "s1"),
    };

    // 调用函数
    auto res = parse_batch_xadd_response(nodes);
    BOOST_TEST(res.error() == error_code(errc::redis_parse_error));
}

BOOST_AUTO_TEST_CASE(serialize_redis_message_success)
{
    // 输入数据
    message input{
        "100-10",
        "hello world!",
        timestamp_t{std::chrono::milliseconds(123)},
        11,  // 用户 ID
    };

    // 调用函数
    auto output = serialize_redis_message(input);

    // 校验结果
    const char* expected = R"%({"user_id":11,"content":"hello world!","timestamp":123})%";
    BOOST_TEST(boost::json::parse(output) == boost::json::parse(expected));
}

BOOST_AUTO_TEST_SUITE_END()
