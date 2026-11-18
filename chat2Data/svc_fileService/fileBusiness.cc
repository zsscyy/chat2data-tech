#include "fileBusiness.h"

#include <algorithm>
#include <ctime>
#include <string>
#include <utility>
#include <vector>

#include <bite_scaffold/log.h>
#include <gflags/gflags.h>

#include "errorHandler.h"
#include "utils.h"

// fdfs.h 必须放在其他头文件之后，并清理 byte 宏，避免与项目代码冲突
#include <bite_scaffold/fdfs.h>
#ifdef byte
#undef byte
#endif

// gflags 变量在 main.cc 中定义，业务层只做声明
DECLARE_string(excel_service);

namespace fileService {

using chat2Data::Chat2DataException;
using chat2Data::ErrorCode;
using chat2Data::FileInfoEntity;
using chat2Data::WorksheetEntity;

namespace {

// 获取worksheet列表的RPC超时时间
constexpr int WORKSHEET_RPC_TIMEOUT_MS = 10000;
// 解析Excel的RPC超时时间
constexpr int PARSE_EXCEL_RPC_TIMEOUT_MS = 30000;

/**
 * @brief 文件信息实体转换为业务层数据结构
 */
FileInfo toFileInfo(const FileInfoEntity& entity) {
    FileInfo fileInfo;
    fileInfo.fileId = entity.getFileId();
    fileInfo.fileName = entity.getFileName();
    fileInfo.fileSize = entity.getFileSize();
    fileInfo.uploadTime = entity.getUploadTime();
    fileInfo.fileExt = entity.getFileExt();
    fileInfo.fdfsFileId = entity.getFdfsFileId();
    fileInfo.userId = entity.getUserId();
    fileInfo.chatSessionId = entity.getChatSessionId();
    return fileInfo;
}

/**
 * @brief 从文件名中提取后缀
 * @return 带点的后缀，无后缀时返回空串
 */
std::string extractExtension(const std::string& fileName) {
    const std::size_t pos = fileName.find_last_of('.');
    if (pos == std::string::npos) {
        return std::string();
    }
    return fileName.substr(pos);
}

} // namespace

FileBusiness::FileBusiness(FileInfoData::ptr fileInfoData, WorksheetData::ptr worksheetData,
                           biterpc::SvcChannels::ptr svcChannels)
    : _fileInfoData(std::move(fileInfoData))
    , _worksheetData(std::move(worksheetData))
    , _svcChannels(std::move(svcChannels)) {
    INF("FileBusiness initialized");
}

std::string FileBusiness::saveFileInfo(const std::string& userId, const FileInfo& fileInfo) {
    if (userId.empty() || fileInfo.fileName.empty() || fileInfo.fileExt.empty() ||
        fileInfo.fileSize == 0) {
        WRN("上传文件元数据参数非法, userId: {}, fileName: {}", userId, fileInfo.fileName);
        throw Chat2DataException(ErrorCode::FILE_PARAM_INVALID);
    }
    if (!isXlsxFile(fileInfo.fileExt) && !isSqliteFile(fileInfo.fileExt)) {
        WRN("不支持的文件类型, fileExt: {}", fileInfo.fileExt);
        throw Chat2DataException(ErrorCode::FILE_PARAM_INVALID);
    }

    const std::string fileId = chat2Data::Utils::generateUuid();
    FileInfoEntity entity(fileId, fileInfo.fileName, fileInfo.fileSize,
                          static_cast<std::uint64_t>(std::time(nullptr)), fileInfo.fileExt,
                          std::string(), userId, std::string());
    _fileInfoData->saveFileInfo(entity);

    INF("文件元数据保存成功, fileId: {}, userId: {}", fileId, userId);
    return fileId;
}

FileInfo FileBusiness::getFileInfo(const std::string& fileId, const std::string& userId) {
    FileInfoEntity entity = loadFileInfoEntityOrThrow(fileId);
    ensureOwner(entity, userId);
    return toFileInfo(entity);
}

void FileBusiness::uploadExcelFile(const std::string& fileId, const std::string& userId,
                                   const std::string& fileData) {
    FileInfoEntity entity = loadFileInfoEntityOrThrow(fileId);
    ensureOwner(entity, userId);
    ensureExcelFile(entity);

    if (fileData.empty()) {
        WRN("Excel文件内容为空, fileId: {}", fileId);
        throw Chat2DataException(ErrorCode::FILE_PARAM_INVALID);
    }

    std::optional<std::string> fdfsFileId = bitefdfs::FDFSClient::upload_from_buff(fileData);
    if (!fdfsFileId.has_value() || fdfsFileId->empty()) {
        ERR("Excel文件上传FastDFS失败, fileId: {}", fileId);
        throw Chat2DataException(ErrorCode::FILE_UPLOAD_FASTDFS_FAILED);
    }

    entity.setFdfsFileId(fdfsFileId.value());
    _fileInfoData->saveFileInfo(entity);

    try {
        // 1. 获取Excel文件的所有worksheet
        std::vector<std::string> worksheetNames = getWorksheetNamesFromParser(fdfsFileId.value());

        // 2. 解析worksheet的表结构与数据
        std::vector<::chat2Data::excelParseService::WorksheetData> worksheets =
            parseExcel(fdfsFileId.value(), worksheetNames);

        // 3. 计算每个worksheet在数据库中的真实表名
        std::vector<WorksheetEntity> mappings;
        mappings.reserve(worksheets.size());
        for (std::size_t index = 0; index < worksheets.size(); ++index) {
            std::string worksheetName = worksheets[index].name();
            if (worksheetName.empty()) {
                worksheetName = "worksheet" + std::to_string(index + 1);
            }
            mappings.emplace_back(fileId, worksheetName, calculateTableName(worksheetName));
        }

        // TODO: 调用数据库子服务ImportExcelData，将worksheets中的列信息与行数据写入数据库。
        //       数据库子服务当前未实现，待其实现后在此处完成真实入库。

        // 4. 保存worksheet与数据库表的映射关系
        _worksheetData->saveWorksheetMappings(fileId, mappings);
    } catch (...) {
        // 解析或映射保存失败时，清理已上传的FastDFS文件，避免产生孤儿文件
        removeFdfsFileQuietly(fdfsFileId.value());
        entity.setFdfsFileId(std::string());
        try {
            _fileInfoData->saveFileInfo(entity);
        } catch (const std::exception& e) {
            WRN("清理Excel文件FastDFS ID失败, fileId: {}, 原因: {}", fileId, e.what());
        }
        throw;
    }

    INF("Excel文件上传并解析完成, fileId: {}, worksheets: {}", fileId, entity.getFileName());
}

std::string FileBusiness::uploadSQLiteFile(const std::string& userId, const std::string& fileName,
                                           const std::string& fileData) {
    // SQLite文件无需关联聊天会话，chatSessionId保存为空串
    if (userId.empty() || fileName.empty() || fileData.empty()) {
        WRN("上传SQLite文件参数非法, userId: {}, fileName: {}", userId, fileName);
        throw Chat2DataException(ErrorCode::FILE_PARAM_INVALID);
    }

    const std::string fileExt = extractExtension(fileName);
    if (!isSqliteFile(fileExt)) {
        WRN("SQLite文件后缀非法, fileName: {}", fileName);
        throw Chat2DataException(ErrorCode::FILE_PARAM_INVALID);
    }

    std::optional<std::string> fdfsFileId = bitefdfs::FDFSClient::upload_from_buff(fileData);
    if (!fdfsFileId.has_value() || fdfsFileId->empty()) {
        ERR("SQLite文件上传FastDFS失败, fileName: {}", fileName);
        throw Chat2DataException(ErrorCode::FILE_SQLITE_UPLOAD_FAILED);
    }

    const std::string fileId = chat2Data::Utils::generateUuid();
    FileInfoEntity entity(fileId, fileName, static_cast<std::uint64_t>(fileData.size()),
                          static_cast<std::uint64_t>(std::time(nullptr)), fileExt,
                          fdfsFileId.value(), userId, std::string());
    try {
        _fileInfoData->saveFileInfo(entity);
    } catch (...) {
        removeFdfsFileQuietly(fdfsFileId.value());
        throw;
    }

    INF("SQLite文件上传成功, fileId: {}, userId: {}", fileId, userId);
    return fileId;
}

std::string FileBusiness::downloadFile(const std::string& fileId, const std::string& userId) {
    FileInfoEntity entity = loadFileInfoEntityOrThrow(fileId);
    ensureOwner(entity, userId);

    if (entity.getFdfsFileId().empty()) {
        WRN("文件尚未上传内容, fileId: {}", fileId);
        throw Chat2DataException(ErrorCode::FILE_NOT_FOUND);
    }

    std::string fileData;
    if (!bitefdfs::FDFSClient::download_to_buff(entity.getFdfsFileId(), fileData)) {
        ERR("文件从FastDFS下载失败, fileId: {}, fdfsFileId: {}", fileId, entity.getFdfsFileId());
        throw Chat2DataException(ErrorCode::FILE_DOWNLOAD_FASTDFS_FAILED);
    }

    INF("文件下载成功, fileId: {}, 大小: {}", fileId, fileData.size());
    return fileData;
}

void FileBusiness::deleteFile(const std::string& fileId, const std::string& userId) {
    FileInfoEntity entity = loadFileInfoEntityOrThrow(fileId);
    ensureOwner(entity, userId);

    // 先删除worksheet映射，再删除文件信息，最后删除FastDFS文件
    _worksheetData->deleteWorksheetMappingsByFileId(fileId);
    _fileInfoData->deleteFileInfoByFileId(fileId);

    // TODO: 通知AI子服务解除文件与聊天会话的关联

    removeFdfsFileQuietly(entity.getFdfsFileId());
    INF("文件删除成功, fileId: {}, userId: {}", fileId, userId);
}

ExcelPreview FileBusiness::previewExcel(const std::string& fileId, const std::string& userId,
                                        int pageNumber, int pageSize) {
    FileInfoEntity entity = loadFileInfoEntityOrThrow(fileId);
    ensureOwner(entity, userId);
    ensureExcelFile(entity);

    if (pageNumber < 1) {
        pageNumber = 1;
    }
    if (pageSize < 1) {
        pageSize = DEFAULT_PAGE_SIZE;
    }
    if (pageSize > MAX_PAGE_SIZE) {
        pageSize = MAX_PAGE_SIZE;
    }

    ExcelPreview preview;
    preview.fileInfo = toFileInfo(entity);

    std::vector<WorksheetEntity> mappings =
        _worksheetData->getWorksheetMappingsByFileId(fileId);
    if (mappings.empty()) {
        WRN("Excel文件没有worksheet映射, fileId: {}", fileId);
        return preview;
    }

    preview.sheets.reserve(mappings.size());
    for (const auto& mapping : mappings) {
        WorksheetPreview sheet;
        sheet.name = mapping.getWorksheetName();
        sheet.currentPage = pageNumber;
        sheet.pageSize = pageSize;

        // TODO: 调用数据库子服务GetTableData(mapping.getTableName(), pageNumber, pageSize)，
        //       填充columns、rows、totalRows、colCount、totalPages。
        //       数据库子服务实现后，若查询失败必须抛出异常，不能静默返回空数据。

        preview.sheets.push_back(std::move(sheet));
    }

    return preview;
}

std::vector<FileInfo> FileBusiness::getFileList(const std::string& userId) {
    std::vector<FileInfoEntity> entities = _fileInfoData->getFileInfoListByUserId(userId);
    std::vector<FileInfo> fileList;
    fileList.reserve(entities.size());
    for (const auto& entity : entities) {
        fileList.push_back(toFileInfo(entity));
    }
    return fileList;
}

void FileBusiness::associateFileChatSession(const std::string& fileId,
                                            const std::string& chatSessionId,
                                            const std::string& userId) {
    if (chatSessionId.empty()) {
        WRN("关联文件与会话参数非法, fileId: {}", fileId);
        throw Chat2DataException(ErrorCode::FILE_PARAM_INVALID);
    }

    FileInfoEntity entity = loadFileInfoEntityOrThrow(fileId);
    ensureOwner(entity, userId);
    entity.setChatSessionId(chatSessionId);
    _fileInfoData->saveFileInfo(entity);

    // TODO: 通知AI子服务更新聊天会话与文件的关联关系

    INF("文件与会话关联成功, fileId: {}, chatSessionId: {}", fileId, chatSessionId);
}

std::string FileBusiness::getSQLiteFdfsFileId(const std::string& fileId) {
    FileInfoEntity entity = loadFileInfoEntityOrThrow(fileId);
    ensureSqliteFile(entity);

    if (entity.getFdfsFileId().empty()) {
        WRN("SQLite文件尚未上传内容, fileId: {}", fileId);
        throw Chat2DataException(ErrorCode::FILE_SQLITE_FILE_NOT_FOUND);
    }
    return entity.getFdfsFileId();
}

std::vector<std::string> FileBusiness::getWorksheetDBTables(const std::string& fileId) {
    // 先确认文件存在，文件不存在时直接报错
    loadFileInfoEntityOrThrow(fileId);

    std::vector<WorksheetEntity> mappings =
        _worksheetData->getWorksheetMappingsByFileId(fileId);
    std::vector<std::string> tableNames;
    tableNames.reserve(mappings.size());
    for (const auto& mapping : mappings) {
        tableNames.push_back(mapping.getTableName());
    }
    return tableNames;
}

FileInfoEntity FileBusiness::loadFileInfoEntityOrThrow(const std::string& fileId) {
    std::optional<FileInfoEntity> entity = _fileInfoData->getFileInfoByFileId(fileId);
    if (!entity.has_value()) {
        WRN("文件不存在, fileId: {}", fileId);
        throw Chat2DataException(ErrorCode::FILE_NOT_FOUND);
    }
    return entity.value();
}

void FileBusiness::ensureOwner(const FileInfoEntity& fileInfo, const std::string& userId) {
    if (fileInfo.getUserId() != userId) {
        WRN("文件不属于当前用户, fileId: {}, owner: {}, current: {}", fileInfo.getFileId(),
            fileInfo.getUserId(), userId);
        throw Chat2DataException(ErrorCode::FILE_PERMISSION_DENIED);
    }
}

void FileBusiness::ensureExcelFile(const FileInfoEntity& fileInfo) {
    if (!isXlsxFile(fileInfo.getFileExt())) {
        WRN("文件不是Excel文件, fileId: {}, fileExt: {}", fileInfo.getFileId(),
            fileInfo.getFileExt());
        throw Chat2DataException(ErrorCode::FILE_PARAM_INVALID);
    }
}

void FileBusiness::ensureSqliteFile(const FileInfoEntity& fileInfo) {
    if (!isSqliteFile(fileInfo.getFileExt())) {
        WRN("文件不是SQLite文件, fileId: {}, fileExt: {}", fileInfo.getFileId(),
            fileInfo.getFileExt());
        throw Chat2DataException(ErrorCode::FILE_PARAM_INVALID);
    }
}

std::vector<std::string> FileBusiness::getWorksheetNamesFromParser(const std::string& fdfsFileId) {
    biterpc::ChannelPtr channel;
    if (_svcChannels) {
        channel = _svcChannels->getNode(FLAGS_excel_service);
    }
    if (!channel) {
        ERR("Excel解析子服务无可用节点");
        throw Chat2DataException(ErrorCode::FILE_EXCEL_WORKSHEETS_GET_FAILED);
    }

    ::chat2Data::excelParseService::GetWorksheetsRequest request;
    request.set_request_id(chat2Data::Utils::generateUuid());
    request.set_fdfs_file_id(fdfsFileId);

    ::chat2Data::excelParseService::GetWorksheetsResponse response;
    brpc::Controller controller;
    controller.set_timeout_ms(WORKSHEET_RPC_TIMEOUT_MS);
    ::chat2Data::excelParseService::ExcelParserService_Stub stub(channel.get());
    stub.GetWorksheets(&controller, &request, &response, nullptr);

    if (controller.Failed()) {
        ERR("获取worksheet列表RPC失败, 原因: {}", controller.ErrorText());
        throw Chat2DataException(ErrorCode::FILE_EXCEL_WORKSHEETS_GET_FAILED);
    }
    if (response.error_code() != 0) {
        ERR("获取worksheet列表失败, errorCode: {}, errorMsg: {}", response.error_code(),
            response.error_msg());
        throw Chat2DataException(ErrorCode::FILE_EXCEL_WORKSHEETS_GET_FAILED);
    }

    std::vector<std::string> worksheetNames;
    worksheetNames.reserve(response.worksheets_size());
    for (const auto& worksheet : response.worksheets()) {
        worksheetNames.push_back(worksheet);
    }
    if (worksheetNames.empty()) {
        ERR("Excel文件中没有worksheet");
        throw Chat2DataException(ErrorCode::FILE_EXCEL_WORKSHEETS_GET_FAILED);
    }
    return worksheetNames;
}

std::vector<::chat2Data::excelParseService::WorksheetData> FileBusiness::parseExcel(
    const std::string& fdfsFileId, const std::vector<std::string>& worksheetNames) {
    biterpc::ChannelPtr channel;
    if (_svcChannels) {
        channel = _svcChannels->getNode(FLAGS_excel_service);
    }
    if (!channel) {
        ERR("Excel解析子服务无可用节点");
        throw Chat2DataException(ErrorCode::FILE_EXCEL_PARSE_FAILED);
    }

    ::chat2Data::excelParseService::ParseExcelRequest request;
    request.set_request_id(chat2Data::Utils::generateUuid());
    request.set_fdfs_file_id(fdfsFileId);
    for (const auto& worksheetName : worksheetNames) {
        request.add_worksheets(worksheetName);
    }

    ::chat2Data::excelParseService::ParseExcelResponse response;
    brpc::Controller controller;
    controller.set_timeout_ms(PARSE_EXCEL_RPC_TIMEOUT_MS);
    ::chat2Data::excelParseService::ExcelParserService_Stub stub(channel.get());
    stub.ParseExcel(&controller, &request, &response, nullptr);

    if (controller.Failed()) {
        ERR("解析Excel RPC失败, 原因: {}", controller.ErrorText());
        throw Chat2DataException(ErrorCode::FILE_EXCEL_PARSE_FAILED);
    }
    if (response.error_code() != 0) {
        ERR("解析Excel失败, errorCode: {}, errorMsg: {}", response.error_code(),
            response.error_msg());
        throw Chat2DataException(ErrorCode::FILE_EXCEL_PARSE_FAILED);
    }

    std::vector<::chat2Data::excelParseService::WorksheetData> worksheets;
    worksheets.reserve(response.worksheets_size());
    for (const auto& worksheet : response.worksheets()) {
        worksheets.push_back(worksheet);
    }
    if (worksheets.empty()) {
        ERR("解析Excel结果为空");
        throw Chat2DataException(ErrorCode::FILE_EXCEL_PARSE_FAILED);
    }
    return worksheets;
}

void FileBusiness::removeFdfsFileQuietly(const std::string& fdfsFileId) {
    if (fdfsFileId.empty()) {
        return;
    }
    if (!bitefdfs::FDFSClient::remove(fdfsFileId)) {
        WRN("删除FastDFS文件失败, fdfsFileId: {}", fdfsFileId);
    }
}

} // namespace fileService
