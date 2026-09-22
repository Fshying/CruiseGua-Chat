//
// Copyright (c) 2023-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_SERVICES_COOKIE_AUTH_SERVICE_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_SERVICES_COOKIE_AUTH_SERVICE_HPP

#include <boost/asio/awaitable.hpp>
#include <boost/beast/http/fields.hpp>
#include <boost/system/result.hpp>

#include <cstdint>

#include "business_types.hpp"

// 包含用于设置和校验用户会话的高层函数。
// 会话 ID 存储在 Redis 中（详见 session_store.hpp）。
// 仅凭会话 ID 就足以认证一个客户端（因此它相当于一个认证令牌）。
// 会话 ID 通过 HTTP cookie 发送给客户端，并由客户端带回。

namespace chat {

// 前向声明
class redis_client;
class mysql_client;

class cookie_auth_service
{
    redis_client* redis_;
    mysql_client* mysql_;

public:
    cookie_auth_service(redis_client& redis, mysql_client& mysql) noexcept : redis_(&redis), mysql_(&mysql) {}

    // 为传入的用户 ID 分配一个新的会话 ID（通过存入 Redis 实现），
    // 并返回一个合适的 Set-Cookie 头。
    boost::asio::awaitable<boost::system::result<std::string>> generate_session_cookie(std::int64_t user_id);

    // 校验用户是否已通过 cookie 认证，并返回该已认证用户的 user_id。
    // 如果 cookie 不存在、无效，或者不匹配任何有效的会话 ID，
    // 则返回 errc::auth_required。
    boost::asio::awaitable<boost::system::result<std::int64_t>> user_id_from_cookie(
        const boost::beast::http::fields& req_headers
    );

    // 校验用户是否已通过 cookie 认证，并返回对应的用户。
    // 行为类似 user_id_from_cookie，但还会到 MySQL 中查询该用户。
    boost::asio::awaitable<boost::system::result<user>> user_from_cookie(
        const boost::beast::http::fields& req_headers
    );
};

}  // namespace chat

#endif
