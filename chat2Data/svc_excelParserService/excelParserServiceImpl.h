#pragma once

#include <memory>

#include <brpc/server.h>

#include "errorHandler.h"
#include "excelParseService.pb.h"

namespace excelParserService {

class ExcelParserBusiness;

/**
 * @brief Excel 解析子服务 RPC 接口实现
 *
 * 实现获取 worksheet 列表与解析 Excel 文件两个 RPC 接口，
 * 只负责参数校验、调用业务层以及填充响应，不负责 RPC 服务器搭建。
 */
class ExcelParserServiceImpl : public ::chat2Data::excelParseService::ExcelParserService {
public:
    using ptr = std::shared_ptr<ExcelParserServiceImpl>;

    /**
     * @brief 构造 RPC 接口实现对象
     * @param excelParserBusiness Excel 解析业务类
     */
    explicit ExcelParserServiceImpl(std::shared_ptr<ExcelParserBusiness> excelParserBusiness);

    ~ExcelParserServiceImpl() override = default;

    void GetWorksheets(::google::protobuf::RpcController* controller,
                       const ::chat2Data::excelParseService::GetWorksheetsRequest* request,
                       ::chat2Data::excelParseService::GetWorksheetsResponse* response,
                       ::google::protobuf::Closure* done) override;

    void ParseExcel(::google::protobuf::RpcController* controller,
                    const ::chat2Data::excelParseService::ParseExcelRequest* request,
                    ::chat2Data::excelParseService::ParseExcelResponse* response,
                    ::google::protobuf::Closure* done) override;

private:
    /**
     * @brief 填充成功响应
     * @param response 响应对象
     * @note 成功时只设置错误码，不设置错误描述
     */
    template <typename Response>
    void fillSuccess(Response* response) {
        response->set_error_code(static_cast<int>(chat2Data::ErrorCode::SUCCESS));
    }

    /**
     * @brief 填充错误响应
     * @param response 响应对象
     * @param code     错误码
     */
    template <typename Response>
    void fillError(Response* response, chat2Data::ErrorCode code) {
        response->set_error_code(static_cast<int>(code));
        response->set_error_msg(chat2Data::error2String(code));
    }

private:
    std::shared_ptr<ExcelParserBusiness> _excelParserBusiness; // Excel 解析业务类
};

} // namespace excelParserService
