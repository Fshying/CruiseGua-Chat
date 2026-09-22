//
// Copyright (c) 2026 Fshying (Fshying@users.noreply.github.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_ERROR_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_ERROR_HPP

#include <boost/assert/source_location.hpp>
#include <boost/system/error_category.hpp>
#include <boost/system/error_code.hpp>
#include <boost/system/result.hpp>

#include <string>
#include <string_view>
#include <utility>

// 错误管理基础设施。使用 Boost.System 的错误码与错误类别。
// 这与 Asio、Beast 和 Redis 的做法保持一致。

namespace chat {

// 本应用程序内部产生的错误所对应的错误码枚举
enum class errc
{
    redis_parse_error = 1,  // 从 Redis 取回的数据与我们期望的格式不符
    redis_command_failed,   // Redis 命令执行失败（例如参数个数不对）
    websocket_parse_error,  // 从客户端收到的数据与我们期望的格式不符
    username_exists,        // 无法创建用户，用户名重复
    email_exists,           // 无法创建用户，邮箱重复
    not_found,              // 无法取到某个资源，它不存在
    invalid_password_hash,  // 发现了一个格式错误的密码哈希
    already_exists,         // 实体已存在，无法再次创建
    requires_auth,   // 请求的资源需要认证，但未提供凭据
                     // 或者凭据无效
    invalid_base64,  // 试图解码一个非法的 base64 字符串
    uncaught_exception,    // 某个 API 处理函数抛出了意料之外的异常
    invalid_content_type,  // 某个端点收到了不支持的 Content-Type
};

// errc 对应的错误类别
const boost::system::error_category& get_chat_category() noexcept;

// 支持从 errc 构造 error_code
inline boost::system::error_code make_error_code(errc v) noexcept
{
    return boost::system::error_code(static_cast<int>(v), get_chat_category());
}

// 把 ec 记录到 stderr
void log_error(boost::system::error_code ec, std::string_view what, std::string_view diagnostics = "");

}  // namespace chat

// 支持从 errc 构造 error_code
namespace boost {
namespace system {

template <>
struct is_error_code_enum<chat::errc>
{
    static constexpr bool value = true;
};
}  // namespace system
}  // namespace boost

// 返回一个带有源码位置信息的 error_code
#define CHAT_RETURN_ERROR(e)                                                      \
    {                                                                             \
        static constexpr auto loc = BOOST_CURRENT_LOCATION;                       \
        return ::boost::system::error_code(::boost::system::error_code(e), &loc); \
    }

// 同上，但用于 co_return
#define CHAT_CO_RETURN_ERROR(e)                                                      \
    {                                                                                \
        static constexpr auto loc = BOOST_CURRENT_LOCATION;                          \
        co_return ::boost::system::error_code(::boost::system::error_code(e), &loc); \
    }

#endif
