#pragma once

#include <memory>
#include <optional>
#include <string>

#include "sessionEntity.h"

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
 * @brief 会话表数据访问层
 *
 * 采用 Cache-Aside（旁路缓存）策略：
 *   - 写：先写 MySQL，再删除 Redis
 *   - 读：先查 Redis，未命中再查 MySQL 并回填，TTL 3 天
 *
 * 会话缓存为 JSON 字符串，key：session:<sessionId>
 */
class SessionData {
public:
    using ptr = std::shared_ptr<SessionData>;

    /**
     * @brief 构造会话数据访问层
     * @param database ODB 数据库句柄
     * @param redis    Redis 客户端句柄
     */
    SessionData(std::shared_ptr<odb::database> database, std::shared_ptr<sw::redis::Redis> redis);

    /**
     * @brief 新增会话
     * @param sessionId 会话id
     * @param userId    用户id
     * @return 写入数据库成功返回 true
     * @note 写库成功后删除缓存，保证下一次读回填最新数据
     */
    bool insertSession(const std::string& sessionId, const std::string& userId);

    /**
     * @brief 按会话id查询会话
     * @param sessionId 会话id
     * @return 查到时返回会话实体，不存在返回空
     */
    std::optional<chat2Data::SessionEntity> getSessionById(const std::string& sessionId);

    /**
     * @brief 删除会话
     * @param sessionId 会话id
     * @return 删除成功返回 true
     * @note 先删 MySQL 再删 Redis，会话本就不存在时视为删除成功
     */
    bool deleteSession(const std::string& sessionId);

private:
    /**
     * @brief 回填会话缓存
     * @param session 会话实体
     * @throws chat2Data::Chat2DataException 缓存序列化或写入失败（USER_SESSION_CACHE_ERROR）
     */
    void cacheSession(const chat2Data::SessionEntity& session);

    /**
     * @brief 从缓存读取会话
     * @param sessionId 会话id
     * @return 命中且字段完整返回会话实体，未命中或缓存异常返回空
     */
    std::optional<chat2Data::SessionEntity> loadSessionFromCache(const std::string& sessionId);

private:
    std::shared_ptr<odb::database> _database; // 数据库句柄
    std::shared_ptr<sw::redis::Redis> _redis; // 缓存句柄
};

} // namespace userService
