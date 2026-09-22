//
// Copyright (c) 2023-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "static_files.hpp"

#include <boost/beast/http/verb.hpp>

#include <filesystem>

using namespace chat;
namespace beast = boost::beast;
namespace http = boost::beast::http;

// 把一个 HTTP 相对路径拼接到本地文件系统路径后面。
// 返回的路径会按当前平台做规范化。
// 这段代码复制自 Boost.Beast 的示例。
static std::string path_cat(std::string_view base, std::string_view path)
{
    constexpr char path_separator = '/';

    if (base.empty())
        return std::string(path);

    std::string result(base);

    if (result.back() == path_separator)
        result.resize(result.size() - 1);

    result.append(path.data(), path.size());

    return result;
}

response_builder::response_type chat::handle_static_file(request_context& ctx, shared_state& st)
{
    auto method = ctx.request_method();

    // 确认我们能处理该方法
    if (method != http::verb::get && method != http::verb::head)
        return ctx.response().method_not_allowed();

    // 请求路径必须是绝对路径，且不能包含 ".."
    auto target = ctx.request_target();
    auto target_path = target.path();
    if (target.empty() || !target.is_path_absolute() || target_path.find("..") != beast::string_view::npos)
        return ctx.response().bad_request_text("Illegal request-target");

    // 特殊目标 / 会得到 index.html
    if (target_path == "/")
        target_path = "/index.html";

    // 构造所请求文件的路径
    std::string path = path_cat(st.doc_root(), target_path);

    // 如果文件名没有扩展名，就推断为 html
    std::filesystem::path p(path);
    if (p.extension().empty())
        path.append(".html");

    // 发送该文件
    return ctx.response().file_response(path.c_str(), method == http::verb::head);
}
