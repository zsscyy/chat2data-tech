#pragma once

#include <memory>

#include <brpc/server.h>

#include "common.h"
#include "errorHandler.h"
#include "fileService.pb.h"

namespace fileService {

class FileBusiness;

/**
 * @brief 文件子服务RPC接口实现
 *
 * 继承proto生成的FileService基类，负责参数校验、附件收发与业务层调用。
 */
class FileServiceImpl : public ::chat2Data::fileService::FileService {
public:
    using ptr = std::shared_ptr<FileServiceImpl>;

    /**
     * @brief 构造RPC接口实现对象
     * @param fileBusiness 文件业务对象
     */
    explicit FileServiceImpl(std::shared_ptr<FileBusiness> fileBusiness);

    ~FileServiceImpl() override = default;

    void UploadFileInfo(::google::protobuf::RpcController* controller,
                        const ::chat2Data::fileService::UploadFileInfoRequest* request,
                        ::chat2Data::fileService::UploadFileInfoResponse* response,
                        ::google::protobuf::Closure* done) override;

    void GetFileInfo(::google::protobuf::RpcController* controller,
                     const ::chat2Data::fileService::GetFileInfoRequest* request,
                     ::chat2Data::fileService::GetFileInfoResponse* response,
                     ::google::protobuf::Closure* done) override;

    void UploadFile(::google::protobuf::RpcController* controller,
                    const ::chat2Data::fileService::UploadFileRequest* request,
                    ::chat2Data::fileService::UploadFileResponse* response,
                    ::google::protobuf::Closure* done) override;

    void DownloadFile(::google::protobuf::RpcController* controller,
                      const ::chat2Data::fileService::DownloadFileRequest* request,
                      ::chat2Data::fileService::DownloadFileResponse* response,
                      ::google::protobuf::Closure* done) override;

    void DeleteFile(::google::protobuf::RpcController* controller,
                    const ::chat2Data::fileService::DeleteFileRequest* request,
                    ::chat2Data::fileService::DeleteFileResponse* response,
                    ::google::protobuf::Closure* done) override;

    void PreviewExcel(::google::protobuf::RpcController* controller,
                      const ::chat2Data::fileService::PreviewExcelRequest* request,
                      ::chat2Data::fileService::PreviewExcelResponse* response,
                      ::google::protobuf::Closure* done) override;

    void GetFileList(::google::protobuf::RpcController* controller,
                     const ::chat2Data::fileService::GetFileListRequest* request,
                     ::chat2Data::fileService::GetFileListResponse* response,
                     ::google::protobuf::Closure* done) override;

    void HandleFileChatSessionMap(
        ::google::protobuf::RpcController* controller,
        const ::chat2Data::fileService::HandleFileChatSessionMapRequest* request,
        ::chat2Data::fileService::HandleFileChatSessionMapResponse* response,
        ::google::protobuf::Closure* done) override;

    void UploadSQLiteFile(::google::protobuf::RpcController* controller,
                          const ::chat2Data::fileService::UploadSQLiteFileRequest* request,
                          ::chat2Data::fileService::UploadSQLiteFileResponse* response,
                          ::google::protobuf::Closure* done) override;

    void GetSQLiteFile(::google::protobuf::RpcController* controller,
                       const ::chat2Data::fileService::GetSQLiteFileRequest* request,
                       ::chat2Data::fileService::GetSQLiteFileResponse* response,
                       ::google::protobuf::Closure* done) override;

    void GetWorksheetDBTables(::google::protobuf::RpcController* controller,
                              const ::chat2Data::fileService::GetWorksheetDBTablesRequest* request,
                              ::chat2Data::fileService::GetWorksheetDBTablesResponse* response,
                              ::google::protobuf::Closure* done) override;

private:
    template <typename Response>
    void fillSuccess(Response* response) {
        response->set_error_code(static_cast<int>(chat2Data::ErrorCode::SUCCESS));
    }

    template <typename Response>
    void fillError(Response* response, chat2Data::ErrorCode code) {
        response->set_error_code(static_cast<int>(code));
        response->set_error_msg(chat2Data::error2String(code));
    }

    std::shared_ptr<FileBusiness> _fileBusiness;
};

} // namespace fileService
