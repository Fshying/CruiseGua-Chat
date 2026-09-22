//
// Copyright (c) 2026 Fshying (Fshying@users.noreply.github.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "util/async_mutex.hpp"

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/system/error_code.hpp>
#include <boost/test/unit_test.hpp>

#include <chrono>
#include <exception>

using namespace chat;
namespace asio = boost::asio;
using boost::system::error_code;

static constexpr auto rethrow_on_error = [](std::exception_ptr ptr) {
    if (ptr)
        std::rethrow_exception(ptr);
};

// 启动一个协程并运行到结束
static void run_coroutine(asio::awaitable<void> (*fn)())
{
    asio::io_context ctx;
    asio::co_spawn(ctx, fn, rethrow_on_error);
    ctx.run();
}

BOOST_AUTO_TEST_SUITE(async_mutex_)

BOOST_AUTO_TEST_CASE(lock)
{
    run_coroutine([]() -> asio::awaitable<void> {
        // I/O 对象
        async_mutex mtx(co_await asio::this_coro::executor);

        // 加锁
        co_await mtx.lock();
        BOOST_TEST(mtx.locked());

        // 解锁
        mtx.unlock();
        BOOST_TEST(!mtx.locked());
    });
}

BOOST_AUTO_TEST_CASE(lock_with_guard)
{
    run_coroutine([]() -> asio::awaitable<void> {
        // I/O 对象
        async_mutex mtx(co_await asio::this_coro::executor);

        // 加锁
        auto guard = co_await mtx.lock_with_guard();
        BOOST_TEST(mtx.locked());

        // 解锁
        guard.reset();
        BOOST_TEST(!mtx.locked());
    });
}

BOOST_AUTO_TEST_CASE(try_lock)
{
    run_coroutine([]() -> asio::awaitable<void> {
        // I/O 对象
        async_mutex mtx(co_await asio::this_coro::executor);

        // 加锁
        bool ok = mtx.try_lock();
        BOOST_TEST(ok);
        BOOST_TEST(mtx.locked());

        // 对已加锁的互斥量再次加锁会失败
        ok = mtx.try_lock();
        BOOST_TEST(!ok);
        BOOST_TEST(mtx.locked());

        // 解锁
        mtx.unlock();
        BOOST_TEST(!mtx.locked());
    });
}

BOOST_AUTO_TEST_CASE(lock_contention)
{
    run_coroutine([]() -> asio::awaitable<void> {
        // I/O 对象
        async_mutex mtx(co_await asio::this_coro::executor);
        asio::steady_timer timer(co_await asio::this_coro::executor);
        asio::experimental::channel<void(error_code)> chan(co_await asio::this_coro::executor, 1);

        // 给互斥量加锁
        auto guard = co_await mtx.lock_with_guard();
        BOOST_TEST(mtx.locked());

        // 启动另一个协程，让它尝试获取该锁
        asio::co_spawn(
            co_await asio::this_coro::executor,
            [&]() -> asio::awaitable<void> {
                // 此时互斥量应由主协程持有
                BOOST_TEST_REQUIRE(mtx.locked());

                // 加锁再解锁
                co_await mtx.lock();
                mtx.unlock();

                // 通知主协程我们已经完成
                bool ok = chan.try_send(error_code());
                BOOST_TEST_REQUIRE(ok);
            },
            rethrow_on_error
        );

        // 让出执行权，使另一个协程在我们持锁期间尝试获取该互斥量
        timer.expires_after(std::chrono::milliseconds(10));
        co_await timer.async_wait();

        // 解锁
        guard.reset();

        // 等待另一个协程结束
        co_await chan.async_receive();

        BOOST_TEST(!mtx.locked());
    });
}

BOOST_AUTO_TEST_SUITE_END()
