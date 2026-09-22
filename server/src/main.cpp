//
// Copyright (c) 2023-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>

#include <cstdlib>
#include <exception>
#include <iostream>

#include "server.hpp"
#include "services/mysql_client.hpp"
#include "services/redis_client.hpp"
#include "shared_state.hpp"

namespace asio = boost::asio;
using namespace chat;

static void main_impl(int argc, char* argv[])
{
    //检查命令行参数：ip地址、端口号和文档根目录
    if (argc != 4)
    {
        std::cerr << "Usage: " << argv[0] << " <address> <port> <doc_root>\n"
                  << "Example:\n"
                  << "    " << argv[0] << " 0.0.0.0 8080 .\n";
        exit(EXIT_FAILURE);
    }

    //
    const char* doc_root = argv[3];                               // 静态文件所在路径
    const char* ip = argv[1];                                     // 服务器监听的 IP 地址
    auto port = static_cast<unsigned short>(std::atoi(argv[2]));  // 端口

    // 应用程序运行的事件循环。服务器是单线程的，
    // 因此把并发提示设置为 1
    asio::io_context ctx(1);

    // 所有连接共享的单例对象
    auto st = std::make_shared<shared_state>(doc_root, ctx.get_executor());

    // 服务器监听的物理端点
    asio::ip::tcp::endpoint listening_endpoint(asio::ip::make_address(ip), port);

    // signal_set 用于捕获 SIGINT 和 SIGTERM，
    // 从而优雅地退出
    asio::signal_set signals(ctx.get_executor(), SIGINT, SIGTERM);

    // 启动 Redis 连接
    st->redis().start_run();

    // 启动 MySQL 连接池
    st->mysql().start_run();

    // 开始监听 HTTP 连接。它会一直运行，直到 context 被停止
    asio::co_spawn(
        // 运行协程所在的执行上下文
        ctx,

        // 实际要运行的协程，以 awaitable 的形式传入
        run_server(listening_endpoint, st),

        // 协程结束后执行。把协程中抛出的任何异常
        // 传播到 main
        [](std::exception_ptr exc) {
            if (exc)
                std::rethrow_exception(exc);
        }
    );

    // 捕获 SIGINT 和 SIGTERM，以便干净地关闭服务器
    signals.async_wait([st, &ctx](boost::system::error_code, int) {
        // 停止 Redis 重连循环
        st->redis().cancel();

        // 停止 MySQL 重连循环
        st->mysql().cancel();

        // 停止 io_context。这会让 run() 返回
        ctx.stop();
    });

    // 运行 io_context。它会阻塞在这里，直到 context 被信号停止，
    // 并且所有尚未完成的异步任务都执行完毕。
    ctx.run();

    // （如果能执行到这里，说明我们收到了 SIGINT 或 SIGTERM）
}

int main(int argc, char* argv[])
{
    try
    {
        main_impl(argc, argv);
    }
    catch (const std::exception& err)
    {
        std::cerr << "Exception in main(): " << err.what() << std::endl;
        return EXIT_FAILURE;
    }
}
