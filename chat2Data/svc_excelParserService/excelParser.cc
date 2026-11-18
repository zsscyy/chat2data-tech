#include "excelParser.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <regex>
#include <unordered_map>
#include <utility>

#include <OpenXLSX/OpenXLSX.hpp>
#include <bite_scaffold/log.h>

namespace excelParserService {

namespace {

// 列类型推断的采样行数
const int SAMPLE_ROW_COUNT = 100;
// 表头所在行号
const int HEADER_ROW_NUMBER = 1;
// 数据起始行号
const int FIRST_DATA_ROW_NUMBER = 2;
// 空列名的默认前缀
const char* const DEFAULT_COLUMN_PREFIX = "column_";
// 数字开头列名的修正前缀
const char* const DIGIT_COLUMN_PREFIX = "col_";
// 空值标识
const char* const NOT_AVAILABLE = "n/a";

/**
 * @brief 判断字符是否属于列名允许的 ASCII 字符
 * @param ch 待检测字符
 * @return 字母、数字或下划线返回 true
 */
bool isAllowedAsciiChar(unsigned char ch) {
    return std::isalnum(ch) != 0 || ch == '_';
}

/**
 * @brief 判断首字节是否为数字字符
 * @param ch 待检测字节
 * @return 是 ASCII 数字返回 true
 */
bool isAsciiDigit(unsigned char ch) {
    return std::isdigit(ch) != 0;
}

/**
 * @brief 将字符串统一转换为小写
 * @param str 原始字符串
 * @return 小写字符串
 */
std::string toLower(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return lower;
}

} // namespace

std::string cellTypeToString(CellType type) {
    switch (type) {
        case CellType::EMPTY:
            return "Empty";
        case CellType::INTEGER:
            return "Integer";
        case CellType::FLOAT:
            return "Float";
        case CellType::BOOLEAN:
            return "Boolean";
        case CellType::STRING:
            return "String";
    }
    return "String";
}

std::string columnTypeToString(ColumnType type) {
    switch (type) {
        case ColumnType::TEXT:
            return "TEXT";
        case ColumnType::BIGINT:
            return "BIGINT";
        case ColumnType::DOUBLE:
            return "DOUBLE";
        case ColumnType::BOOLEAN:
            return "BOOLEAN";
        case ColumnType::DATE:
            return "DATE";
    }
    return "TEXT";
}

std::vector<std::string> ExcelParser::getWorksheetNames(const std::string& filePath) const {
    std::vector<std::string> names;
    if (filePath.empty()) {
        ERR("Excel文件路径为空");
        return names;
    }

    try {
        // 1. 打开Excel文件
        OpenXLSX::XLDocument document;
        document.open(filePath);
        if (!document.isOpen()) {
            ERR("打开Excel文件失败: {}", filePath);
            return names;
        }

        // 2. 获取工作簿中所有worksheet名称
        names = document.workbook().worksheetNames();
        INF("成功获取{}个worksheet, 文件: {}", names.size(), filePath);
        document.close();
    } catch (const OpenXLSX::XLException& e) {
        ERR("获取worksheet名称时OpenXLSX异常, 文件: {}, 原因: {}", filePath, e.what());
    } catch (const std::exception& e) {
        ERR("获取worksheet名称异常, 文件: {}, 原因: {}", filePath, e.what());
    }

    return names;
}

std::unique_ptr<WorksheetData> ExcelParser::parseWorksheet(const std::string& filePath,
                                                           const std::string& worksheetName) const {
    if (filePath.empty()) {
        ERR("Excel文件路径为空");
        return nullptr;
    }

    std::unique_ptr<WorksheetData> worksheetData = std::make_unique<WorksheetData>();
    try {
        // 1. 打开Excel文件
        OpenXLSX::XLDocument document;
        document.open(filePath);
        if (!document.isOpen()) {
            ERR("打开Excel文件失败: {}", filePath);
            return nullptr;
        }

        // 2. 检查并获取worksheet
        OpenXLSX::XLWorkbook workbook = document.workbook();
        if (!workbook.worksheetExists(worksheetName)) {
            ERR("worksheet不存在, 文件: {}, worksheet: {}", filePath, worksheetName);
            return nullptr;
        }

        OpenXLSX::XLWorksheet worksheet = workbook.worksheet(worksheetName);
        worksheetData->_name = worksheetName;
        worksheetData->_totalRows = static_cast<int>(worksheet.rowCount());
        worksheetData->_totalCols = static_cast<int>(worksheet.columnCount());
        if (worksheetData->_totalRows <= 0 || worksheetData->_totalCols <= 0) {
            ERR("worksheet数据为空, 文件: {}, worksheet: {}", filePath, worksheetName);
            return nullptr;
        }

        INF("开始解析worksheet: {}, 总行数: {}, 总列数: {}", worksheetName, worksheetData->_totalRows,
            worksheetData->_totalCols);

        // 3. 解析表头，生成合法列名
        const std::vector<std::string> headerTexts = parseHeaderRow(worksheet, worksheetData->_totalCols);
        for (int columnIndex = 0; columnIndex < static_cast<int>(headerTexts.size()); ++columnIndex) {
            ColumnInfo column;
            column._name = validateColumnName(headerTexts[columnIndex], columnIndex + 1);
            worksheetData->_columns.push_back(column);
        }

        // 4. 采样最多前100行数据推断列类型
        const int sampleRows = std::min(SAMPLE_ROW_COUNT, worksheetData->_totalRows - HEADER_ROW_NUMBER);
        std::vector<std::vector<CellData>> sampledRows;
        sampledRows.reserve(sampleRows);
        for (int rowNumber = FIRST_DATA_ROW_NUMBER; rowNumber < FIRST_DATA_ROW_NUMBER + sampleRows; ++rowNumber) {
            sampledRows.push_back(parseDataRow(worksheet, rowNumber, worksheetData->_totalCols));
        }
        for (int columnIndex = 0; columnIndex < static_cast<int>(worksheetData->_columns.size()); ++columnIndex) {
            std::vector<CellData> columnValues;
            columnValues.reserve(sampledRows.size());
            for (const std::vector<CellData>& row : sampledRows) {
                if (columnIndex < static_cast<int>(row.size())) {
                    columnValues.push_back(row[columnIndex]);
                }
            }
            worksheetData->_columns[columnIndex]._type = inferColumnType(columnValues);
        }

        // 5. 解析表头之外的全部数据行
        for (int rowNumber = FIRST_DATA_ROW_NUMBER; rowNumber <= worksheetData->_totalRows; ++rowNumber) {
            RowData rowData;
            rowData._cells = parseDataRow(worksheet, rowNumber, worksheetData->_totalCols);
            worksheetData->_rows.push_back(std::move(rowData));
        }

        document.close();
        INF("解析worksheet完成: {}, 列数: {}, 数据行数: {}", worksheetData->_name, worksheetData->_columns.size(),
            worksheetData->_rows.size());
        return worksheetData;
    } catch (const OpenXLSX::XLException& e) {
        ERR("解析worksheet时OpenXLSX异常, 文件: {}, worksheet: {}, 原因: {}", filePath, worksheetName, e.what());
    } catch (const std::exception& e) {
        ERR("解析worksheet异常, 文件: {}, worksheet: {}, 原因: {}", filePath, worksheetName, e.what());
    }

    return nullptr;
}

std::string ExcelParser::validateColumnName(const std::string& name, int columnIndex) const {
    // 空列名使用 column_列号 作为默认列名
    if (name.empty()) {
        return DEFAULT_COLUMN_PREFIX + std::to_string(columnIndex);
    }

    // 仅保留字母、数字、下划线以及UTF-8中文等多字节字符，其余字符替换为下划线
    std::string result;
    result.reserve(name.size());
    for (unsigned char ch : name) {
        if (isAllowedAsciiChar(ch) || ch >= 0x80) {
            result.push_back(static_cast<char>(ch));
        } else {
            result.push_back('_');
        }
    }

    // 数字开头的列名需要添加 col_ 前缀，避免生成非法数据库列名
    if (!result.empty() && isAsciiDigit(static_cast<unsigned char>(result.front()))) {
        result = DIGIT_COLUMN_PREFIX + result;
    }
    return result;
}

std::string ExcelParser::trimWhitespace(const std::string& str) const {
    std::size_t begin = 0;
    std::size_t end = str.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(str[begin])) != 0) {
        ++begin;
    }
    while (end > begin && std::isspace(static_cast<unsigned char>(str[end - 1])) != 0) {
        --end;
    }
    return str.substr(begin, end - begin);
}

bool ExcelParser::isNumericString(const std::string& str) const {
    const std::string trimmed = trimWhitespace(str);
    // nan、inf 等不含数字的字符串不应识别为数值
    if (trimmed.empty() || trimmed.find_first_of("0123456789") == std::string::npos) {
        return false;
    }

    // 使用 strtod 解析数值，剩余内容只能是空白字符
    char* end = nullptr;
    std::strtod(trimmed.c_str(), &end);
    if (end == trimmed.c_str()) {
        return false;
    }
    while (*end != '\0') {
        if (std::isspace(static_cast<unsigned char>(*end)) == 0) {
            return false;
        }
        ++end;
    }
    return true;
}

bool ExcelParser::isBooleanString(const std::string& str) const {
    const std::string lower = toLower(trimWhitespace(str));
    return lower == "true" || lower == "1" || lower == "t" || lower == "yes" || lower == "y" || lower == "false" ||
           lower == "0" || lower == "f" || lower == "no" || lower == "n";
}

bool ExcelParser::isDateString(const std::string& str) const {
    // 常见日期格式：yyyy-MM-DD、yyyy/MM/DD、MM-DD-YYYY、MM/DD/YYYY、yyyy.MM.DD、MM.DD.YYYY 及带时分秒的格式
    static const std::vector<std::regex> DATE_PATTERNS = {
        std::regex(R"(^\d{4}-\d{2}-\d{2}$)"),
        std::regex(R"(^\d{4}/\d{2}/\d{2}$)"),
        std::regex(R"(^\d{2}-\d{2}-\d{4}$)"),
        std::regex(R"(^\d{2}/\d{2}/\d{4}$)"),
        std::regex(R"(^\d{4}\.\d{2}\.\d{2}$)"),
        std::regex(R"(^\d{2}\.\d{2}\.\d{4}$)"),
        std::regex(R"(^\d{4}-\d{2}-\d{2}[ T]\d{2}:\d{2}(:\d{2})?$)"),
        std::regex(R"(^\d{4}/\d{2}/\d{2}\s+\d{2}:\d{2}(:\d{2})?$)"),
        std::regex(R"(^\d{4}\.\d{2}\.\d{2}\s+\d{2}:\d{2}(:\d{2})?$)"),
    };

    const std::string trimmed = trimWhitespace(str);
    for (const std::regex& pattern : DATE_PATTERNS) {
        if (std::regex_match(trimmed, pattern)) {
            return true;
        }
    }
    return false;
}

ColumnType ExcelParser::detectColumnType(const CellData& cell) const {
    switch (cell._type) {
        case CellType::INTEGER:
            return ColumnType::BIGINT;
        case CellType::FLOAT:
            return ColumnType::DOUBLE;
        case CellType::BOOLEAN:
            return ColumnType::BOOLEAN;
        case CellType::EMPTY:
        case CellType::STRING:
            break;
    }

    // 字符串类型需要进一步识别数值、布尔值以及日期
    if (isNumericString(cell._value)) {
        return ColumnType::DOUBLE;
    }
    if (isBooleanString(cell._value)) {
        return ColumnType::BOOLEAN;
    }
    if (isDateString(cell._value)) {
        return ColumnType::DATE;
    }
    return ColumnType::TEXT;
}

ColumnType ExcelParser::inferColumnType(const std::vector<CellData>& cellValues) const {
    std::unordered_map<ColumnType, int> typeCount;
    for (const CellData& cell : cellValues) {
        // 空值以及 N/A 不参与类型统计
        const std::string text = trimWhitespace(cell._value);
        if (cell._type == CellType::EMPTY || text.empty() || toLower(text) == NOT_AVAILABLE) {
            continue;
        }
        ++typeCount[detectColumnType(cell)];
    }

    if (typeCount.empty()) {
        return ColumnType::TEXT;
    }

    // 取出现次数最多的类型；次数相同时取决于 unordered_map 的遍历顺序
    ColumnType maxType = ColumnType::TEXT;
    int maxCount = 0;
    for (const auto& pair : typeCount) {
        if (pair.second > maxCount) {
            maxCount = pair.second;
            maxType = pair.first;
        }
    }
    return maxType;
}

CellData ExcelParser::parseCellValue(const OpenXLSX::XLCellValue& value) const {
    CellData cell;
    switch (value.type()) {
        case OpenXLSX::XLValueType::Empty:
            cell._type = CellType::EMPTY;
            break;
        case OpenXLSX::XLValueType::Boolean:
            cell._type = CellType::BOOLEAN;
            cell._value = value.get<bool>() ? "1" : "0";
            break;
        case OpenXLSX::XLValueType::Integer:
            cell._type = CellType::INTEGER;
            cell._value = std::to_string(value.get<int64_t>());
            break;
        case OpenXLSX::XLValueType::Float:
            cell._type = CellType::FLOAT;
            cell._value = std::to_string(value.get<double>());
            break;
        case OpenXLSX::XLValueType::String:
        case OpenXLSX::XLValueType::Error:
        default:
            cell._type = CellType::STRING;
            try {
                cell._value = value.get<std::string>();
            } catch (const std::exception&) {
                cell._value = cellText(value);
            }
            break;
    }
    return cell;
}

std::string ExcelParser::cellText(const OpenXLSX::XLCellValue& value) const {
    switch (value.type()) {
        case OpenXLSX::XLValueType::Empty:
            return std::string();
        case OpenXLSX::XLValueType::Boolean:
            return value.get<bool>() ? "true" : "false";
        case OpenXLSX::XLValueType::Integer:
            return std::to_string(value.get<int64_t>());
        case OpenXLSX::XLValueType::Float:
            return std::to_string(value.get<double>());
        case OpenXLSX::XLValueType::String:
        case OpenXLSX::XLValueType::Error:
        default:
            try {
                return value.get<std::string>();
            } catch (const std::exception&) {
                return std::string();
            }
    }
}

std::vector<std::string> ExcelParser::parseHeaderRow(const OpenXLSX::XLWorksheet& worksheet, int columnCount) const {
    std::vector<std::string> headerTexts;
    headerTexts.reserve(columnCount);
    for (int column = 1; column <= columnCount; ++column) {
        headerTexts.push_back(cellText(worksheet.cell(HEADER_ROW_NUMBER, static_cast<uint16_t>(column)).value()));
    }
    return headerTexts;
}

std::vector<CellData> ExcelParser::parseDataRow(const OpenXLSX::XLWorksheet& worksheet, int rowNumber,
                                                int columnCount) const {
    std::vector<CellData> cells;
    cells.reserve(columnCount);
    for (int column = 1; column <= columnCount; ++column) {
        cells.push_back(parseCellValue(
            worksheet.cell(static_cast<uint32_t>(rowNumber), static_cast<uint16_t>(column)).value()));
    }
    return cells;
}

} // namespace excelParserService
