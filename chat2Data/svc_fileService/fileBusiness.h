#pragma once

#include <memory>
#include <string>
#include <vector>

#include <bite_scaffold/rpc.h>

#include "common.h"
#include "excelParseService.pb.h"
#include "fileInfoData.h"
#include "worksheetData.h"

namespace fileService {

/**
 * @brief 文件子服务业务层
 *
 * 负责编排 FileInfoData、WorksheetData、FastDFS 与 Excel 解析子服务，
 * 实现文件上传、下载、删除、预览、列表查询等业务逻辑。
 */
class FileBusiness {
public:
    using ptr = std::shared_ptr<FileBusiness>;

    /**
     * @brief 构造文件业务对象
     * @param fileInfoData   文件信息数据访问对象
     * @param worksheetData  worksheet映射数据访问对象
     * @param svcChannels    子服务RPC信道管理器
     */
    FileBusiness(FileInfoData::ptr fileInfoData, WorksheetData::ptr worksheetData,
                 biterpc::SvcChannels::ptr svcChannels);

    /**
     * @brief 保存文件元数据（上传文件信息）
     * @param userId   用户ID
     * @param fileInfo 文件元数据
     * @return 生成的文件ID
     */
    std::string saveFileInfo(const std::string& userId, const FileInfo& fileInfo);

    /**
     * @brief 获取文件信息并校验文件归属
     * @param fileId 文件ID
     * @param userId 用户ID
     * @return 文件信息
     */
    FileInfo getFileInfo(const std::string& fileId, const std::string& userId);

    /**
     * @brief 上传Excel文件内容并完成解析、worksheet映射保存
     * @param fileId   文件ID
     * @param userId   用户ID
     * @param fileData Excel文件二进制数据
     */
    void uploadExcelFile(const std::string& fileId, const std::string& userId,
                         const std::string& fileData);

    /**
     * @brief 上传SQLite文件（元数据与文件内容一次性上传）
     * @param userId   用户ID
     * @param fileName 文件名
     * @param fileData SQLite文件二进制数据
     * @return 生成的文件ID
     */
    std::string uploadSQLiteFile(const std::string& userId, const std::string& fileName,
                                 const std::string& fileData);

    /**
     * @brief 下载文件内容
     * @param fileId 文件ID
     * @param userId 用户ID
     * @return 文件二进制数据
     */
    std::string downloadFile(const std::string& fileId, const std::string& userId);

    /**
     * @brief 删除文件及其worksheet映射
     * @param fileId 文件ID
     * @param userId 用户ID
     */
    void deleteFile(const std::string& fileId, const std::string& userId);

    /**
     * @brief 预览Excel文件
     * @param fileId     文件ID
     * @param userId     用户ID
     * @param pageNumber 页码，从1开始
     * @param pageSize   每页行数
     * @return Excel预览结果
     * @note 数据库子服务未实现前返回sheet名称和空数据，后续替换为真实查询
     */
    ExcelPreview previewExcel(const std::string& fileId, const std::string& userId,
                              int pageNumber, int pageSize);

    /**
     * @brief 获取指定用户的文件列表
     * @param userId 用户ID
     * @return 文件列表，按上传时间倒序
     */
    std::vector<FileInfo> getFileList(const std::string& userId);

    /**
     * @brief 关联文件与聊天会话
     * @param fileId        文件ID
     * @param chatSessionId 聊天会话ID
     * @param userId        用户ID
     */
    void associateFileChatSession(const std::string& fileId, const std::string& chatSessionId,
                                  const std::string& userId);

    /**
     * @brief 获取SQLite文件在FastDFS中的文件ID
     * @param fileId 文件ID
     * @return FastDFS文件ID
     * @note 该接口仅供存储子服务内部使用
     */
    std::string getSQLiteFdfsFileId(const std::string& fileId);

    /**
     * @brief 获取文件所有worksheet对应的数据库表名
     * @param fileId 文件ID
     * @return 数据库表名列表
     */
    std::vector<std::string> getWorksheetDBTables(const std::string& fileId);

private:
    chat2Data::FileInfoEntity loadFileInfoEntityOrThrow(const std::string& fileId);
    void ensureOwner(const chat2Data::FileInfoEntity& fileInfo, const std::string& userId);
    void ensureExcelFile(const chat2Data::FileInfoEntity& fileInfo);
    void ensureSqliteFile(const chat2Data::FileInfoEntity& fileInfo);

    std::vector<std::string> getWorksheetNamesFromParser(const std::string& fdfsFileId);
    std::vector<::chat2Data::excelParseService::WorksheetData> parseExcel(
        const std::string& fdfsFileId, const std::vector<std::string>& worksheetNames);
    void removeFdfsFileQuietly(const std::string& fdfsFileId);

    FileInfoData::ptr _fileInfoData;
    WorksheetData::ptr _worksheetData;
    biterpc::SvcChannels::ptr _svcChannels;
};

} // namespace fileService
