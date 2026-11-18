#pragma once

#include <cstdint>
#include <string>

#include <odb/core.hxx>

namespace chat2Data {

/**
 * @brief worksheet与数据库表映射表tbl_worksheet的ODB映射类
 * @note  fileId建立索引；数据库级外键约束通过data/sql下的ALTER TABLE脚本维护，
 *        ODB实体本身不声明关系（保留tbl_fileInfo的自增主键设计）
 */
#pragma db object table("tbl_worksheet")
class WorksheetEntity {
public:
    WorksheetEntity() = default;

    /**
     * @brief 构造worksheet映射实体
     * @param fileId        文件ID
     * @param worksheetName worksheet名称
     * @param tableName   worksheet在数据库中对应的真实表名
     */
    WorksheetEntity(const std::string& fileId, const std::string& worksheetName,
                    const std::string& tableName);

    std::uint64_t getId() const;
    const std::string& getFileId() const;
    const std::string& getWorksheetName() const;
    const std::string& getTableName() const;

    void setFileId(const std::string& fileId);
    void setWorksheetName(const std::string& worksheetName);
    void setTableName(const std::string& tableName);

private:
    friend class odb::access;

    #pragma db id auto column("id") type("BIGINT UNSIGNED")
    std::uint64_t _id = 0;

    #pragma db index column("fileId") type("VARCHAR(32) CHARACTER SET utf8mb4")
    std::string _fileId;

    #pragma db column("worksheetName") type("VARCHAR(32) CHARACTER SET utf8mb4")
    std::string _worksheetName;

    #pragma db column("tableName") type("VARCHAR(64) CHARACTER SET utf8mb4")
    std::string _tableName;
};

} // namespace chat2Data
