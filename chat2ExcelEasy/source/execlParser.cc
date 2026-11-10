#include "execlParser.h"
#include <OpenXLSX.hpp>
#include <sstream>
#include "log.h"

namespace chat2excel {

/**
 * @brief 获取指定Excel文件中所有worksheet名称
 */
std::vector<std::string> ExcelParser::getWorksheetNames(const std::string& filePath) {
    std::vector<std::string> names;
    
    try {
        // 打开Excel文档
        OpenXLSX::XLDocument doc;
        doc.open(filePath);
        
        // 检查文档是否成功打开
        if (!doc.isOpen()) {
            ERR("Failed to open Excel file: {}", filePath);
            return names;
        }
        
        // 获取workbook对象并提取所有worksheet名称
        auto workbook = doc.workbook();
        names = workbook.worksheetNames();
        
        // 记录成功日志
        INF("Successfully retrieved {} worksheet names from file: {}", names.size(), filePath);
        
        // 关闭文档
        doc.close();
    } catch (const OpenXLSX::XLException& e) {
        // 捕获OpenXLSX库异常
        ERR("OpenXLSX exception while getting worksheet names: {}", e.what());
    } catch (const std::exception& e) {
        // 捕获标准异常
        ERR("Exception while getting worksheet names: {}", e.what());
    }
    
    return names;
}

/**
 * @brief 获取指定Excel文件中指定worksheet中所有数据
 */
std::string ExcelParser::getWorksheetData(const std::string& filePath, const std::string& sheetName) {
    std::string result;
    
    try {
        // 打开Excel文档
        OpenXLSX::XLDocument doc;
        doc.open(filePath);
        
        // 检查文档是否成功打开
        if (!doc.isOpen()) {
            ERR("Failed to open Excel file: {}", filePath);
            return "";
        }
        
        auto workbook = doc.workbook();
        
        // 使用worksheetExists方法检查worksheet是否存在
        if (!workbook.worksheetExists(sheetName)) {
            ERR("Worksheet '{}' not found in file: {}", sheetName, filePath);
            doc.close();
            return "";
        }
        
        // 获取指定的worksheet
        auto worksheet = workbook.worksheet(sheetName);
        
        // 记录worksheet信息
        INF("Reading worksheet '{}' with {} rows and {} columns", 
            sheetName, worksheet.rowCount(), worksheet.columnCount());
        
        // 解析worksheet数据
        result = parseWorksheetData(worksheet);
        
        // 记录成功日志
        INF("Successfully read worksheet '{}'", sheetName);
        
        // 关闭文档
        doc.close();
    } catch (const OpenXLSX::XLException& e) {
        // 捕获OpenXLSX库异常
        ERR("OpenXLSX exception while reading worksheet data: {}", e.what());
        return "";
    } catch (const std::exception& e) {
        // 捕获标准异常
        ERR("Exception while reading worksheet data: {}", e.what());
        return "";
    }
    
    return result;
}

/**
 * @brief 解析worksheet中的所有数据
 * 
 * 使用范围for循环迭代行和单元格，提高读取效率
 */
std::string ExcelParser::parseWorksheetData(OpenXLSX::XLWorksheet& worksheet) {
    std::ostringstream result;
    bool firstRow = true;
    
    // 使用范围for循环迭代所有行，效率比通过下标访问更高
    for (auto& row : worksheet.rows()) {
        // 在非第一行前添加换行符
        if (!firstRow) {
            result << "\n";
        }
        firstRow = false;
        
        // 获取当前行所有单元格的值
        std::vector<OpenXLSX::XLCellValue> rowValues = row.values();
        
        bool firstCell = true;
        // 使用范围for循环迭代行中的所有单元格
        for (const auto& cellValue : rowValues) {
            // 在非第一列前添加逗号分隔符
            if (!firstCell) {
                result << ",";
            }
            firstCell = false;
            
            // 解析单元格值并添加到结果中
            result << parseCellValue(cellValue);
        }
    }
    
    return result.str();
}

/**
 * @brief 解析单元格数据，将单元格数值统一转换成字符串
 * 
 * 根据单元格值的类型进行不同的转换：
 * - 空类型：返回空字符串
 * - 布尔类型：真返回"1"，假返回"0"
 * - 整型：以int64_t获取后转成字符串
 * - 浮点数：以double获取后转成字符串，保留小数部分
 * - 其他类型：直接转换成字符串
 */
std::string ExcelParser::parseCellValue(const OpenXLSX::XLCellValue& value) {
    // 处理空类型
    if (value.type() == OpenXLSX::XLValueType::Empty) {
        return "";
    }
    
    // 处理布尔类型：真转换为"1"，假转换为"0"
    if (value.type() == OpenXLSX::XLValueType::Boolean) {
        bool boolValue = value.get<bool>();
        return boolValue ? "1" : "0";
    }
    
    // 处理整型：以int64_t获取后转成字符串
    if (value.type() == OpenXLSX::XLValueType::Integer) {
        int64_t intValue = value.get<int64_t>();
        return std::to_string(intValue);
    }
    
    // 处理浮点数：以double获取后转成字符串，保留小数部分
    if (value.type() == OpenXLSX::XLValueType::Float) {
        double floatValue = value.get<double>();
        std::string strValue = std::to_string(floatValue);
        
        // 去除浮点数尾部多余的0，但保留小数部分
        size_t dotPos = strValue.find('.');
        if (dotPos != std::string::npos) {
            // 找到最后一个非0字符的位置
            size_t lastNonZero = strValue.find_last_not_of('0');
            if (lastNonZero != std::string::npos && lastNonZero > dotPos) {
                // 保留小数部分，只去除尾部的0
                strValue = strValue.substr(0, lastNonZero + 1);
            } else if (lastNonZero == dotPos) {
                // 如果小数点后全是0，则去除小数点
                strValue = strValue.substr(0, dotPos);
            }
        }
        
        return strValue;
    }
    
    // 处理其他类型：直接转换成字符串
    try {
        return value.get<std::string>();
    } catch (...) {
        // 如果转换失败，返回空字符串
        return "";
    }
}

}
