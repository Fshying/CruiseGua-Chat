//
// Copyright (c) 2026 Fshying (Fshying@users.noreply.github.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "services/pubsub_service.hpp"

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/multi_index/indexed_by.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>

#include <memory>
#include <span>
#include <string>
#include <string_view>

using namespace chat;
namespace asio = boost::asio;

namespace {

class pubsub_service_impl final : public pubsub_service
{
    // 容器中保存的元素类型
    struct subscription
    {
        std::string topic_id;
        std::shared_ptr<message_subscriber> subscriber;

        std::string_view topic_id_sv() const noexcept { return topic_id; }
        const message_subscriber* subscriber_ptr() const noexcept { return subscriber.get(); }
    };

    // 我们需要让容器既能按主题 ID、又能按订阅者身份高效检索。
    // 这里用 Boost.MultiIndex 容器来维护这些索引，
    // 使两种操作都只需对数时间。
    // 这是一个类似 multimap 的容器，但访问时间是 logN。
    // clang-format off
    using container_type = boost::multi_index::multi_index_container<
        subscription,
        boost::multi_index::indexed_by<
            // 按主题 ID 建立索引
            boost::multi_index::ordered_non_unique<
                boost::multi_index::const_mem_fun<subscription, std::string_view, &subscription::topic_id_sv>
            >,
            // 按订阅者身份建立索引（比较指针）
            boost::multi_index::ordered_non_unique<
                boost::multi_index::const_mem_fun<subscription, const message_subscriber*, &subscription::subscriber_ptr>
            >
        >
    >;
    // clang-format on

    container_type ct_;
    asio::any_io_executor ex_;

public:
    pubsub_service_impl(asio::any_io_executor ex) : ex_(std::move(ex)) {}

    void subscribe(
        std::shared_ptr<message_subscriber> subscriber,
        std::span<const std::string_view> topic_ids
    ) override final
    {
        // 为每个请求的主题创建一个订阅
        for (auto topic_id : topic_ids)
        {
            ct_.insert(subscription{std::string(topic_id), subscriber});
        }
    }

    void unsubscribe(message_subscriber& subscriber) override final
    {
        // 移除所有与该订阅者匹配的订阅
        ct_.get<1>().erase(&subscriber);
    }

    void publish(std::string_view topic_id, std::string message) override final
    {
        // 把这个字符串放进一个共享对象里，
        // 以避免为每个订阅各拷贝一份
        auto msg_ptr = std::make_shared<std::string>(std::move(message));

        // 取出该主题的所有订阅
        auto [first, last] = ct_.equal_range(topic_id);

        // 并行启动各订阅者的回调
        for (auto it = first; it != last; ++it)
        {
            asio::co_spawn(
                ex_,
                [subs = it->subscriber, msg_ptr] { return subs->on_message(*msg_ptr); },
                asio::detached
            );
        }
    }
};

}  // namespace

std::unique_ptr<pubsub_service> chat::create_pubsub_service(asio::any_io_executor ex)
{
    return std::unique_ptr<pubsub_service>{new pubsub_service_impl(std::move(ex))};
}
