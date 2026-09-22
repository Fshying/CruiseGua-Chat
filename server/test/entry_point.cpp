//
// Copyright (c) 2026 Fshying (Fshying@users.noreply.github.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// 单元测试的 main 函数。我们采用这种写法而不是 BOOST_TEST_MODULE，
// 是为了能够预编译 <boost/test/unit_test.hpp>

#include <boost/test/unit_test.hpp>

#ifdef BOOST_TEST_ALTERNATIVE_INIT_API
int main(int argc, char* argv[])
{
    return ::boost::unit_test::unit_test_main([] { return true; }, argc, argv);
}
#else
::boost::unit_test::test_suite* init_unit_test_suite(int, char*[]) { return nullptr; }
#endif
