#pragma once

#include <cstdint>
#include <string>

#include <odb/core.hxx>

namespace chat2Data {

/**
 * @brief 会话表tbl_session的ODB映射类
 * @note  sessionId唯一；userId允许重复，以支持同一账号多设备登录
 */
#pragma db object table("tbl_session")
class SessionEntity {
public:
    SessionEntity() = default;

    /**
     * @brief 构造会话实体
     * @param sessionId 会话id
     * @param userId 用户id
     */
    SessionEntity(const std::string& sessionId, const std::string& userId);

    std::uint64_t getId() const;
    const std::string& getSessionId() const;
    const std::string& getUserId() const;

private:
    friend class odb::access;

    #pragma db id auto column("id") type("BIGINT UNSIGNED")
    std::uint64_t _id = 0;

    #pragma db unique column("sessionId") type("VARCHAR(32) CHARACTER SET utf8mb4")
    std::string _sessionId;

    #pragma db column("userId") type("VARCHAR(32) CHARACTER SET utf8mb4")
    std::string _userId;
};

} // namespace chat2Data
