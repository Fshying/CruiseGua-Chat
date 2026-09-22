//
// Copyright (c) 2023-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "util/scrypt.hpp"

#include <boost/system/result.hpp>

#include <charconv>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/evp.h>

#include "error.hpp"
#include "util/base64.hpp"

using namespace chat;
using boost::system::result;

static result<std::uint64_t> parse_int(std::string_view from)
{
    std::uint64_t value = 0;
    auto res = std::from_chars(from.begin(), from.end(), value);
    if (res.ec != std::errc())
        CHAT_RETURN_ERROR(std::make_error_code(res.ec))
    else if (res.ptr != from.data() + from.size())
        CHAT_RETURN_ERROR(errc::invalid_password_hash)
    else
        return value;
}

static result<scrypt_params> parse_phc_params(std::string_view from)
{
    // 合理的上界
    constexpr std::uint64_t max_ln = 20;
    constexpr std::uint64_t max_r = 20;

    scrypt_params res{};

    while (true)
    {
        // 取出一个参数
        auto last = from.find(',');
        auto serialized_param = from.substr(0, last);

        // 拆成键/值
        auto param_eq = serialized_param.find('=');
        if (param_eq == std::string_view::npos)
            CHAT_RETURN_ERROR(errc::invalid_password_hash)
        auto name = serialized_param.substr(0, param_eq);
        auto value_str = serialized_param.substr(param_eq + 1);

        // 解析它。忽略未知参数
        if (name == "ln")
        {
            auto value = parse_int(value_str);
            if (value.has_error())
                CHAT_RETURN_ERROR(errc::invalid_password_hash)
            if (*value < 2 || *value > max_ln)
                CHAT_RETURN_ERROR(errc::invalid_password_hash)
            res.ln = value.value();
        }
        else if (name == "r")
        {
            auto value = parse_int(value_str);
            if (value.has_error())
                return value.error();
            if (*value < 1 || *value > max_r)
                CHAT_RETURN_ERROR(errc::invalid_password_hash)
            res.r = value.value();
        }
        else if (name == "p")
        {
            auto value = parse_int(value_str);
            if (value.has_error())
                CHAT_RETURN_ERROR(errc::invalid_password_hash)
            // 不支持 p != 1（OWASP 也不推荐）
            if (*value != 1u)
                CHAT_RETURN_ERROR(errc::invalid_password_hash)
            res.p = value.value();
        }

        // 处理下一个
        if (last == std::string_view::npos)
            break;
        else
            from = from.substr(last + 1);
    }

    return res;
}

result<scrypt_data> chat::scrypt_phc_parse(std::string_view from)
{
    // 开头的 $ 标识符
    if (from.empty() || from.front() != '$')
        CHAT_RETURN_ERROR(errc::invalid_password_hash)
    from = from.substr(1);

    // 算法标识符
    auto count = from.find('$');
    if (count == std::string_view::npos)
        CHAT_RETURN_ERROR(errc::invalid_password_hash)
    auto algo_id = from.substr(0, count);
    if (algo_id != "scrypt")
        CHAT_RETURN_ERROR(errc::invalid_password_hash)  // 我们不认识的算法
    from = from.substr(count + 1);

    // 参数字段。规范中说它是可选的，但我们目前要求必须有
    count = from.find('$');
    if (count == std::string_view::npos)
        CHAT_RETURN_ERROR(errc::invalid_password_hash)
    auto serialized_params = from.substr(0, count);
    auto params_result = parse_phc_params(serialized_params);
    if (params_result.has_error())
        return params_result.error();
    from = from.substr(count + 1);

    // 盐字段。规范中说它是可选的，但我们目前要求必须有
    count = from.find('$');
    if (count == std::string_view::npos)
        CHAT_RETURN_ERROR(errc::invalid_password_hash)
    auto b64_salt = from.substr(0, count);
    auto salt_result = base64_decode(b64_salt, false);
    if (salt_result.has_error())
        CHAT_RETURN_ERROR(errc::invalid_password_hash)
    from = from.substr(count + 1);

    // 哈希字段（字符串剩余部分）
    auto hash_result = base64_decode(from, false);
    if (hash_result.has_error())
        CHAT_RETURN_ERROR(errc::invalid_password_hash)

    return scrypt_data{
        params_result.value(),
        std::move(salt_result).value(),
        std::move(hash_result).value(),
    };
}

std::string chat::scrypt_phc_serialize(
    scrypt_params params,
    std::span<const unsigned char, salt_size> salt,
    std::span<const unsigned char, hash_size> hash
)
{
    std::ostringstream oss;
    oss << "$scrypt$ln=" << params.ln << ",r=" << params.r << ",p=" << params.p << "$"
        << base64_encode(salt, false) << "$" << base64_encode(hash, false);
    return oss.str();
}

// 用给定的盐和参数对给定密码做哈希
std::array<unsigned char, hash_size> chat::scrypt_generate_hash(
    std::string_view passwd,
    scrypt_params params,
    std::span<const unsigned char> salt
)
{
    constexpr std::size_t max_memory = 32 << 20;  // 32MiB

    std::array<unsigned char, hash_size> res{};

    int ec = EVP_PBE_scrypt(
        passwd.data(),
        passwd.size(),
        salt.data(),
        salt.size(),
        1 << params.ln,  // 以 2 为底的对数
        params.r,
        params.p,
        max_memory,
        res.data(),
        res.size()
    );
    if (ec <= 0)
    {
        char errbuff[256]{};
        ERR_error_string_n(ERR_get_error(), errbuff, sizeof(errbuff));
        throw std::runtime_error(errbuff);
    }

    return res;
}

// 比较两段数据，且比较过程能防止时序攻击（timing attack）
bool chat::time_safe_equals(std::span<const unsigned char> s1, std::span<const unsigned char> s2) noexcept
{
    return CRYPTO_memcmp(s1.data(), s2.data(), std::min(s1.size(), s2.size())) == 0 && s1.size() == s2.size();
}
