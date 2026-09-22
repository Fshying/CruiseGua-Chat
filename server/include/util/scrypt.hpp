//
// Copyright (c) 2026 Fshying (Fshying@users.noreply.github.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_UTIL_SCRYPT_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_UTIL_SCRYPT_HPP

#include <boost/system/result.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// 用于对密码做哈希与校验的工具函数，使用
// scrypt 算法（https://en.wikipedia.org/wiki/Scrypt）
// 和 PHC 格式（https://github.com/P-H-C/phc-string-format/blob/master/phc-sf-spec.md）

namespace chat {

// 常量与默认值。这些取值基于 node.js 的默认值
inline constexpr std::size_t salt_size = 32;
inline constexpr std::uint64_t default_ln = 14;
inline constexpr std::uint64_t default_r = 8;
inline constexpr std::uint64_t default_p = 1;
inline constexpr std::size_t hash_size = 32;

// 算法参数，与具体用户无关
struct scrypt_params
{
    std::uint64_t ln{default_ln};
    std::uint64_t r{default_r};
    std::uint64_t p{default_p};
};

// 解析 scrypt PHC 字符串的结果。注意 salt 和 hash 的长度
// 可能与这里列出的默认值不同。允许这一点，
// 我们就可以在不破坏已有数据的前提下调整默认值
struct scrypt_data
{
    scrypt_params params;
    std::vector<unsigned char> salt;
    std::vector<unsigned char> hash;
};

// 解析 PHC scrypt 字符串
boost::system::result<scrypt_data> scrypt_phc_parse(std::string_view from);

// 把给定的参数、salt 和 hash 序列化为 PHC 字符串
std::string scrypt_phc_serialize(
    scrypt_params params,
    std::span<const unsigned char, salt_size> salt,
    std::span<const unsigned char, hash_size> hash
);

// 用给定的 salt 和参数对给定密码做哈希
std::array<unsigned char, hash_size> scrypt_generate_hash(
    std::string_view passwd,
    scrypt_params params,
    std::span<const unsigned char> salt
);

// 比较两段数据，且比较过程能防止时序攻击（timing attack）
bool time_safe_equals(std::span<const unsigned char> s1, std::span<const unsigned char> s2) noexcept;

}  // namespace chat

#endif
