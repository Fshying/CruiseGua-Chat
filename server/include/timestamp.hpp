//
// Copyright (c) 2023-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_TIMESTAMP_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_TIMESTAMP_HPP

#include <chrono>

// 处理时间戳的辅助工具。
// 时间戳的序列化表示是一个 int64_t，内容为自 UNIX 纪元起的毫秒数

namespace chat {

// 时间戳最终会展示给用户，因此需要与系统时钟一致
using timestamp_t = std::chrono::system_clock::time_point;

// 把时间戳转换为其序列化表示
inline std::int64_t serialize_timestamp(timestamp_t input) noexcept
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(input.time_since_epoch()).count();
}

// 从序列化表示创建时间戳
inline timestamp_t parse_timestamp(std::int64_t input) noexcept
{
    return timestamp_t(std::chrono::milliseconds(input));
}

}  // namespace chat

#endif
