#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace fileService {

// 文件缓存有效期：3天，与聊天会话生命周期保持一致
constexpr long FILE_CACHE_TTL_SECONDS = 3L * 24 * 60 * 60;

// Excel预览默认每页行数与最大每页行数
constexpr int DEFAULT_PAGE_SIZE = 50;
constexpr int MAX_PAGE_SIZE = 200;

/**
 * @brief 文件信息（业务层内部数据结构）
 */
struct FileInfo {
    std::string fileId;        // 文件ID
    std::string fileName;      // 文件名称
    std::uint64_t fileSize = 0; // 文件大小（字节）
    std::uint64_t uploadTime = 0; // 上传时间（秒级时间戳）
    std::string fileExt;       // 文件后缀
    std::string fdfsFileId;    // FastDFS文件ID
    std::string userId;        // 用户ID
    std::string chatSessionId; // 聊天会话ID
};

/**
 * @brief worksheet与数据库表的映射（业务层内部数据结构）
 */
struct WorksheetMapping {
    std::string worksheetName; // worksheet名称
    std::string tableName;   // 数据库中对应的真实表名
};

/**
 * @brief 单个worksheet的预览数据
 */
struct WorksheetPreview {
    std::string name;                          // worksheet名称
    int totalRows = 0;                         // 总行数
    int colCount = 0;                          // 列数
    int currentPage = 1;                       // 当前页码
    int totalPages = 0;                        // 总页数
    int pageSize = DEFAULT_PAGE_SIZE;          // 每页行数
    std::vector<std::string> columns;          // 列名列表
    std::vector<std::vector<std::string>> rows; // 当前页行数据
};

/**
 * @brief Excel预览结果
 */
struct ExcelPreview {
    FileInfo fileInfo;                        // 文件信息
    std::vector<WorksheetPreview> sheets;     // worksheet预览数据
};

/**
 * @brief 判断文件后缀是否为Excel文件
 * @param fileExt 文件后缀（可带点，大小写不敏感）
 */
bool isXlsxFile(const std::string& fileExt);

/**
 * @brief 判断文件后缀是否为SQLite文件
 * @param fileExt 文件后缀（可带点，大小写不敏感）
 */
bool isSqliteFile(const std::string& fileExt);

/**
 * @brief 计算worksheet在数据库中的真实表名
 * @param worksheetName worksheet名称
 * @return 形如 <清洗后的worksheet名>_<uuid> 的表名，长度不超过64
 * @note 保留中文，非法字符替换为下划线；首字符为数字时加ws_前缀
 */
std::string calculateTableName(const std::string& worksheetName);

} // namespace fileService
