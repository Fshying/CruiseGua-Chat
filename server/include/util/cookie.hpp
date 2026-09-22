//
// Copyright (c) 2023-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_UTIL_COOKIE_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_UTIL_COOKIE_HPP

#include <chrono>
#include <optional>
#include <string>
#include <string_view>

namespace chat {

// cookie 的 SameSite 属性
enum class same_site_t
{
    strict,
    lax,  // 这是默认值
    none,
};

// Set-Cookie 头的构建器
class set_cookie_builder
{
    std::string_view name_;
    std::string_view value_;
    bool http_only_{false};
    std::optional<std::chrono::seconds> max_age_{};
    same_site_t same_site_{same_site_t::lax};
    bool secure_{false};

public:
    // 为 (cookie 名, cookie 值) 键值对构造一个构建器。
    // name 应当是合法的 HTTP token；value 应当是合法的 cookie 值
    // （详见 cookie.cpp）。否则会抛出异常。
    set_cookie_builder(std::string_view name, std::string_view value);

    // 设置 HttpOnly 属性
    set_cookie_builder& http_only(bool value) noexcept
    {
        http_only_ = value;
        return *this;
    }

    // 设置 Max-Age 属性
    set_cookie_builder& max_age(std::chrono::seconds val) noexcept
    {
        max_age_ = val;
        return *this;
    }

    // 设置 SameSite 属性
    set_cookie_builder& same_site(same_site_t val) noexcept
    {
        same_site_ = val;
        return *this;
    }

    // 设置 Secure 属性
    set_cookie_builder& secure(bool val) noexcept
    {
        secure_ = val;
        return *this;
    }

    // 构建 Set-Cookie 头
    std::string build_header() const;
};

// 一个非拥有型的 (cookie 名, cookie 值) 键值对
struct cookie_pair
{
    std::string_view name;
    std::string_view value;
};
inline bool operator==(const cookie_pair& lhs, const cookie_pair& rhs) noexcept
{
    return lhs.name == rhs.name && lhs.value == rhs.value;
}
inline bool operator!=(const cookie_pair& lhs, const cookie_pair& rhs) noexcept { return !(lhs == rhs); }

// Cookie 头的零拷贝解析器（Beast 风格）。
// 用于解析传入的 cookie。
//
// 如果在遍历字符串的过程中遇到解析错误，
// 该容器的行为等同于：用「只包含第一个非法字符之前
// （不含该字符）的那些字符」的字符串来构造列表。
class cookie_list
{
    std::string_view header_;

public:
    // cookie 名、cookie 值
    using value_type = cookie_pair;

    // 迭代器
    class const_iterator
    {
    public:
        using value_type = cookie_list::value_type;
        using pointer = const value_type*;
        using reference = const value_type&;
        using difference_type = std::ptrdiff_t;
        using iterator_category = std::input_iterator_tag;

        const_iterator() = default;

        bool operator==(const const_iterator& rhs) const noexcept
        {
            return val_ == rhs.val_ && next_ == rhs.next_ && last_ == rhs.last_;
        }

        bool operator!=(const const_iterator& rhs) const noexcept { return !(*this == rhs); }

        reference operator*() const noexcept { return val_; }

        pointer operator->() const noexcept { return &val_; }

        const_iterator& operator++() noexcept
        {
            increment(false);
            return *this;
        }

        const_iterator operator++(int) noexcept
        {
            auto temp = *this;
            ++(*this);
            return temp;
        }

    private:
        friend class cookie_list;

        value_type val_;
        const char* next_{};
        const char* last_{};

        const_iterator(const char* first, const char* last) noexcept : next_(first), last_(last)
        {
            // 解析第一个 cookie。该函数由 begin() 调用
            increment(true);
        }

        void increment(bool is_first) noexcept;
        void reset() noexcept { *this = const_iterator(); }
    };

    // 构造一个空的 cookie 列表
    cookie_list() = default;

    // 从头部字符串构造一个 cookie 列表。
    // 不会对头部内容做任何拷贝。
    explicit cookie_list(std::string_view header) noexcept;

    // 解析第一个 cookie，并返回指向它的迭代器
    const_iterator begin() const noexcept
    {
        return header_.empty() ? const_iterator()
                               : const_iterator(header_.data(), header_.data() + header_.size());
    }

    // 末尾之后（one-past-the-end）的哨兵迭代器
    const_iterator end() const noexcept { return const_iterator(); }
};

}  // namespace chat

#endif
