#include "sessionData.h"

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
#include "sessionEntity-odb.hxx"

namespace userService {

using chat2Data::Chat2DataException;
using chat2Data::ErrorCode;
using chat2Data::SessionEntity;

namespace {

// 会话缓存 key 前缀
constexpr const char* SESSION_KEY_PREFIX = "session:";

// 会话缓存过期时间，3天
constexpr long long SESSION_CACHE_TTL_SECONDS = 3 * 24 * 60 * 60;

/**
 * @brief 拼接会话缓存 key
 * @param sessionId 会话id
 * @return 完整缓存 key
 */
std::string makeSessionKey(const std::string& sessionId) {
    return std::string(SESSION_KEY_PREFIX) + sessionId;
}

} // namespace

SessionData::SessionData(std::shared_ptr<odb::database> database, std::shared_ptr<sw::redis::Redis> redis)
    : _database(std::move(database)), _redis(std::move(redis)) {}

bool SessionData::insertSession(const std::string& sessionId, const std::string& userId) {
    SessionEntity session(sessionId, userId);
    try {
        odb::transaction transaction(_database->begin());
        _database->persist(session);
        transaction.commit();
    } catch (const odb::exception& e) {
        ERR("保存会话信息失败, sessionId: {}, 原因: {}", sessionId, e.what());
        throw Chat2DataException(ErrorCode::USER_SESSION_CREATE_ERROR);
    }

    // 写策略：先写数据库，再删除缓存，保证下一次读从数据库回填最新数据
    try {
        _redis->del(makeSessionKey(sessionId));
    } catch (const std::exception& e) {
        // 删除失败不影响已成功的数据库写入，残留缓存由 3 天 TTL 兜底
        WRN("删除会话缓存失败, sessionId: {}, 原因: {}", sessionId, e.what());
    }
    return true;
}

std::optional<SessionEntity> SessionData::getSessionById(const std::string& sessionId) {
    std::optional<SessionEntity> cachedSession = loadSessionFromCache(sessionId);
    if (cachedSession.has_value()) {
        return cachedSession;
    }

    SessionEntity session;
    try {
        odb::transaction transaction(_database->begin());
        typedef odb::query<SessionEntity> query;
        odb::result<SessionEntity> result = _database->query<SessionEntity>(query::sessionId == sessionId);
        if (result.empty()) {
            transaction.commit();
            return std::nullopt;
        }
        session = *result.begin();
        transaction.commit();
    } catch (const odb::exception& e) {
        ERR("按会话id查询会话失败, sessionId: {}, 原因: {}", sessionId, e.what());
        throw Chat2DataException(ErrorCode::USER_SESSION_INVALID);
    }

    cacheSession(session);
    return session;
}

bool SessionData::deleteSession(const std::string& sessionId) {
    try {
        odb::transaction transaction(_database->begin());
        typedef odb::query<SessionEntity> query;
        _database->erase_query<SessionEntity>(query::sessionId == sessionId);
        transaction.commit();
    } catch (const odb::exception& e) {
        ERR("删除会话失败, sessionId: {}, 原因: {}", sessionId, e.what());
        throw Chat2DataException(ErrorCode::USER_SESSION_DELETE_ERROR);
    }

    // 先删数据库再删缓存；删缓存失败要抛异常，保证调用方感知失败
    try {
        _redis->del(makeSessionKey(sessionId));
    } catch (const std::exception& e) {
        ERR("删除会话缓存失败, sessionId: {}, 原因: {}", sessionId, e.what());
        throw Chat2DataException(ErrorCode::USER_SESSION_DELETE_ERROR);
    }
    return true;
}

void SessionData::cacheSession(const SessionEntity& session) {
    try {
        // 1. 序列化为 JSON 字符串
        Json::Value sessionJson;
        sessionJson["sessionId"] = session.getSessionId();
        sessionJson["userId"] = session.getUserId();

        std::optional<std::string> jsonStr = biteutil::JSON::serialize(sessionJson);
        if (!jsonStr.has_value()) {
            ERR("序列化会话缓存失败, sessionId: {}", session.getSessionId());
            throw Chat2DataException(ErrorCode::USER_SESSION_CACHE_ERROR);
        }

        // 2. 写入 Redis，TTL 3 天；写缓存失败必须抛异常
        _redis->setex(makeSessionKey(session.getSessionId()), std::chrono::seconds(SESSION_CACHE_TTL_SECONDS),
                      jsonStr.value());
    } catch (const std::exception& e) {
        ERR("回填会话缓存失败, sessionId: {}, 原因: {}", session.getSessionId(), e.what());
        throw Chat2DataException(ErrorCode::USER_SESSION_CACHE_ERROR);
    }
}

std::optional<SessionEntity> SessionData::loadSessionFromCache(const std::string& sessionId) {
    std::optional<std::string> jsonStr;
    try {
        jsonStr = _redis->get(makeSessionKey(sessionId));
    } catch (const std::exception& e) {
        WRN("读取会话缓存失败, sessionId: {}, 原因: {}", sessionId, e.what());
        return std::nullopt;
    }

    if (!jsonStr.has_value() || jsonStr->empty()) {
        return std::nullopt;
    }

    std::optional<Json::Value> sessionJson = biteutil::JSON::unserialize(jsonStr.value());
    if (!sessionJson.has_value() || !sessionJson->isObject()) {
        WRN("会话缓存反序列化失败, sessionId: {}", sessionId);
        return std::nullopt;
    }

    const Json::Value& value = sessionJson.value();
    if (!value.isMember("sessionId") || !value.isMember("userId")) {
        WRN("会话缓存字段不完整, sessionId: {}", sessionId);
        return std::nullopt;
    }

    return SessionEntity(value["sessionId"].asString(), value["userId"].asString());
}

} // namespace userService
