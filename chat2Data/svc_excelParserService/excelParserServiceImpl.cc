#include "excelParserServiceImpl.h"

#include <string>
#include <utility>
#include <vector>

#include <bite_scaffold/log.h>

#include "excelParserBusiness.h"

namespace excelParserService {

ExcelParserServiceImpl::ExcelParserServiceImpl(std::shared_ptr<ExcelParserBusiness> excelParserBusiness)
    : _excelParserBusiness(std::move(excelParserBusiness)) {}

void ExcelParserServiceImpl::GetWorksheets(::google::protobuf::RpcController* controller,
                                           const ::chat2Data::excelParseService::GetWorksheetsRequest* request,
                                           ::chat2Data::excelParseService::GetWorksheetsResponse* response,
                                           ::google::protobuf::Closure* done) {
    // 0. 构造ClosureGuard对象，该对象以RAII机制管理done->Run()方法的调用
    brpc::ClosureGuard doneGuard(done);

    // 1. 解析RPC请求中参数
    response->set_request_id(request->request_id());

    // 2. 参数校验
    if (request->fdfs_file_id().empty()) {
        WRN("获取worksheet列表参数非法，FastDFS文件ID为空, requestId: {}", request->request_id());
        fillError(response, chat2Data::ErrorCode::EXCEL_FILE_PATH_INVALID);
        return;
    }

    try {
        // 3. 业务逻辑处理：获取所有worksheet名称
        const std::vector<std::string> worksheets = _excelParserBusiness->getWorksheets(request->fdfs_file_id());

        // 4. 设置RPC响应
        for (const std::string& worksheetName : worksheets) {
            response->add_worksheets(worksheetName);
        }
        fillSuccess(response);
        INF("获取worksheet列表成功, requestId: {}, worksheet数量: {}", request->request_id(), worksheets.size());
    } catch (const chat2Data::Chat2DataException& e) {
        ERR("获取worksheet列表失败, requestId: {}, 原因: {}", request->request_id(), e.what());
        fillError(response, e.getErrorCode());
    } catch (const std::exception& e) {
        ERR("获取worksheet列表发生未知异常, requestId: {}, 原因: {}", request->request_id(), e.what());
        fillError(response, chat2Data::ErrorCode::EXCEL_PARSE_FAILED);
    }
}

void ExcelParserServiceImpl::ParseExcel(::google::protobuf::RpcController* controller,
                                        const ::chat2Data::excelParseService::ParseExcelRequest* request,
                                        ::chat2Data::excelParseService::ParseExcelResponse* response,
                                        ::google::protobuf::Closure* done) {
    // 0. 构造ClosureGuard对象，该对象以RAII机制管理done->Run()方法的调用
    brpc::ClosureGuard doneGuard(done);

    // 1. 解析RPC请求中参数
    response->set_request_id(request->request_id());

    // 2. 参数校验
    if (request->fdfs_file_id().empty()) {
        WRN("解析Excel文件参数非法，FastDFS文件ID为空, requestId: {}", request->request_id());
        fillError(response, chat2Data::ErrorCode::EXCEL_FILE_PATH_INVALID);
        return;
    }
    if (request->worksheets().empty()) {
        WRN("解析Excel文件参数非法，worksheet列表为空, requestId: {}", request->request_id());
        fillError(response, chat2Data::ErrorCode::EXCEL_DATA_INVALID);
        return;
    }

    try {
        // 3. 业务逻辑处理：解析指定worksheet的表结构和表数据
        const std::vector<std::string> worksheets(request->worksheets().begin(), request->worksheets().end());
        const std::vector<::chat2Data::excelParseService::WorksheetData> worksheetDataList =
            _excelParserBusiness->parseExcel(request->fdfs_file_id(), worksheets);

        // 4. 设置RPC响应
        for (const ::chat2Data::excelParseService::WorksheetData& worksheetData : worksheetDataList) {
            response->add_worksheets()->CopyFrom(worksheetData);
        }
        fillSuccess(response);
        INF("解析Excel文件成功, requestId: {}, worksheet数量: {}", request->request_id(), worksheetDataList.size());
    } catch (const chat2Data::Chat2DataException& e) {
        ERR("解析Excel文件失败, requestId: {}, 原因: {}", request->request_id(), e.what());
        fillError(response, e.getErrorCode());
    } catch (const std::exception& e) {
        ERR("解析Excel文件发生未知异常, requestId: {}, 原因: {}", request->request_id(), e.what());
        fillError(response, chat2Data::ErrorCode::EXCEL_PARSE_FAILED);
    }
}

} // namespace excelParserService
