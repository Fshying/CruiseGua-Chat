//
// Copyright (c) 2023-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_SERVICES_REDIS_SERIALIZATION_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_SERVICES_REDIS_SERIALIZATION_HPP

#include <boost/redis/resp3/node.hpp>
#include <boost/system/result.hpp>

#include <span>
#include <vector>

#include "business_types.hpp"

// 包含解析 Redis 响应的函数定义。用于实现 redis_client。
// Boost.Redis 并不开箱即用地支持 stream，因此这里的解析并不简单

namespace chat {

using node_span = std::span<const boost::redis::resp3::node>;

// 解析批量获取多个房间历史记录的结果
// （即多个批量执行的 XREVRANGE）
boost::system::result<std::vector<message_batch>> parse_room_history_batch(node_span from);

// 解析一批 XADD 的响应。每个 XADD 的响应是一个字符串，
// 其中包含所插入记录的 ID。用 vector<string> 调用 execute 是不行的，
// 因为 Boost.Redis 会尝试把结果解析成「单个包含字符串数组的响应」，
// 而不是「多个各自包含单个字符串的响应」
boost::system::result<std::vector<std::string>> parse_batch_xadd_response(node_span from);

// 我们把消息以序列化 JSON 对象的形式存放在 stream 中。
// 把一条消息序列化成它的 JSON 表示
std::string serialize_redis_message(const message& msg);

}  // namespace chat

#endif
