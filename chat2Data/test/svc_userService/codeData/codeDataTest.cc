#include <chrono>
#include <ctime>
#include <memory>
#include <string>
#include <thread>

#include <gtest/gtest.h>
#include <sw/redis++/redis.h>

#include <bite_scaffold/log.h>
#include <bite_scaffold/redis.h>

#include "verifyCodeData.h"

namespace {

// 测试数据固定值，每个用例开始与结束时都会清理，避免污染缓存
constexpr const char* TEST_CODE_ID = "test_code_0001";
constexpr const char* TEST_CODE_ID_2 = "test_code_0002";
constexpr const char* TEST_CODE = "123456";
constexpr const char* TEST_EMAIL = "test_code_0001@example.com";

// 验证码缓存 key 前缀与过期时间，与数据访问层实现保持一致
constexpr const char* VERIFY_CODE_KEY_PREFIX = "verifyCode:";
constexpr long long VERIFY_CODE_TTL_SECONDS = 5 * 60;
// TTL 检查容差，避免读写之间耗时导致断言失败
constexpr long long TTL_CHECK_TOLERANCE_SECONDS = 60;
// 过期用例中把 TTL 改成 1 秒，等待 2 秒后 key 应被 Redis 清理
constexpr long long SHORT_TTL_SECONDS = 1;
constexpr int EXPIRE_WAIT_SECONDS = 2;

/**
 * @brief 验证码数据访问层单元测试
 *
 * 直连 dev-redis，用例结束删除自身写入的缓存 key。
 */
class CodeDataTest : public testing::Test {
protected:
    static void SetUpTestSuite() {
        bitelog::log_settings logSettings;
        logSettings.level = 2;
        logSettings.path = "stdout";
        bitelog::bitelog_init(logSettings);

        biteredis::redis_settings redisSettings;
        redisSettings.host = "dev-redis";
        redisSettings.port = 6379;
        redisSettings.passwd = "123456";
        redisSettings.db = 0;
        redisSettings.connection_pool_size = 4;
        s_redis = biteredis::RedisFactory::create(redisSettings);

        s_codeData = std::make_shared<userService::VerifyCodeData>(s_redis);
    }

    void SetUp() override {
        cleanTestData();
    }

    void TearDown() override {
        cleanTestData();
    }

    /**
     * @brief 清理测试验证码缓存 key
     */
    static void cleanTestData() {
        s_redis->del({
            std::string(VERIFY_CODE_KEY_PREFIX) + TEST_CODE_ID,
            std::string(VERIFY_CODE_KEY_PREFIX) + TEST_CODE_ID_2,
        });
    }

    static std::shared_ptr<sw::redis::Redis> s_redis;
    static userService::VerifyCodeData::ptr s_codeData;
};

std::shared_ptr<sw::redis::Redis> CodeDataTest::s_redis = nullptr;
userService::VerifyCodeData::ptr CodeDataTest::s_codeData = nullptr;

/**
 * @brief 正常流程：保存后可按 codeId 读取全部字段，TTL 约 5 分钟
 * @note 缓存 value 为 JSON 字符串，不是 hash
 */
TEST_F(CodeDataTest, SaveAndQuery) {
    const std::time_t createTime = std::time(nullptr);
    ASSERT_TRUE(s_codeData->saveVerifyCode(TEST_CODE_ID, TEST_CODE, TEST_EMAIL, createTime));

    auto info = s_codeData->getVerifyCode(TEST_CODE_ID);
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->codeId, TEST_CODE_ID);
    EXPECT_EQ(info->code, TEST_CODE);
    EXPECT_EQ(info->email, TEST_EMAIL);
    EXPECT_EQ(info->createTime, createTime);

    const std::string key = std::string(VERIFY_CODE_KEY_PREFIX) + TEST_CODE_ID;
    EXPECT_EQ(s_redis->type(key), "string");
    const std::string json = s_redis->get(key).value_or("");
    EXPECT_NE(json.find("\"codeId\""), std::string::npos);
    EXPECT_NE(json.find("\"verifyCode\""), std::string::npos);

    const long long ttl = s_redis->ttl(key);
    EXPECT_GT(ttl, VERIFY_CODE_TTL_SECONDS - TTL_CHECK_TOLERANCE_SECONDS);
    EXPECT_LE(ttl, VERIFY_CODE_TTL_SECONDS);

    // 读策略：读取不删除 key，验证码在 TTL 到期前可重复读取
    EXPECT_EQ(s_redis->exists(key), 1);
    EXPECT_TRUE(s_codeData->getVerifyCode(TEST_CODE_ID).has_value());
}

/**
 * @brief 用后即删：删除后 key 不存在、再次读取返回空，重复删除返回 false
 */
TEST_F(CodeDataTest, DeleteAfterUse) {
    const std::time_t createTime = std::time(nullptr);
    ASSERT_TRUE(s_codeData->saveVerifyCode(TEST_CODE_ID, TEST_CODE, TEST_EMAIL, createTime));

    EXPECT_TRUE(s_codeData->deleteVerifyCode(TEST_CODE_ID));
    EXPECT_EQ(s_redis->exists(std::string(VERIFY_CODE_KEY_PREFIX) + TEST_CODE_ID), 0);
    EXPECT_FALSE(s_codeData->getVerifyCode(TEST_CODE_ID).has_value());

    // 重复删除：key 已不存在，返回 false
    EXPECT_FALSE(s_codeData->deleteVerifyCode(TEST_CODE_ID));
}

/**
 * @brief 异常场景：验证码过期被 Redis 清理后读取返回空
 */
TEST_F(CodeDataTest, ExpiredCodeReturnsNullopt) {
    const std::time_t createTime = std::time(nullptr);
    ASSERT_TRUE(s_codeData->saveVerifyCode(TEST_CODE_ID_2, TEST_CODE, TEST_EMAIL, createTime));

    // 将 TTL 改为 1 秒，等待其自然过期
    s_redis->expire(std::string(VERIFY_CODE_KEY_PREFIX) + TEST_CODE_ID_2,
                    std::chrono::seconds(SHORT_TTL_SECONDS));
    std::this_thread::sleep_for(std::chrono::seconds(EXPIRE_WAIT_SECONDS));

    EXPECT_FALSE(s_codeData->getVerifyCode(TEST_CODE_ID_2).has_value());
    const long long ttl = s_redis->ttl(std::string(VERIFY_CODE_KEY_PREFIX) + TEST_CODE_ID_2);
    EXPECT_EQ(ttl, -2);
}

/**
 * @brief 边界场景：读取不存在的验证码返回空，不抛异常
 */
TEST_F(CodeDataTest, MissingCodeReturnsNullopt) {
    EXPECT_FALSE(s_codeData->getVerifyCode("not_exist_code").has_value());
}

} // namespace
