#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <odb/database.hxx>
#include <odb/query.hxx>
#include <odb/transaction.hxx>
#include <sw/redis++/redis.h>

#include <bite_scaffold/log.h>
#include <bite_scaffold/odb.h>
#include <bite_scaffold/redis.h>

#include "errorHandler.h"
#include "fileInfoData.h"
#include "fileInfoEntity-odb.hxx"
#include "worksheetEntity-odb.hxx"

namespace {

using chat2Data::FileInfoEntity;
using chat2Data::WorksheetEntity;
using fileService::FileInfoData;

// 测试数据固定值，每个用例开始与结束时都会清理，避免污染数据库
constexpr const char* TEST_FILE_ID = "test_file_0001";
constexpr const char* TEST_FILE_NAME = "sales.xlsx";
constexpr const char* TEST_FILE_EXT = "xlsx";
constexpr const char* TEST_FDFS_FILE_ID = "group1/M00/00/00/test_file_0001.xlsx";
constexpr const char* TEST_USER_ID = "test_user_0001";
constexpr const char* TEST_CHAT_SESSION_ID = "test_session_0001";
constexpr std::uint64_t TEST_FILE_SIZE = 1024;
constexpr std::uint64_t TEST_UPLOAD_TIME = 1763200000;
// 用于排序与列表查询的第二个文件
constexpr const char* TEST_FILE_ID_2 = "test_file_0002";
constexpr const char* TEST_FILE_NAME_2 = "report.xlsx";
constexpr std::uint64_t TEST_UPLOAD_TIME_2 = 1763203600;

// 文件缓存 key 前缀与过期时间，与数据访问层实现保持一致
constexpr const char* FILE_CACHE_KEY_PREFIX = "file:";
constexpr long long FILE_CACHE_TTL_SECONDS = 3LL * 24 * 60 * 60;
// TTL 检查容差，避免读写之间耗时导致断言失败
constexpr long long TTL_CHECK_TOLERANCE_SECONDS = 100;

/**
 * @brief 文件信息数据访问层单元测试
 *
 * 直连 dev-mysql / dev-redis，用例结束清理自身插入的数据。
 */
class FileInfoDataTest : public testing::Test {
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

        s_fileInfoData = std::make_shared<FileInfoData>(s_database, s_redis);
    }

    void SetUp() override {
        cleanTestData();
    }

    void TearDown() override {
        cleanTestData();
    }

    /**
     * @brief 清理测试数据：先删 worksheet（外键子表），再删文件信息与缓存
     */
    static void cleanTestData() {
        {
            odb::transaction transaction(s_database->begin());
            s_database->erase_query<WorksheetEntity>(odb::query<WorksheetEntity>::fileId == TEST_FILE_ID);
            s_database->erase_query<WorksheetEntity>(odb::query<WorksheetEntity>::fileId == TEST_FILE_ID_2);
            s_database->erase_query<FileInfoEntity>(odb::query<FileInfoEntity>::fileId == TEST_FILE_ID);
            s_database->erase_query<FileInfoEntity>(odb::query<FileInfoEntity>::fileId == TEST_FILE_ID_2);
            transaction.commit();
        }
        s_redis->del({
            std::string(FILE_CACHE_KEY_PREFIX) + TEST_FILE_ID,
            std::string(FILE_CACHE_KEY_PREFIX) + TEST_FILE_ID_2,
        });
    }

    /**
     * @brief 构造测试用文件信息实体
     */
    static FileInfoEntity makeFileInfo(const std::string& fileId = TEST_FILE_ID,
                                       const std::string& fileName = TEST_FILE_NAME,
                                       std::uint64_t uploadTime = TEST_UPLOAD_TIME) {
        return FileInfoEntity(fileId, fileName, TEST_FILE_SIZE, uploadTime, TEST_FILE_EXT,
                              TEST_FDFS_FILE_ID, TEST_USER_ID, TEST_CHAT_SESSION_ID);
    }

    static std::shared_ptr<odb::database> s_database;
    static std::shared_ptr<sw::redis::Redis> s_redis;
    static FileInfoData::ptr s_fileInfoData;
};

std::shared_ptr<odb::database> FileInfoDataTest::s_database = nullptr;
std::shared_ptr<sw::redis::Redis> FileInfoDataTest::s_redis = nullptr;
FileInfoData::ptr FileInfoDataTest::s_fileInfoData = nullptr;

/**
 * @brief 正常流程：新增文件后可按 fileId 查询，且写入 string 类型的 JSON 缓存，TTL 为 3 天
 */
TEST_F(FileInfoDataTest, InsertAndQueryByFileId) {
    ASSERT_TRUE(s_fileInfoData->saveFileInfo(makeFileInfo()));

    auto queried = s_fileInfoData->getFileInfoByFileId(TEST_FILE_ID);
    ASSERT_TRUE(queried.has_value());
    EXPECT_EQ(queried->getFileId(), TEST_FILE_ID);
    EXPECT_EQ(queried->getFileName(), TEST_FILE_NAME);
    EXPECT_EQ(queried->getFileSize(), TEST_FILE_SIZE);
    EXPECT_EQ(queried->getUploadTime(), TEST_UPLOAD_TIME);
    EXPECT_EQ(queried->getFileExt(), TEST_FILE_EXT);
    EXPECT_EQ(queried->getFdfsFileId(), TEST_FDFS_FILE_ID);
    EXPECT_EQ(queried->getUserId(), TEST_USER_ID);
    EXPECT_EQ(queried->getChatSessionId(), TEST_CHAT_SESSION_ID);

    // 缓存 value 为 JSON 字符串，不是 hash
    const std::string key = std::string(FILE_CACHE_KEY_PREFIX) + TEST_FILE_ID;
    EXPECT_EQ(s_redis->type(key), "string");
    const std::string json = s_redis->get(key).value_or("");
    EXPECT_NE(json.find("\"fileId\""), std::string::npos);
    EXPECT_NE(json.find("\"fdfsFileId\""), std::string::npos);

    // 缓存 TTL 为 3 天
    const long long ttl = s_redis->ttl(key);
    EXPECT_GT(ttl, FILE_CACHE_TTL_SECONDS - TTL_CHECK_TOLERANCE_SECONDS);
    EXPECT_LE(ttl, FILE_CACHE_TTL_SECONDS);
}

/**
 * @brief 缓存未命中：删除缓存后仍能从数据库回源，并重新回填缓存
 */
TEST_F(FileInfoDataTest, CacheMissFallsBackToDatabase) {
    ASSERT_TRUE(s_fileInfoData->saveFileInfo(makeFileInfo()));
    ASSERT_TRUE(s_fileInfoData->getFileInfoByFileId(TEST_FILE_ID).has_value());

    const std::string key = std::string(FILE_CACHE_KEY_PREFIX) + TEST_FILE_ID;
    ASSERT_EQ(s_redis->exists(key), 1);

    s_redis->del(key);
    auto queried = s_fileInfoData->getFileInfoByFileId(TEST_FILE_ID);
    ASSERT_TRUE(queried.has_value());
    EXPECT_GT(queried->getId(), 0U);

    // 回源后缓存被重新写入
    EXPECT_EQ(s_redis->exists(key), 1);
}

/**
 * @brief 缓存命中：删除数据库记录后仍能从缓存返回，缓存实体没有数据库主键 id
 */
TEST_F(FileInfoDataTest, CacheHitWithoutDatabase) {
    ASSERT_TRUE(s_fileInfoData->saveFileInfo(makeFileInfo()));
    ASSERT_TRUE(s_fileInfoData->getFileInfoByFileId(TEST_FILE_ID).has_value());

    {
        odb::transaction transaction(s_database->begin());
        s_database->erase_query<FileInfoEntity>(odb::query<FileInfoEntity>::fileId == TEST_FILE_ID);
        transaction.commit();
    }

    auto cached = s_fileInfoData->getFileInfoByFileId(TEST_FILE_ID);
    ASSERT_TRUE(cached.has_value());
    EXPECT_EQ(cached->getFileId(), TEST_FILE_ID);
    EXPECT_EQ(cached->getId(), 0U);
}

/**
 * @brief upsert：同一 fileId 再次保存应更新而不是插入，并失效缓存
 */
TEST_F(FileInfoDataTest, SaveFileInfoUpsert) {
    ASSERT_TRUE(s_fileInfoData->saveFileInfo(makeFileInfo()));
    ASSERT_TRUE(s_fileInfoData->getFileInfoByFileId(TEST_FILE_ID).has_value());
    const std::string key = std::string(FILE_CACHE_KEY_PREFIX) + TEST_FILE_ID;
    ASSERT_EQ(s_redis->exists(key), 1);

    FileInfoEntity changed = makeFileInfo(TEST_FILE_ID, "sales_v2.xlsx", TEST_UPLOAD_TIME + 60);
    ASSERT_TRUE(s_fileInfoData->saveFileInfo(changed));

    // 写数据库后删除缓存
    EXPECT_EQ(s_redis->exists(key), 0);

    auto updated = s_fileInfoData->getFileInfoByFileId(TEST_FILE_ID);
    ASSERT_TRUE(updated.has_value());
    EXPECT_EQ(updated->getFileName(), "sales_v2.xlsx");
    EXPECT_EQ(updated->getUploadTime(), TEST_UPLOAD_TIME + 60);
}

/**
 * @brief 删除：删除文件信息后查询为空，缓存同步失效
 */
TEST_F(FileInfoDataTest, DeleteFileInfo) {
    ASSERT_TRUE(s_fileInfoData->saveFileInfo(makeFileInfo()));
    ASSERT_TRUE(s_fileInfoData->getFileInfoByFileId(TEST_FILE_ID).has_value());

    EXPECT_TRUE(s_fileInfoData->deleteFileInfoByFileId(TEST_FILE_ID));
    EXPECT_FALSE(s_fileInfoData->getFileInfoByFileId(TEST_FILE_ID).has_value());
    EXPECT_EQ(s_redis->exists(std::string(FILE_CACHE_KEY_PREFIX) + TEST_FILE_ID), 0);
}

/**
 * @brief 列表查询：只返回指定用户的文件，并按上传时间倒序
 */
TEST_F(FileInfoDataTest, ListByUserIdOrderedByUploadTime) {
    ASSERT_TRUE(s_fileInfoData->saveFileInfo(makeFileInfo(TEST_FILE_ID, TEST_FILE_NAME, TEST_UPLOAD_TIME)));
    ASSERT_TRUE(s_fileInfoData->saveFileInfo(
        makeFileInfo(TEST_FILE_ID_2, TEST_FILE_NAME_2, TEST_UPLOAD_TIME_2)));

    auto fileList = s_fileInfoData->getFileInfoListByUserId(TEST_USER_ID);
    ASSERT_EQ(fileList.size(), 2U);
    EXPECT_EQ(fileList[0].getFileId(), TEST_FILE_ID_2);
    EXPECT_EQ(fileList[1].getFileId(), TEST_FILE_ID);

    EXPECT_TRUE(s_fileInfoData->getFileInfoListByUserId("not_exist_user").empty());
}

/**
 * @brief 边界场景：文件不存在时返回 std::nullopt
 */
TEST_F(FileInfoDataTest, MissingFileReturnsNullopt) {
    EXPECT_FALSE(s_fileInfoData->getFileInfoByFileId("not_exist_file").has_value());
}

} // namespace
