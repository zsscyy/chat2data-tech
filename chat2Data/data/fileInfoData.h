#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "fileInfoEntity.h"

namespace odb {
class database;
} // namespace odb

namespace sw {
namespace redis {
class Redis;
} // namespace redis
} // namespace sw

namespace fileService {

/**
 * @brief 文件信息表tbl_fileInfo的数据访问层
 *
 * 采用 Cache-Aside 策略：
 *   - 写：写 MySQL 成功后删除 Redis 缓存
 *   - 读：先查 Redis，未命中再查 MySQL 并回填 Redis
 *   - 缓存 key：file:<fileId>，值为 JSON 字符串，TTL 3 天
 */
class FileInfoData {
public:
    using ptr = std::shared_ptr<FileInfoData>;

    /**
     * @brief 构造文件信息数据访问层
     * @param database ODB 数据库句柄
     * @param redis    Redis 客户端句柄
     */
    FileInfoData(std::shared_ptr<odb::database> database, std::shared_ptr<sw::redis::Redis> redis);

    /**
     * @brief 保存或更新文件信息
     * @param fileInfo 文件信息实体，按 fileId 判断是否已存在
     * @return 保存成功返回 true
     * @note 保存成功后会删除对应缓存；数据库异常抛出 FILE_INFO_SAVE_FAILED
     */
    bool saveFileInfo(const chat2Data::FileInfoEntity& fileInfo);

    /**
     * @brief 按文件ID查询文件信息
     * @param fileId 文件ID
     * @return 存在则返回文件信息，不存在返回 std::nullopt
     * @note 数据库异常抛出 FILE_QUERY_FAILED
     */
    std::optional<chat2Data::FileInfoEntity> getFileInfoByFileId(const std::string& fileId);

    /**
     * @brief 按文件ID删除文件信息
     * @param fileId 文件ID
     * @return 删除成功返回 true
     * @note 数据库异常抛出 FILE_DELETE_FAILED
     */
    bool deleteFileInfoByFileId(const std::string& fileId);

    /**
     * @brief 获取指定用户上传的全部文件信息
     * @param userId 用户ID
     * @return 文件信息列表，按上传时间倒序
     */
    std::vector<chat2Data::FileInfoEntity> getFileInfoListByUserId(const std::string& userId);

private:
    bool saveFileInfoToCache(const chat2Data::FileInfoEntity& fileInfo);
    std::optional<chat2Data::FileInfoEntity> getFileInfoFromCache(const std::string& fileId);
    void deleteFileInfoFromCache(const std::string& fileId);

    std::shared_ptr<odb::database> _db;
    std::shared_ptr<sw::redis::Redis> _redis;
};

} // namespace fileService
