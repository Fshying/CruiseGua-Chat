//
// Copyright (c) 2026 Fshying (Fshying@users.noreply.github.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "util/password_hash.hpp"

#include <openssl/rand.h>
#include <stdexcept>

#include "error.hpp"
#include "util/scrypt.hpp"

using namespace chat;

std::string chat::hash_password(std::string_view passwd)
{
    // 默认参数
    constexpr scrypt_params params{};

    // 生成盐值。这里使用私有的随机数生成器，
    // 因为这些哈希永远不会暴露给用户
    std::array<unsigned char, salt_size> salt{};
    int ec = RAND_priv_bytes(salt.data(), salt.size());
    if (ec <= 0)
        throw std::runtime_error("Hashing password: RAND_priv_bytes");

    // 生成哈希
    auto hash = scrypt_generate_hash(passwd, params, salt);

    // 把所有参数格式化成 P-H-C 字符串
    return scrypt_phc_serialize(params, salt, hash);
}

bool chat::verify_password(std::string_view passwd, std::string_view hashed_passwd)
{
    // 反序列化密码哈希
    auto data_result = scrypt_phc_parse(hashed_passwd);
    if (data_result.has_error())
    {
        log_error(data_result.error(), "verify_password: malformed hash");
        return false;
    }
    const auto& stored_data = data_result.value();

    // 用当初使用的参数对传入的密码做哈希
    auto incoming_hash = scrypt_generate_hash(passwd, stored_data.params, stored_data.salt);

    // 比较密码
    return time_safe_equals(stored_data.hash, incoming_hash);
}
