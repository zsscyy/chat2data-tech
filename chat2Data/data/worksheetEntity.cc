#include "worksheetEntity.h"

namespace chat2Data {

WorksheetEntity::WorksheetEntity(const std::string& fileId, const std::string& worksheetName,
                                 const std::string& tableName)
    : _fileId(fileId), _worksheetName(worksheetName), _tableName(tableName) {}

std::uint64_t WorksheetEntity::getId() const {
    return _id;
}

const std::string& WorksheetEntity::getFileId() const {
    return _fileId;
}

const std::string& WorksheetEntity::getWorksheetName() const {
    return _worksheetName;
}

const std::string& WorksheetEntity::getTableName() const {
    return _tableName;
}

void WorksheetEntity::setFileId(const std::string& fileId) {
    _fileId = fileId;
}

void WorksheetEntity::setWorksheetName(const std::string& worksheetName) {
    _worksheetName = worksheetName;
}

void WorksheetEntity::setTableName(const std::string& tableName) {
    _tableName = tableName;
}

} // namespace chat2Data
