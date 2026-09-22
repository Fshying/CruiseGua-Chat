//
// Copyright (c) 2023-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_SERVICES_PUBSUB_SERVICE_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_SERVICES_PUBSUB_SERVICE_HPP

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/error.hpp>

#include <memory>
#include <span>
#include <string>
#include <string_view>

// 一个内存中的发布-订阅机制。用于在客户端之间广播消息。

namespace chat {

// 任何订阅者都必须实现这个接口
class message_subscriber
{
public:
    virtual ~message_subscriber() {}

    // 收到消息时调用。该函数是协程，
    // 以便在其中编写异步代码。
    virtual boost::asio::awaitable<void> on_message(std::string_view message) = 0;
};

// 这是一个用于缩短编译时间的接口。
class pubsub_service
{
    // subscribe_guarded 的实现
    struct subscriber_deleter
    {
        pubsub_service& self;

        void operator()(message_subscriber* subscriber) const noexcept { self.unsubscribe(*subscriber); }
    };

public:
    virtual ~pubsub_service() {}

    // 把订阅者对象订阅到给定的主题 ID 上。当收到这些主题中任意一个的消息时
    // （即有人调用 publish），就会调用 message_subscriber::on_message。
    virtual void subscribe(
        std::shared_ptr<message_subscriber> subscriber,
        std::span<const std::string_view> topic_ids
    ) = 0;

    // 移除该订阅者的所有订阅。
    // 订阅按订阅者身份匹配（即比较指针）。
    // 如果该订阅者不存在，此函数不做任何事。
    virtual void unsubscribe(message_subscriber& subscriber) = 0;

    // 向给定主题发布一条消息。
    // 所有订阅者会并行收到通知，各自拥有自己的协程。
    virtual void publish(std::string_view topic_id, std::string message) = 0;

    // RAII 风格的订阅。guard 被销毁时，订阅会被移除。
    using subscriber_guard = std::unique_ptr<message_subscriber, subscriber_deleter>;
    subscriber_guard subscribe_guarded(
        std::shared_ptr<message_subscriber> subscriber,
        std::span<const std::string_view> topic_ids
    )
    {
        auto* ptr = subscriber.get();
        subscribe(std::move(subscriber), topic_ids);
        return subscriber_guard(ptr, subscriber_deleter{*this});
    }
};

// 创建具体的 pubsub_service。executor 用于启动运行订阅回调的协程。
std::unique_ptr<pubsub_service> create_pubsub_service(boost::asio::any_io_executor ex);

}  // namespace chat

#endif
