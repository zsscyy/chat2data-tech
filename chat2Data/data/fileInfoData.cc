#include "fileInfoData.h"

#include <algorithm>
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
#include "fileInfoEntity-odb.hxx"

namespace fileService {

using chat2Data::Chat2DataException;
using chat2Data::ErrorCode;
using chat2Data::FileInfoEntity;

namespace {

// 文件信息缓存 key 前缀与 TTL（3天，与聊天会话生命周期保持一致）
constexpr const char* FILE_CACHE_KEY_PREFIX = "file:";
constexpr long FILE_CACHE_TTL_SECONDS = 3L * 24 * 60 * 60;

// 缓存 JSON 字段名
constexpr const char* JSON_FIELD_FILE_ID = "fileId";
constexpr const char* JSON_FIELD_FILE_NAME = "fileName";
constexpr const char* JSON_FIELD_FILE_SIZE = "fileSize";
constexpr const char* JSON_FIELD_UPLOAD_TIME = "uploadTime";
constexpr const char* JSON_FIELD_FILE_EXT = "fileExt";
constexpr const char* JSON_FIELD_FDFS_FILE_ID = "fdfsFileId";
constexpr const char* JSON_FIELD_USER_ID = "userId";
constexpr const char* JSON_FIELD_CHAT_SESSION_ID = "chatSessionId";

/**
 * @brief 构造文件信息缓存key
 */
std::string makeFileCacheKey(const std::string& fileId) {
    return std::string(FILE_CACHE_KEY_PREFIX) + fileId;
}

} // namespace

FileInfoData::FileInfoData(std::shared_ptr<odb::database> database,
                           std::shared_ptr<sw::redis::Redis> redis)
    : _db(std::move(database)), _redis(std::move(redis)) {
    INF("FileInfoData initialized");
}

bool FileInfoData::saveFileInfo(const FileInfoEntity& fileInfo) {
    try {
        odb::transaction trans(_db->begin());
        FileInfoEntity existing;
        if (_db->query_one<FileInfoEntity>(
                odb::query<FileInfoEntity>::fileId == fileInfo.getFileId(), existing)) {
            existing.setFileName(fileInfo.getFileName());
            existing.setFileSize(fileInfo.getFileSize());
            existing.setUploadTime(fileInfo.getUploadTime());
            existing.setFileExt(fileInfo.getFileExt());
            existing.setFdfsFileId(fileInfo.getFdfsFileId());
            existing.setUserId(fileInfo.getUserId());
            existing.setChatSessionId(fileInfo.getChatSessionId());
            _db->update(existing);
        } else {
            FileInfoEntity entity = fileInfo;
            _db->persist(entity);
        }
        trans.commit();
    } catch (const odb::exception& e) {
        ERR("保存文件信息失败, fileId: {}, 原因: {}", fileInfo.getFileId(), e.what());
        throw Chat2DataException(ErrorCode::FILE_INFO_SAVE_FAILED);
    }

    deleteFileInfoFromCache(fileInfo.getFileId());
    INF("文件信息保存成功, fileId: {}", fileInfo.getFileId());
    return true;
}

std::optional<FileInfoEntity> FileInfoData::getFileInfoByFileId(const std::string& fileId) {
    std::optional<FileInfoEntity> cached = getFileInfoFromCache(fileId);
    if (cached.has_value()) {
        return cached;
    }

    std::optional<FileInfoEntity> result;
    try {
        odb::transaction trans(_db->begin());
        FileInfoEntity entity;
        if (_db->query_one<FileInfoEntity>(
                odb::query<FileInfoEntity>::fileId == fileId, entity)) {
            result = entity;
        }
        trans.commit();
    } catch (const odb::exception& e) {
        ERR("查询文件信息失败, fileId: {}, 原因: {}", fileId, e.what());
        throw Chat2DataException(ErrorCode::FILE_QUERY_FAILED);
    }

    if (!result.has_value()) {
        WRN("文件信息不存在, fileId: {}", fileId);
        return std::nullopt;
    }

    saveFileInfoToCache(result.value());
    return result;
}

bool FileInfoData::deleteFileInfoByFileId(const std::string& fileId) {
    try {
        odb::transaction trans(_db->begin());
        _db->erase_query<FileInfoEntity>(
            odb::query<FileInfoEntity>::fileId == fileId);
        trans.commit();
    } catch (const odb::exception& e) {
        ERR("删除文件信息失败, fileId: {}, 原因: {}", fileId, e.what());
        throw Chat2DataException(ErrorCode::FILE_DELETE_FAILED);
    }

    deleteFileInfoFromCache(fileId);
    INF("文件信息删除成功, fileId: {}", fileId);
    return true;
}

std::vector<FileInfoEntity> FileInfoData::getFileInfoListByUserId(const std::string& userId) {
    std::vector<FileInfoEntity> fileList;
    try {
        odb::transaction trans(_db->begin());
        odb::result<FileInfoEntity> result = _db->query<FileInfoEntity>(
            odb::query<FileInfoEntity>::userId == userId);
        for (const auto& fileInfo : result) {
            fileList.push_back(fileInfo);
        }
        trans.commit();
    } catch (const odb::exception& e) {
        ERR("查询用户文件列表失败, userId: {}, 原因: {}", userId, e.what());
        throw Chat2DataException(ErrorCode::FILE_QUERY_FAILED);
    }

    std::sort(fileList.begin(), fileList.end(),
              [](const FileInfoEntity& left, const FileInfoEntity& right) {
                  return left.getUploadTime() > right.getUploadTime();
              });
    return fileList;
}

bool FileInfoData::saveFileInfoToCache(const FileInfoEntity& fileInfo) {
    try {
        Json::Value root;
        root[JSON_FIELD_FILE_ID] = fileInfo.getFileId();
        root[JSON_FIELD_FILE_NAME] = fileInfo.getFileName();
        root[JSON_FIELD_FILE_SIZE] = Json::UInt64(fileInfo.getFileSize());
        root[JSON_FIELD_UPLOAD_TIME] = Json::UInt64(fileInfo.getUploadTime());
        root[JSON_FIELD_FILE_EXT] = fileInfo.getFileExt();
        root[JSON_FIELD_FDFS_FILE_ID] = fileInfo.getFdfsFileId();
        root[JSON_FIELD_USER_ID] = fileInfo.getUserId();
        root[JSON_FIELD_CHAT_SESSION_ID] = fileInfo.getChatSessionId();

        std::optional<std::string> jsonStr = biteutil::JSON::serialize(root);
        if (!jsonStr.has_value()) {
            WRN("序列化文件信息缓存失败, fileId: {}", fileInfo.getFileId());
            return false;
        }

        _redis->setex(makeFileCacheKey(fileInfo.getFileId()),
                      std::chrono::seconds(FILE_CACHE_TTL_SECONDS), jsonStr.value());
        return true;
    } catch (const std::exception& e) {
        WRN("写入文件信息缓存失败, fileId: {}, 原因: {}", fileInfo.getFileId(), e.what());
        return false;
    }
}

std::optional<FileInfoEntity> FileInfoData::getFileInfoFromCache(const std::string& fileId) {
    const std::string key = makeFileCacheKey(fileId);
    std::optional<std::string> jsonStr;
    try {
        jsonStr = _redis->get(key);
    } catch (const std::exception& e) {
        WRN("读取文件信息缓存失败, key: {}, 原因: {}", key, e.what());
        return std::nullopt;
    }

    if (!jsonStr.has_value() || jsonStr->empty()) {
        return std::nullopt;
    }

    std::optional<Json::Value> root = biteutil::JSON::unserialize(jsonStr.value());
    if (!root.has_value() || !root->isObject()) {
        WRN("文件信息缓存反序列化失败, key: {}", key);
        return std::nullopt;
    }

    const Json::Value& value = root.value();
    if (!value.isMember(JSON_FIELD_FILE_ID) || !value.isMember(JSON_FIELD_FILE_NAME) ||
        !value.isMember(JSON_FIELD_FILE_SIZE) || !value.isMember(JSON_FIELD_UPLOAD_TIME) ||
        !value.isMember(JSON_FIELD_FILE_EXT) || !value.isMember(JSON_FIELD_FDFS_FILE_ID) ||
        !value.isMember(JSON_FIELD_USER_ID) || !value.isMember(JSON_FIELD_CHAT_SESSION_ID)) {
        WRN("文件信息缓存字段不完整, key: {}", key);
        return std::nullopt;
    }

    return FileInfoEntity(value[JSON_FIELD_FILE_ID].asString(),
                          value[JSON_FIELD_FILE_NAME].asString(),
                          value[JSON_FIELD_FILE_SIZE].asUInt64(),
                          value[JSON_FIELD_UPLOAD_TIME].asUInt64(),
                          value[JSON_FIELD_FILE_EXT].asString(),
                          value[JSON_FIELD_FDFS_FILE_ID].asString(),
                          value[JSON_FIELD_USER_ID].asString(),
                          value[JSON_FIELD_CHAT_SESSION_ID].asString());
}

void FileInfoData::deleteFileInfoFromCache(const std::string& fileId) {
    try {
        _redis->del(makeFileCacheKey(fileId));
    } catch (const std::exception& e) {
        WRN("删除文件信息缓存失败, fileId: {}, 原因: {}", fileId, e.what());
    }
}

} // namespace fileService
