//
// Copyright (c) 2026 Fshying (Fshying@users.noreply.github.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_SERVICES_REDIS_CLIENT_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_SERVICES_REDIS_CLIENT_HPP

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/system/error_code.hpp>
#include <boost/system/result.hpp>

#include <chrono>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "business_types.hpp"

// 一个高层、专用的 Redis 客户端。它实现服务器所需的各种操作，
// 把实际的 Redis 命令抽象掉。

namespace chat {

// 使用接口是为了缩短编译时间并提高可测试性
class redis_client
{
public:
    virtual ~redis_client() {}

    // 以分离（detached）模式启动 Redis 运行任务。此函数必须被调用一次，
    // 其他操作才能正常推进，重连循环也才能持续运行
    virtual void start_run() = 0;

    // 取消 Redis 运行任务。在关闭时调用
    virtual void cancel() = 0;

    // 单个房间一次最多能取回的消息条数
    static constexpr std::size_t message_batch_size = 100;

    // 作为 get_room_history 的输入参数
    struct room_histoy_request
    {
        // 要查询的房间 ID
        std::string_view room_id;

        // 我们已持有的该房间的最后一条消息；留空表示「从最新开始」
        std::optional<std::string_view> last_message_id;
    };

    // 批量获取多个房间的历史记录
    virtual boost::asio::awaitable<boost::system::result<std::vector<message_batch>>> get_room_history(
        std::span<const room_histoy_request> reqs
    ) = 0;

    // 把一批消息插入某个房间的历史记录中。
    // 返回这些被插入消息的 ID
    virtual boost::asio::awaitable<boost::system::result<std::vector<std::string>>> store_messages(
        std::string_view room_id,
        std::span<const message> messages
    ) = 0;

    // 把某个键设置为给定值，并指定生存时间（TTL）。
    // 如果该键已存在，操作会以 already_exists 失败
    virtual boost::asio::awaitable<boost::system::error_code> set_nonexisting_key(
        std::string_view key,
        std::string_view value,
        std::chrono::seconds ttl
    ) = 0;

    // 获取指定的键，并以 int64_t 形式返回。
    // 如果该键不存在，返回 not_found
    virtual boost::asio::awaitable<boost::system::result<std::int64_t>> get_int_key(std::string_view key) = 0;
};

// 创建 redis_client 的具体实现
std::unique_ptr<redis_client> create_redis_client(boost::asio::any_io_executor ex);

}  // namespace chat

#endif
