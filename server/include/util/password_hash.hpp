//
// Copyright (c) 2023-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_UTIL_PASSWORD_HASH_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_UTIL_PASSWORD_HASH_HPP

#include <string>
#include <string_view>

namespace chat {

// 使用 scrypt 和随机盐对密码做哈希。返回一个 PHC 格式的字符串，
// 它可以存入数据库，并传给 verify_password
std::string hash_password(std::string_view passwd);

// 检查传入的密码是否与给定的密码哈希匹配
bool verify_password(std::string_view passwd, std::string_view hashed_passwd);

}  // namespace chat

#endif
