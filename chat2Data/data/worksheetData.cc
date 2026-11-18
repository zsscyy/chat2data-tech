#include "worksheetData.h"

#include <chrono>
#include <string>
#include <utility>

#include <bite_scaffold/log.h>
#include <bite_scaffold/util.h>
#include <odb/database.hxx>
#include <odb/query.hxx>
#include <odb/result.hxx>
#include <odb/transaction.hxx>
#include <sw/redis++/redis.h>

#include "errorHandler.h"
#include "worksheetEntity-odb.hxx"

namespace fileService {

using chat2Data::Chat2DataException;
using chat2Data::ErrorCode;
using chat2Data::WorksheetEntity;

namespace {

// worksheet映射缓存 key 前缀与 TTL（3天，与聊天会话生命周期保持一致）
constexpr const char* WORKSHEET_CACHE_KEY_PREFIX = "worksheet:";
constexpr long WORKSHEET_CACHE_TTL_SECONDS = 3L * 24 * 60 * 60;

// 缓存 JSON 字段名
constexpr const char* JSON_FIELD_FILE_ID = "fileId";
constexpr const char* JSON_FIELD_WORKSHEETS = "worksheets";
constexpr const char* JSON_FIELD_WORKSHEET_NAME = "worksheetName";
constexpr const char* JSON_FIELD_TABLE_NAME = "tableName";
// 兼容旧版本缓存字段，读取到旧数据时仍能正确解析
constexpr const char* LEGACY_JSON_FIELD_DB_TABLE_NAME = "dbTableName";

/**
 * @brief 构造worksheet映射缓存key
 */
std::string makeWorksheetCacheKey(const std::string& fileId) {
    return std::string(WORKSHEET_CACHE_KEY_PREFIX) + fileId;
}

} // namespace

WorksheetData::WorksheetData(std::shared_ptr<odb::database> database,
                             std::shared_ptr<sw::redis::Redis> redis)
    : _db(std::move(database)), _redis(std::move(redis)) {
    INF("WorksheetData initialized");
}

bool WorksheetData::saveWorksheetMappings(const std::string& fileId,
                                          const std::vector<WorksheetEntity>& worksheets) {
    try {
        odb::transaction trans(_db->begin());
        _db->erase_query<WorksheetEntity>(
            odb::query<WorksheetEntity>::fileId == fileId);
        for (const auto& worksheet : worksheets) {
            WorksheetEntity entity = worksheet;
            _db->persist(entity);
        }
        trans.commit();
    } catch (const odb::exception& e) {
        ERR("保存worksheet映射失败, fileId: {}, 原因: {}", fileId, e.what());
        throw Chat2DataException(ErrorCode::FILE_WORKSHEET_SAVE_FAILED);
    }

    deleteWorksheetMappingsFromCache(fileId);
    INF("worksheet映射保存成功, fileId: {}, 数量: {}", fileId, worksheets.size());
    return true;
}

std::vector<WorksheetEntity> WorksheetData::getWorksheetMappingsByFileId(const std::string& fileId) {
    std::vector<WorksheetEntity> cached = getWorksheetMappingsFromCache(fileId);
    if (!cached.empty()) {
        return cached;
    }

    std::vector<WorksheetEntity> result;
    try {
        odb::transaction trans(_db->begin());
        odb::result<WorksheetEntity> queryResult = _db->query<WorksheetEntity>(
            odb::query<WorksheetEntity>::fileId == fileId);
        for (const auto& worksheet : queryResult) {
            result.push_back(worksheet);
        }
        trans.commit();
    } catch (const odb::exception& e) {
        ERR("查询worksheet映射失败, fileId: {}, 原因: {}", fileId, e.what());
        throw Chat2DataException(ErrorCode::FILE_QUERY_FAILED);
    }

    if (!result.empty()) {
        saveWorksheetMappingsToCache(fileId, result);
    }
    return result;
}

bool WorksheetData::deleteWorksheetMappingsByFileId(const std::string& fileId) {
    try {
        odb::transaction trans(_db->begin());
        _db->erase_query<WorksheetEntity>(
            odb::query<WorksheetEntity>::fileId == fileId);
        trans.commit();
    } catch (const odb::exception& e) {
        ERR("删除worksheet映射失败, fileId: {}, 原因: {}", fileId, e.what());
        throw Chat2DataException(ErrorCode::FILE_DELETE_FAILED);
    }

    deleteWorksheetMappingsFromCache(fileId);
    INF("worksheet映射删除成功, fileId: {}", fileId);
    return true;
}

bool WorksheetData::saveWorksheetMappingsToCache(
    const std::string& fileId, const std::vector<WorksheetEntity>& worksheets) {
    try {
        Json::Value root;
        root[JSON_FIELD_FILE_ID] = fileId;
        Json::Value worksheetArray(Json::arrayValue);
        for (const auto& worksheet : worksheets) {
            Json::Value item;
            item[JSON_FIELD_FILE_ID] = worksheet.getFileId();
            item[JSON_FIELD_WORKSHEET_NAME] = worksheet.getWorksheetName();
            item[JSON_FIELD_TABLE_NAME] = worksheet.getTableName();
            worksheetArray.append(item);
        }
        root[JSON_FIELD_WORKSHEETS] = worksheetArray;

        std::optional<std::string> jsonStr = biteutil::JSON::serialize(root);
        if (!jsonStr.has_value()) {
            WRN("序列化worksheet映射缓存失败, fileId: {}", fileId);
            return false;
        }

        _redis->setex(makeWorksheetCacheKey(fileId),
                      std::chrono::seconds(WORKSHEET_CACHE_TTL_SECONDS), jsonStr.value());
        return true;
    } catch (const std::exception& e) {
        WRN("写入worksheet映射缓存失败, fileId: {}, 原因: {}", fileId, e.what());
        return false;
    }
}

std::vector<WorksheetEntity> WorksheetData::getWorksheetMappingsFromCache(const std::string& fileId) {
    std::vector<WorksheetEntity> result;
    const std::string key = makeWorksheetCacheKey(fileId);

    std::optional<std::string> jsonStr;
    try {
        jsonStr = _redis->get(key);
    } catch (const std::exception& e) {
        WRN("读取worksheet映射缓存失败, key: {}, 原因: {}", key, e.what());
        return result;
    }

    if (!jsonStr.has_value() || jsonStr->empty()) {
        return result;
    }

    std::optional<Json::Value> root = biteutil::JSON::unserialize(jsonStr.value());
    if (!root.has_value() || !root->isObject()) {
        WRN("worksheet映射缓存反序列化失败, key: {}", key);
        return result;
    }

    const Json::Value& value = root.value();
    if (!value.isMember(JSON_FIELD_WORKSHEETS) || !value[JSON_FIELD_WORKSHEETS].isArray()) {
        WRN("worksheet映射缓存字段不完整, key: {}", key);
        return result;
    }

    for (const auto& item : value[JSON_FIELD_WORKSHEETS]) {
        if (!item.isMember(JSON_FIELD_FILE_ID) || !item.isMember(JSON_FIELD_WORKSHEET_NAME) ||
            (!item.isMember(JSON_FIELD_TABLE_NAME) &&
             !item.isMember(LEGACY_JSON_FIELD_DB_TABLE_NAME))) {
            WRN("worksheet映射缓存条目字段不完整, key: {}", key);
            return std::vector<WorksheetEntity>();
        }

        // 新缓存使用 tableName；旧缓存使用 dbTableName，读取时兼容，写入时只写新字段
        const std::string tableName = item.isMember(JSON_FIELD_TABLE_NAME)
                                          ? item[JSON_FIELD_TABLE_NAME].asString()
                                          : item[LEGACY_JSON_FIELD_DB_TABLE_NAME].asString();
        result.emplace_back(item[JSON_FIELD_FILE_ID].asString(),
                            item[JSON_FIELD_WORKSHEET_NAME].asString(), tableName);
    }
    return result;
}

void WorksheetData::deleteWorksheetMappingsFromCache(const std::string& fileId) {
    try {
        _redis->del(makeWorksheetCacheKey(fileId));
    } catch (const std::exception& e) {
        WRN("删除worksheet映射缓存失败, fileId: {}, 原因: {}", fileId, e.what());
    }
}

} // namespace fileService
