//
// Copyright (c) 2026 Fshying (Fshying@users.noreply.github.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_UTIL_WEBSOCKET_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_UTIL_WEBSOCKET_HPP

#include <boost/asio/awaitable.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/system/error_code.hpp>

#include <memory>
#include <string_view>

namespace chat {

// 对 Beast websocket stream 的封装，它负责处理并发写，
// 并通过把 Beast 的模板实例化放在单独的 .cpp 文件里来缩短编译时间。
class websocket
{
    // pimpl 惯用法，用于避免包含重量级的 Beast 头文件
    struct impl;
    std::unique_ptr<impl> impl_;

    boost::asio::awaitable<boost::system::error_code> write_locked_impl(std::string_view buff);
    boost::asio::awaitable<void> lock_writes_impl();
    void unlock_writes_impl() noexcept;

    struct write_guard_deleter
    {
        void operator()(websocket* sock) const noexcept { sock->unlock_writes_impl(); }
    };

public:
    using upgrade_request_type = boost::beast::http::request<boost::beast::http::string_body>;

    // 构造函数、赋值运算符、析构函数
    websocket(
        boost::asio::ip::tcp::socket sock,
        upgrade_request_type&& upgrade_request,
        boost::beast::flat_buffer buffer
    );
    websocket(const websocket&) = delete;
    websocket(websocket&&) noexcept;
    websocket& operator=(const websocket&) = delete;
    websocket& operator=(websocket&&) noexcept;
    ~websocket();

    // 返回用于升级协议的 HTTP 请求
    const upgrade_request_type& upgrade_request() const noexcept;

    // 执行 websocket 握手。必须在任何其他操作之前调用
    boost::asio::awaitable<boost::system::error_code> accept();

    // 从客户端读取一条消息。返回的视图在下一次读取执行前有效。
    // 同一时刻只应有一个未完成的读操作
    // （与写不同，读操作不会被串行化）。
    boost::asio::awaitable<boost::system::result<std::string_view>> read();

    // 向客户端写入一条消息。写操作是串行化的：
    // 对同一个 websocket 可以安全地并发发起两个写操作。
    // 一次 write 大致等价于 lock_writes() + write_locked() + 释放 guard
    boost::asio::awaitable<boost::system::error_code> write(std::string_view buff);

    // 锁定写操作，直到返回的 guard 被销毁。
    // 在此期间其他调用 write 的协程会被挂起，直到 guard 被释放。
    using write_guard = std::unique_ptr<websocket, write_guard_deleter>;
    boost::asio::awaitable<write_guard> lock_writes()
    {
        co_await lock_writes_impl();
        co_return write_guard(this);
    }

    // 绕过写锁直接写入。调用本函数之前必须已经调用过 lock_writes()。
    boost::asio::awaitable<boost::system::error_code> write_locked(
        std::string_view buff,
        [[maybe_unused]] write_guard& guard
    )
    {
        assert(guard.get() != nullptr);
        return write_locked_impl(buff);
    }

    // 关闭 websocket，并把 close_code 发送给客户端。
    boost::asio::awaitable<boost::system::error_code> close(unsigned close_code);
};

}  // namespace chat

#endif
