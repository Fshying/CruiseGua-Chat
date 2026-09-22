//
// Copyright (c) 2026 Fshying (Fshying@users.noreply.github.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_BUSINESS_TYPES_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_BUSINESS_TYPES_HPP

#include <string>
#include <unordered_map>

#include "timestamp.hpp"

// 本文件包含业务对象的定义
namespace chat {

// 应用程序的用户
struct user
{
    // 用户 ID
    std::int64_t id;

    // 用户名
    std::string username;
};

// 包含认证信息的用户。
// 将其与 user 定义分开，可以避免在非必要的情况下加载认证信息。
struct auth_user
{
    // 用户 ID
    std::int64_t id;

    // PHC 格式的密码哈希
    std::string hashed_password;
};

// 一条聊天消息
struct message
{
    // 消息 ID
    std::string id;

    // 消息的实际内容
    std::string content;

    // 服务器收到该消息时的 UTC 时间戳
    timestamp_t timestamp;

    // 发送该消息的用户 ID
    std::int64_t user_id{};
};

// 一批房间历史消息
struct message_batch
{
    // 该批次中的消息
    std::vector<message> messages;

    // 如果还有更多消息可以加载，则为 true
    bool has_more{};
};

// 一个聊天室
struct room
{
    // 房间 ID
    std::string id;

    // 面向用户的房间名称
    std::string name;

    // 房间的初始消息历史
    message_batch history;
};

// 从用户 ID 到用户名的映射
using username_map = std::unordered_map<std::int64_t, std::string>;

}  // namespace chat

#endif
