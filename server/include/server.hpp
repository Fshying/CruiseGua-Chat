//
// Copyright (c) 2026 Fshying (Fshying@users.noreply.github.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_LISTENER_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_LISTENER_HPP

#include <boost/asio/awaitable.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <memory>

namespace chat {

// 前向声明
class shared_state;

// 运行 HTTP 服务器。它会在循环中接受连接，直到
// 底层的 I/O 上下文被停止。如果监听器无法启动
// （例如要绑定的端口不可用），则抛出异常。
boost::asio::awaitable<void> run_server(
    boost::asio::ip::tcp::endpoint listening_endpoint,
    std::shared_ptr<shared_state> state
);

}  // namespace chat

#endif
