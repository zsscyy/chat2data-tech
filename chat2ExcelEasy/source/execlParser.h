#pragma once

#include <string>
#include <vector>

namespace OpenXLSX {
    class XLWorksheet;
    class XLCellValue;
}

namespace chat2excel {

/**
 * @brief Excel解析器工具类
 * 
 * 该类提供Excel文件解析功能，支持获取worksheet名称列表和读取worksheet数据。
 * 所有方法均为静态方法，可直接通过类名调用。
 */
class ExcelParser {
public:
    /**
     * @brief 获取指定Excel文件中所有worksheet名称
     * 
     * @param filePath Excel文件路径
     * @return std::vector<std::string> worksheet名称列表
     * 
     * @note 如果文件打开失败或发生异常，返回空列表
     * @note 使用bite_scaffold库的日志系统记录错误信息
     */
    static std::vector<std::string> getWorksheetNames(const std::string& filePath);
    
    /**
     * @brief 获取指定Excel文件中指定worksheet中所有数据
     * 
     * @param filePath Excel文件路径
     * @param sheetName worksheet名称
     * @return std::string 解析后的CSV格式字符串
     *         - 每行数据用换行符(\n)分隔
     *         - 每行中的列数据用逗号(,)分隔
     *         - 第一行为标题行，后续行为数据行
     *         - 行数和列数与原worksheet保持一致
     * 
     * @note 如果文件打开失败、worksheet不存在或发生异常，返回空字符串
     * @note 使用bite_scaffold库的日志系统记录错误信息
     */
    static std::string getWorksheetData(const std::string& filePath, const std::string& sheetName);

private:
    /**
     * @brief 解析worksheet中的所有数据
     * 
     * @param worksheet XLWorksheet对象引用
     * @return std::string 解析后的CSV格式字符串
     * 
     * @note 使用范围for循环迭代行和单元格，提高读取效率
     */
    static std::string parseWorksheetData(OpenXLSX::XLWorksheet& worksheet);
    
    /**
     * @brief 解析单元格数据，将单元格数值统一转换成字符串
     * 
     * @param value XLCellValue对象引用
     * @return std::string 单元格值的字符串表示
     *         - 空类型：返回空字符串
     *         - 布尔类型：真返回"1"，假返回"0"
     *         - 整型：以int64_t获取后转成字符串
     *         - 浮点数：以double获取后转成字符串，保留小数部分
     *         - 其他类型：直接转换成字符串
     */
    static std::string parseCellValue(const OpenXLSX::XLCellValue& value);
};

}
