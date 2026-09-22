//
// Copyright (c) 2026 Fshying (Fshying@users.noreply.github.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_UTIL_ASYNC_MUTEX_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_UTIL_ASYNC_MUTEX_HPP

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <boost/system/error_code.hpp>

#include <cassert>
#include <memory>

namespace chat {

// 一个异步互斥量，用于在异步代码中保证互斥。它类似
// Python 的 asyncio.Mutex。注意它不是线程安全的——
// 它保证的是协程之间的互斥。
// TODO：这里大概可以再简化
class async_mutex
{
    // 互斥量是否已被锁定？
    bool locked_{false};

    // 充当条件变量，使得等待获取互斥量的协程
    // 能在别的协程释放它时收到通知
    boost::asio::experimental::channel<void(boost::system::error_code)> chan_;

    struct guard_deleter
    {
        void operator()(async_mutex* self) const noexcept { self->unlock(); }
    };

public:
    // 构造函数、赋值运算符、析构函数
    async_mutex(boost::asio::any_io_executor ex) : chan_(std::move(ex)) {}
    async_mutex(const async_mutex&) = delete;
    async_mutex(async_mutex&&) = default;
    async_mutex& operator=(const async_mutex&) = delete;
    async_mutex& operator=(async_mutex&&) = default;
    ~async_mutex() = default;

    // 互斥量是否已被锁定？
    bool locked() const noexcept { return locked_; }

    // 挂起当前协程，直到能够获取互斥量，然后获取它
    boost::asio::awaitable<void> lock()
    {
        // 大多数情况下这个循环会执行零次（未锁定时）
        // 或一次（已锁定时）。竞态条件可能导致某个并非由 async_receive
        // 唤醒的协程抢先获得锁，这个循环就是用来防止这种情况的。
        while (locked_)
        {
            // 等待通知
            co_await chan_.async_receive();
        }

        // 标记为已锁定
        locked_ = true;
    }

    // 尝试获取，但不挂起
    bool try_lock() noexcept
    {
        if (locked_)
            return false;
        locked_ = true;
        return true;
    }

    // 解锁。调用时互斥量必须处于已锁定状态
    void unlock() noexcept
    {
        // 解锁
        assert(locked_);
        locked_ = false;

        // 通知所有正在等待的协程
        chan_.try_send(boost::system::error_code());
    }

    using guard = std::unique_ptr<async_mutex, guard_deleter>;
    boost::asio::awaitable<guard> lock_with_guard()
    {
        co_await lock();
        co_return guard(this);
    }
};

}  // namespace chat

#endif
