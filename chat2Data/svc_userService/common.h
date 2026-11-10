#pragma once

#include <string>

namespace userService {

/**
 * @brief 用户信息，供RPC层组装响应使用
 */
struct UserInfo {
    std::string userId;   // 用户id
    std::string nickname; // 用户昵称
    std::string email;    // 用户邮箱
};

} // namespace userService
