#pragma once

#include <memory>
#include <string>
#include <vector>

#include "excelParseService.pb.h"

namespace excelParserService {

class ExcelParser;
struct WorksheetData;

/**
 * @brief Excel 解析业务类
 *
 * 对 ExcelParser 的封装：负责从 FastDFS 下载 Excel 文件到本地临时文件，
 * 调用解析器完成 worksheet 名称与表数据解析，并组织成 RPC 层需要的 proto 结构。
 */
class ExcelParserBusiness {
public:
    using ptr = std::shared_ptr<ExcelParserBusiness>;

    ExcelParserBusiness();
    ~ExcelParserBusiness() = default;

    ExcelParserBusiness(const ExcelParserBusiness&) = delete;
    ExcelParserBusiness& operator=(const ExcelParserBusiness&) = delete;

    /**
     * @brief 获取 Excel 文件中所有 worksheet 名称
     * @param fdfsFileId FastDFS 文件 ID
     * @return worksheet 名称列表
     * @throws chat2Data::Chat2DataException 文件 ID 为空、文件下载失败或文件中没有 worksheet
     */
    std::vector<std::string> getWorksheets(const std::string& fdfsFileId) const;

    /**
     * @brief 解析 Excel 文件中指定 worksheet 的表结构以及表数据
     * @param fdfsFileId  FastDFS 文件 ID
     * @param worksheets  待解析的 worksheet 名称列表
     * @return proto 格式的 worksheet 数据列表
     * @throws chat2Data::Chat2DataException 参数非法、文件下载失败或解析失败
     */
    std::vector<::chat2Data::excelParseService::WorksheetData> parseExcel(
        const std::string& fdfsFileId, const std::vector<std::string>& worksheets) const;

private:
    /**
     * @brief 将解析器结果转换为 proto 格式
     * @param worksheetData 解析器解析结果
     * @return proto 格式的 worksheet 数据
     */
    ::chat2Data::excelParseService::WorksheetData convertToProtoWorksheetData(
        const WorksheetData& worksheetData) const;

    /**
     * @brief 从 FastDFS 下载文件到本地临时文件
     * @param fdfsFileId    FastDFS 文件 ID
     * @param tempFilePath  下载后的本地文件路径
     * @throws chat2Data::Chat2DataException 下载失败
     */
    void downloadExcelFile(const std::string& fdfsFileId, const std::string& tempFilePath) const;

    /**
     * @brief 生成唯一的本地临时文件路径
     * @return 临时文件路径
     */
    std::string makeTempFilePath() const;

private:
    std::shared_ptr<ExcelParser> _excelParser; // Excel 解析器实例
};

} // namespace excelParserService
