//
// Copyright (c) 2026 Fshying (Fshying@users.noreply.github.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_API_CHAT_WEBSOCKET_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_API_CHAT_WEBSOCKET_HPP

#include <boost/asio/awaitable.hpp>
#include <boost/system/error_code.hpp>

#include <memory>

#include "util/websocket.hpp"

namespace chat {

// 前向声明
class shared_state;

// 运行聊天 websocket 会话，直到发生错误。
boost::asio::awaitable<boost::system::error_code> handle_chat_websocket(
    websocket socket,
    std::shared_ptr<shared_state> state
);

}  // namespace chat

#endif
