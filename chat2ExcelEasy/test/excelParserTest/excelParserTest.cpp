#include <gtest/gtest.h>

#include <OpenXLSX.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "execlParser.h"
#include "log.h"

namespace chat2excel {
namespace test {

namespace fs = std::filesystem;

// 测试Excel文件存放目录，由CMake在编译期通过宏注入
static const std::string kDataDir = TEST_DATA_DIR;

// 各类测试用Excel文件路径
static const std::string kSingleSheetFile = kDataDir + "/single_sheet.xlsx";
static const std::string kMultiSheetFile = kDataDir + "/multi_sheets.xlsx";
static const std::string kCellTypeFile = kDataDir + "/cell_types.xlsx";
static const std::string kInvalidExcelFile = kDataDir + "/invalid.xlsx";

// 成绩表worksheet的期望解析结果
static const std::string kStudentSheetData =
    "姓名,性别,年龄,C语言成绩\n"
    "张三,男,18,90\n"
    "李四,女,19,95\n"
    "王五,男,20,88";

/**
 * @brief ExcelParser测试夹具
 *
 * 在SetUpTestSuite中使用OpenXLSX生成测试Excel文件：
 * - single_sheet.xlsx：仅包含一个worksheet（成绩表）
 * - multi_sheets.xlsx：包含多个worksheet（成绩表、学生信息、班级信息、单行表）
 * - cell_types.xlsx：包含各种单元格值类型（字符串、整数、浮点数、布尔、空）
 * - invalid.xlsx：内容不是合法Excel的文件，用于验证异常场景
 */
class ExcelParserTest : public ::testing::Test {
public:
    static void SetUpTestSuite() {
        // 确保测试数据目录存在
        fs::create_directories(kDataDir);

        createSingleSheetFile();
        createMultiSheetFile();
        createCellTypeFile();
        createInvalidExcelFile();
    }

protected:
    /**
     * @brief 向worksheet中写入学生成绩数据（1行表头 + 3行数据）
     */
    static void writeStudentSheet(OpenXLSX::XLWorksheet& worksheet) {
        worksheet.cell("A1").value() = "姓名";
        worksheet.cell("B1").value() = "性别";
        worksheet.cell("C1").value() = "年龄";
        worksheet.cell("D1").value() = "C语言成绩";

        worksheet.cell("A2").value() = "张三";
        worksheet.cell("B2").value() = "男";
        worksheet.cell("C2").value() = 18;
        worksheet.cell("D2").value() = 90;

        worksheet.cell("A3").value() = "李四";
        worksheet.cell("B3").value() = "女";
        worksheet.cell("C3").value() = 19;
        worksheet.cell("D3").value() = 95;

        worksheet.cell("A4").value() = "王五";
        worksheet.cell("B4").value() = "男";
        worksheet.cell("C4").value() = 20;
        worksheet.cell("D4").value() = 88;
    }

    /**
     * @brief 生成仅包含单个worksheet的Excel文件
     */
    static void createSingleSheetFile() {
        OpenXLSX::XLDocument doc;
        doc.create(kSingleSheetFile, true);

        auto worksheet = doc.workbook().worksheet(1);
        worksheet.setName("成绩表");
        writeStudentSheet(worksheet);

        doc.save();
        doc.close();
    }

    /**
     * @brief 生成包含多个worksheet的Excel文件
     */
    static void createMultiSheetFile() {
        OpenXLSX::XLDocument doc;
        doc.create(kMultiSheetFile, true);

        auto workbook = doc.workbook();

        // 第1个worksheet：成绩表
        workbook.worksheet(1).setName("成绩表");
        auto scoreSheet = workbook.worksheet("成绩表");
        writeStudentSheet(scoreSheet);

        // 第2个worksheet：学生信息
        workbook.addWorksheet("学生信息");
        auto studentSheet = workbook.worksheet("学生信息");
        studentSheet.cell("A1").value() = "姓名";
        studentSheet.cell("B1").value() = "学号";
        studentSheet.cell("C1").value() = "班级";
        studentSheet.cell("A2").value() = "张三";
        studentSheet.cell("B2").value() = "2024001";
        studentSheet.cell("C2").value() = "计算机1班";
        studentSheet.cell("A3").value() = "李四";
        studentSheet.cell("B3").value() = "2024002";
        studentSheet.cell("C3").value() = "计算机2班";

        // 第3个worksheet：班级信息
        workbook.addWorksheet("班级信息");
        auto classSheet = workbook.worksheet("班级信息");
        classSheet.cell("A1").value() = "班级";
        classSheet.cell("B1").value() = "人数";
        classSheet.cell("A2").value() = "计算机1班";
        classSheet.cell("B2").value() = 30;

        // 第4个worksheet：单行表，仅包含表头，用于验证无数据行时的解析结果
        workbook.addWorksheet("单行表");
        auto singleRowSheet = workbook.worksheet("单行表");
        singleRowSheet.cell("A1").value() = "姓名";
        singleRowSheet.cell("B1").value() = "成绩";

        doc.save();
        doc.close();
    }

    /**
     * @brief 生成包含各种单元格值类型的Excel文件
     */
    static void createCellTypeFile() {
        OpenXLSX::XLDocument doc;
        doc.create(kCellTypeFile, true);

        auto worksheet = doc.workbook().worksheet(1);
        worksheet.setName("类型测试");

        // 表头
        worksheet.cell("A1").value() = "字符串";
        worksheet.cell("B1").value() = "整数";
        worksheet.cell("C1").value() = "浮点数";
        worksheet.cell("D1").value() = "布尔值";
        worksheet.cell("E1").value() = "空值";
        worksheet.cell("F1").value() = "备注";

        // 第1行数据：E2为空单元格
        worksheet.cell("A2").value() = "张三";
        worksheet.cell("B2").value() = 90;
        worksheet.cell("C2").value() = 2.5;
        worksheet.cell("D2").value() = true;
        worksheet.cell("F2").value() = "第一行备注";

        // 第2行数据：E3为空单元格
        worksheet.cell("A3").value() = "李四";
        worksheet.cell("B3").value() = -5;
        worksheet.cell("C3").value() = 1.25;
        worksheet.cell("D3").value() = false;
        worksheet.cell("F3").value() = "第二行备注";

        doc.save();
        doc.close();
    }

    /**
     * @brief 生成内容非法的Excel文件，用于验证异常处理
     */
    static void createInvalidExcelFile() {
        std::ofstream out(kInvalidExcelFile);
        out << "this is not a valid excel file";
    }

    /**
     * @brief 按指定分隔符拆分字符串
     */
    static std::vector<std::string> split(const std::string& text, char delimiter) {
        std::vector<std::string> parts;
        std::string part;
        std::istringstream stream(text);
        while (std::getline(stream, part, delimiter)) {
            parts.push_back(part);
        }
        return parts;
    }
};

//------------------------------------------------------------------------------
// getWorksheetNames 测试
//------------------------------------------------------------------------------

// 单个worksheet：应返回唯一的worksheet名称
TEST_F(ExcelParserTest, GetWorksheetNames_SingleSheet) {
    std::vector<std::string> names = ExcelParser::getWorksheetNames(kSingleSheetFile);

    ASSERT_EQ(1u, names.size());
    EXPECT_EQ("成绩表", names[0]);
}

// 多个worksheet：应按workbook中的顺序返回全部worksheet名称
TEST_F(ExcelParserTest, GetWorksheetNames_MultipleSheets) {
    std::vector<std::string> names = ExcelParser::getWorksheetNames(kMultiSheetFile);

    ASSERT_EQ(4u, names.size());
    EXPECT_EQ("成绩表", names[0]);
    EXPECT_EQ("学生信息", names[1]);
    EXPECT_EQ("班级信息", names[2]);
    EXPECT_EQ("单行表", names[3]);
}

// 文件不存在：应返回空列表而不是崩溃
TEST_F(ExcelParserTest, GetWorksheetNames_FileNotExist) {
    std::vector<std::string> names = ExcelParser::getWorksheetNames(kDataDir + "/not_exist.xlsx");
    EXPECT_TRUE(names.empty());
}

// 文件内容非法：应捕获异常并返回空列表
TEST_F(ExcelParserTest, GetWorksheetNames_InvalidExcelFile) {
    std::vector<std::string> names = ExcelParser::getWorksheetNames(kInvalidExcelFile);
    EXPECT_TRUE(names.empty());
}

//------------------------------------------------------------------------------
// getWorksheetData 测试
//------------------------------------------------------------------------------

// 单个worksheet：解析结果应与原始数据完全一致（行/列/分隔符）
TEST_F(ExcelParserTest, GetWorksheetData_SingleSheet) {
    std::string data = ExcelParser::getWorksheetData(kSingleSheetFile, "成绩表");
    EXPECT_EQ(kStudentSheetData, data);
}

// 多个worksheet：每个worksheet的数据应相互独立且解析正确
TEST_F(ExcelParserTest, GetWorksheetData_MultipleSheets) {
    // 成绩表
    EXPECT_EQ(kStudentSheetData, ExcelParser::getWorksheetData(kMultiSheetFile, "成绩表"));

    // 学生信息
    EXPECT_EQ("姓名,学号,班级\n"
              "张三,2024001,计算机1班\n"
              "李四,2024002,计算机2班",
              ExcelParser::getWorksheetData(kMultiSheetFile, "学生信息"));

    // 班级信息
    EXPECT_EQ("班级,人数\n计算机1班,30",
              ExcelParser::getWorksheetData(kMultiSheetFile, "班级信息"));
}

// 仅表头无数据行：结果中不应出现多余的换行符
TEST_F(ExcelParserTest, GetWorksheetData_OnlyHeaderRow) {
    EXPECT_EQ("姓名,成绩", ExcelParser::getWorksheetData(kMultiSheetFile, "单行表"));
}

// 单元格类型转换：空、布尔、整数、浮点数、字符串
TEST_F(ExcelParserTest, GetWorksheetData_CellValueTypes) {
    std::string data = ExcelParser::getWorksheetData(kCellTypeFile, "类型测试");

    EXPECT_EQ("字符串,整数,浮点数,布尔值,空值,备注\n"
              "张三,90,2.5,1,,第一行备注\n"
              "李四,-5,1.25,0,,第二行备注",
              data);
}

// 浮动类型：整数形式的浮点数应去掉多余的小数部分，小数部分必须保留
TEST_F(ExcelParserTest, GetWorksheetData_FloatFormat) {
    std::string data = ExcelParser::getWorksheetData(kCellTypeFile, "类型测试");
    std::vector<std::string> rows = split(data, '\n');

    ASSERT_EQ(3u, rows.size());
    std::vector<std::string> secondRow = split(rows[1], ',');
    ASSERT_EQ(6u, secondRow.size());
    EXPECT_EQ("2.5", secondRow[2]);

    std::vector<std::string> thirdRow = split(rows[2], ',');
    ASSERT_EQ(6u, thirdRow.size());
    EXPECT_EQ("1.25", thirdRow[2]);
}

// 行列数量一致性：解析结果的行数、列数应与原worksheet保持一致
TEST_F(ExcelParserTest, GetWorksheetData_RowAndColumnCount) {
    std::string data = ExcelParser::getWorksheetData(kSingleSheetFile, "成绩表");
    std::vector<std::string> rows = split(data, '\n');

    // 1行表头 + 3行数据
    ASSERT_EQ(4u, rows.size());
    for (const auto& row : rows) {
        EXPECT_EQ(4u, split(row, ',').size());
    }
}

// worksheet不存在：应返回空字符串
TEST_F(ExcelParserTest, GetWorksheetData_SheetNotExist) {
    EXPECT_EQ("", ExcelParser::getWorksheetData(kSingleSheetFile, "不存在的表"));
}

// 文件不存在：应返回空字符串
TEST_F(ExcelParserTest, GetWorksheetData_FileNotExist) {
    EXPECT_EQ("", ExcelParser::getWorksheetData(kDataDir + "/not_exist.xlsx", "成绩表"));
}

// 文件内容非法：应捕获异常并返回空字符串
TEST_F(ExcelParserTest, GetWorksheetData_InvalidExcelFile) {
    EXPECT_EQ("", ExcelParser::getWorksheetData(kInvalidExcelFile, "成绩表"));
}

// 重复解析同一文件：多次调用结果应保持一致
TEST_F(ExcelParserTest, GetWorksheetData_RepeatedParse) {
    EXPECT_EQ(ExcelParser::getWorksheetData(kSingleSheetFile, "成绩表"),
              ExcelParser::getWorksheetData(kSingleSheetFile, "成绩表"));
}

}  // namespace test
}  // namespace chat2excel

int main(int argc, char** argv) {
    // 初始化bite_scaffold日志系统，避免ExcelParser中日志输出空指针
    bitelog::bitelog_init();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
