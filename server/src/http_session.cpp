//
// Copyright (c) 2023-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "http_session.hpp"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/cancel_after.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http/message_generator.hpp>
#include <boost/beast/http/parser.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/verb.hpp>
#include <boost/beast/websocket/error.hpp>
#include <boost/beast/websocket/rfc6455.hpp>
#include <boost/system/error_code.hpp>
#include <boost/url/url.hpp>
#include <boost/variant2/variant.hpp>

#include <algorithm>
#include <exception>
#include <iterator>
#include <optional>
#include <string_view>
#include <utility>

#include "api/auth.hpp"
#include "api/chat_websocket.hpp"
#include "error.hpp"
#include "request_context.hpp"
#include "shared_state.hpp"
#include "static_files.hpp"

namespace beast = boost::beast;
namespace http = boost::beast::http;
namespace asio = boost::asio;
using namespace chat;
using boost::system::error_code;

namespace {

// 端点处理函数的签名
using handler_fn = asio::awaitable<http::message_generator> (*)(request_context&, shared_state&);

// 标识客户端可以调用的单个端点
struct api_endpoint
{
    // 请求路径。
    std::string_view path;

    // 请求方法。如果同一个路径允许多个方法，
    // 就创建多个 path 相同但 method 不同的 api_endpoint 对象。
    http::verb method;

    // 客户端请求该端点时要调用的函数。
    handler_fn handler;
};

// 应用程序支持的所有端点。
// 相同 path 的端点对象应当连续存放。
// 实际路径带有 /api 前缀，在查表之前会先去掉该前缀。
constexpr api_endpoint endpoints[] = {
    {"/create-account", http::verb::post, handle_create_account},
    {"/login",          http::verb::post, handle_login         },
};

asio::awaitable<http::message_generator> handle_http_request_impl(request_context& ctx, shared_state& st)
{
    using namespace std::chrono_literals;

    // 尝试解析请求目标（request target）
    auto ec = ctx.parse_request_target();
    if (ec)
        co_return ctx.response().bad_request_text("Invalid request target");
    auto target = ctx.request_target();

    // 对 URL 做规范化
    boost::urls::url normalized(target);
    normalized.normalize();

    // 如果第一个路径段是 "api"，说明该请求要访问某个 API 端点。
    // 因为上一步已经对 URL 做了规范化，这里可以基于编码后的实体
    // 直接做字符比较
    auto segs = target.encoded_segments();
    if (!segs.empty() && segs.front() == "api")
    {
        // 取出 "/api" 之后的 URL 路径
        constexpr std::string_view api_prefix = "/api";
        assert(normalized.encoded_path().starts_with(api_prefix));
        std::string_view endpoint_path = normalized.encoded_path().substr(api_prefix.size());

        // 尝试匹配某个已定义的端点。
        // 端点数量不多，这里线性查找反而更合适。
        auto first = std::find_if(
            std::begin(endpoints),
            std::end(endpoints),
            [endpoint_path](const api_endpoint& e) { return e.path == endpoint_path; }
        );

        // 如果路径没有匹配上，返回 404
        if (first == std::end(endpoints))
            co_return ctx.response().not_found_text();

        // first 指向一段路径相同、方法可能不同的端点区间的开头。
        // 找出方法匹配的那一个
        handler_fn handler = nullptr;
        for (auto it = first; it != std::end(endpoints) && it->path == endpoint_path; ++it)
        {
            if (it->method == ctx.request_method())
            {
                handler = it->handler;
                break;
            }
        }

        // 如果在这里没找到端点，说明客户端请求的方法
        // 没有对应的处理函数
        if (handler == nullptr)
            co_return ctx.response().method_not_allowed();

        // 调用该端点，并为整个数据库访问操作加上超时。
        // 使用 co_spawn 可以让我们给协程搭配任意的完成令牌（completion token）。
        // asio::cancel_after 会在指定截止时间之后发出取消信号，
        // 一旦超时该操作就会失败。
        // co_spawn 不支持返回像 http::message_generator 这种非默认构造的参数，
        // 所以我们用 optional 来承接。
        std::optional<http::message_generator> gen;
        co_await asio::co_spawn(
            // 使用与当前协程相同的执行器
            co_await asio::this_coro::executor,

            // 实际要运行的协程
            [handler, &gen, &ctx, &st]() -> asio::awaitable<void> { gen = co_await handler(ctx, st); },

            // 为整个操作设置超时。返回一个可以被 co_await 的对象。
            // 等价于 asio::cancel_after(30s, asio::deferred)。
            asio::cancel_after(30s)
        );

        // 能走到这里，说明处理函数成功结束，
        // optional 中已经填好了响应。
        co_return std::move(gen).value();
    }
    else
    {
        // 静态文件
        co_return handle_static_file(ctx, st);
    }
}

asio::awaitable<http::message_generator> handle_http_request(
    http::request<http::string_body>&& req,
    shared_state& st
)
{
    // 构建请求上下文
    request_context ctx(std::move(req));

    // 普通的失败不用异常来传递，但
    // 未处理的异常不应该让服务器崩溃。
    try
    {
        co_return co_await handle_http_request_impl(ctx, st);
    }
    catch (const std::exception& err)
    {
        co_return ctx.response().internal_server_error(errc::uncaught_exception, err.what());
    }
}

}  // namespace

asio::awaitable<void> chat::run_http_session(
    boost::asio::ip::tcp::socket&& socket,
    std::shared_ptr<shared_state> state
)
{
    error_code ec;

    // 用于读取客户端请求的缓冲区
    beast::flat_buffer buff;

    // stream 让我们能设置连接的服务质量参数，
    // 例如超时时间。
    beast::tcp_stream stream(std::move(socket));

    while (true)
    {
        // 为每条消息新建一个解析器
        http::request_parser<http::string_body> parser;

        // 对请求体的字节大小设置一个合理的上限，
        // 以防止滥用。
        parser.body_limit(10000);

        // 设置超时时间。
        stream.expires_after(std::chrono::seconds(30));

        // 读取一个请求
        co_await http::async_read(stream, buff, parser.get(), asio::redirect_error(ec));

        if (ec == http::error::end_of_stream)
        {
            // 说明对端关闭了连接
            stream.socket().shutdown(asio::ip::tcp::socket::shutdown_send, ec);
            co_return;
        }
        else if (ec)
        {
            // 发生了未知错误
            co_return log_error(ec, "read");
        }

        // 判断这是不是一个 WebSocket 升级请求
        if (beast::websocket::is_upgrade(parser.get()))
        {
            // 创建 websocket，同时把 socket 和缓冲区的所有权转移过去
            // （这里之后不再使用它们）
            websocket ws(stream.release_socket(), parser.release(), std::move(buff));

            // 执行会话握手
            ec = co_await ws.accept();
            if (ec)
            {
                log_error(ec, "websocket accept");
                co_return;
            }

            // 运行 websocket 会话。它会一直运行，直到客户端
            // 关闭连接或发生错误。
            auto err = co_await handle_chat_websocket(std::move(ws), state);
            if (err && err != beast::websocket::error::closed)
                log_error(err, "Running chat websocket session");
            co_return;
        }

        // 这是一个普通的 HTTP 请求。
        // 尝试处理它并生成响应
        http::message_generator msg = co_await handle_http_request(parser.release(), *state);

        // 判断是否需要关闭连接
        bool keep_alive = msg.keep_alive();

        // 发送响应
        co_await beast::async_write(stream, std::move(msg), asio::redirect_error(ec));
        if (ec)
        {
            log_error(ec, "write");
            co_return;
        }

        // 这说明应该关闭连接，通常是因为
        // 响应中带有 "Connection: close" 语义。
        if (!keep_alive)
        {
            stream.socket().shutdown(asio::ip::tcp::socket::shutdown_send, ec);
            co_return;
        }
    }
}
