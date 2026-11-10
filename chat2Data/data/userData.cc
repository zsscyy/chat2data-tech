#include "userData.h"

#include <chrono>
#include <string>
#include <utility>

#include <bite_scaffold/log.h>
#include <bite_scaffold/util.h>
#include <odb/database.hxx>
#include <odb/query.hxx>
#include <odb/transaction.hxx>
#include <sw/redis++/redis.h>

#include "errorHandler.h"
#include "userEntity-odb.hxx"

namespace userService {

using chat2Data::Chat2DataException;
using chat2Data::ErrorCode;
using chat2Data::UserEntity;
using chat2Data::UserStatus;

namespace {

// 用户缓存 key 前缀，三个维度分别指向同一份用户信息
constexpr const char* USER_ID_KEY_PREFIX = "user:userId:";
constexpr const char* USER_NICKNAME_KEY_PREFIX = "user:nickname:";
constexpr const char* USER_EMAIL_KEY_PREFIX = "user:email:";

// 用户信息缓存过期时间，1小时
constexpr long long USER_CACHE_TTL_SECONDS = 60 * 60;

/**
 * @brief 拼接用户缓存 key
 * @param prefix key 前缀
 * @param value  用户id、昵称或邮箱
 * @return 完整缓存 key
 */
std::string makeUserKey(const std::string& prefix, const std::string& value) {
    return prefix + value;
}

} // namespace

UserData::UserData(std::shared_ptr<odb::database> database, std::shared_ptr<sw::redis::Redis> redis)
    : _database(std::move(database)), _redis(std::move(redis)) {}

bool UserData::saveUserToDb(const UserEntity& user) {
    bool isUpdate = false;
    std::string oldNickname;
    std::string oldEmail;
    try {
        odb::transaction transaction(_database->begin());
        typedef odb::query<UserEntity> query;
        odb::result<UserEntity> result = _database->query<UserEntity>(query::userId == user.getUserId());
        if (!result.empty()) {
            // 用户已存在：更新全部用户字段
            UserEntity existing = *result.begin();
            oldNickname = existing.getNickname();
            oldEmail = existing.getEmail();
            existing.setNickname(user.getNickname());
            existing.setEmail(user.getEmail());
            existing.setPassword(user.getPassword());
            existing.setStatus(user.getStatus());
            _database->update(existing);
            isUpdate = true;
        } else {
            // 用户不存在：插入新记录
            UserEntity created(user.getUserId(), user.getNickname(), user.getEmail(), user.getPassword(),
                               user.getStatus());
            _database->persist(created);
        }
        transaction.commit();
    } catch (const std::exception& e) {
        // 写库失败返回 false，由业务层统一转换为业务错误码
        ERR("保存用户信息失败, userId: {}, 原因: {}", user.getUserId(), e.what());
        return false;
    }

    // 缓存中可能同时存在新、旧昵称/邮箱维度的 key，全部失效
    invalidateUserCache(user.getUserId(), user.getNickname(), user.getEmail());
    if (isUpdate && (oldNickname != user.getNickname() || oldEmail != user.getEmail())) {
        invalidateUserCache(user.getUserId(), oldNickname, oldEmail);
    }
    return true;
}

bool UserData::updateUserStatus(const std::string& userId, UserStatus status) {
    UserEntity user;
    try {
        odb::transaction transaction(_database->begin());
        typedef odb::query<UserEntity> query;
        odb::result<UserEntity> result = _database->query<UserEntity>(query::userId == userId);
        if (result.empty()) {
            transaction.commit();
            WRN("更新用户状态时用户不存在, userId: {}", userId);
            return false;
        }
        user = *result.begin();
        user.setStatus(status);
        _database->update(user);
        transaction.commit();
    } catch (const odb::exception& e) {
        ERR("更新用户状态失败, userId: {}, 原因: {}", userId, e.what());
        throw Chat2DataException(ErrorCode::USER_SAVE_USER_INFO_FAILED);
    }

    // 缓存中携带 status 字段，用户信息变更后必须一并失效
    invalidateUserCache(user);
    return true;
}

std::optional<UserEntity> UserData::getUserById(const std::string& userId) {
    std::optional<UserEntity> cachedUser = loadUserFromCache(makeUserKey(USER_ID_KEY_PREFIX, userId));
    if (cachedUser.has_value()) {
        return cachedUser;
    }

    std::optional<UserEntity> user = getUserByUserIdFromDb(userId);
    if (!user.has_value()) {
        return std::nullopt;
    }
    cacheUser(user.value());
    return user;
}

std::optional<UserEntity> UserData::getUserByNickname(const std::string& nickname) {
    std::optional<UserEntity> cachedUser = loadUserFromCache(makeUserKey(USER_NICKNAME_KEY_PREFIX, nickname));
    if (cachedUser.has_value()) {
        return cachedUser;
    }

    std::optional<UserEntity> user = getUserByNicknameFromDb(nickname);
    if (!user.has_value()) {
        return std::nullopt;
    }
    cacheUser(user.value());
    return user;
}

std::optional<UserEntity> UserData::getUserByEmail(const std::string& email) {
    std::optional<UserEntity> cachedUser = loadUserFromCache(makeUserKey(USER_EMAIL_KEY_PREFIX, email));
    if (cachedUser.has_value()) {
        return cachedUser;
    }

    std::optional<UserEntity> user = getUserByEmailFromDb(email);
    if (!user.has_value()) {
        return std::nullopt;
    }
    cacheUser(user.value());
    return user;
}

std::optional<UserEntity> UserData::getUserByUserIdFromDb(const std::string& userId) {
    UserEntity user;
    try {
        odb::transaction transaction(_database->begin());
        typedef odb::query<UserEntity> query;
        odb::result<UserEntity> result = _database->query<UserEntity>(query::userId == userId);
        if (result.empty()) {
            transaction.commit();
            return std::nullopt;
        }
        user = *result.begin();
        transaction.commit();
    } catch (const odb::exception& e) {
        ERR("按用户id查询用户失败, userId: {}, 原因: {}", userId, e.what());
        throw Chat2DataException(ErrorCode::USER_NOT_FOUND);
    }
    return user;
}

std::optional<UserEntity> UserData::getUserByNicknameFromDb(const std::string& nickname) {
    UserEntity user;
    try {
        odb::transaction transaction(_database->begin());
        typedef odb::query<UserEntity> query;
        odb::result<UserEntity> result = _database->query<UserEntity>(query::nickname == nickname);
        if (result.empty()) {
            transaction.commit();
            return std::nullopt;
        }
        user = *result.begin();
        transaction.commit();
    } catch (const odb::exception& e) {
        ERR("按昵称查询用户失败, nickname: {}, 原因: {}", nickname, e.what());
        throw Chat2DataException(ErrorCode::USER_NOT_FOUND);
    }
    return user;
}

std::optional<UserEntity> UserData::getUserByEmailFromDb(const std::string& email) {
    UserEntity user;
    try {
        odb::transaction transaction(_database->begin());
        typedef odb::query<UserEntity> query;
        odb::result<UserEntity> result = _database->query<UserEntity>(query::email == email);
        if (result.empty()) {
            transaction.commit();
            return std::nullopt;
        }
        user = *result.begin();
        transaction.commit();
    } catch (const odb::exception& e) {
        ERR("按邮箱查询用户失败, email: {}, 原因: {}", email, e.what());
        throw Chat2DataException(ErrorCode::USER_NOT_FOUND);
    }
    return user;
}

bool UserData::isNicknameExists(const std::string& nickname) {
    // 直接查 MySQL，保证唯一性判断不受缓存影响
    return getUserByNicknameFromDb(nickname).has_value();
}

bool UserData::isEmailExists(const std::string& email) {
    // 直接查 MySQL，保证唯一性判断不受缓存影响
    return getUserByEmailFromDb(email).has_value();
}

void UserData::cacheUser(const UserEntity& user) {
    try {
        // 1. 序列化为 JSON 字符串
        Json::Value userJson;
        userJson["userId"] = user.getUserId();
        userJson["nickname"] = user.getNickname();
        userJson["password"] = user.getPassword();
        userJson["email"] = user.getEmail();
        userJson["status"] = static_cast<int>(user.getStatus());

        std::optional<std::string> jsonStr = biteutil::JSON::serialize(userJson);
        if (!jsonStr.has_value()) {
            ERR("序列化用户缓存失败, userId: {}", user.getUserId());
            throw Chat2DataException(ErrorCode::USER_CACHE_ERROR);
        }

        // 2. 同一份 JSON 按三个维度分别写入，TTL 1 小时
        const std::chrono::seconds ttl(USER_CACHE_TTL_SECONDS);
        _redis->setex(makeUserKey(USER_ID_KEY_PREFIX, user.getUserId()), ttl, jsonStr.value());
        _redis->setex(makeUserKey(USER_NICKNAME_KEY_PREFIX, user.getNickname()), ttl, jsonStr.value());
        _redis->setex(makeUserKey(USER_EMAIL_KEY_PREFIX, user.getEmail()), ttl, jsonStr.value());
    } catch (const Chat2DataException&) {
        // 缓存写失败必须让调用方感知（课件 5.6），直接上抛
        throw;
    } catch (const std::exception& e) {
        ERR("回填用户缓存失败, userId: {}, 原因: {}", user.getUserId(), e.what());
        throw Chat2DataException(ErrorCode::USER_CACHE_ERROR);
    }
}

void UserData::invalidateUserCache(const UserEntity& user) {
    invalidateUserCache(user.getUserId(), user.getNickname(), user.getEmail());
}

void UserData::invalidateUserCache(const std::string& userId, const std::string& nickname,
                                   const std::string& email) {
    try {
        // 一次命令删除三个维度的缓存，避免多次往返
        _redis->del({
            makeUserKey(USER_ID_KEY_PREFIX, userId),
            makeUserKey(USER_NICKNAME_KEY_PREFIX, nickname),
            makeUserKey(USER_EMAIL_KEY_PREFIX, email),
        });
    } catch (const std::exception& e) {
        // 删除失败时由 1 小时 TTL 兜底
        WRN("删除用户缓存失败, userId: {}, 原因: {}", userId, e.what());
    }
}

std::optional<UserEntity> UserData::loadUserFromCache(const std::string& key) {
    std::optional<std::string> jsonStr;
    try {
        jsonStr = _redis->get(key);
    } catch (const std::exception& e) {
        WRN("读取用户缓存失败, key: {}, 原因: {}", key, e.what());
        return std::nullopt;
    }

    if (!jsonStr.has_value() || jsonStr->empty()) {
        return std::nullopt;
    }

    std::optional<Json::Value> userJson = biteutil::JSON::unserialize(jsonStr.value());
    if (!userJson.has_value() || !userJson->isObject()) {
        WRN("用户缓存反序列化失败, key: {}", key);
        return std::nullopt;
    }

    const Json::Value& value = userJson.value();
    if (!value.isMember("userId") || !value.isMember("nickname") || !value.isMember("email") ||
        !value.isMember("password") || !value.isMember("status")) {
        WRN("用户缓存字段不完整, key: {}", key);
        return std::nullopt;
    }

    const UserStatus status = static_cast<UserStatus>(value["status"].asInt());
    return UserEntity(value["userId"].asString(), value["nickname"].asString(), value["email"].asString(),
                      value["password"].asString(), status);
}

} // namespace userService
