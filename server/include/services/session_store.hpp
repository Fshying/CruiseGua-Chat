//
// Copyright (c) 2026 Fshying (Fshying@users.noreply.github.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_SERVICES_SESSION_STORE_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_SERVICES_SESSION_STORE_HPP

#include <boost/asio/awaitable.hpp>
#include <boost/system/result.hpp>

#include <chrono>
#include <string_view>

// 包含用于管理用户会话的函数。会话以 (session_id, user_id) 键值对的形式
// 存储在 Redis 中，并带有一定的过期时间。会话会一直有效，
// 直到对应的 Redis 键过期或被删除。
// 这些函数相对底层。在 API 处理函数中检查认证时，
// 请优先使用 cookie_auth_service.hpp。

namespace chat {

// 前向声明
class redis_client;

class session_store
{
    redis_client* redis_;

public:
    session_store(redis_client& cli) noexcept : redis_(&cli) {}

    // 为给定用户生成并存储一个新的会话，并指定其有效时长
    boost::asio::awaitable<boost::system::result<std::string>> generate_session_id(
        std::int64_t user_id,
        std::chrono::seconds session_duration
    );

    // 获取传入的会话 ID 对应的用户 ID。
    // 会话 ID 由 generate_session_id 生成。
    // 如果给定的会话 ID 不存在，返回 errc::not_found。
    boost::asio::awaitable<boost::system::result<std::int64_t>> get_user_by_session(
        std::string_view session_id
    );
};

}  // namespace chat

#endif
