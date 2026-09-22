//
// Copyright (c) 2026 Fshying (Fshying@users.noreply.github.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_STATIC_FILES_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_STATIC_FILES_HPP

#include "request_context.hpp"
#include "shared_state.hpp"

namespace chat {

class http_handler;

// 根据传入的请求，尝试从文档根目录提供静态文件。
// 如果找不到该文件，返回 404。
response_builder::response_type handle_static_file(request_context& ctx, shared_state& st);

}  // namespace chat

#endif
