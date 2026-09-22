//
// Copyright (c) 2023-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_BUSINESS_TYPES_METADATA_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_BUSINESS_TYPES_METADATA_HPP

#include <boost/describe/class.hpp>

#include "business_types.hpp"

// 包含业务类型的 Boost.Describe 元数据。
// 这些元数据没有放进主头文件，以缩短编译时间。

namespace chat {

BOOST_DESCRIBE_STRUCT(auth_user, (), (id, hashed_password))
BOOST_DESCRIBE_STRUCT(user, (), (id, username))

}  // namespace chat

#endif
