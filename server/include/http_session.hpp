//
// Copyright (c) 2026 Fshying (Fshying@users.noreply.github.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_HTTP_SESSION_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_HTTP_SESSION_HPP

#include <boost/asio/awaitable.hpp>

#include <memory>

namespace chat {

// 前向声明
class shared_state;

// 运行一个 HTTP 会话，直到连接关闭或遇到错误。
// 视客户端请求的内容，它会通过 HTTP 提供静态文件，
// 或者运行一个 websocket 会话。
boost::asio::awaitable<void> run_http_session(
    boost::asio::ip::tcp::socket&& socket,
    std::shared_ptr<shared_state> state
);

}  // namespace chat

#endif
