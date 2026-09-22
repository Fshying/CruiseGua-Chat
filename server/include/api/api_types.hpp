//
// Copyright (c) 2023-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_API_API_TYPES_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_API_API_TYPES_HPP

#include <boost/system/error_code.hpp>
#include <boost/system/result.hpp>
#include <boost/variant2/variant.hpp>

#include <span>
#include <string>
#include <string_view>

#include "business_types.hpp"

// 本文件包含 HTTP 与 websocket API 对象的类型定义。
// 入站请求的类型是「拥有型」（owning），因为它们要在解析之后继续使用，
// 并且与 API 中的类型和字段名完全一致。
// 响应与出站事件的类型是非拥有型且轻量的，
// 因为它们只作为序列化过程中的中间类型使用。
// 它们的字段名和类型并不与实际 API 消息完全一致，
// 而是只包含足以生成这些消息的信息，以此避免多余的拷贝。

namespace chat {

//
// 入站消息（HTTP 请求与 websocket 客户端事件）
//

// POST /create-account 的请求
struct create_account_request
{
    // 要创建的用户的用户名
    std::string username;

    // 新用户的邮箱（登录标识）。
    std::string email;

    // 使用的密码。
    std::string password;

    // 从 JSON 字符串解析出请求
    static boost::system::result<create_account_request> from_json(std::string_view from);
};

// POST /login 的请求
struct login_request
{
    // 待认证用户的邮箱（登录标识）。
    std::string email;

    // 使用的密码。
    std::string password;

    // 从 JSON 字符串解析出请求
    static boost::system::result<login_request> from_json(std::string_view from);
};

// 客户端发送的一条消息
struct client_message
{
    std::string content;
};

// 客户端发送的、请求把消息广播给房间内其他客户端的事件
struct client_messages_event
{
    std::string roomId;
    std::vector<client_message> messages;
};

// 客户端发送的、请求某个房间更多历史记录的事件。
struct request_room_history_event
{
    std::string roomId;

    // 客户端已持有的、时间最早的那条消息的 ID。
    // 这是一种分页机制。
    std::string firstMessageId;
};

// 一个 variant，可以表示从客户端收到的任意事件；
// 如果客户端发送了非法消息，则表示为 error_code
using any_client_event = boost::variant2::variant<
    boost::system::error_code,  // 非法，用于上报错误
    client_messages_event,
    request_room_history_event>;

// 把从 websocket 客户端收到的消息解析成一个 variant，
// 其中保存任意一种合法的客户端事件。
any_client_event parse_client_event(std::string_view from);

//
// 出站消息（HTTP 响应与服务器事件）
//

// 在 api_error 中使用，用于把具体的错误情况告知客户端。
enum class api_error_id
{
    // 通用错误，适用于没有更具体错误 ID 的情况
    bad_request = 0,

    // 登录尝试失败（例如用户名或密码错误）
    login_failed,

    // 创建账号失败，所选邮箱已存在
    email_exists,

    // 创建账号失败，所选用户名已存在
    username_exists,
};

// 一个 REST API 错误。用于 HTTP 错误响应中。
struct api_error
{
    // 所发生错误的标识符。
    api_error_id error_id;

    // 人类可读的错误说明。
    std::string_view error_message;

    // 把对象序列化为 JSON 字符串。
    std::string to_json() const;
};

// 客户端连接时发送给它的事件
struct hello_event
{
    // 当前已认证的用户
    const user& me;

    // 聊天室列表，附带部分消息历史
    std::span<const room> rooms;

    // 用户 ID -> 用户名的映射，用于把用户 ID 解析成用户名
    const username_map& usernames;

    // 把对象序列化为 JSON 字符串。
    std::string to_json() const;
};

// 服务器广播给房间内所有客户端，用于通知有新消息到达
struct server_messages_event
{
    // 房间 ID
    std::string_view room_id;

    // 发送这些消息的用户
    const user& sending_user;

    // 实际的消息。所有消息都必须满足 this->user_id == sending_user.id
    std::span<const message> messages;

    // 把对象序列化为 JSON 字符串。
    std::string to_json() const;
};

// 作为 request_room_history_event 的响应发送给客户端
struct room_history_event
{
    // 房间 ID
    std::string_view room_id;

    // 实际的消息
    const message_batch& history;

    // 用户 ID -> 用户名的映射，用于把用户 ID 解析成用户名
    const username_map& usernames;

    // 把对象序列化为 JSON 字符串。
    std::string to_json() const;
};

}  // namespace chat

#endif
