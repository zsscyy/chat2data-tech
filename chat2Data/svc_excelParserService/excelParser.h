#pragma once

#include <memory>
#include <string>
#include <vector>

namespace OpenXLSX {
class XLCellValue;
class XLWorksheet;
} // namespace OpenXLSX

namespace excelParserService {

// 单元格数据类型，与 OpenXLSX 的 XLValueType 对应
enum class CellType {
    EMPTY,
    INTEGER,
    FLOAT,
    BOOLEAN,
    STRING,
};

// 列数据类型，对应数据库列类型
enum class ColumnType {
    TEXT,
    BIGINT,
    DOUBLE,
    BOOLEAN,
    DATE,
};

// 单元格数据
struct CellData {
    std::string _value;               // 单元格数值
    CellType _type = CellType::EMPTY; // 单元格类型
};

// 列信息
struct ColumnInfo {
    std::string _name;                   // 列名
    ColumnType _type = ColumnType::TEXT; // 列类型
};

// worksheet 的一行数据
struct RowData {
    std::vector<CellData> _cells; // 该行所有单元格
};

// worksheet 解析结果
struct WorksheetData {
    std::string _name;                // worksheet 名称
    std::vector<ColumnInfo> _columns; // 列信息
    std::vector<RowData> _rows;       // 数据行（不含表头）
    int _totalRows = 0;               // 总行数（含表头）
    int _totalCols = 0;               // 总列数
};

/**
 * @brief 将单元格类型转换为 proto 中使用的类型名称
 * @param type 单元格类型
 * @return 类型名称：Empty、Integer、Float、Boolean、String
 */
std::string cellTypeToString(CellType type);

/**
 * @brief 将列类型转换为数据库列类型名称
 * @param type 列类型
 * @return 类型名称：TEXT、BIGINT、DOUBLE、BOOLEAN、DATE
 */
std::string columnTypeToString(ColumnType type);

/**
 * @brief Excel 文件解析器
 *
 * 基于 OpenXLSX 实现，向业务层提供 worksheet 名称解析与 worksheet 表结构、表数据解析能力。
 * OpenXLSX 接口异常、文件打开失败、worksheet 不存在等情况统一返回空结果，由业务层转换为错误码。
 */
class ExcelParser {
public:
    ExcelParser() = default;
    ~ExcelParser() = default;

    ExcelParser(const ExcelParser&) = delete;
    ExcelParser& operator=(const ExcelParser&) = delete;

    /**
     * @brief 获取 Excel 文件中所有 worksheet 名称
     * @param filePath Excel 文件路径
     * @return worksheet 名称列表；文件路径为空、打开失败或解析异常时返回空列表
     */
    std::vector<std::string> getWorksheetNames(const std::string& filePath) const;

    /**
     * @brief 解析指定 worksheet 的表结构以及表数据
     * @param filePath      Excel 文件路径
     * @param worksheetName worksheet 名称
     * @return worksheet 解析结果；文件路径为空、打开失败、worksheet 不存在或数据为空时返回空指针
     */
    std::unique_ptr<WorksheetData> parseWorksheet(const std::string& filePath,
                                                  const std::string& worksheetName) const;

private:
    /**
     * @brief 校验并修正列名
     * @param name        原始列名
     * @param columnIndex 列号（从 1 开始）
     * @return 合法列名；原始列名为空时返回 column_列号
     */
    std::string validateColumnName(const std::string& name, int columnIndex) const;

    /**
     * @brief 去除字符串首尾空白字符
     * @param str 原始字符串
     * @return 去除首尾空白后的字符串
     */
    std::string trimWhitespace(const std::string& str) const;

    /**
     * @brief 判断字符串是否为数值
     * @param str 待检测字符串
     * @return 是数值返回 true
     */
    bool isNumericString(const std::string& str) const;

    /**
     * @brief 判断字符串是否为布尔值
     * @param str 待检测字符串
     * @return 是布尔值返回 true
     */
    bool isBooleanString(const std::string& str) const;

    /**
     * @brief 判断字符串是否为日期
     * @param str 待检测字符串
     * @return 是日期返回 true
     */
    bool isDateString(const std::string& str) const;

    /**
     * @brief 根据单个单元格推断列类型
     * @param cell 单元格数据
     * @return 推断出的列类型
     */
    ColumnType detectColumnType(const CellData& cell) const;

    /**
     * @brief 根据采样数据推断列类型，取出现次数最多的类型
     * @param cellValues 某列采样单元格
     * @return 出现次数最多的列类型，采样为空时返回 TEXT
     * @note 出现次数相同时，结果取决于 unordered_map 的遍历顺序
     */
    ColumnType inferColumnType(const std::vector<CellData>& cellValues) const;

    /**
     * @brief 解析数据单元格，按需求转换数值并保留单元格类型
     * @param value OpenXLSX 单元格值
     * @return 单元格数据
     */
    CellData parseCellValue(const OpenXLSX::XLCellValue& value) const;

    /**
     * @brief 获取单元格文本，用于解析表头
     * @param value OpenXLSX 单元格值
     * @return 单元格文本
     */
    std::string cellText(const OpenXLSX::XLCellValue& value) const;

    /**
     * @brief 解析表头行
     * @param worksheet   worksheet 对象
     * @param columnCount 列数
     * @return 各列表头文本
     */
    std::vector<std::string> parseHeaderRow(const OpenXLSX::XLWorksheet& worksheet, int columnCount) const;

    /**
     * @brief 解析一行数据
     * @param worksheet   worksheet 对象
     * @param rowNumber   行号（从 1 开始）
     * @param columnCount 列数
     * @return 该行单元格数据
     */
    std::vector<CellData> parseDataRow(const OpenXLSX::XLWorksheet& worksheet, int rowNumber,
                                       int columnCount) const;
};

} // namespace excelParserService
