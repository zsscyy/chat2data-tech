#pragma once

#include <memory>
#include <string>

#include <bite_scaffold/rpc.h>

#include "common.h"
#include "userEntity.h"

namespace userService {

class SessionManager;
class UserData;
class VerifyCodeData;

/**
 * @brief 用户业务类
 *
 * 完成用户注册、密码登录、验证码登录、会话登录、退出登录、获取用户信息等业务，
 * 密码采用 bcrypt 加密存储，用户信息与在线状态存放在 MySQL，验证码存放在 Redis。
 */
class UserBusiness {
public:
    using ptr = std::shared_ptr<UserBusiness>;

    /**
     * @brief 构造用户业务类
     * @param sessionManager 会话管理类
     * @param verifyCodeData 验证码数据访问层
     * @param userData       用户数据访问层
     * @param svcChannels    子服务信道管理器，用于调用通知服务、数据库服务
     */
    UserBusiness(std::shared_ptr<SessionManager> sessionManager, std::shared_ptr<VerifyCodeData> verifyCodeData,
                 std::shared_ptr<UserData> userData, biterpc::SvcChannels::ptr svcChannels);

    /**
     * @brief 检测昵称是否唯一
     * @param nickname 昵称
     * @return 昵称可用返回 true
     */
    bool isNicknameUnique(const std::string& nickname);

    /**
     * @brief 检测邮箱是否唯一
     * @param email 邮箱
     * @return 邮箱可用返回 true
     */
    bool isEmailUnique(const std::string& email);

    /**
     * @brief 用户注册
     * @param nickname 昵称
     * @param email    邮箱
     * @param password 明文密码
     * @return 新生成的用户id
     * @throws chat2Data::Chat2DataException 密码加密失败、写库失败（昵称/邮箱重复由数据库唯一约束拒绝）
     */
    std::string registerUser(const std::string& nickname, const std::string& email, const std::string& password);

    /**
     * @brief 密码登录
     * @param username 用户名（昵称或邮箱）
     * @param password 明文密码
     * @return 新创建的会话id
     * @throws chat2Data::Chat2DataException 用户不存在、密码错误、会话创建失败
     */
    std::string loginWithPassword(const std::string& username, const std::string& password);

    /**
     * @brief 获取验证码
     * @param email 邮箱
     * @return 验证码id，供后续验证码登录使用
     * @throws chat2Data::Chat2DataException 保存验证码失败、通知服务不可用或发送失败
     */
    std::string getVerifyCode(const std::string& email);

    /**
     * @brief 验证码登录
     * @param email 邮箱
     * @param codeId 验证码id
     * @param code   用户输入的验证码
     * @return 新创建的会话id
     * @throws chat2Data::Chat2DataException 验证码过期/错误、用户不存在、会话创建失败
     */
    std::string loginWithVerifyCode(const std::string& email, const std::string& codeId, const std::string& code);

    /**
     * @brief 会话登录
     * @param sessionId 会话id
     * @return 会话有效且状态更新成功返回 true
     * @throws chat2Data::Chat2DataException 会话无效、用户不存在
     */
    bool loginWithSession(const std::string& sessionId);

    /**
     * @brief 退出登录
     * @param sessionId 会话id
     * @return 退出成功返回 true
     * @throws chat2Data::Chat2DataException 会话无效、会话删除失败、用户不存在
     * @note 数据库子服务不可用时跳过连接清理，仅记录告警
     */
    bool logout(const std::string& sessionId);

    /**
     * @brief 获取用户信息
     * @param sessionId 会话id
     * @return 用户信息
     * @throws chat2Data::Chat2DataException 会话无效、用户不存在
     */
    UserInfo getUserInfo(const std::string& sessionId);

    /**
     * @brief 检查会话是否有效
     * @param sessionId 会话id
     * @param userId    输出参数，会话有效时回填对应的用户id
     * @return 会话存在且用户处于在线状态返回 true
     * @throws chat2Data::Chat2DataException 会话无效、用户不存在
     */
    bool isSessionValid(const std::string& sessionId, std::string& userId);

private:
    /**
     * @brief bcrypt 加密密码
     * @param password 明文密码
     * @return bcrypt 哈希字符串
     * @throws chat2Data::Chat2DataException 加密失败
     */
    std::string encryptPassword(const std::string& password);

    /**
     * @brief 校验密码
     * @param password       明文密码
     * @param hashedPassword 数据库中的 bcrypt 哈希
     * @return 密码匹配返回 true
     */
    bool verifyPassword(const std::string& password, const std::string& hashedPassword);

    /**
     * @brief 更新用户在线状态，用户不存在时抛异常
     * @param userId 用户id
     * @param status 目标状态
     * @throws chat2Data::Chat2DataException 用户不存在或写库失败
     */
    void updateStatus(const std::string& userId, chat2Data::UserStatus status);

    /**
     * @brief 调用通知服务发送验证码
     * @param email 邮箱
     * @param code  验证码
     * @throws chat2Data::Chat2DataException 通知服务无可用节点或发送失败
     */
    void sendVerifyCode(const std::string& email, const std::string& code);

    /**
     * @brief 调用数据库服务清理用户所有连接
     * @param userId 用户id
     * @note 数据库服务不可用时只记录告警
     */
    void deleteUserAllConn(const std::string& userId);

private:
    std::shared_ptr<SessionManager> _sessionManager; // 会话管理类
    std::shared_ptr<VerifyCodeData> _verifyCodeData; // 验证码数据访问层
    std::shared_ptr<UserData> _userData;             // 用户数据访问层
    biterpc::SvcChannels::ptr _svcChannels;          // 子服务信道管理器
};

} // namespace userService
