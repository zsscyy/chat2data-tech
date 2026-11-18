#include "fileInfoEntity.h"

namespace chat2Data {

FileInfoEntity::FileInfoEntity(const std::string& fileId, const std::string& fileName, std::uint64_t fileSize,
                               std::uint64_t uploadTime, const std::string& fileExt,
                               const std::string& fdfsFileId, const std::string& userId,
                               const std::string& chatSessionId)
    : _fileId(fileId)
    , _fileName(fileName)
    , _fileSize(fileSize)
    , _uploadTime(uploadTime)
    , _fileExt(fileExt)
    , _fdfsFileId(fdfsFileId)
    , _userId(userId)
    , _chatSessionId(chatSessionId) {}

std::uint64_t FileInfoEntity::getId() const {
    return _id;
}

const std::string& FileInfoEntity::getFileId() const {
    return _fileId;
}

const std::string& FileInfoEntity::getFileName() const {
    return _fileName;
}

std::uint64_t FileInfoEntity::getFileSize() const {
    return _fileSize;
}

std::uint64_t FileInfoEntity::getUploadTime() const {
    return _uploadTime;
}

const std::string& FileInfoEntity::getFileExt() const {
    return _fileExt;
}

const std::string& FileInfoEntity::getFdfsFileId() const {
    return _fdfsFileId;
}

const std::string& FileInfoEntity::getUserId() const {
    return _userId;
}

const std::string& FileInfoEntity::getChatSessionId() const {
    return _chatSessionId;
}

void FileInfoEntity::setFileName(const std::string& fileName) {
    _fileName = fileName;
}

void FileInfoEntity::setFileSize(std::uint64_t fileSize) {
    _fileSize = fileSize;
}

void FileInfoEntity::setUploadTime(std::uint64_t uploadTime) {
    _uploadTime = uploadTime;
}

void FileInfoEntity::setFileExt(const std::string& fileExt) {
    _fileExt = fileExt;
}

void FileInfoEntity::setFdfsFileId(const std::string& fdfsFileId) {
    _fdfsFileId = fdfsFileId;
}

void FileInfoEntity::setUserId(const std::string& userId) {
    _userId = userId;
}

void FileInfoEntity::setChatSessionId(const std::string& chatSessionId) {
    _chatSessionId = chatSessionId;
}

} // namespace chat2Data
