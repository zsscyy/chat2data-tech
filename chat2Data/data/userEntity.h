#pragma once

#include <cstdint>
#include <string>

#include <odb/core.hxx>

namespace chat2Data {

// 用户在线状态，与tbl_user.status字段一一对应
enum class UserStatus : std::uint8_t {
    Offline = 0, // 未登录
    Online = 1   // 已登录
};

/**
 * @brief 用户表tbl_user的ODB映射类
 * @note  nickname、email均唯一，以支持昵称登录与邮箱登录
 */
#pragma db object table("tbl_user")
class UserEntity {
public:
    UserEntity() = default;

    /**
     * @brief 构造用户实体
     * @param userId 用户id
     * @param nickname 用户昵称
     * @param email 用户邮箱
     * @param password 加密后的密码
     * @param status 用户在线状态
     */
    UserEntity(const std::string& userId, const std::string& nickname, const std::string& email,
               const std::string& password, UserStatus status);

    std::uint64_t getId() const;
    const std::string& getUserId() const;
    const std::string& getNickname() const;
    const std::string& getEmail() const;
    const std::string& getPassword() const;
    UserStatus getStatus() const;

    void setStatus(UserStatus status);
    void setPassword(const std::string& password);
    void setNickname(const std::string& nickname);
    void setEmail(const std::string& email);

private:
    friend class odb::access;

    #pragma db id auto column("id") type("BIGINT UNSIGNED")
    std::uint64_t _id = 0;

    #pragma db unique column("userId") type("VARCHAR(32) CHARACTER SET utf8mb4")
    std::string _userId;

    #pragma db unique column("nickname") type("VARCHAR(32) CHARACTER SET utf8mb4")
    std::string _nickname;

    #pragma db unique column("email") type("VARCHAR(255) CHARACTER SET utf8mb4")
    std::string _email;

    #pragma db column("password") type("TEXT CHARACTER SET utf8mb4")
    std::string _password;

    #pragma db column("status") type("TINYINT")
    UserStatus _status = UserStatus::Offline;
};

} // namespace chat2Data
