//
// Copyright (c) 2026 Fshying (Fshying@users.noreply.github.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "util/cookie.hpp"

#include <boost/test/unit_test.hpp>

#include <string_view>

using namespace chat;

BOOST_AUTO_TEST_SUITE(cookie)

BOOST_AUTO_TEST_CASE(set_cookie_builder_success)
{
    std::chrono::seconds max_age{120};

    // 常规情况
    BOOST_TEST(
        set_cookie_builder("cookie_name", "cookie_value").build_header() == "cookie_name=cookie_value"
    );

    // 值中可以包含某些特殊字符
    BOOST_TEST(
        set_cookie_builder("cookie_name", "val=!uewith%char$s").build_header() ==
        "cookie_name=val=!uewith%char$s"
    );

    // 各个属性单独设置
    BOOST_TEST(set_cookie_builder("name", "val").http_only(true).build_header() == "name=val; HttpOnly");
    BOOST_TEST(set_cookie_builder("name", "val").secure(true).build_header() == "name=val; Secure");
    BOOST_TEST(set_cookie_builder("name", "val").max_age(max_age).build_header() == "name=val; Max-Age=120");
    BOOST_TEST(
        set_cookie_builder("name", "val").same_site(same_site_t::none).build_header() ==
        "name=val; SameSite=None"
    );

    // 同时指定所有属性也可以正常工作
    BOOST_TEST(
        set_cookie_builder("name", "val")
            .http_only(true)
            .max_age(max_age)
            .same_site(same_site_t::strict)
            .secure(true)
            .build_header() == "name=val; HttpOnly; Max-Age=120; SameSite=Strict; Secure"
    );
}

BOOST_AUTO_TEST_CASE(cookie_list_default_ctor)
{
    cookie_list l;
    BOOST_TEST((l.begin() == l.end()));
}

BOOST_AUTO_TEST_CASE(cookie_list_)
{
    struct
    {
        std::string_view header;
        std::vector<cookie_pair> expected;
    } test_cases[] = {
        {"",                                  {}                                                    },
        {"name=val",                          {{"name", "val"}}                                     },
        {"name=\"val\"",                      {{"name", "\"val\""}}                                 },
        {"name=val; lang=en-US",              {{"name", "val"}, {"lang", "en-US"}}                  },
        {"    name=val; lang=en-US  ",        {{"name", "val"}, {"lang", "en-US"}}                  },
        {"name=val; lang=en-US; key=other  ", {{"name", "val"}, {"lang", "en-US"}, {"key", "other"}}},
        {"invalid; lang=en-US",               {}                                                    },
        {"name=val; invalid",                 {{"name", "val"}}                                     },
        {"name=val;lang=en-US",               {{"name", "val"}}                                     },
        {"name=val;",                         {{"name", "val"}}                                     },
        {"name=val; lang=\"invalid",          {{"name", "val"}}                                     },
    };

    for (const auto& tc : test_cases)
    {
        BOOST_TEST_CONTEXT(tc.header)
        {
            cookie_list l{tc.header};
            std::vector<cookie_pair> actual{l.begin(), l.end()};
            BOOST_TEST(actual == tc.expected);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()
