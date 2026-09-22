//
// Copyright (c) 2026 Fshying (Fshying@users.noreply.github.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_SHARED_STATE_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_SHARED_STATE_HPP

#include <boost/asio/any_io_executor.hpp>
#include <memory>
#include <string>

/*`shared_state.hpp的公共接口让调用者：
    - 构造一份共享状态。
    - 读取静态文件目录。
    - 以引用方式借用四个服务。
    它没有把 `unique_ptr` 直接返回给调用者，因此调用者可以使用服务，却不能夺走所有权。
    四个服务的创建和销毁仍由 `shared_state` 集中负责。

shared_state 是「服务器进程内所有会话共享的一批服务对象」的容器，
    本质上是一个应用级的依赖注入容器 / 服务定位器（service locator）。
    它被 main 以一个 shared_ptr 持有，再传给每个连接，保证：
- 每个连接看到的是同一份 Redis/MySQL/PubSub 客户端（共享连接）。
- 只要还有会话在用它，它就不会被销毁（shared_ptr 引用计数）。
*/


namespace chat {

// 前向声明
/*
编译器此时知道这些是类，但不知道它们有多大、有哪些成员。因此不能直接写redis_client redis_;，
却可以先保存指针类型std::unique_ptr<redis_client>

前置声明让编译器知道 `redis_client` 是一个类，因此可以先声明它的指针或引用
*/
class session_map;
class redis_client;
class mysql_client;
class cookie_auth_service;
class pubsub_service;

// 包含服务器中所有会话共享的单例对象
class shared_state
{   
    /*
     匿名结构体 impl_:
     -匿名结构体只有一个实例 impl_，作用是「把成员打包」，外部（包括派生类，虽然这里没有）无法通过类型名访问。
    */
    struct
    {
        std::string doc_root_;
        std::unique_ptr<redis_client> redis_;
        std::unique_ptr<mysql_client> mysql_;
        std::unique_ptr<cookie_auth_service> cookie_auth_;
        std::unique_ptr<pubsub_service> pubsub_;
    } impl_;

public:
    //构造函数：接收静态文件根目录 + 执行器（executor）
    shared_state(std::string doc_root, boost::asio::any_io_executor ex);
    //拷贝构造/拷贝赋值 = delete：内部 unique_ptr 本来也不可拷贝，且语义上它是唯一实例。
    shared_state(const shared_state&) = delete;//禁止复制
    shared_state(shared_state&&) noexcept;//允许移动
    shared_state& operator=(const shared_state&) = delete;
    shared_state& operator=(shared_state&&) noexcept;
    ~shared_state();

    //访问器全部返回引用而非所有权：对象由 shared_state 独占，调用方只是借用

    /*
    分析：const std::string& doc_root() const noexcept { return impl_.doc_root_; }

    - 返回类型中的 `&`：返回原字符串的引用，避免复制。
    - 返回类型中的 `const`：调用者不能通过这个引用修改字符串。
    - 函数参数列表后的 `const`：这个成员函数承诺不修改当前 `shared_state` 的可观察状态，也能用于 `const shared_state`。
    - `noexcept`：函数承诺不抛出异常。
    */
    const std::string& doc_root() const noexcept { return impl_.doc_root_; }
    redis_client& redis() noexcept { return *impl_.redis_; }
    mysql_client& mysql() noexcept { return *impl_.mysql_; }
    cookie_auth_service& cookie_auth() noexcept { return *impl_.cookie_auth_; }
    pubsub_service& pubsub() noexcept { return *impl_.pubsub_; }
};

}  // namespace chat

#endif
