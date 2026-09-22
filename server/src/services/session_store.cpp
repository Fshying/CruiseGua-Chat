//
// Copyright (c) 2026 Fshying (Fshying@users.noreply.github.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "services/session_store.hpp"

#include <boost/system/result.hpp>

#include <array>
#include <openssl/rand.h>
#include <stdexcept>
#include <string>
#include <string_view>

#include "error.hpp"
#include "services/redis_client.hpp"
#include "util/base64.hpp"

using namespace chat;
namespace asio = boost::asio;
using boost::system::result;

static constexpr std::size_t session_id_size = 16;  // 字节

static std::string generate_identifier()
{
    // 生成随机会话 ID。这里使用公开的随机数生成器，
    // 因为这个值会暴露给用户
    std::array<unsigned char, session_id_size> sid{};
    int ec = RAND_bytes(sid.data(), sid.size());
    if (ec <= 0)
        throw std::runtime_error("Generating session ID: RAND_bytes");

    // 对会话 ID 做 base64 编码，这样它就能通过
    // cookie 传输，或者存入 Redis
    return base64_encode(sid);
}

static std::string get_redis_key(std::string_view session_id)
{
    constexpr std::string_view prefix = "session_";

    std::string res;
    res.reserve(prefix.size() + session_id.size());
    res += prefix;
    res += session_id;
    return res;
}

using namespace chat;

asio::awaitable<result<std::string>> session_store::generate_session_id(
    std::int64_t user_id,
    std::chrono::seconds session_duration
)
{
    // 把用户 ID 转成字符串
    auto user_id_str = std::to_string(user_id);

    while (true)
    {
        // 生成一个标识符
        auto id = generate_identifier();
        auto redis_key = get_redis_key(id);

        // 尝试插入
        auto err = co_await redis_->set_nonexisting_key(redis_key, user_id_str, session_duration);

        // 成功就结束。如果发生冲突（可能性很小），就重新生成一个新 ID。
        // 遇到未知错误则直接退出
        if (!err)
            co_return id;
        else if (err != errc::already_exists)
            co_return err;
    }
}

asio::awaitable<result<std::int64_t>> session_store::get_user_by_session(std::string_view session_id)
{
    auto redis_key = get_redis_key(session_id);
    co_return co_await redis_->get_int_key(redis_key);
}
