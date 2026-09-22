//
// Copyright (c) 2026 Fshying (Fshying@users.noreply.github.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "server.hpp"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/this_coro.hpp>

#include <exception>
#include <memory>

#include "error.hpp"
#include "http_session.hpp"
#include "shared_state.hpp"

namespace asio = boost::asio;
using namespace chat;

static void log_exception(std::exception_ptr ptr)
{
    try
    {
        // 重新抛出是获取底层异常对象的唯一方式
        std::rethrow_exception(ptr);
    }
    catch (const std::exception& exc)
    {
        log_error(errc::uncaught_exception, "Uncaught exception in HTTP session handler", exc.what());
    }
}

/**
 * @brief 运行 TCP 服务器，持续接受客户端连接并为每个连接派生独立的 HTTP 会话协程。
 *
 * 该函数是一个协程，负责在指定端点上监听 TCP 连接。每当接受到一个新连接时，
 * 会通过 asio::co_spawn 启动一个独立的 run_http_session 协程来处理该连接，
 * 从而使服务器能够立即返回并继续监听后续连接。
 *
 * @param listening_endpoint 服务器监听的 TCP 端点（包含 IP 地址和端口号）。
 * @param st 所有会话共享的状态对象，用于在会话之间传递共享资源（如数据库连接等）。
 *
 * @return asio::awaitable<void> 一个可等待对象；该协程在 io_context 被停止前会持续运行，
 *         正常情况下不会主动返回。若 async_accept 抛出异常，则异常会向上传播。
 */
asio::awaitable<void> chat::run_server(
    asio::ip::tcp::endpoint listening_endpoint,// 监听的端点
    std::shared_ptr<shared_state> st
)
{
    // 获取当前协程关联的执行器（executor）
    auto ex = co_await asio::this_coro::executor;

    // 创建 TCP acceptor 对象，用于接受传入的连接
    asio::ip::tcp::acceptor acceptor(ex);

    // ---------- 配置并启动监听 ----------
    // 配置 acceptor：在指定的端点监听，并允许地址复用
    acceptor.open(listening_endpoint.protocol());
    acceptor.set_option(asio::socket_base::reuse_address(true));
    acceptor.bind(listening_endpoint);
    acceptor.listen();

    // ---------- 主接受循环 ----------
    // 在一个无限循环中接受连接。当 io_context 被停止时，
    // 该协程不会再被调度，循环也就随之结束
    while (true)
    {
        // 接受一个新连接。如果这一步失败，终止程序是最好的选择，
        // 因此这里使用会抛异常的重载（即不使用 asio::as_tuple
        // 或 asio::redirect_error）。
        asio::ip::tcp::socket sock = co_await acceptor.async_accept();

        // ---------- 为新连接派生会话协程 ----------
        // 为这个连接启动一个新会话。每个会话都有自己的协程，
        // 这样我们就能马上回去继续监听新连接。
        asio::co_spawn(
            // 使用与当前协程相同的执行器
            ex,

            // 要运行的函数
            run_http_session(std::move(sock), st),

            // 如果某个会话抛出了异常，只记录日志而不向外传播。
            // 这样单个会话中的未处理错误只影响该会话，
            // 而不会把整个服务器拖垮
            [](std::exception_ptr exc) {
                if (exc)
                    log_exception(exc);
            }
        );
    }
}
