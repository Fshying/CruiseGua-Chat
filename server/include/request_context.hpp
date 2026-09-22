//
// Copyright (c) 2023-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_REQUEST_CONTEXT_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_REQUEST_CONTEXT_HPP

#include <boost/beast/http/error.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/beast/http/fields.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/message_generator.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/beast/http/verb.hpp>
#include <boost/url/error_types.hpp>
#include <boost/url/url_view.hpp>

#include <cassert>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#include "api/api_types.hpp"
#include "error.hpp"

// 包含 request_context 类，它封装一个 Boost.Beast HTTP 请求，
// 并提供便捷的方式来构建 HTTP 响应。

namespace chat {

// 提供构建 HTTP 响应的便捷方式。该类由 request_context 实例化。
// 所有真正返回响应的函数最多只能调用一次，
// 因为它们会移动内部状态。
class response_builder
{
public:
    // HTTP 响应的类型。我们使用类型擦除的 message_generator 类，
    // 以便用统一的接口承载不同类型的 body（例如字符串、文件……）。
    using response_type = boost::beast::http::message_generator;

    // 在响应中设置一个 cookie。set_cookie_header 必须是完整的
    // Set-Cookie 头的值，即 cookie_builder::build_header() 的返回值
    response_builder& set_cookie(std::string_view set_cookie_header)
    {
        assert(!used_);
        header_.set(boost::beast::http::field::set_cookie, set_cookie_header);
        return *this;
    }

    // 设置响应的 content-type
    response_builder& set_content_type(std::string_view value)
    {
        assert(!used_);
        header_.set(boost::beast::http::field::content_type, value);
        return *this;
    }

    // 以响应形式发送一个文件。如果 only_headers 为 true，则只发送
    // 头部而不发送 body（对 HEAD 请求有用）。
    // 如果文件不存在，则发送 404 响应。
    // path 必须是绝对路径。这里不对 path 做任何净化处理——
    // 必须由调用方小心防止目录遍历攻击。
    response_type file_response(const char* path, bool only_headers = false);

    // 发送一个带 JSON body 的 200 响应。类型 T 必须有一个 to_json() const
    // 成员函数，返回完成 JSON 序列化的字符串。
    template <class T>
    response_type json_response(const T& value)
    {
        return json_response_impl(value.to_json());
    }

    // 返回一个空响应（204）。
    response_type empty_response();

    // 返回一个 "method not allowed" 响应，body 为简单的纯文本。
    response_type method_not_allowed()
    {
        return plaintext_response(boost::beast::http::status::method_not_allowed, "Method not allowed");
    }

    // 返回一个 "bad request" 响应，body 为简单的纯文本。
    response_type bad_request_text(std::string why)
    {
        return plaintext_response(boost::beast::http::status::bad_request, std::move(why));
    }

    // 返回一个 "not found" 响应，body 为简单的纯文本。
    response_type not_found_text()
    {
        return plaintext_response(boost::beast::http::status::not_found, "Not found");
    }

    // 返回一个错误响应，body 是描述具体情况的 JSON。
    // 该响应的 JSON 结构见 api_error 结构体。
    // 供 API 使用，用来告知那些在正常使用中很可能发生、
    // 且应当由客户端处理的错误，例如登录失败。
    response_type json_error(
        boost::beast::http::status status,
        api_error_id error_id,
        std::string_view error_message
    );

    // 返回一个 bad request 错误响应，body 为上面所述的 JSON。
    response_type bad_request_json(api_error_id error_id, std::string_view error_message)
    {
        return json_error(boost::beast::http::status::bad_request, error_id, error_message);
    }

    // 返回一个 bad request 错误响应，body 为上面所述的 JSON，
    // 并使用通用错误 ID。
    // 用于不需要客户端特殊处理的参数校验错误。
    response_type bad_request_json(std::string_view error_message)
    {
        return bad_request_json(api_error_id::bad_request, error_message);
    }

    // 返回一个服务器内部错误响应。错误信息会被记录到日志，
    // 但不会放进响应里。
    response_type internal_server_error(boost::system::error_code ec, std::string_view what = {});

private:
    using header_type = boost::beast::http::response_header<boost::beast::http::fields>;

    bool keep_alive_;
    header_type header_;
    bool used_{};

    response_builder(unsigned version, bool keep_alive);
    response_type plaintext_response(boost::beast::http::status status, std::string content);
    response_type json_response_impl(std::string serialized_json);

    header_type move_header()
    {
        assert(!used_);
        used_ = true;
        return std::move(header_);
    }

    template <class Body, class... Args>
    boost::beast::http::response<Body> build_response(Args&&... args)
    {
        boost::beast::http::response<Body> res{move_header(), std::forward<Args>(args)...};
        res.keep_alive(keep_alive_);
        return res;
    }

    friend class request_context;
};

// 封装一个 Boost.Beast 请求，并提供构建响应的便捷方式。
// 用于传给 HTTP API 处理函数。
class request_context
{
public:
    // Boost.Beast 的请求类型
    using request_type = boost::beast::http::request<boost::beast::http::string_body>;

    // 构造函数
    request_context(request_type&& req)
        : request_(std::move(req)), response_(request_.version(), request_.keep_alive())
    {
    }

    // 把 HTTP 请求行中的请求目标解析为 URL。失败时返回 error_code。
    // 应当在调用任何 API 处理函数之前先调用本函数。
    boost::system::error_code parse_request_target();

    // 把请求目标作为 URL 返回。必须先成功调用过 parse_request_target。
    const boost::urls::url_view& request_target() const
    {
        assert(target_.has_value());
        return *target_;
    }

    // 返回请求的 HTTP 方法。
    boost::beast::http::verb request_method() const noexcept { return request_.method(); }

    // 尝试把请求 body 解析为 JSON，并把结果转换成类型 T。
    // T 必须有一个签名如下的静态成员函数：
    // result<T> from_json(std::string_view)。
    // 在解析之前会先校验请求的 content-type。
    template <class T>
    boost::system::result<T> parse_json_body() const
    {
        // 校验 content-type
        if (!is_json_content_type())
            CHAT_RETURN_ERROR(errc::invalid_content_type)

        // 解析 JSON
        return T::from_json(request_.body());
    }

    // 返回一个 response_builder 对象
    response_builder& response() noexcept { return response_; }

private:
    request_type request_;
    response_builder response_;
    std::optional<boost::urls::url_view> target_;

    bool is_json_content_type() const;
};

}  // namespace chat

#endif
