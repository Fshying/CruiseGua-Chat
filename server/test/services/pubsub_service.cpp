//
// Copyright (c) 2026 Fshying (Fshying@users.noreply.github.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "services/pubsub_service.hpp"

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/test/unit_test.hpp>

#include <memory>
#include <string>
#include <string_view>

using namespace chat;
namespace asio = boost::asio;

BOOST_AUTO_TEST_SUITE(pubsub_service_)

// 一个订阅者实现，只记录收到的消息
struct stub_subscriber final : public message_subscriber
{
    std::vector<std::string> messages;

    asio::awaitable<void> on_message(std::string_view message) override final
    {
        messages.emplace_back(message);
        co_return;
    }
};

std::shared_ptr<stub_subscriber> create_subscriber() { return std::make_shared<stub_subscriber>(); }

using string_vector = std::vector<std::string>;

struct fixture
{
    asio::io_context ctx;
    std::unique_ptr<pubsub_service> pubsub{create_pubsub_service(ctx.get_executor())};
    std::shared_ptr<stub_subscriber> sub1{create_subscriber()};
    std::shared_ptr<stub_subscriber> sub2{create_subscriber()};

    // 清理之前的状态，发布一条消息，然后运行 I/O 上下文，
    // 以便触发订阅回调
    void publish_and_run(std::string_view topic_id, std::string msg)
    {
        sub1->messages.clear();
        sub2->messages.clear();
        pubsub->publish(topic_id, std::move(msg));
        ctx.restart();
        ctx.run();
    }
};

BOOST_FIXTURE_TEST_CASE(publish, fixture)
{
    // 数据
    constexpr std::string_view sub1_topics[] = {"r1", "r2"};
    constexpr std::string_view sub2_topics[] = {"r3", "r1"};

    // 订阅
    pubsub->subscribe(sub1, sub1_topics);
    pubsub->subscribe(sub2, sub2_topics);

    // 向主题 r1 发布
    publish_and_run("r1", "some message");
    BOOST_TEST(sub1->messages == string_vector{"some message"});
    BOOST_TEST(sub2->messages == string_vector{"some message"});

    // 向主题 r2 发布
    publish_and_run("r2", "another message");
    BOOST_TEST(sub1->messages == string_vector{"another message"});
    BOOST_TEST(sub2->messages == string_vector{});

    // 向主题 r3 发布
    publish_and_run("r3", "more messages here!");
    BOOST_TEST(sub1->messages == string_vector{});
    BOOST_TEST(sub2->messages == string_vector{"more messages here!"});

    // 向没有任何人订阅的主题发布
    publish_and_run("unknown", "this message will get to noone");
    BOOST_TEST(sub1->messages == string_vector{});
    BOOST_TEST(sub2->messages == string_vector{});
}

BOOST_FIXTURE_TEST_CASE(unsubscribe, fixture)
{
    // 数据
    constexpr std::string_view topic_ids[] = {"r1", "r2"};

    // 订阅
    pubsub->subscribe(sub1, topic_ids);

    // 能收到消息
    publish_and_run("r1", "some message");
    BOOST_TEST(sub1->messages == string_vector{"some message"});

    // 取消订阅
    pubsub->unsubscribe(*sub1);

    // 不再收到消息
    publish_and_run("r1", "some message");
    BOOST_TEST(sub1->messages == string_vector{});
}

// 边界情况：试图移除一个并不存在的订阅者时不应崩溃
BOOST_FIXTURE_TEST_CASE(remove_session_not_present, fixture)
{
    BOOST_CHECK_NO_THROW(pubsub->unsubscribe(*sub1));
}

// RAII 风格的订阅
BOOST_FIXTURE_TEST_CASE(subscribe_guarded, fixture)
{
    // 数据
    constexpr std::string_view topic_ids[] = {"r1", "r2"};

    {
        // 订阅
        auto guard = pubsub->subscribe_guarded(sub1, topic_ids);

        // 能收到消息
        publish_and_run("r1", "some message");
        BOOST_TEST(sub1->messages == string_vector{"some message"});
    }

    // guard 离开作用域后，订阅者会被移除
    publish_and_run("r1", "some message");
    BOOST_TEST(sub1->messages == string_vector{});
}

BOOST_AUTO_TEST_SUITE_END()
