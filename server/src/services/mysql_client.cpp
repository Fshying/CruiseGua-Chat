//
// Copyright (c) 2026 Fshying (Fshying@users.noreply.github.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "error.hpp"
#include "services/mysql_client.hpp"

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/describe/class.hpp>
#include <boost/mysql/any_address.hpp>
#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/common_server_errc.hpp>
#include <boost/mysql/connection.hpp>
#include <boost/mysql/connection_pool.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/handshake_params.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/static_results.hpp>
#include <boost/mysql/with_params.hpp>
#include <boost/system/error_code.hpp>
#include <boost/system/result.hpp>

#include <cstdlib>
#include <exception>
#include <string>
#include <string_view>

#include "business_types.hpp"
#include "business_types_metadata.hpp"  // static_results 需要它

using namespace chat;
namespace mysql = boost::mysql;
namespace asio = boost::asio;
using boost::system::error_code;
using boost::system::result;

namespace {

// 返回某个环境变量的值；如果该变量未定义，则返回 default_value
std::string getenv_or(const char* name, const char* default_value)
{
    const char* res = std::getenv(name);
    return res == nullptr ? default_value : res;
}

// 返回要使用的连接池参数
mysql::pool_params get_pool_params()
{
    return {
        // 服务器地址。主机名来自环境变量，
        // 端口使用默认值。
        .server_address = mysql::host_and_port{getenv_or("MYSQL_HOST", "localhost")},

        // 登录使用的用户名
        .username = "root",

        // 密码
        .password = getenv_or("MYSQL_PASSWORD", "123456"),

        // 要使用的数据库
        .database = "servertech_chat",
    };
}

// 把错误记录到 std::cerr
void log_mysql_error(error_code ec, std::string_view what, const mysql::diagnostics& diagnostics)
{
    log_error(
        ec,
        what,
        diagnostics.client_message().empty() ? diagnostics.server_message() : diagnostics.client_message()
    );
}

class mysql_client_impl final : public mysql_client
{
    mysql::connection_pool pool_;

public:
    mysql_client_impl(asio::any_io_executor ex) : pool_(std::move(ex), get_pool_params()) {}

    void start_run() override final
    {
        asio::co_spawn(
            pool_.get_executor(),
            [pool = &pool_]() { return pool->async_run(asio::use_awaitable); },
            [](std::exception_ptr exc) {
                if (exc)
                    std::rethrow_exception(exc);
            }
        );
    }

    void cancel() override final { pool_.cancel(); }

    asio::awaitable<result<std::int64_t>> create_user(
        std::string_view username,
        std::string_view email,
        std::string_view hashed_password
    ) final override
    {
        error_code ec;
        mysql::diagnostics diag;
        mysql::results result;

        // 获取一个连接
        mysql::pooled_connection conn = co_await pool_.async_get_connection(diag, asio::redirect_error(ec));
        if (ec)
        {
            log_mysql_error(ec, "MySQL error when retrieving a connection", diag);
            co_return ec;
        }

        // 执行插入
        co_await conn->async_execute(
            mysql::with_params(
                "INSERT INTO users (username, email, password) VALUES ({}, {}, {})",
                username,
                email,
                hashed_password
            ),
            result,
            diag,
            asio::redirect_error(ec)
        );

        // 检测重复
        if (ec == mysql::common_server_errc::er_dup_entry)
        {
            // 根据 MySQL 文档，er_dup_entry 的错误消息
            // 格式为：Duplicate entry '%s' for key %d
            if (diag.server_message().ends_with("'users.username'"))
                co_return errc::username_exists;
            else if (diag.server_message().ends_with("'users.email'"))
                co_return errc::email_exists;
        }

        // 未知错误
        if (ec)
        {
            log_mysql_error(ec, "MySQL error while creating user", diag);
            co_return ec;
        }

        // 完成。为了能兼容任意列类型，MySQL 把 last_insert_id 报告为 uint64_t，
        // 但我们的 id 字段定义是 BIGINT（int64）。
        // 连接会自动归还到连接池。语句（statement）
        // 也会由连接池自动释放。
        co_return static_cast<std::int64_t>(result.last_insert_id());
    }

    asio::awaitable<result<auth_user>> get_user_by_email(std::string_view email) final override
    {
        mysql::diagnostics diag;
        error_code ec;

        // 获取一个连接
        auto conn = co_await pool_.async_get_connection(diag, asio::redirect_error(ec));
        if (ec)
        {
            log_mysql_error(ec, "MySQL error when retrieving a connection", diag);
            co_return ec;
        }

        // static_results 要求 SQL 字段名
        // 与 C++ 结构体字段名一致，因此这里使用 SQL 别名
        mysql::static_results<auth_user> result;
        co_await conn->async_execute(
            mysql::with_params("SELECT id, password AS hashed_password FROM users WHERE email = {}", email),
            result,
            diag,
            asio::redirect_error(ec)
        );
        if (ec)
        {
            log_mysql_error(ec, "MySQL error while retrieving user by email", diag);
            co_return ec;
        }

        // 返回结果。
        // 连接会自动归还到连接池。语句（statement）
        // 也会由连接池自动释放。
        if (result.rows().empty())
            co_return errc::not_found;
        co_return std::move(result.rows()[0]);
    }

    asio::awaitable<result<user>> get_user_by_id(std::int64_t user_id) final override
    {
        mysql::diagnostics diag;
        error_code ec;

        // 获取一个连接
        auto conn = co_await pool_.async_get_connection(diag, asio::redirect_error(ec));
        if (ec)
        {
            log_mysql_error(ec, "MySQL error when retrieving a connection", diag);
            co_return ec;
        }

        // 执行查询
        mysql::static_results<user> result;
        co_await conn->async_execute(
            mysql::with_params("SELECT id, username  FROM users WHERE id = {}", user_id),
            result,
            diag,
            asio::redirect_error(ec)
        );
        if (ec)
        {
            log_mysql_error(ec, "MySQL error when retrieving a user by id", diag);
            co_return ec;
        }

        // 返回结果。
        // 连接会自动归还到连接池。语句（statement）
        // 也会由连接池自动释放。
        if (result.rows().empty())
            co_return errc::not_found;
        co_return std::move(result.rows()[0]);
    }

    asio::awaitable<result<username_map>> get_usernames(std::span<const std::int64_t> user_ids) final override
    {
        // 确认至少有一个用户 ID。
        // 否则生成的查询语句可能是非法的。
        if (user_ids.empty())
            co_return username_map();

        mysql::diagnostics diag;
        error_code ec;

        // 获取一个连接
        auto conn = co_await pool_.async_get_connection(diag, asio::redirect_error(ec));
        if (ec)
        {
            log_mysql_error(ec, "MySQL error when retrieving a connection", diag);
            co_return ec;
        }

        // 执行查询。
        // 之所以可以放心这么做，是因为前面已经确认 user_ids 非空。
        // 否则客户端生成的查询语句会是非法的。
        using row_t = std::tuple<std::int64_t, std::string>;
        mysql::static_results<row_t> result;
        co_await conn->async_execute(
            mysql::with_params("SELECT id, username FROM users WHERE id IN ({})", user_ids),
            result,
            diag,
            asio::redirect_error(ec)
        );
        if (ec)
        {
            log_mysql_error(ec, "MySQL error while retrieving the username map", diag);
            co_return ec;
        }

        // 我们没有做任何会改变连接状态的操作，因此可以
        // 显式归还连接，表示无需重置。
        conn.return_without_reset();

        // 返回结果
        std::unordered_map<std::int64_t, std::string> res;
        for (auto& elm : result.rows())
            res.insert({std::get<0>(elm), std::move(std::get<1>(elm))});

        // 完成
        co_return res;
    }
};

}  // namespace

std::unique_ptr<mysql_client> chat::create_mysql_client(asio::any_io_executor ex)
{
    return std::unique_ptr<mysql_client>{new mysql_client_impl(std::move(ex))};
}
