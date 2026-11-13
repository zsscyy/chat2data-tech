#include "userBusiness.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <crypt.h>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

#include <sys/random.h>

#include <bite_scaffold/log.h>
#include <gflags/gflags.h>

#include "dbService.pb.h"
#include "errorHandler.h"
#include "notifyService.pb.h"
#include "sessionManager.h"
#include "userData.h"
#include "utils.h"
#include "verifyCodeData.h"

// gflags 变量在 main.cc 中定义，业务层只做声明
DECLARE_string(notify_service);
DECLARE_string(db_service);

namespace userService {

namespace {

// bcrypt 盐前缀，cost 因子固定为 10
constexpr const char* BCRYPT_SALT_PREFIX = "$2b$10$";
// 随机盐字节数
constexpr std::size_t BCRYPT_SALT_RANDOM_BYTES = 16;
// bcrypt 盐中随机部分的字符数
constexpr std::size_t BCRYPT_SALT_ENCODED_CHARS = 22;
// 标准Base64中的 '+' 在bcrypt盐中非法，需替换为 '.'
constexpr char BCRYPT_INVALID_SALT_CHAR = '+';
constexpr char BCRYPT_SALT_CHAR = '.';
// 验证码位数
constexpr int VERIFY_CODE_DIGITS = 6;
// 验证码取值范围
// 辅助RPC调用超时时间，3秒，避免依赖服务未就绪时长时间阻塞
constexpr int AUXILIARY_RPC_TIMEOUT_MS = 3000;
constexpr std::uint64_t VERIFY_CODE_RANGE = 1000000;

// 从操作系统密码学安全随机源读取字节
// @param output 输出缓冲区
// @param size   需要读取的字节数
// @throws std::runtime_error 随机源读取失败
void fillSecureRandom(void* output, std::size_t size) {
    auto* bytes = static_cast<unsigned char*>(output);
    std::size_t offset = 0;
    while (offset < size) {
        const ssize_t count = getrandom(bytes + offset, size - offset, 0);
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            throw std::runtime_error("getrandom failed");
        }
        offset += static_cast<std::size_t>(count);
    }
}

// 生成6位数字验证码
// @return 6位验证码字符串
std::string generateVerifyCode() {
    // 拒绝采样避免取模偏差，验证码随机数由 getrandom 提供。
    constexpr std::uint64_t randomRange = (std::uint64_t{1} << 32);
    constexpr std::uint64_t acceptedRange = randomRange - (randomRange % VERIFY_CODE_RANGE);
    std::uint32_t randomValue = 0;
    do {
        fillSecureRandom(&randomValue, sizeof(randomValue));
    } while (randomValue >= acceptedRange);
    const int codeValue = static_cast<int>(randomValue % VERIFY_CODE_RANGE);
    std::ostringstream codeStream;
    codeStream << std::setw(VERIFY_CODE_DIGITS) << std::setfill('0') << codeValue;
    return codeStream.str();
}

// 将错误码转换为整型，便于填充proto响应
// @param code 错误码
// @return 错误码整数
int toErrorValue(chat2Data::ErrorCode code) {
    return static_cast<int>(code);
}

} // namespace

// 构造用户业务层对象
// @param sessionManager 会话管理器
// @param verifyCodeData 验证码数据访问对象
// @param userData       用户数据访问对象
// @param svcChannels    子服务RPC信道管理器
UserBusiness::UserBusiness(std::shared_ptr<SessionManager> sessionManager,
                           std::shared_ptr<VerifyCodeData> verifyCodeData, std::shared_ptr<UserData> userData,
                           biterpc::SvcChannels::ptr svcChannels)
    : _sessionManager(std::move(sessionManager))
    , _verifyCodeData(std::move(verifyCodeData))
    , _userData(std::move(userData))
    , _svcChannels(std::move(svcChannels)) {}

// 判断昵称是否唯一
// @param nickname 用户昵称
// @return 昵称未被占用返回 true, 已存在返回 false
bool UserBusiness::isNicknameUnique(const std::string& nickname) {
    return !_userData->isNicknameExists(nickname);
}

// 判断邮箱是否唯一
// @param email 用户邮箱
// @return 邮箱未被占用返回 true, 已存在返回 false
bool UserBusiness::isEmailUnique(const std::string& email) {
    return !_userData->isEmailExists(email);
}

// 用户注册
// @param nickname 用户昵称
// @param email    用户邮箱
// @param password 用户密码（明文，入库前加密）
// @return 注册成功后生成的新用户ID
// @throws 写库失败时抛出 USER_SAVE_USER_INFO_FAILED
std::string UserBusiness::registerUser(const std::string& nickname, const std::string& email,
                                       const std::string& password) {
    const std::string encryptedPassword = encryptPassword(password);
    const std::string userId = chat2Data::Utils::generateUuid();
    chat2Data::UserEntity user(userId, nickname, email, encryptedPassword, chat2Data::UserStatus::Offline);
    // 不在业务层前置查重：昵称/邮箱重复由数据库唯一约束拒绝，统一返回 USER_SAVE_USER_INFO_FAILED
    if (!_userData->saveUserToDb(user)) {
        ERR("用户注册写库失败, nickname: {}", nickname);
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::USER_SAVE_USER_INFO_FAILED);
    }

    INF("用户注册成功, userId: {}, nickname: {}", userId, nickname);
    return userId;
}

// 昵称/邮箱 + 密码登录
// @param username 登录账号，先按昵称查找，未命中再按邮箱查找
// @param password 登录密码（明文）
// @return 登录成功后创建的会话ID
// @throws 用户不存在或密码错误时抛出
std::string UserBusiness::loginWithPassword(const std::string& username, const std::string& password) {
    // 用户名先按昵称查找，未命中再按邮箱查找
    std::optional<chat2Data::UserEntity> user = _userData->getUserByNickname(username);
    if (!user.has_value()) {
        user = _userData->getUserByEmail(username);
    }
    if (!user.has_value()) {
        WRN("登录失败，用户不存在, username: {}", username);
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::USER_NOT_FOUND);
    }

    if (!verifyPassword(password, user->getPassword())) {
        WRN("登录失败，密码错误, userId: {}", user->getUserId());
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::USER_LOGIN_FAILED);
    }

    // 先更新数据库在线状态（同时失效缓存），再创建会话
    updateStatus(user->getUserId(), chat2Data::UserStatus::Online);
    const std::string sessionId = _sessionManager->createSession(user->getUserId());

    INF("密码登录成功, userId: {}, sessionId: {}", user->getUserId(), sessionId);
    return sessionId;
}

// 生成并发送验证码
// @param email 接收验证码的邮箱
// @return 验证码标识codeId，用于后续验证码登录
// @throws 通知子服务不可用或发送失败时抛出 NOTIFY_SEND_FAILED
std::string UserBusiness::getVerifyCode(const std::string& email) {
    const std::string code = generateVerifyCode();
    const std::string codeId = chat2Data::Utils::generateUuid();
    _verifyCodeData->saveVerifyCode(codeId, code, email, std::time(nullptr));

    INF("验证码已生成, codeId: {}, email: {}", codeId, email);
    // 通知服务不可用或发送失败时直接抛异常
    sendVerifyCode(email, code);
    return codeId;
}

// 邮箱验证码登录
// @param email  用户邮箱
// @param codeId 验证码标识
// @param code   用户输入的验证码
// @return 登录成功后创建的会话ID
// @throws 验证码过期/不匹配或用户不存在时抛出
std::string UserBusiness::loginWithVerifyCode(const std::string& email, const std::string& codeId,
                                              const std::string& code) {
    std::optional<VerifyCodeInfo> verifyCode = _verifyCodeData->getVerifyCode(codeId);
    if (!verifyCode.has_value()) {
        WRN("验证码登录失败，验证码不存在或已过期, codeId: {}", codeId);
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::USER_VERIFY_CODE_EXPIRED);
    }
    if (verifyCode->email != email || verifyCode->code != code) {
        WRN("验证码登录失败，邮箱或验证码不匹配, codeId: {}", codeId);
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::USER_VERIFY_CODE_ERROR);
    }

    std::optional<chat2Data::UserEntity> user = _userData->getUserByEmail(email);
    if (!user.has_value()) {
        WRN("验证码登录失败，用户不存在, email: {}", email);
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::USER_NOT_FOUND);
    }

    // 先更新在线状态，再创建会话，最后删除已使用的验证码
    updateStatus(user->getUserId(), chat2Data::UserStatus::Online);
    const std::string sessionId = _sessionManager->createSession(user->getUserId());
    _verifyCodeData->deleteVerifyCode(codeId);

    INF("验证码登录成功, userId: {}, sessionId: {}", user->getUserId(), sessionId);
    return sessionId;
}

// 会话登录
// @param sessionId 会话ID
// @return 会话有效且用户状态更新成功返回 true
// @throws 会话无效时抛出 USER_SESSION_INVALID
bool UserBusiness::loginWithSession(const std::string& sessionId) {
    const std::string userId = _sessionManager->getUserIdBySessionId(sessionId);
    if (userId.empty()) {
        WRN("会话登录失败，会话无效, sessionId: {}", sessionId);
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::USER_SESSION_INVALID);
    }

    updateStatus(userId, chat2Data::UserStatus::Online);
    INF("会话登录成功, userId: {}, sessionId: {}", userId, sessionId);
    return true;
}

// 退出登录
// @param sessionId 会话ID
// @return 退出成功返回 true
// @throws 会话无效时抛出 USER_SESSION_INVALID
// @note 依次更新离线状态、删除会话、清理该用户的所有数据库连接
bool UserBusiness::logout(const std::string& sessionId) {
    const std::string userId = _sessionManager->getUserIdBySessionId(sessionId);
    if (userId.empty()) {
        WRN("退出登录失败，会话无效, sessionId: {}", sessionId);
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::USER_SESSION_INVALID);
    }

    // 先更新在线状态（同时失效缓存），再删除会话，最后清理数据库连接
    updateStatus(userId, chat2Data::UserStatus::Offline);
    _sessionManager->deleteSession(sessionId);
    deleteUserAllConn(userId);

    INF("退出登录成功, userId: {}, sessionId: {}", userId, sessionId);
    return true;
}

// 获取用户个人信息
// @param sessionId 会话ID
// @return 用户信息（用户ID、昵称、邮箱）
// @throws 会话无效或用户不存在时抛出
UserInfo UserBusiness::getUserInfo(const std::string& sessionId) {
    const std::string userId = _sessionManager->getUserIdBySessionId(sessionId);
    if (userId.empty()) {
        WRN("获取用户信息失败，会话无效, sessionId: {}", sessionId);
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::USER_SESSION_INVALID);
    }

    std::optional<chat2Data::UserEntity> user = _userData->getUserById(userId);
    if (!user.has_value()) {
        WRN("获取用户信息失败，用户不存在, userId: {}", userId);
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::USER_NOT_FOUND);
    }

    UserInfo userInfo;
    userInfo.userId = user->getUserId();
    userInfo.nickname = user->getNickname();
    userInfo.email = user->getEmail();
    return userInfo;
}

// 检查会话是否有效
// @param sessionId 会话ID
// @param userId    输出参数，会话有效时写入对应的用户ID
// @return 会话存在且用户处于在线状态返回 true
// @throws 会话无效或用户不存在时抛出
bool UserBusiness::isSessionValid(const std::string& sessionId, std::string& userId) {
    // 1. 先检测会话本身是否有效
    if (!_sessionManager->isSessionValid(sessionId)) {
        WRN("检查会话失败，会话无效, sessionId: {}", sessionId);
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::USER_SESSION_INVALID);
    }

    // 2. 通过会话id获取用户id
    userId = _sessionManager->getUserIdBySessionId(sessionId);
    if (userId.empty()) {
        WRN("检查会话失败，会话无效, sessionId: {}", sessionId);
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::USER_SESSION_INVALID);
    }

    // 3. 通过用户id获取用户信息
    std::optional<chat2Data::UserEntity> user = _userData->getUserById(userId);
    if (!user.has_value()) {
        WRN("检查会话失败，用户不存在, userId: {}", userId);
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::USER_NOT_FOUND);
    }

    // 4. 只有在线状态的用户才算会话有效
    return user->getStatus() == chat2Data::UserStatus::Online;
}

// 使用bcrypt算法加密密码
// @param password 明文密码
// @return bcrypt密文（$2b$10$ 前缀 + 22位随机盐）
// @throws 随机盐读取失败或加密失败时抛出 USER_PASSWORD_ENCRYPT_ERROR
std::string UserBusiness::encryptPassword(const std::string& password) {
    std::array<unsigned char, BCRYPT_SALT_RANDOM_BYTES> randomBytes{};
    try {
        fillSecureRandom(randomBytes.data(), randomBytes.size());
    } catch (const std::exception& e) {
        ERR("读取密码盐随机数失败, 原因: {}", e.what());
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::USER_PASSWORD_ENCRYPT_ERROR);
    }

    // 16字节随机盐经Base64编码后取前22位，作为bcrypt盐的随机部分
    std::string encodedSalt =
        chat2Data::Utils::base64Encode(randomBytes.data(), randomBytes.size()).substr(0, BCRYPT_SALT_ENCODED_CHARS);
    // bcrypt盐字符集不含 '+'，逐个替换为 '.'，保证crypt_r可接受
    std::replace(encodedSalt.begin(), encodedSalt.end(), BCRYPT_INVALID_SALT_CHAR, BCRYPT_SALT_CHAR);

    const std::string salt = std::string(BCRYPT_SALT_PREFIX) + encodedSalt;
    crypt_data cryptData{};
    char* hashedPassword = crypt_r(password.c_str(), salt.c_str(), &cryptData);
    if (hashedPassword == nullptr || hashedPassword[0] == '*' || hashedPassword[0] == '\0') {
        ERR("密码加密失败, salt: {}", salt);
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::USER_PASSWORD_ENCRYPT_ERROR);
    }
    return std::string(hashedPassword);
}

// 校验明文密码与bcrypt密文是否匹配
// @param password       明文密码
// @param hashedPassword 数据库中存储的bcrypt密文
// @return 密码匹配返回 true, 不匹配或crypt_r执行异常返回 false
bool UserBusiness::verifyPassword(const std::string& password, const std::string& hashedPassword) {
    crypt_data cryptData{};
    char* computedPassword = crypt_r(password.c_str(), hashedPassword.c_str(), &cryptData);
    if (computedPassword == nullptr || computedPassword[0] == '*' || computedPassword[0] == '\0') {
        WRN("密码校验失败，crypt_r执行异常");
        return false;
    }
    return hashedPassword == computedPassword;
}

// 更新用户在线状态
// @param userId 用户ID
// @param status 目标状态（Offline/Online）
// @throws 用户不存在导致更新失败时抛出 USER_NOT_FOUND
void UserBusiness::updateStatus(const std::string& userId, chat2Data::UserStatus status) {
    if (!_userData->updateUserStatus(userId, status)) {
        ERR("更新用户在线状态失败，用户不存在, userId: {}", userId);
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::USER_NOT_FOUND);
    }
}

// 调用通知子服务发送验证码邮件
// @param email 收件人邮箱
// @param code  验证码
// @throws 通知子服务无可用节点、RPC失败或返回错误码时抛出 NOTIFY_SEND_FAILED
void UserBusiness::sendVerifyCode(const std::string& email, const std::string& code) {
    biterpc::ChannelPtr channel;
    if (_svcChannels) {
        channel = _svcChannels->getNode(FLAGS_notify_service);
    }
    if (!channel) {
        WRN("通知子服务无可用节点, email: {}", email);
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::NOTIFY_SEND_FAILED);
    }

    ::chat2Data::notifyService::SendVerifyCodeRequest request;
    request.set_request_id(chat2Data::Utils::generateUuid());
    request.set_email(email);
    request.set_code(code);

    ::chat2Data::notifyService::SendVerifyCodeResponse response;
    brpc::Controller controller;
    controller.set_timeout_ms(AUXILIARY_RPC_TIMEOUT_MS);
    ::chat2Data::notifyService::NotifyService_Stub stub(channel.get());
    stub.SendVerifyCode(&controller, &request, &response, nullptr);

    if (controller.Failed()) {
        WRN("验证码邮件发送RPC失败, email: {}, 原因: {}", email, controller.ErrorText());
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::NOTIFY_SEND_FAILED);
    }
    if (response.error_code() != toErrorValue(chat2Data::ErrorCode::SUCCESS)) {
        WRN("验证码邮件发送失败, email: {}, notifyErrorCode: {}, error: {}", email, response.error_code(),
            response.error_msg());
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::NOTIFY_SEND_FAILED);
    }
    INF("验证码邮件发送成功, email: {}", email);
}

// 调用数据库子服务清理指定用户的全部数据库连接
// @param userId 用户ID
// @note 该操作为尽力而为：数据库子服务无可用节点或调用失败时只记录日志，不影响退出登录流程
void UserBusiness::deleteUserAllConn(const std::string& userId) {
    biterpc::ChannelPtr channel;
    if (_svcChannels) {
        channel = _svcChannels->getNode(FLAGS_db_service);
    }
    if (!channel) {
        WRN("数据库子服务无可用节点，跳过用户连接清理, userId: {}", userId);
        return;
    }

    ::chat2Data::DatabaseService::DeleteUserAllConnRequest request;
    request.set_request_id(chat2Data::Utils::generateUuid());
    request.set_user_id(userId);

    ::chat2Data::DatabaseService::DeleteUserAllConnResponse response;
    brpc::Controller controller;
    controller.set_timeout_ms(AUXILIARY_RPC_TIMEOUT_MS);
    ::chat2Data::DatabaseService::DatabaseService_Stub stub(channel.get());
    stub.DeleteUserAllConn(&controller, &request, &response, nullptr);

    if (controller.Failed()) {
        WRN("删除用户所有连接RPC失败, userId: {}, 原因: {}", userId, controller.ErrorText());
        return;
    }
    if (response.error_code() != toErrorValue(chat2Data::ErrorCode::SUCCESS)) {
        WRN("删除用户所有连接失败, userId: {}, error: {}", userId, response.error_msg());
        return;
    }
    INF("删除用户所有连接成功, userId: {}", userId);
}

} // namespace userService
