#include <memory>
#include <string>

#include <gtest/gtest.h>
#include <odb/database.hxx>
#include <odb/query.hxx>
#include <odb/transaction.hxx>
#include <sw/redis++/redis.h>

#include <bite_scaffold/log.h>
#include <bite_scaffold/odb.h>
#include <bite_scaffold/redis.h>

#include "sessionData.h"
#include "sessionEntity-odb.hxx"

namespace {

using chat2Data::SessionEntity;

// 测试数据固定值，每个用例开始与结束时都会清理，避免污染数据库
constexpr const char* TEST_SESSION_ID = "test_session_0001";
constexpr const char* TEST_USER_ID = "test_session_user_0001";
constexpr const char* TEST_SESSION_ID_2 = "test_session_0002";

// 会话缓存 key 前缀与过期时间，与数据访问层实现保持一致
constexpr const char* SESSION_KEY_PREFIX = "session:";
constexpr long long SESSION_CACHE_TTL_SECONDS = 3LL * 24 * 60 * 60;

/**
 * @brief 会话数据访问层单元测试
 *
 * 直连 dev-mysql / dev-redis，用例结束清理自身插入的数据。
 */
class SessionDataTest : public testing::Test {
protected:
    static void SetUpTestSuite() {
        bitelog::log_settings logSettings;
        logSettings.level = 2;
        logSettings.path = "stdout";
        bitelog::bitelog_init(logSettings);

        biteodb::mysql_settings dbSettings;
        dbSettings.host = "dev-mysql";
        dbSettings.port = 3306;
        dbSettings.user = "root";
        dbSettings.passwd = "123456";
        dbSettings.db = "chat2Data";
        dbSettings.cset = "utf8mb4";
        dbSettings.connection_pool_size = 2;
        s_database = biteodb::DBFactory::mysql(dbSettings);

        biteredis::redis_settings redisSettings;
        redisSettings.host = "dev-redis";
        redisSettings.port = 6379;
        redisSettings.passwd = "123456";
        redisSettings.db = 0;
        redisSettings.connection_pool_size = 4;
        s_redis = biteredis::RedisFactory::create(redisSettings);

        s_sessionData = std::make_shared<userService::SessionData>(s_database, s_redis);
    }

    void SetUp() override {
        cleanTestData();
    }

    void TearDown() override {
        cleanTestData();
    }

    /**
     * @brief 清理测试会话：数据库按 sessionId 删除，缓存删除对应 key
     */
    static void cleanTestData() {
        {
            odb::transaction transaction(s_database->begin());
            s_database->erase_query<SessionEntity>(odb::query<SessionEntity>::sessionId == TEST_SESSION_ID);
            s_database->erase_query<SessionEntity>(odb::query<SessionEntity>::sessionId == TEST_SESSION_ID_2);
            transaction.commit();
        }
        s_redis->del({
            std::string(SESSION_KEY_PREFIX) + TEST_SESSION_ID,
            std::string(SESSION_KEY_PREFIX) + TEST_SESSION_ID_2,
        });
    }

    static std::shared_ptr<odb::database> s_database;
    static std::shared_ptr<sw::redis::Redis> s_redis;
    static userService::SessionData::ptr s_sessionData;
};

std::shared_ptr<odb::database> SessionDataTest::s_database = nullptr;
std::shared_ptr<sw::redis::Redis> SessionDataTest::s_redis = nullptr;
userService::SessionData::ptr SessionDataTest::s_sessionData = nullptr;

/**
 * @brief 正常流程：新增会话后可按 sessionId 查询，缓存 TTL 约 3 天
 */
TEST_F(SessionDataTest, InsertAndQueryById) {
    ASSERT_TRUE(s_sessionData->insertSession(TEST_SESSION_ID, TEST_USER_ID));

    auto queried = s_sessionData->getSessionById(TEST_SESSION_ID);
    ASSERT_TRUE(queried.has_value());
    EXPECT_EQ(queried->getSessionId(), TEST_SESSION_ID);
    EXPECT_EQ(queried->getUserId(), TEST_USER_ID);

    const long long ttl = s_redis->ttl(std::string(SESSION_KEY_PREFIX) + TEST_SESSION_ID);
    EXPECT_GT(ttl, SESSION_CACHE_TTL_SECONDS - 100);
    EXPECT_LE(ttl, SESSION_CACHE_TTL_SECONDS);

    // 缓存 value 为 JSON 字符串，不是 hash
    const std::string key = std::string(SESSION_KEY_PREFIX) + TEST_SESSION_ID;
    EXPECT_EQ(s_redis->type(key), "string");
    EXPECT_NE(s_redis->get(key).value_or("").find("\"sessionId\""), std::string::npos);

    // 再次查询应命中缓存，缓存构造的实体没有数据库主键 id
    auto cached = s_sessionData->getSessionById(TEST_SESSION_ID);
    ASSERT_TRUE(cached.has_value());
    EXPECT_EQ(cached->getId(), 0U);
}

/**
 * @brief 写策略：新增会话写库成功后删除同 key 的残留缓存
 */
TEST_F(SessionDataTest, InsertSessionRemovesStaleCache) {
    const std::string key = std::string(SESSION_KEY_PREFIX) + TEST_SESSION_ID;
    s_redis->setex(key, 60, "{\"sessionId\":\"stale\",\"userId\":\"stale_user\"}");
    ASSERT_EQ(s_redis->exists(key), 1);

    ASSERT_TRUE(s_sessionData->insertSession(TEST_SESSION_ID, TEST_USER_ID));
    EXPECT_EQ(s_redis->exists(key), 0);

    // 缓存失效后按 sessionId 查询会回源数据库，得到新写入的会话
    auto queried = s_sessionData->getSessionById(TEST_SESSION_ID);
    ASSERT_TRUE(queried.has_value());
    EXPECT_EQ(queried->getUserId(), TEST_USER_ID);
}

/**
 * @brief 缓存未命中：删除缓存后仍能从数据库回源，并重新回填缓存
 */
TEST_F(SessionDataTest, CacheMissFallsBackToDatabase) {
    ASSERT_TRUE(s_sessionData->insertSession(TEST_SESSION_ID, TEST_USER_ID));

    ASSERT_TRUE(s_sessionData->getSessionById(TEST_SESSION_ID).has_value());
    ASSERT_EQ(s_redis->exists(std::string(SESSION_KEY_PREFIX) + TEST_SESSION_ID), 1);

    s_redis->del(std::string(SESSION_KEY_PREFIX) + TEST_SESSION_ID);
    auto queried = s_sessionData->getSessionById(TEST_SESSION_ID);
    ASSERT_TRUE(queried.has_value());
    EXPECT_GT(queried->getId(), 0U);

    EXPECT_EQ(s_redis->exists(std::string(SESSION_KEY_PREFIX) + TEST_SESSION_ID), 1);
}

/**
 * @brief 正常流程：删除会话同时清理数据库与缓存
 */
TEST_F(SessionDataTest, DeleteSessionRemovesDatabaseAndCache) {
    ASSERT_TRUE(s_sessionData->insertSession(TEST_SESSION_ID, TEST_USER_ID));
    ASSERT_TRUE(s_sessionData->getSessionById(TEST_SESSION_ID).has_value());
    ASSERT_EQ(s_redis->exists(std::string(SESSION_KEY_PREFIX) + TEST_SESSION_ID), 1);

    EXPECT_TRUE(s_sessionData->deleteSession(TEST_SESSION_ID));
    EXPECT_FALSE(s_sessionData->getSessionById(TEST_SESSION_ID).has_value());
    EXPECT_EQ(s_redis->exists(std::string(SESSION_KEY_PREFIX) + TEST_SESSION_ID), 0);
}

/**
 * @brief 边界场景：删除不存在的会话视为成功，查询不存在会话返回空
 */
TEST_F(SessionDataTest, DeleteMissingSessionIsIdempotent) {
    EXPECT_TRUE(s_sessionData->deleteSession(TEST_SESSION_ID_2));
    EXPECT_TRUE(s_sessionData->deleteSession(TEST_SESSION_ID_2));
    EXPECT_FALSE(s_sessionData->getSessionById(TEST_SESSION_ID_2).has_value());
    EXPECT_FALSE(s_sessionData->getSessionById("not_exist_session").has_value());
}

/**
 * @brief 边界场景：同一用户可持有多个会话，删除其中一个不影响另一个
 */
TEST_F(SessionDataTest, MultipleSessionsOfSameUser) {
    ASSERT_TRUE(s_sessionData->insertSession(TEST_SESSION_ID, TEST_USER_ID));
    ASSERT_TRUE(s_sessionData->insertSession(TEST_SESSION_ID_2, TEST_USER_ID));

    EXPECT_TRUE(s_sessionData->deleteSession(TEST_SESSION_ID));
    EXPECT_FALSE(s_sessionData->getSessionById(TEST_SESSION_ID).has_value());

    auto remained = s_sessionData->getSessionById(TEST_SESSION_ID_2);
    ASSERT_TRUE(remained.has_value());
    EXPECT_EQ(remained->getUserId(), TEST_USER_ID);
}

} // namespace
