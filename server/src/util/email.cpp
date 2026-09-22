//
// Copyright (c) 2026 Fshying (Fshying@users.noreply.github.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "util/email.hpp"

#include <boost/regex/v5/icu.hpp>
#include <boost/regex/v5/regex_match.hpp>

static constexpr std::string_view email_regex_str =
    R"REGEX(^(([^<>()\[\]\\.,;:\s@"]+(\.[^<>()\[\]\\.,;:\s@"]+)*)|(".+"))@((\[[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}])|(([a-zA-Z\-0-9]+\.)+[a-zA-Z]{2,}))$)REGEX";

// 邮箱可能包含 Unicode 字符。我们使用支持 Unicode 的正则表达式
// 来校验它。这需要 ICU 支持。如果不需要
// Unicode 支持，可以改用普通的 boost::regex 或 std::regex
static const auto email_regex = boost::make_u32regex(
    email_regex_str.begin(),
    email_regex_str.end(),
    boost::regex_constants::ECMAScript
);

bool chat::is_email(std::string_view email)
{
    return boost::regex_match(email.begin(), email.end(), email_regex);
}
