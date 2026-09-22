//
// Copyright (c) 2026 Fshying (Fshying@users.noreply.github.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_SERVICES_MYSQL_CLIENT_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_SERVICES_MYSQL_CLIENT_HPP

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/system/result.hpp>
#include <boost/variant2/variant.hpp>

#include <memory>
#include <span>
#include <string_view>

#include "business_types.hpp"

// 一个高层、专用的 MySQL 客户端。它实现服务器所需的各种操作，
// 把实际的 SQL 操作抽象掉。

namespace chat {

// 使用接口是为了缩短编译时间并提高可测试性
class mysql_client
{
public:
    virtual ~mysql_client() {}

    // 以分离（detached）模式启动 MySQL 连接池任务。此函数必须被调用一次，
    // 其他操作才能正常推进，重连循环也才能持续运行
    virtual void start_run() = 0;

    // 取消 MySQL 连接池任务。在关闭时调用
    virtual void cancel() = 0;

    // 用给定的属性创建一个新的用户对象。
    // 成功时返回新建对象的 ID。
    // 如果传入的用户名或邮箱已存在，则返回 errc::username_exists
    // 或 errc::email_exists。
    virtual boost::asio::awaitable<boost::system::result<std::int64_t>> create_user(
        std::string_view username,
        std::string_view email,
        std::string_view hashed_password
    ) = 0;

    // 根据用户邮箱获取其认证信息。
    // 如果用户不存在，返回 errc::not_found。
    virtual boost::asio::awaitable<boost::system::result<auth_user>> get_user_by_email(std::string_view email
    ) = 0;

    // 根据 ID 获取用户。
    // 如果不存在，返回 errc::not_found。
    virtual boost::asio::awaitable<boost::system::result<user>> get_user_by_id(std::int64_t user_id) = 0;

    // 获取传入的 user_ids 对应的用户名。
    // 出于效率考虑，查询以批量方式进行。
    // 如果某个用户 ID 不存在，则不会出现在返回的映射中。
    virtual boost::asio::awaitable<boost::system::result<username_map>> get_usernames(
        std::span<const std::int64_t> user_ids
    ) = 0;
};

// 创建 mysql_client 的具体实现
std::unique_ptr<mysql_client> create_mysql_client(boost::asio::any_io_executor ex);

}  // namespace chat

#endif
