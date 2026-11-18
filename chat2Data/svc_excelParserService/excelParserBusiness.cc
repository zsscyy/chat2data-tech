#include "excelParserBusiness.h"

#include <cstdio>
#include <memory>
#include <utility>

#include <bite_scaffold/log.h>

// fdfs.h 引入的 fastcommon 会定义 byte 宏，必须在日志等C++依赖之后包含，
// 包含后立即取消定义，避免影响后续C++标准库与spdlog/fmt头文件
#include <bite_scaffold/fdfs.h>
#ifdef byte
#undef byte
#endif

#include "errorHandler.h"
#include "excelParser.h"
#include "utils.h"

namespace excelParserService {

namespace {

// 本地临时文件目录
const char* const TEMP_FILE_DIR = "/tmp/";
// 本地临时文件前缀
const char* const TEMP_FILE_PREFIX = "chat2Data_excel_";
// 本地临时文件后缀
const char* const TEMP_FILE_SUFFIX = ".xlsx";

/**
 * @brief 本地临时文件守卫
 *
 * 利用 RAII 在离开作用域时删除临时文件，避免异常路径下遗留文件。
 */
class TempFileGuard {
public:
    explicit TempFileGuard(std::string filePath)
        : _filePath(std::move(filePath)) {}

    ~TempFileGuard() {
        if (!_filePath.empty()) {
            std::remove(_filePath.c_str());
        }
    }

    TempFileGuard(const TempFileGuard&) = delete;
    TempFileGuard& operator=(const TempFileGuard&) = delete;

    const std::string& getFilePath() const {
        return _filePath;
    }

private:
    std::string _filePath; // 临时文件路径
};

} // namespace

ExcelParserBusiness::ExcelParserBusiness()
    : _excelParser(std::make_shared<ExcelParser>()) {
    INF("ExcelParserBusiness initialized");
}

std::string ExcelParserBusiness::makeTempFilePath() const {
    return std::string(TEMP_FILE_DIR) + TEMP_FILE_PREFIX + chat2Data::Utils::generateUuid() + TEMP_FILE_SUFFIX;
}

void ExcelParserBusiness::downloadExcelFile(const std::string& fdfsFileId, const std::string& tempFilePath) const {
    if (!bitefdfs::FDFSClient::download_to_file(fdfsFileId, tempFilePath)) {
        ERR("从FastDFS下载Excel文件失败, fdfsFileId: {}, 临时文件: {}", fdfsFileId, tempFilePath);
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::EXCEL_FILE_OPEN_FAILED);
    }
    INF("从FastDFS下载Excel文件成功, fdfsFileId: {}, 临时文件: {}", fdfsFileId, tempFilePath);
}

std::vector<std::string> ExcelParserBusiness::getWorksheets(const std::string& fdfsFileId) const {
    if (fdfsFileId.empty()) {
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::EXCEL_FILE_PATH_INVALID);
    }

    // 临时文件由 TempFileGuard 负责删除，保证异常路径也能清理
    TempFileGuard tempFile(makeTempFilePath());
    downloadExcelFile(fdfsFileId, tempFile.getFilePath());

    std::vector<std::string> worksheets = _excelParser->getWorksheetNames(tempFile.getFilePath());
    if (worksheets.empty()) {
        ERR("Excel文件中没有可用的worksheet, fdfsFileId: {}", fdfsFileId);
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::EXCEL_FILE_OPEN_FAILED);
    }

    INF("获取worksheet列表成功, fdfsFileId: {}, worksheet数量: {}", fdfsFileId, worksheets.size());
    return worksheets;
}

std::vector<::chat2Data::excelParseService::WorksheetData> ExcelParserBusiness::parseExcel(
    const std::string& fdfsFileId, const std::vector<std::string>& worksheets) const {
    if (fdfsFileId.empty()) {
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::EXCEL_FILE_PATH_INVALID);
    }
    if (worksheets.empty()) {
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::EXCEL_DATA_INVALID);
    }

    TempFileGuard tempFile(makeTempFilePath());
    downloadExcelFile(fdfsFileId, tempFile.getFilePath());

    std::vector<::chat2Data::excelParseService::WorksheetData> result;
    result.reserve(worksheets.size());
    for (const std::string& worksheetName : worksheets) {
        std::unique_ptr<WorksheetData> worksheetData =
            _excelParser->parseWorksheet(tempFile.getFilePath(), worksheetName);
        if (!worksheetData) {
            ERR("解析worksheet失败, fdfsFileId: {}, worksheet: {}", fdfsFileId, worksheetName);
            throw chat2Data::Chat2DataException(chat2Data::ErrorCode::EXCEL_PARSE_FAILED);
        }
        result.push_back(convertToProtoWorksheetData(*worksheetData));
    }

    INF("解析Excel文件完成, fdfsFileId: {}, 解析worksheet数量: {}", fdfsFileId, result.size());
    return result;
}

::chat2Data::excelParseService::WorksheetData ExcelParserBusiness::convertToProtoWorksheetData(
    const WorksheetData& worksheetData) const {
    ::chat2Data::excelParseService::WorksheetData protoData;
    protoData.set_name(worksheetData._name);
    protoData.set_total_rows(worksheetData._totalRows);
    protoData.set_total_cols(worksheetData._totalCols);

    for (const ColumnInfo& column : worksheetData._columns) {
        ::chat2Data::excelParseService::ColumnInfo* protoColumn = protoData.add_columns();
        protoColumn->set_name(column._name);
        protoColumn->set_type(columnTypeToString(column._type));
    }

    for (const RowData& row : worksheetData._rows) {
        ::chat2Data::excelParseService::RowData* protoRow = protoData.add_rows();
        for (const CellData& cell : row._cells) {
            ::chat2Data::excelParseService::CellData* protoCell = protoRow->add_cells();
            protoCell->set_value(cell._value);
            protoCell->set_type(cellTypeToString(cell._type));
        }
    }

    return protoData;
}

} // namespace excelParserService
