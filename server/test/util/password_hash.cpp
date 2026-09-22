//
// Copyright (c) 2026 Fshying (Fshying@users.noreply.github.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "util/password_hash.hpp"

#include <boost/test/unit_test.hpp>

#include <string_view>

using namespace chat;

BOOST_AUTO_TEST_SUITE(password_hash)

BOOST_AUTO_TEST_CASE(success)
{
    constexpr std::string_view password = "some_password";

    // 对密码做哈希
    auto hash = hash_password(password);

    // 它是用 scrypt 做哈希、用 PHC 格式编码的
    std::string_view prefix = "$scrypt$";
    BOOST_TEST(hash.substr(0, prefix.size()) == prefix);

    // 再次对同一密码做哈希会得到不同的值，因为盐不同
    auto hash2 = hash_password(password);
    BOOST_TEST(hash != hash2);

    // 用正确的密码校验会成功
    BOOST_TEST(verify_password(password, hash));

    // 用错误的密码校验会失败
    BOOST_TEST(!verify_password("bad_password", hash));
}

BOOST_AUTO_TEST_SUITE_END()
