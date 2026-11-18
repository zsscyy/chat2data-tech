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
#include "worksheetData.h"
#include "worksheetEntity-odb.hxx"

namespace {

using chat2Data::FileInfoEntity;
using chat2Data::WorksheetEntity;
using fileService::FileInfoData;
using fileService::WorksheetData;

// 测试数据固定值，每个用例开始与结束时都会清理，避免污染数据库
constexpr const char* TEST_FILE_ID = "test_file_1001";
constexpr const char* TEST_FILE_NAME = "sales.xlsx";
constexpr const char* TEST_FILE_EXT = "xlsx";
constexpr const char* TEST_FDFS_FILE_ID = "group1/M00/00/00/test_file_1001.xlsx";
constexpr const char* TEST_USER_ID = "test_user_1001";
constexpr const char* TEST_CHAT_SESSION_ID = "test_session_1001";
constexpr std::uint64_t TEST_FILE_SIZE = 2048;
constexpr std::uint64_t TEST_UPLOAD_TIME = 1763300000;

constexpr const char* TEST_WORKSHEET_NAME_1 = "Sheet1";
constexpr const char* TEST_WORKSHEET_NAME_2 = "销售明细";
constexpr const char* TEST_DB_TABLE_NAME_1 = "sheet1_1001";
constexpr const char* TEST_DB_TABLE_NAME_2 = "sales_1001";

// worksheet映射缓存 key 前缀与过期时间，与数据访问层实现保持一致
constexpr const char* WORKSHEET_CACHE_KEY_PREFIX = "worksheet:";
constexpr long long WORKSHEET_CACHE_TTL_SECONDS = 3LL * 24 * 60 * 60;
// TTL 检查容差，避免读写之间耗时导致断言失败
constexpr long long TTL_CHECK_TOLERANCE_SECONDS = 100;

/**
 * @brief worksheet映射数据访问层单元测试
 *
 * 直连 dev-mysql / dev-redis，用例结束清理自身插入的数据。
 * 由于 tbl_worksheet.fileId 对 tbl_fileInfo.fileId 有外键约束，
 * 每个用例会先写入一条文件信息记录。
 */
class WorksheetDataTest : public testing::Test {
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
        s_worksheetData = std::make_shared<WorksheetData>(s_database, s_redis);
    }

    void SetUp() override {
        cleanTestData();
        // 先写文件信息，满足 tbl_worksheet 的外键约束
        s_fileInfoData->saveFileInfo(FileInfoEntity(TEST_FILE_ID, TEST_FILE_NAME, TEST_FILE_SIZE,
                                                    TEST_UPLOAD_TIME, TEST_FILE_EXT, TEST_FDFS_FILE_ID,
                                                    TEST_USER_ID, TEST_CHAT_SESSION_ID));
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
            s_database->erase_query<FileInfoEntity>(odb::query<FileInfoEntity>::fileId == TEST_FILE_ID);
            transaction.commit();
        }
        s_redis->del({
            std::string(WORKSHEET_CACHE_KEY_PREFIX) + TEST_FILE_ID,
        });
    }

    /**
     * @brief 构造测试用worksheet映射列表
     */
    static std::vector<WorksheetEntity> makeWorksheets() {
        return {
            WorksheetEntity(TEST_FILE_ID, TEST_WORKSHEET_NAME_1, TEST_DB_TABLE_NAME_1),
            WorksheetEntity(TEST_FILE_ID, TEST_WORKSHEET_NAME_2, TEST_DB_TABLE_NAME_2),
        };
    }

    static std::shared_ptr<odb::database> s_database;
    static std::shared_ptr<sw::redis::Redis> s_redis;
    static FileInfoData::ptr s_fileInfoData;
    static WorksheetData::ptr s_worksheetData;
};

std::shared_ptr<odb::database> WorksheetDataTest::s_database = nullptr;
std::shared_ptr<sw::redis::Redis> WorksheetDataTest::s_redis = nullptr;
FileInfoData::ptr WorksheetDataTest::s_fileInfoData = nullptr;
WorksheetData::ptr WorksheetDataTest::s_worksheetData = nullptr;

/**
 * @brief 正常流程：保存映射后可按 fileId 查询，且写入 string 类型的 JSON 缓存，TTL 为 3 天
 */
TEST_F(WorksheetDataTest, SaveAndQueryMappings) {
    ASSERT_TRUE(s_worksheetData->saveWorksheetMappings(TEST_FILE_ID, makeWorksheets()));

    auto mappings = s_worksheetData->getWorksheetMappingsByFileId(TEST_FILE_ID);
    ASSERT_EQ(mappings.size(), 2U);
    EXPECT_EQ(mappings[0].getFileId(), TEST_FILE_ID);
    EXPECT_EQ(mappings[0].getWorksheetName(), TEST_WORKSHEET_NAME_1);
    EXPECT_EQ(mappings[0].getTableName(), TEST_DB_TABLE_NAME_1);
    EXPECT_EQ(mappings[1].getWorksheetName(), TEST_WORKSHEET_NAME_2);
    EXPECT_EQ(mappings[1].getTableName(), TEST_DB_TABLE_NAME_2);

    // 缓存 value 为 JSON 字符串，包含 worksheets 数组
    const std::string key = std::string(WORKSHEET_CACHE_KEY_PREFIX) + TEST_FILE_ID;
    EXPECT_EQ(s_redis->type(key), "string");
    const std::string json = s_redis->get(key).value_or("");
    EXPECT_NE(json.find("\"worksheets\""), std::string::npos);
    EXPECT_NE(json.find("\"tableName\""), std::string::npos);

    // 缓存 TTL 为 3 天
    const long long ttl = s_redis->ttl(key);
    EXPECT_GT(ttl, WORKSHEET_CACHE_TTL_SECONDS - TTL_CHECK_TOLERANCE_SECONDS);
    EXPECT_LE(ttl, WORKSHEET_CACHE_TTL_SECONDS);
}

/**
 * @brief 旧缓存兼容：历史缓存字段为 dbTableName，读取时仍能解析为 tableName
 */
TEST_F(WorksheetDataTest, LegacyCacheFieldCompatible) {
    const std::string key = std::string(WORKSHEET_CACHE_KEY_PREFIX) + TEST_FILE_ID;
    const std::string legacyJson =
        "{\"fileId\":\"test_file_1001\",\"worksheets\":["
        "{\"fileId\":\"test_file_1001\",\"worksheetName\":\"Sheet1\","
        "\"dbTableName\":\"legacy_table_1001\"}]}";
    s_redis->setex(key, 600, legacyJson);

    auto mappings = s_worksheetData->getWorksheetMappingsByFileId(TEST_FILE_ID);
    ASSERT_EQ(mappings.size(), 1U);
    EXPECT_EQ(mappings[0].getWorksheetName(), "Sheet1");
    EXPECT_EQ(mappings[0].getTableName(), "legacy_table_1001");
}

/**
 * @brief 缓存未命中：删除缓存后从数据库回源，并重新回填缓存
 */
TEST_F(WorksheetDataTest, CacheMissFallsBackToDatabase) {
    ASSERT_TRUE(s_worksheetData->saveWorksheetMappings(TEST_FILE_ID, makeWorksheets()));
    ASSERT_EQ(s_worksheetData->getWorksheetMappingsByFileId(TEST_FILE_ID).size(), 2U);

    const std::string key = std::string(WORKSHEET_CACHE_KEY_PREFIX) + TEST_FILE_ID;
    ASSERT_EQ(s_redis->exists(key), 1);

    s_redis->del(key);
    auto mappings = s_worksheetData->getWorksheetMappingsByFileId(TEST_FILE_ID);
    ASSERT_EQ(mappings.size(), 2U);
    // 回源得到的实体带数据库主键
    EXPECT_GT(mappings[0].getId(), 0U);
    EXPECT_EQ(s_redis->exists(key), 1);
}

/**
 * @brief 缓存命中：删除数据库记录后仍能从缓存返回
 */
TEST_F(WorksheetDataTest, CacheHitWithoutDatabase) {
    ASSERT_TRUE(s_worksheetData->saveWorksheetMappings(TEST_FILE_ID, makeWorksheets()));
    ASSERT_EQ(s_worksheetData->getWorksheetMappingsByFileId(TEST_FILE_ID).size(), 2U);

    {
        odb::transaction transaction(s_database->begin());
        s_database->erase_query<WorksheetEntity>(odb::query<WorksheetEntity>::fileId == TEST_FILE_ID);
        transaction.commit();
    }

    auto mappings = s_worksheetData->getWorksheetMappingsByFileId(TEST_FILE_ID);
    ASSERT_EQ(mappings.size(), 2U);
    EXPECT_EQ(mappings[0].getId(), 0U);
}

/**
 * @brief 重复保存幂等：同一文件再次保存映射时替换旧映射，不产生重复记录
 */
TEST_F(WorksheetDataTest, SaveMappingsIsIdempotent) {
    ASSERT_TRUE(s_worksheetData->saveWorksheetMappings(TEST_FILE_ID, makeWorksheets()));

    std::vector<WorksheetEntity> replaced = {
        WorksheetEntity(TEST_FILE_ID, "Sheet1", "sheet1_v2_1001"),
    };
    ASSERT_TRUE(s_worksheetData->saveWorksheetMappings(TEST_FILE_ID, replaced));

    auto mappings = s_worksheetData->getWorksheetMappingsByFileId(TEST_FILE_ID);
    ASSERT_EQ(mappings.size(), 1U);
    EXPECT_EQ(mappings[0].getTableName(), "sheet1_v2_1001");
}

/**
 * @brief 删除：删除映射后查询为空，缓存同步失效
 */
TEST_F(WorksheetDataTest, DeleteMappings) {
    ASSERT_TRUE(s_worksheetData->saveWorksheetMappings(TEST_FILE_ID, makeWorksheets()));
    ASSERT_EQ(s_worksheetData->getWorksheetMappingsByFileId(TEST_FILE_ID).size(), 2U);

    EXPECT_TRUE(s_worksheetData->deleteWorksheetMappingsByFileId(TEST_FILE_ID));
    EXPECT_TRUE(s_worksheetData->getWorksheetMappingsByFileId(TEST_FILE_ID).empty());
    EXPECT_EQ(s_redis->exists(std::string(WORKSHEET_CACHE_KEY_PREFIX) + TEST_FILE_ID), 0);
}

/**
 * @brief 外键级联：删除文件信息后，其 worksheet 映射被数据库自动级联删除
 */
TEST_F(WorksheetDataTest, ForeignKeyCascadeDelete) {
    ASSERT_TRUE(s_worksheetData->saveWorksheetMappings(TEST_FILE_ID, makeWorksheets()));
    ASSERT_TRUE(s_fileInfoData->deleteFileInfoByFileId(TEST_FILE_ID));

    // 清掉缓存后回源，确认数据库中的映射已被级联删除
    s_redis->del(std::string(WORKSHEET_CACHE_KEY_PREFIX) + TEST_FILE_ID);
    EXPECT_TRUE(s_worksheetData->getWorksheetMappingsByFileId(TEST_FILE_ID).empty());

    // 重新写回文件信息，避免 TearDown 的清理逻辑受外键顺序影响
    ASSERT_TRUE(s_fileInfoData->saveFileInfo(FileInfoEntity(TEST_FILE_ID, TEST_FILE_NAME, TEST_FILE_SIZE,
                                                            TEST_UPLOAD_TIME, TEST_FILE_EXT,
                                                            TEST_FDFS_FILE_ID, TEST_USER_ID,
                                                            TEST_CHAT_SESSION_ID)));
}

/**
 * @brief 边界场景：文件没有 worksheet 映射时返回空列表
 */
TEST_F(WorksheetDataTest, MissingFileReturnsEmptyList) {
    EXPECT_TRUE(s_worksheetData->getWorksheetMappingsByFileId("not_exist_file").empty());
}

} // namespace
