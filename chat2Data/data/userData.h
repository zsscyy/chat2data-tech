#pragma once

#include <memory>
#include <optional>
#include <string>

#include "userEntity.h"

namespace odb {
class database;
} // namespace odb

namespace sw {
namespace redis {
class Redis;
} // namespace redis
} // namespace sw

namespace userService {

/**
 * @brief 用户表数据访问层
 *
 * 采用 Cache-Aside（旁路缓存）策略：
 *   - 写：写 MySQL 成功后删除 Redis 缓存，MySQL 是唯一真数据源
 *   - 读：先查 Redis，未命中再查 MySQL 并回填 Redis
 *
 * 用户信息缓存为 JSON 字符串，TTL 1 小时，三个维度的 key 同时维护：
 *   user:userId:<userId>、user:nickname:<nickname>、user:email:<email>
 */
class UserData {
public:
    using ptr = std::shared_ptr<UserData>;

    /**
     * @brief 构造用户数据访问层
     * @param database ODB 数据库句柄
     * @param redis    Redis 客户端句柄
     */
    UserData(std::shared_ptr<odb::database> database, std::shared_ptr<sw::redis::Redis> redis);

    /**
     * @brief 保存用户信息：用户已存在则更新，不存在则插入
     * @param user 用户实体，按 userId 判断是否已存在
     * @return 保存成功返回 true，数据库操作失败返回 false
     * @note 成功后会同时失效新旧昵称/邮箱维度的缓存
     */
    bool saveUserToDb(const chat2Data::UserEntity& user);

    /**
     * @brief 更新用户在线状态
     * @param userId 用户id
     * @param status 目标状态
     * @return 更新成功返回 true，用户不存在返回 false
     */
    bool updateUserStatus(const std::string& userId, chat2Data::UserStatus status);

    /**
     * @brief 按用户id查询用户
     * @param userId 用户id
     * @return 查到时返回用户实体，不存在返回空
     */
    std::optional<chat2Data::UserEntity> getUserById(const std::string& userId);

    /**
     * @brief 按昵称查询用户
     * @param nickname 昵称
     * @return 查到时返回用户实体，不存在返回空
     */
    std::optional<chat2Data::UserEntity> getUserByNickname(const std::string& nickname);

    /**
     * @brief 按邮箱查询用户
     * @param email 邮箱
     * @return 查到时返回用户实体，不存在返回空
     */
    std::optional<chat2Data::UserEntity> getUserByEmail(const std::string& email);

    /**
     * @brief 判断昵称是否已存在
     * @param nickname 昵称
     * @return 已存在返回 true
     * @note 直接查 MySQL，不走缓存，保证唯一性判断权威
     */
    bool isNicknameExists(const std::string& nickname);

    /**
     * @brief 判断邮箱是否已存在
     * @param email 邮箱
     * @return 已存在返回 true
     * @note 直接查 MySQL，不走缓存，保证唯一性判断权威
     */
    bool isEmailExists(const std::string& email);

private:
    /**
     * @brief 将用户实体回填到三个维度的缓存
     * @param user 用户实体
     * @throws chat2Data::Chat2DataException 缓存序列化或写入失败（USER_CACHE_ERROR）
     */
    void cacheUser(const chat2Data::UserEntity& user);

    /**
     * @brief 删除用户三个维度的缓存
     * @param user 用户实体
     * @note 删除失败只记录告警：此时数据库写入已成功，抛异常会让调用方误以为写库失败，
     *       旧缓存由 1 小时 TTL 兜底
     */
    void invalidateUserCache(const chat2Data::UserEntity& user);

    /**
     * @brief 按 id/昵称/邮箱三个维度删除用户缓存
     * @param userId   用户id
     * @param nickname 用户昵称
     * @param email    用户邮箱
     * @note 更新用户信息时用于清理“旧昵称/旧邮箱”对应的缓存
     */
    void invalidateUserCache(const std::string& userId, const std::string& nickname, const std::string& email);

    /**
     * @brief 从指定 key 读取用户缓存
     * @param key 缓存 key
     * @return 命中且字段完整返回用户实体，未命中或缓存异常返回空
     */
    std::optional<chat2Data::UserEntity> loadUserFromCache(const std::string& key);

    /**
     * @brief 按用户id从MySQL查询（不走缓存）
     */
    std::optional<chat2Data::UserEntity> getUserByUserIdFromDb(const std::string& userId);

    /**
     * @brief 按昵称从MySQL查询（不走缓存）
     */
    std::optional<chat2Data::UserEntity> getUserByNicknameFromDb(const std::string& nickname);

    /**
     * @brief 按邮箱从MySQL查询（不走缓存）
     */
    std::optional<chat2Data::UserEntity> getUserByEmailFromDb(const std::string& email);

private:
    std::shared_ptr<odb::database> _database; // 数据库句柄
    std::shared_ptr<sw::redis::Redis> _redis; // 缓存句柄
};

} // namespace userService
