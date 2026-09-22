//
// Copyright (c) 2026 Fshying (Fshying@users.noreply.github.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "api/api_types.hpp"

#include <boost/describe/class.hpp>
#include <boost/json/array.hpp>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/serialize.hpp>
#include <boost/json/value_from.hpp>
#include <boost/json/value_to.hpp>
#include <boost/system/error_code.hpp>
#include <boost/system/result.hpp>
#include <boost/variant2/variant.hpp>

#include <cstdint>
#include <string_view>

#include "business_types.hpp"
#include "error.hpp"
#include "timestamp.hpp"

using namespace chat;
using boost::system::error_code;
using boost::system::result;

namespace chat {

//
// BOOST_DESCRIBE_STRUCT 用于给结构体加上反射能力。
// boost::json 的 value_to、value_from 和 try_value_from 会借助它
// 自动生成 JSON 解析/序列化代码。
//
// Describe 元数据定义在这个 .cpp 文件里，以缩短编译时间。
// 必须注意不要在其他文件里重复定义这些元数据，那会造成 ODR 违规。
//
// 我们只需要给入站类型加这种元数据，因为它们与客户端使用的
// 线格式（wire format）是完全一致的表示。
//

BOOST_DESCRIBE_STRUCT(create_account_request, (), (username, email, password))
BOOST_DESCRIBE_STRUCT(login_request, (), (email, password))
BOOST_DESCRIBE_STRUCT(client_message, (), (content))
BOOST_DESCRIBE_STRUCT(client_messages_event, (), (roomId, messages))
BOOST_DESCRIBE_STRUCT(request_room_history_event, (), (roomId, firstMessageId))

}  // namespace chat

namespace {

// 我们也为出站类型定义了一些带 Describe 元数据的辅助结构体，
// 这样序列化代码写起来更简单。

// API 错误的线格式
struct wire_api_error
{
    std::string_view id;
    std::string_view message;
};
BOOST_DESCRIBE_STRUCT(wire_api_error, (), (id, message))

// 用户的线格式
struct wire_user
{
    std::int64_t id;
    std::string_view username;
};
BOOST_DESCRIBE_STRUCT(wire_user, (), (id, username))

// 服务器消息的线格式
struct wire_server_message
{
    std::string_view id;
    std::string_view content;
    wire_user user;
    std::int64_t timestamp;
};
BOOST_DESCRIBE_STRUCT(wire_server_message, (), (id, content, user, timestamp))

}  // namespace

//
// 入站类型（HTTP 请求、websocket 客户端事件）
//

// HTTP 请求的辅助函数
template <class RequestType>
static result<RequestType> parse_generic_request(std::string_view from)
{
    // 解析 JSON
    error_code ec;
    auto msg = boost::json::parse(from, ec);
    if (ec)
        CHAT_RETURN_ERROR(ec)

    // 解析进结构体
    return boost::json::try_value_to<RequestType>(msg);
}

result<create_account_request> create_account_request::from_json(std::string_view from)
{
    return parse_generic_request<create_account_request>(from);
}

result<login_request> login_request::from_json(std::string_view from)
{
    return parse_generic_request<login_request>(from);
}

chat::any_client_event chat::parse_client_event(std::string_view from)
{
    error_code ec;

    // 解析 JSON
    auto msg = boost::json::parse(from, ec);
    if (ec)
        CHAT_RETURN_ERROR(ec)

    // 取出消息类型
    const auto* obj = msg.if_object();
    if (!obj)
        CHAT_RETURN_ERROR(errc::websocket_parse_error)
    auto it = obj->find("type");
    if (it == obj->end())
        CHAT_RETURN_ERROR(errc::websocket_parse_error)
    const auto& type = it->value();

    // 取出 payload
    it = obj->find("payload");
    if (it == obj->end())
        CHAT_RETURN_ERROR(errc::websocket_parse_error)
    const auto& payload = it->value();

    // 按消息类型分别解析
    if (type == "clientMessages")
    {
        // 解析 payload
        auto parsed_payload = boost::json::try_value_to<client_messages_event>(payload);
        if (parsed_payload.has_error())
            CHAT_RETURN_ERROR(parsed_payload.error())
        return parsed_payload.value();
    }
    else if (type == "requestRoomHistory")
    {
        // 解析 payload
        auto parsed_payload = boost::json::try_value_to<request_room_history_event>(payload);
        if (parsed_payload.has_error())
            CHAT_RETURN_ERROR(parsed_payload.error())
        return parsed_payload.value();
    }
    else
    {
        // 未知类型
        CHAT_RETURN_ERROR(errc::websocket_parse_error)
    }
}

//
// 出站类型（HTTP 响应、websocket 服务器事件）
//

static std::string_view to_string(api_error_id input)
{
    switch (input)
    {
    case api_error_id::login_failed: return "LOGIN_FAILED";
    case api_error_id::username_exists: return "USERNAME_EXISTS";
    case api_error_id::email_exists: return "EMAIL_EXISTS";
    case api_error_id::bad_request:
    default: return "BAD_REQUEST";
    }
}

std::string api_error::to_json() const
{
    wire_api_error err{to_string(error_id), error_message};
    return boost::json::serialize(boost::json::value_from(err));
}

static boost::json::value serialize_message(const message& input, std::string_view username)
{
    return boost::json::value_from(wire_server_message{
        input.id,
        input.content,
        wire_user{input.user_id, username},
        serialize_timestamp(input.timestamp),
    });
}

static boost::json::array serialize_messages(std::span<const message> messages, const username_map& usernames)
{
    boost::json::array res;
    res.reserve(messages.size());
    for (const auto& msg : messages)
    {
        // 在映射中查找用户名。找不到时默认为空
        auto it = usernames.find(msg.user_id);
        auto username = it == usernames.end() ? std::string_view() : std::string_view(it->second);

        res.push_back(serialize_message(msg, username));
    }
    return res;
}

static boost::json::array serialize_messages(std::span<const message> messages, const user& sending_user)
{
    boost::json::array res;
    res.reserve(messages.size());
    for (const auto& msg : messages)
    {
        assert(msg.user_id == sending_user.id);
        res.push_back(serialize_message(msg, sending_user.username));
    }
    return res;
}

static boost::json::object serialize_room(const room& input, const username_map& usernames)
{
    boost::json::object res({
        {"id",              input.id              },
        {"name",            input.name            },
        {"hasMoreMessages", input.history.has_more},
    });
    res.emplace("messages", serialize_messages(input.history.messages, usernames));
    return res;
}

static std::string serialize_event(std::string_view type, boost::json::object payload)
{
    boost::json::object evt;
    evt.emplace("type", type);
    evt.emplace("payload", std::move(payload));
    return boost::json::serialize(evt);
}

std::string hello_event::to_json() const
{
    // 当前用户
    auto json_me = boost::json::value_from(wire_user{me.id, me.username});

    // 房间
    boost::json::array json_rooms;
    json_rooms.reserve(rooms.size());
    for (const auto& room : rooms)
        json_rooms.push_back(serialize_room(room, usernames));

    // 事件
    boost::json::object payload;
    payload.emplace("me", std::move(json_me));
    payload.emplace("rooms", std::move(json_rooms));
    return serialize_event("hello", std::move(payload));
}

std::string server_messages_event::to_json() const
{
    boost::json::object payload;
    payload.emplace("roomId", room_id);
    payload.emplace("messages", serialize_messages(messages, sending_user));
    return serialize_event("serverMessages", std::move(payload));
}

std::string room_history_event::to_json() const
{
    boost::json::object payload;
    payload.emplace("roomId", room_id);
    payload.emplace("messages", serialize_messages(history.messages, usernames));
    payload.emplace("hasMoreMessages", history.has_more);
    return serialize_event("roomHistory", std::move(payload));
}
