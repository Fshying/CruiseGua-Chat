//
// Copyright (c) 2023-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_UTIL_EMAIL_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_UTIL_EMAIL_HPP

namespace chat {

// 如果给定字符串是合法的邮箱（通过模式匹配判断），返回 true
bool is_email(std::string_view str);

}  // namespace chat

#endif
