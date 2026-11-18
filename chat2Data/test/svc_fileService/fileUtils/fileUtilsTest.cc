#include <string>

#include <gtest/gtest.h>

#include "common.h"

namespace {

using fileService::calculateTableName;
using fileService::isSqliteFile;
using fileService::isXlsxFile;

// 数据库表名最大长度，与实现保持一致
constexpr std::size_t TABLE_NAME_MAX_LENGTH = 64;
// worksheet名称部分的最大长度，与实现保持一致
constexpr std::size_t TABLE_NAME_PREFIX_MAX_LENGTH = 31;

/**
 * @brief 文件子服务纯工具函数单元测试
 *
 * 不依赖 MySQL / Redis，只校验文件后缀判断与表名生成规则。
 */
TEST(FileUtilsTest, IsXlsxFile) {
    EXPECT_TRUE(isXlsxFile(".xlsx"));
    EXPECT_TRUE(isXlsxFile(".XLSX"));
    EXPECT_TRUE(isXlsxFile(".Xlsx"));
    EXPECT_FALSE(isXlsxFile(".xls"));
    EXPECT_FALSE(isXlsxFile(".db"));
    EXPECT_FALSE(isXlsxFile(""));
}

TEST(FileUtilsTest, IsSqliteFile) {
    EXPECT_TRUE(isSqliteFile(".db"));
    EXPECT_TRUE(isSqliteFile(".DB"));
    EXPECT_FALSE(isSqliteFile(".sqlite"));
    EXPECT_FALSE(isSqliteFile(".xlsx"));
    EXPECT_FALSE(isSqliteFile(""));
}

/**
 * @brief 表名 = 规范化worksheet名称 + 下划线 + 去掉连字符的UUID
 */
TEST(FileUtilsTest, TableNameKeepsValidCharacters) {
    const std::string tableName = calculateTableName("Sheet1");
    EXPECT_EQ(tableName.rfind("Sheet1_", 0), 0U);
    EXPECT_EQ(tableName.find('-'), std::string::npos);
    EXPECT_LE(tableName.size(), TABLE_NAME_MAX_LENGTH);
}

/**
 * @brief 非法字符替换为下划线
 */
TEST(FileUtilsTest, TableNameReplacesInvalidCharacters) {
    EXPECT_EQ(calculateTableName("my sheet").rfind("my_sheet_", 0), 0U);
    EXPECT_EQ(calculateTableName("a-b.c").rfind("a_b_c_", 0), 0U);
}

/**
 * @brief 空名称使用默认前缀worksheet
 */
TEST(FileUtilsTest, TableNameFallsBackToDefault) {
    EXPECT_EQ(calculateTableName("").rfind("worksheet_", 0), 0U);
    EXPECT_EQ(calculateTableName("   ").rfind("____", 0), 0U);
}

/**
 * @brief 数字开头时补ws_前缀，保证表名不以数字开头
 */
TEST(FileUtilsTest, TableNamePrefixesDigitStart) {
    EXPECT_EQ(calculateTableName("2024").rfind("ws_2024_", 0), 0U);
}

/**
 * @brief 长名称截断，整体表名不超过数据库上限
 */
TEST(FileUtilsTest, TableNameTruncatesLongName) {
    const std::string longName(100, 'a');
    const std::string tableName = calculateTableName(longName);
    EXPECT_LE(tableName.size(), TABLE_NAME_MAX_LENGTH);
    EXPECT_EQ(tableName.rfind(std::string(TABLE_NAME_PREFIX_MAX_LENGTH, 'a') + "_", 0), 0U);
}

/**
 * @brief 每次生成都带不同的UUID后缀，避免表名冲突
 */
TEST(FileUtilsTest, TableNameIsUniquePerCall) {
    EXPECT_NE(calculateTableName("Sheet1"), calculateTableName("Sheet1"));
}

} // namespace
