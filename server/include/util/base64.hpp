//
// Copyright (c) 2023-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_UTIL_BASE64_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_UTIL_BASE64_HPP

#include <boost/system/result.hpp>

#include <span>
#include <string>
#include <string_view>

namespace chat {

// 把给定输入编码为 base64 字符串。如果 !with_padding，
// 则不会在字符串中添加填充。
std::string base64_encode(std::span<const unsigned char> input, bool with_padding = true);

// 解码给定输入，把它按 base64 字符串来解释。如果 !with_padding，
// 则字符串末尾不要求有填充。
boost::system::result<std::vector<unsigned char>> base64_decode(
    std::string_view input,
    bool with_padding = true
);

}  // namespace chat

#endif
