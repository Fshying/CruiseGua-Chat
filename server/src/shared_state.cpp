//
// Copyright (c) 2026 Fshying (Fshying@users.noreply.github.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "shared_state.hpp"

#include <boost/asio/any_io_executor.hpp>

#include <memory>

#include "services/cookie_auth_service.hpp"
#include "services/mysql_client.hpp"
#include "services/pubsub_service.hpp"
#include "services/redis_client.hpp"

using namespace chat;

//
// 构造函数：初始化共享状态，集中管理应用运行所需的全部核心资源。
// 参数:
//   doc_root - 静态文件服务的根目录路径，用于响应 HTTP 静态资源请求
//   ex       - Boost.Asio 异步执行器，用于创建所有异步 I/O 客户端和服务
//
shared_state::shared_state(std::string doc_root, boost::asio::any_io_executor ex)
    : impl_{
          // 将静态文件根目录移入匿名结构体
          std::move(doc_root),
          // 基于执行器创建 Redis 客户端
          create_redis_client(ex),
          // 基于执行器创建 MySQL 客户端
          create_mysql_client(ex),
          // 创建 Cookie 认证服务，依赖已初始化的 Redis 和 MySQL 客户端
          std::make_unique<cookie_auth_service>(redis(), mysql()),
          // 基于执行器创建发布/订阅服务
          create_pubsub_service(ex),
      }
{
}

shared_state::shared_state(shared_state&& rhs) noexcept : impl_(std::move(rhs.impl_)) {}

shared_state& shared_state::operator=(shared_state&& rhs) noexcept
{
    impl_ = std::move(rhs.impl_);
    return *this;
}

shared_state::~shared_state() {}
