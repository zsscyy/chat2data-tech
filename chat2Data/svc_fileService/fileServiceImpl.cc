#include "fileServiceImpl.h"

#include <string>
#include <utility>

#include <brpc/controller.h>
#include <butil/iobuf.h>
#include <bite_scaffold/log.h>

#include "fileBusiness.h"
#include "utils.h"

namespace fileService {

namespace {

/**
 * @brief 从RPC请求中提取attachment中的文件数据
 * @param controller RPC控制器
 * @param fileData   输出参数，文件二进制数据
 */
void extractRequestAttachment(::google::protobuf::RpcController* controller,
                              std::string& fileData) {
    auto* brpcController = static_cast<brpc::Controller*>(controller);
    brpcController->request_attachment().copy_to(&fileData);
}

/**
 * @brief 将文件数据写入RPC响应的attachment
 * @param controller RPC控制器
 * @param fileData   文件二进制数据
 */
void setResponseAttachment(::google::protobuf::RpcController* controller,
                           const std::string& fileData) {
    auto* brpcController = static_cast<brpc::Controller*>(controller);
    brpcController->response_attachment().append(fileData);
}

} // namespace

FileServiceImpl::FileServiceImpl(std::shared_ptr<FileBusiness> fileBusiness)
    : _fileBusiness(std::move(fileBusiness)) {}

void FileServiceImpl::UploadFileInfo(
    ::google::protobuf::RpcController* controller,
    const ::chat2Data::fileService::UploadFileInfoRequest* request,
    ::chat2Data::fileService::UploadFileInfoResponse* response,
    ::google::protobuf::Closure* done) {
    brpc::ClosureGuard doneGuard(done);
    response->set_request_id(request->request_id());

    // Excel 上传前必须先创建聊天会话，session_id 不允许为空
    if (request->session_id().empty() || request->user_id().empty() ||
        request->file_info().filename().empty() || request->file_info().file_ext().empty()) {
        WRN("上传文件信息参数非法, requestId: {}, sessionId: {}", request->request_id(),
            request->session_id());
        fillError(response, chat2Data::ErrorCode::FILE_PARAM_INVALID);
        return;
    }

    try {
        FileInfo fileInfo;
        fileInfo.fileName = request->file_info().filename();
        fileInfo.fileSize = static_cast<std::uint64_t>(request->file_info().file_size());
        fileInfo.fileExt = request->file_info().file_ext();

        const std::string fileId = _fileBusiness->saveFileInfo(request->user_id(), fileInfo);
        response->mutable_result()->set_file_id(fileId);
        fillSuccess(response);
    } catch (const chat2Data::Chat2DataException& e) {
        ERR("上传文件信息失败, requestId: {}, 原因: {}", request->request_id(), e.what());
        fillError(response, e.getErrorCode());
    } catch (const std::exception& e) {
        ERR("上传文件信息发生未知异常, requestId: {}, 原因: {}", request->request_id(), e.what());
        fillError(response, chat2Data::ErrorCode::FILE_UNKNOWN_ERROR);
    }
}

void FileServiceImpl::GetFileInfo(::google::protobuf::RpcController* controller,
                                  const ::chat2Data::fileService::GetFileInfoRequest* request,
                                  ::chat2Data::fileService::GetFileInfoResponse* response,
                                  ::google::protobuf::Closure* done) {
    brpc::ClosureGuard doneGuard(done);
    response->set_request_id(request->request_id());

    if (request->file_id().empty() || request->user_id().empty()) {
        WRN("获取文件信息参数非法, requestId: {}", request->request_id());
        fillError(response, chat2Data::ErrorCode::FILE_PARAM_INVALID);
        return;
    }

    try {
        FileInfo fileInfo = _fileBusiness->getFileInfo(request->file_id(), request->user_id());
        auto* result = response->mutable_result();
        result->set_file_id(fileInfo.fileId);
        result->set_file_name(fileInfo.fileName);
        result->set_file_size(static_cast<int64_t>(fileInfo.fileSize));
        result->set_upload_time(static_cast<int64_t>(fileInfo.uploadTime));
        result->set_file_ext(fileInfo.fileExt);
        fillSuccess(response);
    } catch (const chat2Data::Chat2DataException& e) {
        ERR("获取文件信息失败, requestId: {}, 原因: {}", request->request_id(), e.what());
        fillError(response, e.getErrorCode());
    } catch (const std::exception& e) {
        ERR("获取文件信息发生未知异常, requestId: {}, 原因: {}", request->request_id(), e.what());
        fillError(response, chat2Data::ErrorCode::FILE_UNKNOWN_ERROR);
    }
}

void FileServiceImpl::UploadFile(::google::protobuf::RpcController* controller,
                                 const ::chat2Data::fileService::UploadFileRequest* request,
                                 ::chat2Data::fileService::UploadFileResponse* response,
                                 ::google::protobuf::Closure* done) {
    brpc::ClosureGuard doneGuard(done);
    response->set_request_id(request->request_id());

    if (request->file_id().empty() || request->user_id().empty()) {
        WRN("上传文件参数非法, requestId: {}", request->request_id());
        fillError(response, chat2Data::ErrorCode::FILE_PARAM_INVALID);
        return;
    }

    try {
        std::string fileData;
        extractRequestAttachment(controller, fileData);
        if (fileData.empty()) {
            WRN("上传文件内容为空, requestId: {}", request->request_id());
            fillError(response, chat2Data::ErrorCode::FILE_PARAM_INVALID);
            return;
        }

        _fileBusiness->uploadExcelFile(request->file_id(), request->user_id(), fileData);
        fillSuccess(response);
    } catch (const chat2Data::Chat2DataException& e) {
        ERR("上传Excel文件失败, requestId: {}, 原因: {}", request->request_id(), e.what());
        fillError(response, e.getErrorCode());
    } catch (const std::exception& e) {
        ERR("上传Excel文件发生未知异常, requestId: {}, 原因: {}", request->request_id(), e.what());
        fillError(response, chat2Data::ErrorCode::FILE_UNKNOWN_ERROR);
    }
}

void FileServiceImpl::DownloadFile(::google::protobuf::RpcController* controller,
                                   const ::chat2Data::fileService::DownloadFileRequest* request,
                                   ::chat2Data::fileService::DownloadFileResponse* response,
                                   ::google::protobuf::Closure* done) {
    brpc::ClosureGuard doneGuard(done);
    response->set_request_id(request->request_id());

    if (request->file_id().empty() || request->user_id().empty()) {
        WRN("下载文件参数非法, requestId: {}", request->request_id());
        fillError(response, chat2Data::ErrorCode::FILE_PARAM_INVALID);
        return;
    }

    try {
        const std::string fileData =
            _fileBusiness->downloadFile(request->file_id(), request->user_id());
        setResponseAttachment(controller, fileData);
        fillSuccess(response);
    } catch (const chat2Data::Chat2DataException& e) {
        ERR("下载文件失败, requestId: {}, 原因: {}", request->request_id(), e.what());
        fillError(response, e.getErrorCode());
    } catch (const std::exception& e) {
        ERR("下载文件发生未知异常, requestId: {}, 原因: {}", request->request_id(), e.what());
        fillError(response, chat2Data::ErrorCode::FILE_UNKNOWN_ERROR);
    }
}

void FileServiceImpl::DeleteFile(::google::protobuf::RpcController* controller,
                                 const ::chat2Data::fileService::DeleteFileRequest* request,
                                 ::chat2Data::fileService::DeleteFileResponse* response,
                                 ::google::protobuf::Closure* done) {
    brpc::ClosureGuard doneGuard(done);
    response->set_request_id(request->request_id());

    if (request->file_id().empty() || request->user_id().empty()) {
        WRN("删除文件参数非法, requestId: {}", request->request_id());
        fillError(response, chat2Data::ErrorCode::FILE_PARAM_INVALID);
        return;
    }

    try {
        _fileBusiness->deleteFile(request->file_id(), request->user_id());
        fillSuccess(response);
    } catch (const chat2Data::Chat2DataException& e) {
        ERR("删除文件失败, requestId: {}, 原因: {}", request->request_id(), e.what());
        fillError(response, e.getErrorCode());
    } catch (const std::exception& e) {
        ERR("删除文件发生未知异常, requestId: {}, 原因: {}", request->request_id(), e.what());
        fillError(response, chat2Data::ErrorCode::FILE_UNKNOWN_ERROR);
    }
}

void FileServiceImpl::PreviewExcel(::google::protobuf::RpcController* controller,
                                   const ::chat2Data::fileService::PreviewExcelRequest* request,
                                   ::chat2Data::fileService::PreviewExcelResponse* response,
                                   ::google::protobuf::Closure* done) {
    brpc::ClosureGuard doneGuard(done);
    response->set_request_id(request->request_id());

    if (request->file_id().empty() || request->user_id().empty()) {
        WRN("预览Excel参数非法, requestId: {}", request->request_id());
        fillError(response, chat2Data::ErrorCode::FILE_PARAM_INVALID);
        return;
    }

    try {
        ExcelPreview preview = _fileBusiness->previewExcel(
            request->file_id(), request->user_id(), request->page_number(), request->page_size());

        auto* result = response->mutable_result();
        result->set_file_id(preview.fileInfo.fileId);
        result->set_file_name(preview.fileInfo.fileName);
        result->set_file_size(static_cast<int64_t>(preview.fileInfo.fileSize));
        result->set_file_ext(preview.fileInfo.fileExt);

        auto* excelData = result->mutable_excel_data();
        for (const auto& sheet : preview.sheets) {
            auto* protoSheet = excelData->add_sheets();
            protoSheet->set_name(sheet.name);
            protoSheet->set_total_rows(sheet.totalRows);
            protoSheet->set_col_count(sheet.colCount);
            protoSheet->set_current_page(sheet.currentPage);
            protoSheet->set_total_pages(sheet.totalPages);
            protoSheet->set_page_size(sheet.pageSize);
            for (const auto& column : sheet.columns) {
                protoSheet->add_columns(column);
            }
            for (const auto& row : sheet.rows) {
                auto* protoRow = protoSheet->add_data();
                for (const auto& cell : row) {
                    protoRow->add_cells(cell);
                }
            }
        }
        fillSuccess(response);
    } catch (const chat2Data::Chat2DataException& e) {
        ERR("预览Excel失败, requestId: {}, 原因: {}", request->request_id(), e.what());
        fillError(response, e.getErrorCode());
    } catch (const std::exception& e) {
        ERR("预览Excel发生未知异常, requestId: {}, 原因: {}", request->request_id(), e.what());
        fillError(response, chat2Data::ErrorCode::FILE_UNKNOWN_ERROR);
    }
}

void FileServiceImpl::GetFileList(::google::protobuf::RpcController* controller,
                                  const ::chat2Data::fileService::GetFileListRequest* request,
                                  ::chat2Data::fileService::GetFileListResponse* response,
                                  ::google::protobuf::Closure* done) {
    brpc::ClosureGuard doneGuard(done);
    response->set_request_id(request->request_id());

    if (request->user_id().empty()) {
        WRN("获取文件列表参数非法, requestId: {}", request->request_id());
        fillError(response, chat2Data::ErrorCode::FILE_PARAM_INVALID);
        return;
    }

    try {
        std::vector<FileInfo> fileList = _fileBusiness->getFileList(request->user_id());
        auto* result = response->mutable_result();
        for (const auto& fileInfo : fileList) {
            auto* item = result->add_file_list();
            item->set_file_id(fileInfo.fileId);
            item->set_file_name(fileInfo.fileName);
            item->set_file_size(static_cast<int64_t>(fileInfo.fileSize));
            item->set_upload_time(static_cast<int64_t>(fileInfo.uploadTime));
            item->set_chat_session_id(fileInfo.chatSessionId);
        }
        fillSuccess(response);
    } catch (const chat2Data::Chat2DataException& e) {
        ERR("获取文件列表失败, requestId: {}, 原因: {}", request->request_id(), e.what());
        fillError(response, e.getErrorCode());
    } catch (const std::exception& e) {
        ERR("获取文件列表发生未知异常, requestId: {}, 原因: {}", request->request_id(), e.what());
        fillError(response, chat2Data::ErrorCode::FILE_UNKNOWN_ERROR);
    }
}

void FileServiceImpl::HandleFileChatSessionMap(
    ::google::protobuf::RpcController* controller,
    const ::chat2Data::fileService::HandleFileChatSessionMapRequest* request,
    ::chat2Data::fileService::HandleFileChatSessionMapResponse* response,
    ::google::protobuf::Closure* done) {
    brpc::ClosureGuard doneGuard(done);
    response->set_request_id(request->request_id());

    if (request->file_id().empty() || request->chat_session_id().empty() ||
        request->user_id().empty()) {
        WRN("关联文件与会话参数非法, requestId: {}", request->request_id());
        fillError(response, chat2Data::ErrorCode::FILE_PARAM_INVALID);
        return;
    }

    try {
        _fileBusiness->associateFileChatSession(request->file_id(), request->chat_session_id(),
                                                request->user_id());
        fillSuccess(response);
    } catch (const chat2Data::Chat2DataException& e) {
        ERR("关联文件与会话失败, requestId: {}, 原因: {}", request->request_id(), e.what());
        fillError(response, e.getErrorCode());
    } catch (const std::exception& e) {
        ERR("关联文件与会话发生未知异常, requestId: {}, 原因: {}", request->request_id(), e.what());
        fillError(response, chat2Data::ErrorCode::FILE_UNKNOWN_ERROR);
    }
}

void FileServiceImpl::UploadSQLiteFile(
    ::google::protobuf::RpcController* controller,
    const ::chat2Data::fileService::UploadSQLiteFileRequest* request,
    ::chat2Data::fileService::UploadSQLiteFileResponse* response,
    ::google::protobuf::Closure* done) {
    brpc::ClosureGuard doneGuard(done);
    response->set_request_id(request->request_id());

    // SQLite文件无需关联聊天会话，session_id允许为空；
    // Excel的session_id约束在UploadFileInfo中处理
    if (request->filename().empty() || request->user_id().empty()) {
        WRN("上传SQLite文件参数非法, requestId: {}", request->request_id());
        fillError(response, chat2Data::ErrorCode::FILE_PARAM_INVALID);
        return;
    }

    try {
        std::string fileData;
        extractRequestAttachment(controller, fileData);
        if (fileData.empty()) {
            WRN("上传SQLite文件内容为空, requestId: {}", request->request_id());
            fillError(response, chat2Data::ErrorCode::FILE_PARAM_INVALID);
            return;
        }

        const std::string fileId =
            _fileBusiness->uploadSQLiteFile(request->user_id(), request->filename(), fileData);
        response->mutable_result()->set_file_id(fileId);
        fillSuccess(response);
    } catch (const chat2Data::Chat2DataException& e) {
        ERR("上传SQLite文件失败, requestId: {}, 原因: {}", request->request_id(), e.what());
        fillError(response, e.getErrorCode());
    } catch (const std::exception& e) {
        ERR("上传SQLite文件发生未知异常, requestId: {}, 原因: {}", request->request_id(), e.what());
        fillError(response, chat2Data::ErrorCode::FILE_UNKNOWN_ERROR);
    }
}

void FileServiceImpl::GetSQLiteFile(::google::protobuf::RpcController* controller,
                                    const ::chat2Data::fileService::GetSQLiteFileRequest* request,
                                    ::chat2Data::fileService::GetSQLiteFileResponse* response,
                                    ::google::protobuf::Closure* done) {
    brpc::ClosureGuard doneGuard(done);
    response->set_request_id(request->request_id());

    if (request->file_id().empty()) {
        WRN("获取SQLite文件参数非法, requestId: {}", request->request_id());
        fillError(response, chat2Data::ErrorCode::FILE_PARAM_INVALID);
        return;
    }

    try {
        const std::string fdfsFileId = _fileBusiness->getSQLiteFdfsFileId(request->file_id());
        response->mutable_result()->set_fdfsfileid(fdfsFileId);
        fillSuccess(response);
    } catch (const chat2Data::Chat2DataException& e) {
        ERR("获取SQLite文件失败, requestId: {}, 原因: {}", request->request_id(), e.what());
        fillError(response, e.getErrorCode());
    } catch (const std::exception& e) {
        ERR("获取SQLite文件发生未知异常, requestId: {}, 原因: {}", request->request_id(), e.what());
        fillError(response, chat2Data::ErrorCode::FILE_UNKNOWN_ERROR);
    }
}

void FileServiceImpl::GetWorksheetDBTables(
    ::google::protobuf::RpcController* controller,
    const ::chat2Data::fileService::GetWorksheetDBTablesRequest* request,
    ::chat2Data::fileService::GetWorksheetDBTablesResponse* response,
    ::google::protobuf::Closure* done) {
    brpc::ClosureGuard doneGuard(done);
    response->set_request_id(request->request_id());

    if (request->file_id().empty()) {
        WRN("获取worksheet数据库表名参数非法, requestId: {}", request->request_id());
        fillError(response, chat2Data::ErrorCode::FILE_PARAM_INVALID);
        return;
    }

    try {
        std::vector<std::string> tableNames =
            _fileBusiness->getWorksheetDBTables(request->file_id());
        auto* result = response->mutable_result();
        for (const auto& tableName : tableNames) {
            result->add_worksheet_dbtables(tableName);
        }
        fillSuccess(response);
    } catch (const chat2Data::Chat2DataException& e) {
        ERR("获取worksheet数据库表名失败, requestId: {}, 原因: {}", request->request_id(), e.what());
        fillError(response, e.getErrorCode());
    } catch (const std::exception& e) {
        ERR("获取worksheet数据库表名发生未知异常, requestId: {}, 原因: {}", request->request_id(),
            e.what());
        fillError(response, chat2Data::ErrorCode::FILE_UNKNOWN_ERROR);
    }
}

} // namespace fileService
