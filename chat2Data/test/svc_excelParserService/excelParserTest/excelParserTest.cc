#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include <OpenXLSX/OpenXLSX.hpp>
#include <bite_scaffold/log.h>
#include <gtest/gtest.h>

#include "excelParser.h"

namespace excelParserService {

namespace {

const char* const TEST_FILE_NAME = "chat2Data_excelParserTest.xlsx";

/**
 * @brief 创建测试用Excel文件
 * @param filePath 文件路径
 */
void createTestWorkbook(const std::string& filePath) {
    std::remove(filePath.c_str());

    OpenXLSX::XLDocument document;
    document.create(filePath, OpenXLSX::XLForceOverwrite);

    OpenXLSX::XLWorkbook workbook = document.workbook();
    OpenXLSX::XLWorksheet worksheet = workbook.worksheet("Sheet1");

    // 表头：包含空列名、数字开头列名、中文加非法字符列名
    worksheet.cell("A1").value() = "id";
    worksheet.cell("B1").value() = "name";
    worksheet.cell("C1").value() = "salary";
    worksheet.cell("D1").value() = "active";
    worksheet.cell("E1").value() = "hire_date";
    worksheet.cell("G1").value() = "1st col!";
    worksheet.cell("H1").value() = "金额(元)";

    // 第1行数据
    worksheet.cell("A2").value() = 1;
    worksheet.cell("B2").value() = "Alice";
    worksheet.cell("C2").value() = 12.5;
    worksheet.cell("D2").value() = true;
    worksheet.cell("E2").value() = "2024-01-02";
    worksheet.cell("G2").value() = "x";
    worksheet.cell("H2").value() = 100;

    // 第2行数据
    worksheet.cell("A3").value() = 2;
    worksheet.cell("B3").value() = "Bob";
    worksheet.cell("C3").value() = 20;
    worksheet.cell("D3").value() = false;
    worksheet.cell("E3").value() = "2024/03/04";
    worksheet.cell("G3").value() = "y";
    worksheet.cell("H3").value() = 200;

    // 第3行数据：部分单元格为字符串形式
    worksheet.cell("A4").value() = 3;
    worksheet.cell("B4").value() = "";
    worksheet.cell("C4").value() = "30.5";
    worksheet.cell("D4").value() = "yes";
    worksheet.cell("E4").value() = "05/06/2024";
    worksheet.cell("G4").value() = "z";
    worksheet.cell("H4").value() = 300;

    // 第二个worksheet用于验证worksheet列表
    workbook.addWorksheet("Second");
    OpenXLSX::XLWorksheet second = workbook.worksheet("Second");
    second.cell("A1").value() = "value";

    document.save();
    document.close();
}

} // namespace

class ExcelParserTest : public ::testing::Test {
protected:
    void SetUp() override {
        _filePath = std::string(::testing::TempDir()) + TEST_FILE_NAME;
        createTestWorkbook(_filePath);
        _parser = std::make_unique<ExcelParser>();
    }

    void TearDown() override {
        std::remove(_filePath.c_str());
    }

    std::string _filePath;
    std::unique_ptr<ExcelParser> _parser;
};

TEST_F(ExcelParserTest, GetWorksheetNamesSuccess) {
    const std::vector<std::string> names = _parser->getWorksheetNames(_filePath);
    ASSERT_EQ(2U, names.size());
    EXPECT_EQ("Sheet1", names[0]);
    EXPECT_EQ("Second", names[1]);
}

TEST_F(ExcelParserTest, GetWorksheetNamesWithInvalidPath) {
    EXPECT_TRUE(_parser->getWorksheetNames("").empty());
    EXPECT_TRUE(_parser->getWorksheetNames("/tmp/not_exists_excel_file.xlsx").empty());
}

TEST_F(ExcelParserTest, ParseWorksheetHeader) {
    std::unique_ptr<WorksheetData> data = _parser->parseWorksheet(_filePath, "Sheet1");
    ASSERT_NE(nullptr, data);
    ASSERT_EQ(8U, data->_columns.size());

    EXPECT_EQ("id", data->_columns[0]._name);
    EXPECT_EQ("name", data->_columns[1]._name);
    EXPECT_EQ("column_6", data->_columns[5]._name);
    EXPECT_EQ("col_1st_col_", data->_columns[6]._name);
    EXPECT_EQ("金额_元_", data->_columns[7]._name);
}

TEST_F(ExcelParserTest, ParseWorksheetColumnTypes) {
    std::unique_ptr<WorksheetData> data = _parser->parseWorksheet(_filePath, "Sheet1");
    ASSERT_NE(nullptr, data);
    ASSERT_EQ(8U, data->_columns.size());

    EXPECT_EQ(ColumnType::BIGINT, data->_columns[0]._type);
    EXPECT_EQ(ColumnType::TEXT, data->_columns[1]._type);
    EXPECT_EQ(ColumnType::DOUBLE, data->_columns[2]._type);
    EXPECT_EQ(ColumnType::BOOLEAN, data->_columns[3]._type);
    EXPECT_EQ(ColumnType::DATE, data->_columns[4]._type);
    EXPECT_EQ(ColumnType::TEXT, data->_columns[5]._type);
    EXPECT_EQ(ColumnType::TEXT, data->_columns[6]._type);
    EXPECT_EQ(ColumnType::BIGINT, data->_columns[7]._type);
}

TEST_F(ExcelParserTest, ParseWorksheetRows) {
    std::unique_ptr<WorksheetData> data = _parser->parseWorksheet(_filePath, "Sheet1");
    ASSERT_NE(nullptr, data);
    EXPECT_EQ(4, data->_totalRows);
    EXPECT_EQ(8, data->_totalCols);
    ASSERT_EQ(3U, data->_rows.size());
    ASSERT_EQ(8U, data->_rows[0]._cells.size());

    // 整型转int64字符串
    EXPECT_EQ("1", data->_rows[0]._cells[0]._value);
    EXPECT_EQ("Integer", cellTypeToString(data->_rows[0]._cells[0]._type));

    // 浮点型按double转字符串
    EXPECT_EQ("12.500000", data->_rows[0]._cells[2]._value);
    EXPECT_EQ("Float", cellTypeToString(data->_rows[0]._cells[2]._type));

    // 布尔型转为0/1
    EXPECT_EQ("1", data->_rows[0]._cells[3]._value);
    EXPECT_EQ("0", data->_rows[1]._cells[3]._value);
    EXPECT_EQ("Boolean", cellTypeToString(data->_rows[0]._cells[3]._type));

    // 空单元格
    EXPECT_EQ("", data->_rows[0]._cells[5]._value);
    EXPECT_EQ("Empty", cellTypeToString(data->_rows[0]._cells[5]._type));

    // 字符串单元格
    EXPECT_EQ("Alice", data->_rows[0]._cells[1]._value);
    EXPECT_EQ("String", cellTypeToString(data->_rows[0]._cells[1]._type));
}

TEST_F(ExcelParserTest, ParseNotExistWorksheet) {
    EXPECT_EQ(nullptr, _parser->parseWorksheet(_filePath, "NotExist"));
    EXPECT_EQ(nullptr, _parser->parseWorksheet("", "Sheet1"));
    EXPECT_EQ(nullptr, _parser->parseWorksheet("/tmp/not_exists_excel_file.xlsx", "Sheet1"));
}

} // namespace excelParserService

int main(int argc, char** argv) {
    // 解析器内部使用bite_scaffold日志宏，测试前需要先初始化日志器
    bitelog::log_settings logSettings;
    logSettings.level = 2;
    logSettings.path = "stdout";
    bitelog::bitelog_init(logSettings);

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
