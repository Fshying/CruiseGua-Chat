//
// Copyright (c) 2023-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "api/chat_websocket.hpp"

#include <boost/asio/awaitable.hpp>
#include <boost/beast/websocket/rfc6455.hpp>
#include <boost/system/error_code.hpp>
#include <boost/variant2/variant.hpp>

#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "api/api_types.hpp"
#include "business_types.hpp"
#include "error.hpp"
#include "services/cookie_auth_service.hpp"
#include "services/pubsub_service.hpp"
#include "services/redis_client.hpp"
#include "services/room_history_service.hpp"
#include "shared_state.hpp"
#include "util/websocket.hpp"

using namespace chat;
namespace asio = boost::asio;
using boost::system::error_code;

namespace {

// 目前房间是静态写死的。
static constexpr std::array<std::string_view, 4> room_ids{
    "beast",
    "async",
    "db",
    "wasm",
};

static constexpr std::array<std::string_view, room_ids.size()> room_names{
    "Boost.Beast",
    "Boost.Async",
    "Database connectors",
    "Web assembly",
};

// 一个拥有型类型，保存 hello 事件所需的数据。
struct hello_data
{
    std::vector<room> rooms;
    username_map usernames;
};

// 获取发送 hello 事件所需的数据
static asio::awaitable<boost::system::result<hello_data>> get_hello_data(shared_state& st)
{
    // 获取房间历史记录
    room_history_service history_service(st.redis(), st.mysql());
    auto history_result = co_await history_service.get_room_history(room_ids);
    if (history_result.has_error())
        co_return history_result.error();
    assert(history_result->first.size() == room_ids.size());

    // 组装 hello 数据
    hello_data res{{}, std::move(history_result->second)};
    res.rooms.reserve(room_ids.size());
    for (std::size_t i = 0; i < room_ids.size(); ++i)
    {
        res.rooms.push_back(
            room{std::string(room_ids[i]), std::string(room_names[i]), std::move(history_result->first[i])}
        );
    }

    co_return res;
}

struct event_handler_visitor
{
    const user& current_user;
    websocket& ws;
    shared_state& st;

    // 解析错误
    asio::awaitable<error_code> operator()(error_code ec) const noexcept { co_return ec; }

    // 消息事件
    asio::awaitable<error_code> operator()(client_messages_event& evt) const
    {
        // 设置时间戳
        auto timestamp = timestamp_t::clock::now();

        // 组装消息数组
        std::vector<message> msgs;
        msgs.reserve(evt.messages.size());
        for (auto& msg : evt.messages)
        {
            msgs.push_back(message{
                "",  // 空 ID，稍后由 Redis 分配
                std::move(msg.content),
                timestamp,
                current_user.id,
            });
        }

        // 存入 Redis
        auto ids_result = co_await st.redis().store_messages(evt.roomId, msgs);
        if (ids_result.has_error())
            co_return ids_result.error();
        auto& ids = ids_result.value();

        // 把消息 ID 填好
        assert(msgs.size() == ids.size());
        for (std::size_t i = 0; i < msgs.size(); ++i)
            msgs[i].id = std::move(ids[i]);

        // 用手头已有的数据组装一个 server_messages 事件
        server_messages_event server_evt{evt.roomId, current_user, msgs};

        // 把该事件广播给所有客户端
        st.pubsub().publish(evt.roomId, server_evt.to_json());
        co_return error_code();
    }

    // 请求房间历史记录的事件
    asio::awaitable<error_code> operator()(chat::request_room_history_event& evt) const
    {
        // 获取房间历史记录
        room_history_service svc(st.redis(), st.mysql());
        auto history = co_await svc.get_room_history(evt.roomId);
        if (history.has_error())
            co_return history.error();

        // 组装一个 room_history 事件
        chat::room_history_event response_evt{evt.roomId, history->first, history->second};
        auto payload = response_evt.to_json();

        // 发送它
        co_return co_await ws.write(payload);
    }
};

// 消息通过 pubsub_service 在各个会话之间广播。
// 要使用它，我们必须实现 message_subscriber 接口。
// 每个 websocket 会话都会成为一个订阅者。
// 我们把房间 ID 用作主题 ID，把 websocket 消息的 payload 用作订阅消息。
class chat_websocket_session final : public message_subscriber,
                                     public std::enable_shared_from_this<chat_websocket_session>
{
    websocket ws_;
    std::shared_ptr<shared_state> st_;

public:
    chat_websocket_session(websocket socket, std::shared_ptr<shared_state> state) noexcept
        : ws_(std::move(socket)), st_(std::move(state))
    {
    }

    // 订阅者回调
    asio::awaitable<void> on_message(std::string_view serialized_message) override final
    {
        co_await ws_.write(serialized_message);  // 忽略错误码（TODO：要不要记日志？）
    }

    // 运行该会话，直到结束
    asio::awaitable<error_code> run()
    {
        error_code ec;

        // 检查用户是否已认证
        auto user_result = co_await st_->cookie_auth().user_from_cookie(ws_.upgrade_request());
        if (user_result.has_error())
        {
            // 如果没有认证，就关闭 websocket。在 websocket 中做认证检查时，
            // 这是更推荐的做法，而不是让 websocket 升级失败——
            // 因为客户端拿不到升级失败的具体信息。
            log_error(user_result.error(), "Websocket authentication failed");
            co_await ws_.close(boost::beast::websocket::policy_error);  // 忽略返回值
            co_return error_code();
        }
        const auto& current_user = user_result.value();

        // 锁住 websocket 的写操作。这样可以确保在 hello 之前不会写入任何消息。
        auto write_guard = co_await ws_.lock_writes();

        // 订阅可用房间的消息
        auto pubsub_guard = st_->pubsub().subscribe_guarded(shared_from_this(), room_ids);

        // 获取 hello 消息所需的数据
        auto hello_data = co_await get_hello_data(*st_);
        if (hello_data.has_error())
            co_return hello_data.error();

        // 组装 hello 事件并写入
        hello_event hello_evt{current_user, hello_data->rooms, hello_data->usernames};
        auto serialized_hello = hello_evt.to_json();
        ec = co_await ws_.write_locked(serialized_hello, write_guard);
        if (ec)
            co_return ec;

        // hello 发出之后，就可以开始通过 websocket 发送消息了
        write_guard.reset();

        // 读取后续消息并分发处理
        while (true)
        {
            // 读取一条消息
            auto raw_msg = co_await ws_.read();
            if (raw_msg.has_error())
                co_return raw_msg.error();

            // 反序列化
            auto msg = chat::parse_client_event(raw_msg.value());

            // 分发
            auto err = co_await boost::variant2::visit(event_handler_visitor{current_user, ws_, *st_}, msg);
            if (err)
                co_return err;
        }
    }
};

}  // namespace

asio::awaitable<error_code> chat::handle_chat_websocket(websocket socket, std::shared_ptr<shared_state> state)
{
    auto sess = std::make_shared<chat_websocket_session>(std::move(socket), std::move(state));
    co_return co_await sess->run();
}
