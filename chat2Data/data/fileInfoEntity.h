#pragma once

#include <cstdint>
#include <string>

#include <odb/core.hxx>

namespace chat2Data {

/**
 * @brief 文件信息表tbl_fileInfo的ODB映射类
 * @note  fileId唯一标识一个文件；fdfsFileId用于FastDFS下载、删除；
 *        chatSessionId仅Excel文件使用，SQLite文件为空串
 */
#pragma db object table("tbl_fileInfo")
class FileInfoEntity {
public:
    FileInfoEntity() = default;

    /**
     * @brief 构造文件信息实体
     * @param fileId        文件ID
     * @param fileName      文件名称
     * @param fileSize      文件大小（字节）
     * @param uploadTime    上传时间（秒级时间戳）
     * @param fileExt       文件后缀
     * @param fdfsFileId    FastDFS文件ID
     * @param userId        用户ID
     * @param chatSessionId 聊天会话ID
     */
    FileInfoEntity(const std::string& fileId, const std::string& fileName, std::uint64_t fileSize,
                   std::uint64_t uploadTime, const std::string& fileExt, const std::string& fdfsFileId,
                   const std::string& userId, const std::string& chatSessionId);

    std::uint64_t getId() const;
    const std::string& getFileId() const;
    const std::string& getFileName() const;
    std::uint64_t getFileSize() const;
    std::uint64_t getUploadTime() const;
    const std::string& getFileExt() const;
    const std::string& getFdfsFileId() const;
    const std::string& getUserId() const;
    const std::string& getChatSessionId() const;

    void setFileName(const std::string& fileName);
    void setFileSize(std::uint64_t fileSize);
    void setUploadTime(std::uint64_t uploadTime);
    void setFileExt(const std::string& fileExt);
    void setFdfsFileId(const std::string& fdfsFileId);
    void setUserId(const std::string& userId);
    void setChatSessionId(const std::string& chatSessionId);

private:
    friend class odb::access;

    #pragma db id auto column("id") type("BIGINT UNSIGNED")
    std::uint64_t _id = 0;

    #pragma db unique column("fileId") type("VARCHAR(32) CHARACTER SET utf8mb4")
    std::string _fileId;

    #pragma db column("fileName") type("VARCHAR(64) CHARACTER SET utf8mb4")
    std::string _fileName;

    #pragma db column("fileSize") type("BIGINT UNSIGNED")
    std::uint64_t _fileSize = 0;

    #pragma db column("uploadTime") type("BIGINT UNSIGNED")
    std::uint64_t _uploadTime = 0;

    #pragma db column("fileExt") type("VARCHAR(8) CHARACTER SET utf8mb4")
    std::string _fileExt;

    #pragma db column("fdfsFileId") type("VARCHAR(64) CHARACTER SET utf8mb4")
    std::string _fdfsFileId;

    #pragma db column("userId") type("VARCHAR(32) CHARACTER SET utf8mb4")
    std::string _userId;

    #pragma db column("chatSessionId") type("VARCHAR(32) CHARACTER SET utf8mb4")
    std::string _chatSessionId;
};

} // namespace chat2Data
