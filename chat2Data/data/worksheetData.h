#pragma once

#include <memory>
#include <string>
#include <vector>

#include "worksheetEntity.h"

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
 * @brief worksheet映射表tbl_worksheet的数据访问层
 *
 * 采用 Cache-Aside 策略：
 *   - 写：写 MySQL 成功后删除 Redis 缓存
 *   - 读：先查 Redis，未命中再查 MySQL 并回填 Redis
 *   - 缓存 key：worksheet:<fileId>，值为 JSON 字符串，TTL 3 天
 */
class WorksheetData {
public:
    using ptr = std::shared_ptr<WorksheetData>;

    /**
     * @brief 构造worksheet映射数据访问层
     * @param database ODB 数据库句柄
     * @param redis    Redis 客户端句柄
     */
    WorksheetData(std::shared_ptr<odb::database> database, std::shared_ptr<sw::redis::Redis> redis);

    /**
     * @brief 保存文件对应的全部worksheet映射
     * @param fileId     文件ID
     * @param worksheets worksheet映射列表
     * @return 保存成功返回 true
     * @note 同一事务内先删除该文件的旧映射再插入，保证重复上传时幂等；
     *       数据库异常抛出 FILE_WORKSHEET_SAVE_FAILED
     */
    bool saveWorksheetMappings(const std::string& fileId,
                              const std::vector<chat2Data::WorksheetEntity>& worksheets);

    /**
     * @brief 按文件ID查询worksheet映射
     * @param fileId 文件ID
     * @return worksheet映射列表，不存在返回空列表
     */
    std::vector<chat2Data::WorksheetEntity> getWorksheetMappingsByFileId(const std::string& fileId);

    /**
     * @brief 按文件ID删除worksheet映射
     * @param fileId 文件ID
     * @return 删除成功返回 true
     */
    bool deleteWorksheetMappingsByFileId(const std::string& fileId);

private:
    bool saveWorksheetMappingsToCache(const std::string& fileId,
                                      const std::vector<chat2Data::WorksheetEntity>& worksheets);
    std::vector<chat2Data::WorksheetEntity> getWorksheetMappingsFromCache(const std::string& fileId);
    void deleteWorksheetMappingsFromCache(const std::string& fileId);

    std::shared_ptr<odb::database> _db;
    std::shared_ptr<sw::redis::Redis> _redis;
};

} // namespace fileService
