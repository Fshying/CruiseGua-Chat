//
// Copyright (c) 2026 Fshying (Fshying@users.noreply.github.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "util/websocket.hpp"

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/websocket/rfc6455.hpp>
#include <boost/beast/websocket/stream.hpp>
#include <boost/system/error_code.hpp>
#include <boost/system/result.hpp>

#include <memory>
#include <string_view>

#include "error.hpp"
#include "util/async_mutex.hpp"

namespace asio = boost::asio;
namespace beast = boost::beast;
using namespace chat;
using boost::system::error_code;
using boost::system::result;

struct websocket::impl
{
    // 真正的 websocket 对象
    beast::websocket::stream<beast::tcp_stream> ws;

    // 用于升级协议的 HTTP 请求
    websocket::upgrade_request_type upgrade_request;

    // 读取客户端数据的缓冲区
    beast::flat_buffer read_buffer;

    // 用于把写操作串行化的互斥量
    async_mutex write_mtx_;

    // 确保不会同时发起两个读操作
    bool reading{false};

    impl(
        asio::ip::tcp::socket&& sock,
        websocket::upgrade_request_type&& upgrade_req,
        beast::flat_buffer&& buff
    )
        : ws(std::move(sock)),
          upgrade_request(std::move(upgrade_req)),
          read_buffer(std::move(buff)),
          write_mtx_(ws.get_executor())
    {
    }

    // 用 RAII 方式设置和清除 reading 标志
    struct read_guard_deleter
    {
        void operator()(impl* self) const noexcept { self->reading = false; }
    };
    using read_guard = std::unique_ptr<impl, read_guard_deleter>;
    read_guard lock_reads() noexcept
    {
        reading = true;
        return read_guard(this);
    }
};

static std::string_view buffer_to_sv(asio::const_buffer buff) noexcept
{
    return std::string_view(static_cast<const char*>(buff.data()), buff.size());
}

websocket::websocket(asio::ip::tcp::socket sock, upgrade_request_type&& req, beast::flat_buffer buff)
    : impl_(new impl(std::move(sock), std::move(req), std::move(buff)))
{
}

websocket::websocket(websocket&& rhs) noexcept : impl_(std::move(rhs.impl_)) {}

websocket& websocket::operator=(websocket&& rhs) noexcept
{
    impl_ = std::move(rhs.impl_);
    return *this;
}

websocket::~websocket() {}

const websocket::upgrade_request_type& websocket::upgrade_request() const noexcept
{
    return impl_->upgrade_request;
}

asio::awaitable<error_code> websocket::accept()
{
    // 为 websocket 设置建议的超时参数
    impl_->ws.set_option(beast::websocket::stream_base::timeout::suggested(beast::role_type::server));

    // 设置装饰器，用于修改握手响应中的 Server 字段
    impl_->ws.set_option(beast::websocket::stream_base::decorator([](beast::websocket::response_type& res) {
        res.set(
            beast::http::field::server,
            std::string(BOOST_BEAST_VERSION_STRING) + " websocket-chat-multi"
        );
    }));

    // 接受 websocket 握手
    auto [ec] = co_await impl_->ws.async_accept(impl_->upgrade_request, asio::as_tuple);
    co_return ec;
}

asio::awaitable<result<std::string_view>> websocket::read()
{
    assert(!impl_->reading);

    error_code ec;

    // 执行读取
    {
        auto guard = impl_->lock_reads();
        impl_->read_buffer.clear();
        co_await impl_->ws.async_read(impl_->read_buffer, asio::redirect_error(ec));
    }

    // 检查结果
    if (ec)
        co_return ec;

    // 转换成 string_view（不会发生拷贝）
    co_return buffer_to_sv(impl_->read_buffer.data());
}

asio::awaitable<error_code> websocket::write_locked_impl(std::string_view buff)
{
    assert(impl_->write_mtx_.locked());

    // 执行写入
    error_code ec;
    co_await impl_->ws.async_write(asio::buffer(buff), asio::redirect_error(ec));
    co_return ec;
}

asio::awaitable<error_code> websocket::write(std::string_view message)
{
    // 等待连接变为空闲
    auto guard = co_await lock_writes();

    // 写入
    co_return co_await write_locked(message, guard);
}

asio::awaitable<void> websocket::lock_writes_impl() { return impl_->write_mtx_.lock(); }

void websocket::unlock_writes_impl() noexcept { impl_->write_mtx_.unlock(); }

asio::awaitable<error_code> websocket::close(unsigned close_code)
{
    error_code ec;
    co_await impl_->ws.async_close(beast::websocket::close_reason(close_code), asio::redirect_error(ec));
    co_return ec;
}
