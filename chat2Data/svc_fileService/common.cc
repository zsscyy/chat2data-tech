#include "common.h"

#include <algorithm>
#include <cctype>

#include "utils.h"

namespace fileService {

namespace {

// 表名中worksheet名称部分的最大长度，为下划线和UUID预留空间
constexpr std::size_t TABLE_NAME_PREFIX_MAX_LENGTH = 31;
// 数据库表名最大长度
constexpr std::size_t TABLE_NAME_MAX_LENGTH = 64;

/**
 * @brief 将文件后缀统一为小写
 */
std::string toLower(const std::string& value) {
    std::string lowerValue = value;
    std::transform(lowerValue.begin(), lowerValue.end(), lowerValue.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return lowerValue;
}

} // namespace

bool isXlsxFile(const std::string& fileExt) {
    return toLower(fileExt) == ".xlsx";
}

bool isSqliteFile(const std::string& fileExt) {
    return toLower(fileExt) == ".db";
}

std::string calculateTableName(const std::string& worksheetName) {
    std::string prefix;
    prefix.reserve(worksheetName.size());
    for (unsigned char ch : worksheetName) {
        // 保留中文等多字节字符和合法的ASCII字符，其余替换为下划线
        if (ch >= 0x80 || std::isalnum(ch) != 0 || ch == '_') {
            prefix.push_back(static_cast<char>(ch));
        } else {
            prefix.push_back('_');
        }
    }

    if (prefix.empty()) {
        prefix = "worksheet";
    }
    if (std::isdigit(static_cast<unsigned char>(prefix.front())) != 0) {
        prefix = "ws_" + prefix;
    }
    if (prefix.size() > TABLE_NAME_PREFIX_MAX_LENGTH) {
        prefix.resize(TABLE_NAME_PREFIX_MAX_LENGTH);
    }

    std::string uuid = chat2Data::Utils::generateUuid();
    uuid.erase(std::remove(uuid.begin(), uuid.end(), '-'), uuid.end());

    std::string tableName = prefix + "_" + uuid;
    if (tableName.size() > TABLE_NAME_MAX_LENGTH) {
        tableName.resize(TABLE_NAME_MAX_LENGTH);
    }
    return tableName;
}

} // namespace fileService
