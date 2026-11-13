#include <iterator>
#include <memory>
#include <string>
#include <utility>

#include <gtest/gtest.h>
#include <odb/database.hxx>
#include <odb/query.hxx>
#include <odb/transaction.hxx>
#include <sw/redis++/redis.h>

#include <bite_scaffold/log.h>
#include <bite_scaffold/odb.h>
#include <bite_scaffold/redis.h>

#include "errorHandler.h"
#include "userData.h"
#include "userEntity-odb.hxx"

namespace {

using chat2Data::Chat2DataException;
using chat2Data::ErrorCode;
using chat2Data::UserEntity;
using chat2Data::UserStatus;

// 测试数据固定值，每个用例开始与结束时都会清理，避免污染数据库
constexpr const char* TEST_USER_ID = "test_user_0001";
constexpr const char* TEST_NICKNAME = "test_nick_0001";
constexpr const char* TEST_EMAIL = "test_user_0001@example.com";
constexpr const char* TEST_PASSWORD = "$2b$10$testpasswordhashvalue";
constexpr const char* TEST_OTHER_USER_ID = "test_user_0002";
constexpr const char* TEST_OTHER_EMAIL = "test_user_0002@example.com";
// upsert 用例中用于替换的昵称与邮箱
constexpr const char* TEST_NICKNAME_2 = "test_nick_0001_new";
constexpr const char* TEST_EMAIL_2 = "test_user_0001_new@example.com";
// 用于验证“存在性检测直查数据库、不受缓存影响”的幽灵昵称
constexpr const char* TEST_GHOST_NICKNAME = "ghost_nick_0001";

// 用户缓存 key 前缀与过期时间，与数据访问层实现保持一致
constexpr const char* USER_ID_KEY_PREFIX = "user:userId:";
constexpr const char* USER_NICKNAME_KEY_PREFIX = "user:nickname:";
constexpr const char* USER_EMAIL_KEY_PREFIX = "user:email:";
constexpr long long USER_CACHE_TTL_SECONDS = 60 * 60;
// TTL 检查容差，避免读写之间耗时导致断言失败
constexpr long long TTL_CHECK_TOLERANCE_SECONDS = 100;
// 幽灵缓存用例的短 TTL
constexpr long long GHOST_CACHE_TTL_SECONDS = 60;

/**
 * @brief 用户数据访问层单元测试
 *
 * 直连 dev-mysql / dev-redis，用例结束清理自身插入的数据。
 */
class UserDataTest : public testing::Test {
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

        s_userData = std::make_shared<userService::UserData>(s_database, s_redis);
    }

    void SetUp() override {
        cleanTestData();
    }

    void TearDown() override {
        cleanTestData();
    }

    /**
     * @brief 清理测试用户：数据库按 userId/昵称/邮箱删除，缓存删除三个维度 key
     */
    static void cleanTestData() {
        {
            odb::transaction transaction(s_database->begin());
            s_database->erase_query<UserEntity>(odb::query<UserEntity>::userId == TEST_USER_ID);
            s_database->erase_query<UserEntity>(odb::query<UserEntity>::userId == TEST_OTHER_USER_ID);
            s_database->erase_query<UserEntity>(odb::query<UserEntity>::nickname == TEST_NICKNAME);
            s_database->erase_query<UserEntity>(odb::query<UserEntity>::email == TEST_EMAIL);
            s_database->erase_query<UserEntity>(odb::query<UserEntity>::nickname == TEST_NICKNAME_2);
            s_database->erase_query<UserEntity>(odb::query<UserEntity>::email == TEST_EMAIL_2);
            s_database->erase_query<UserEntity>(odb::query<UserEntity>::email == TEST_OTHER_EMAIL);
            transaction.commit();
        }
        s_redis->del({
            std::string(USER_ID_KEY_PREFIX) + TEST_USER_ID,
            std::string(USER_ID_KEY_PREFIX) + TEST_OTHER_USER_ID,
            std::string(USER_NICKNAME_KEY_PREFIX) + TEST_NICKNAME,
            std::string(USER_NICKNAME_KEY_PREFIX) + TEST_NICKNAME_2,
            std::string(USER_NICKNAME_KEY_PREFIX) + TEST_GHOST_NICKNAME,
            std::string(USER_EMAIL_KEY_PREFIX) + TEST_EMAIL,
            std::string(USER_EMAIL_KEY_PREFIX) + TEST_EMAIL_2,
            std::string(USER_EMAIL_KEY_PREFIX) + TEST_OTHER_EMAIL,
        });
    }

    static std::shared_ptr<odb::database> s_database;
    static std::shared_ptr<sw::redis::Redis> s_redis;
    static userService::UserData::ptr s_userData;
};

std::shared_ptr<odb::database> UserDataTest::s_database = nullptr;
std::shared_ptr<sw::redis::Redis> UserDataTest::s_redis = nullptr;
userService::UserData::ptr UserDataTest::s_userData = nullptr;

/**
 * @brief 正常流程：新增用户后可按 userId 查询，且写入缓存并设置 1 小时过期
 */
TEST_F(UserDataTest, InsertAndQueryById) {
    UserEntity user(TEST_USER_ID, TEST_NICKNAME, TEST_EMAIL, TEST_PASSWORD, UserStatus::Offline);
    ASSERT_TRUE(s_userData->saveUserToDb(user));

    auto queried = s_userData->getUserById(TEST_USER_ID);
    ASSERT_TRUE(queried.has_value());
    EXPECT_EQ(queried->getUserId(), TEST_USER_ID);
    EXPECT_EQ(queried->getNickname(), TEST_NICKNAME);
    EXPECT_EQ(queried->getEmail(), TEST_EMAIL);
    EXPECT_EQ(queried->getPassword(), TEST_PASSWORD);
    EXPECT_EQ(queried->getStatus(), UserStatus::Offline);

    const long long ttl = s_redis->ttl(std::string(USER_ID_KEY_PREFIX) + TEST_USER_ID);
    EXPECT_GT(ttl, USER_CACHE_TTL_SECONDS - TTL_CHECK_TOLERANCE_SECONDS);
    EXPECT_LE(ttl, USER_CACHE_TTL_SECONDS);

    // 缓存 value 为 JSON 字符串，不是 hash
    const std::string key = std::string(USER_ID_KEY_PREFIX) + TEST_USER_ID;
    EXPECT_EQ(s_redis->type(key), "string");
    const std::string json = s_redis->get(key).value_or("");
    EXPECT_NE(json.find("\"userId\""), std::string::npos);
    EXPECT_NE(json.find("\"status\""), std::string::npos);

    // 再次查询应命中缓存，缓存构造的实体没有数据库主键 id
    auto cached = s_userData->getUserById(TEST_USER_ID);
    ASSERT_TRUE(cached.has_value());
    EXPECT_EQ(cached->getId(), 0U);
}

/**
 * @brief 正常流程：昵称与邮箱两个维度均可查询，并支持存在性判断
 */
TEST_F(UserDataTest, QueryByNicknameAndEmail) {
    UserEntity user(TEST_USER_ID, TEST_NICKNAME, TEST_EMAIL, TEST_PASSWORD, UserStatus::Offline);
    ASSERT_TRUE(s_userData->saveUserToDb(user));

    auto byNickname = s_userData->getUserByNickname(TEST_NICKNAME);
    ASSERT_TRUE(byNickname.has_value());
    EXPECT_EQ(byNickname->getUserId(), TEST_USER_ID);

    auto byEmail = s_userData->getUserByEmail(TEST_EMAIL);
    ASSERT_TRUE(byEmail.has_value());
    EXPECT_EQ(byEmail->getNickname(), TEST_NICKNAME);

    EXPECT_TRUE(s_userData->isNicknameExists(TEST_NICKNAME));
    EXPECT_TRUE(s_userData->isEmailExists(TEST_EMAIL));
    EXPECT_FALSE(s_userData->isNicknameExists("not_exist_nickname"));
    EXPECT_FALSE(s_userData->isEmailExists("not_exist_email@example.com"));
}

/**
 * @brief 存在性检测直查数据库：缓存里有“幽灵记录”也不能影响判断
 */
TEST_F(UserDataTest, ExistsCheckBypassesCache) {
    // 只往缓存写一条昵称记录，数据库里没有这个用户
    s_redis->setex(std::string(USER_NICKNAME_KEY_PREFIX) + TEST_GHOST_NICKNAME, GHOST_CACHE_TTL_SECONDS,
                   "{\"userId\":\"ghost\",\"nickname\":\"ghost_nick_0001\"}");

    // 存在性检测直接查 MySQL，应返回 false
    EXPECT_FALSE(s_userData->isNicknameExists(TEST_GHOST_NICKNAME));
}

/**
 * @brief 缓存未命中：删除缓存后仍能从数据库回源，并重新回填缓存
 */
TEST_F(UserDataTest, CacheMissFallsBackToDatabase) {
    UserEntity user(TEST_USER_ID, TEST_NICKNAME, TEST_EMAIL, TEST_PASSWORD, UserStatus::Offline);
    ASSERT_TRUE(s_userData->saveUserToDb(user));

    // 先查询一次触发缓存回填
    ASSERT_TRUE(s_userData->getUserById(TEST_USER_ID).has_value());
    ASSERT_EQ(s_redis->exists(std::string(USER_ID_KEY_PREFIX) + TEST_USER_ID), 1);

    // 清空缓存模拟未命中，查询应回到数据库（返回带真实主键的实体）
    s_redis->del({
        std::string(USER_ID_KEY_PREFIX) + TEST_USER_ID,
        std::string(USER_NICKNAME_KEY_PREFIX) + TEST_NICKNAME,
        std::string(USER_EMAIL_KEY_PREFIX) + TEST_EMAIL,
    });
    auto queried = s_userData->getUserById(TEST_USER_ID);
    ASSERT_TRUE(queried.has_value());
    EXPECT_GT(queried->getId(), 0U);

    // 回源后缓存被重新写入
    EXPECT_EQ(s_redis->exists(std::string(USER_ID_KEY_PREFIX) + TEST_USER_ID), 1);
}

/**
 * @brief 缓存命中：昵称、邮箱维度同样先命中缓存，不必回源数据库
 */
TEST_F(UserDataTest, NicknameAndEmailCacheHit) {
    UserEntity user(TEST_USER_ID, TEST_NICKNAME, TEST_EMAIL, TEST_PASSWORD, UserStatus::Offline);
    ASSERT_TRUE(s_userData->saveUserToDb(user));

    // 首次查询回填昵称、邮箱两个维度的缓存
    ASSERT_TRUE(s_userData->getUserByNickname(TEST_NICKNAME).has_value());
    ASSERT_TRUE(s_userData->getUserByEmail(TEST_EMAIL).has_value());

    // 删除数据库记录，仅保留缓存，用于验证查询确实走了缓存
    {
        odb::transaction transaction(s_database->begin());
        s_database->erase_query<UserEntity>(odb::query<UserEntity>::userId == TEST_USER_ID);
        transaction.commit();
    }

    auto byNickname = s_userData->getUserByNickname(TEST_NICKNAME);
    ASSERT_TRUE(byNickname.has_value());
    EXPECT_EQ(byNickname->getUserId(), TEST_USER_ID);
    EXPECT_EQ(byNickname->getId(), 0U);

    auto byEmail = s_userData->getUserByEmail(TEST_EMAIL);
    ASSERT_TRUE(byEmail.has_value());
    EXPECT_EQ(byEmail->getUserId(), TEST_USER_ID);
    EXPECT_EQ(byEmail->getId(), 0U);
}

/**
 * @brief 状态更新：数据库状态变更后缓存被失效，再次查询得到新状态
 */
TEST_F(UserDataTest, UpdateStatusInvalidatesCache) {
    UserEntity user(TEST_USER_ID, TEST_NICKNAME, TEST_EMAIL, TEST_PASSWORD, UserStatus::Offline);
    ASSERT_TRUE(s_userData->saveUserToDb(user));
    ASSERT_TRUE(s_userData->getUserById(TEST_USER_ID).has_value());
    ASSERT_EQ(s_redis->exists(std::string(USER_ID_KEY_PREFIX) + TEST_USER_ID), 1);

    EXPECT_TRUE(s_userData->updateUserStatus(TEST_USER_ID, UserStatus::Online));
    EXPECT_EQ(s_redis->exists(std::string(USER_ID_KEY_PREFIX) + TEST_USER_ID), 0);

    auto queried = s_userData->getUserById(TEST_USER_ID);
    ASSERT_TRUE(queried.has_value());
    EXPECT_EQ(queried->getStatus(), UserStatus::Online);

    // 用户不存在时返回 false，不抛异常
    EXPECT_FALSE(s_userData->updateUserStatus("not_exist_user", UserStatus::Online));
}

/**
 * @brief upsert：saveUserToDb 在用户不存在时插入、存在时更新，并失效新旧昵称/邮箱缓存
 */
TEST_F(UserDataTest, SaveUserToDbUpsert) {
    UserEntity user(TEST_USER_ID, TEST_NICKNAME, TEST_EMAIL, TEST_PASSWORD, UserStatus::Offline);
    ASSERT_TRUE(s_userData->saveUserToDb(user));

    auto inserted = s_userData->getUserById(TEST_USER_ID);
    ASSERT_TRUE(inserted.has_value());
    EXPECT_EQ(inserted->getNickname(), TEST_NICKNAME);

    // 触发缓存回填，确认旧昵称维度的 key 存在
    ASSERT_TRUE(s_userData->getUserByNickname(TEST_NICKNAME).has_value());
    ASSERT_EQ(s_redis->exists(std::string(USER_NICKNAME_KEY_PREFIX) + TEST_NICKNAME), 1);
    ASSERT_EQ(s_redis->exists(std::string(USER_EMAIL_KEY_PREFIX) + TEST_EMAIL), 1);

    // 同一 userId 再次保存：应更新而不是插入
    UserEntity changed(TEST_USER_ID, TEST_NICKNAME_2, TEST_EMAIL_2, TEST_PASSWORD, UserStatus::Online);
    ASSERT_TRUE(s_userData->saveUserToDb(changed));

    auto updated = s_userData->getUserById(TEST_USER_ID);
    ASSERT_TRUE(updated.has_value());
    EXPECT_EQ(updated->getNickname(), TEST_NICKNAME_2);
    EXPECT_EQ(updated->getEmail(), TEST_EMAIL_2);
    EXPECT_EQ(updated->getStatus(), UserStatus::Online);

    // 旧昵称、旧邮箱维度的缓存应已失效
    EXPECT_EQ(s_redis->exists(std::string(USER_NICKNAME_KEY_PREFIX) + TEST_NICKNAME), 0);
    EXPECT_EQ(s_redis->exists(std::string(USER_EMAIL_KEY_PREFIX) + TEST_EMAIL), 0);

    // 旧昵称已查不到，说明数据库中是更新而非新增
    EXPECT_FALSE(s_userData->getUserByNickname(TEST_NICKNAME).has_value());
}

/**
 * @brief 异常场景：昵称重复写入被数据库唯一约束拒绝
 * @note 数据层返回 false，由业务层统一转成 USER_SAVE_USER_INFO_FAILED
 */
TEST_F(UserDataTest, DuplicateNicknameRejected) {
    UserEntity first(TEST_USER_ID, TEST_NICKNAME, TEST_EMAIL, TEST_PASSWORD, UserStatus::Offline);
    ASSERT_TRUE(s_userData->saveUserToDb(first));

    UserEntity duplicate(TEST_OTHER_USER_ID, TEST_NICKNAME, TEST_OTHER_EMAIL, TEST_PASSWORD, UserStatus::Offline);
    EXPECT_FALSE(s_userData->saveUserToDb(duplicate));

    // 原用户数据不受影响
    auto original = s_userData->getUserByNickname(TEST_NICKNAME);
    ASSERT_TRUE(original.has_value());
    EXPECT_EQ(original->getUserId(), TEST_USER_ID);
}

/**
 * @brief 异常场景：邮箱重复写入被数据库唯一约束拒绝
 * @note 数据层返回 false，由业务层统一转换为业务错误码
 */
TEST_F(UserDataTest, DuplicateEmailRejected) {
    UserEntity first(TEST_USER_ID, TEST_NICKNAME, TEST_EMAIL, TEST_PASSWORD, UserStatus::Offline);
    ASSERT_TRUE(s_userData->saveUserToDb(first));

    UserEntity duplicate(TEST_OTHER_USER_ID, "test_nick_0002", TEST_EMAIL, TEST_PASSWORD, UserStatus::Offline);
    EXPECT_FALSE(s_userData->saveUserToDb(duplicate));
}

/**
 * @brief 边界场景：用户不存在时各查询返回空，不抛异常
 */
TEST_F(UserDataTest, MissingUserReturnsNullopt) {
    EXPECT_FALSE(s_userData->getUserById("not_exist_user").has_value());
    EXPECT_FALSE(s_userData->getUserByNickname("not_exist_nickname").has_value());
    EXPECT_FALSE(s_userData->getUserByEmail("not_exist_email@example.com").has_value());
}

} // namespace
