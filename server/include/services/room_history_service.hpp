//
// Copyright (c) 2023-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_SERVICES_ROOM_HISTORY_SERVICE_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_SERVICES_ROOM_HISTORY_SERVICE_HPP

#include <boost/asio/awaitable.hpp>
#include <boost/system/result.hpp>

#include <span>
#include <string_view>
#include <utility>

#include "business_types.hpp"

// 包含用于获取房间聊天历史的函数

namespace chat {

class mysql_client;
class redis_client;

class room_history_service
{
    redis_client* redis_;
    mysql_client* mysql_;

public:
    room_history_service(redis_client& redis, mysql_client& mysql) noexcept : redis_(&redis), mysql_(&mysql)
    {
    }

    // 批量获取多个房间的历史记录。返回的 vector
    // 会与 room_ids 的每个元素一一对应。
    // 同时返回一个 (user_id, username) 映射，其中包含
    // 在所取历史记录中出现过的每个用户。
    // 如果某个 room_id 不存在，则为该房间返回一个空的 message_batch。
    // 如果某条消息引用了不存在的 user_id，
    // 该条目不会出现在映射中
    boost::asio::awaitable<boost::system::result<std::pair<std::vector<message_batch>, username_map>>> get_room_history(
        std::span<const std::string_view> room_ids
    );

    // 与上面相同，但只针对单个房间。
    boost::asio::awaitable<boost::system::result<std::pair<message_batch, username_map>>> get_room_history(
        std::string_view room_id
    );
};

}  // namespace chat

#endif
