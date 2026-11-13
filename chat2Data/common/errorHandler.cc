#include "errorHandler.h"
#include <unordered_map>

namespace chat2Data {

namespace {
// 操作成功错误码
constexpr int SUCCESS_CODE_VALUE = 0;
// 每个子服务错误码段的长度
constexpr int SERVICE_CODE_RANGE_SIZE = 100;
// 各子服务错误码段起始值
constexpr int USER_SERVICE_CODE_BASE = 100;
constexpr int FILE_SERVICE_CODE_BASE = 200;
constexpr int DB_SERVICE_CODE_BASE = 300;
constexpr int EXCEL_SERVICE_CODE_BASE = 400;
constexpr int NOTIFY_SERVICE_CODE_BASE = 500;
constexpr int AI_SERVICE_CODE_BASE = 600;
constexpr int GATEWAY_SERVICE_CODE_BASE = 700;
} // namespace

std::string error2String(ErrorCode code) {
    static std::unordered_map<ErrorCode, std::string> errorMap = {
        {ErrorCode::SUCCESS, "操作成功"},
        
        // 用户子服务错误码描述符
        {ErrorCode::USER_PASSWORD_ENCRYPT_ERROR, "密码加密失败"},
        {ErrorCode::USER_PASSWORD_INVALID, "密码无效"},
        {ErrorCode::USER_NICKNAME_EMPTY, "昵称不能为空"},
        {ErrorCode::USER_SESSION_CREATE_ERROR, "会话创建失败"},
        {ErrorCode::USER_SESSION_CACHE_ERROR, "会话缓存保存失败"},
        {ErrorCode::USER_SESSION_DELETE_ERROR, "会话删除失败"},
        {ErrorCode::USER_SESSION_INVALID, "会话无效或已过期"},
        {ErrorCode::USER_SAVE_VERIFY_CODE_FAILED, "保存验证码失败"},
        {ErrorCode::USER_VERIFY_CODE_EXPIRED, "验证码已过期"},
        {ErrorCode::USER_VERIFY_CODE_ERROR, "验证码错误"},
        {ErrorCode::USER_LOGIN_FAILED, "登录失败"},
        {ErrorCode::USER_SESSION_LOGIN_FAILED, "会话登录失败"},
        {ErrorCode::USER_EMAIL_LOGIN_PARAM_ERROR, "邮箱登录参数错误，邮箱、验证码、验证码id为空"},
        {ErrorCode::USER_SAVE_USER_INFO_FAILED, "保存用户信息失败"},
        {ErrorCode::USER_NOT_FOUND, "用户不存在"},
        {ErrorCode::USER_NICKNAME_EXIST, "昵称已存在"},
        {ErrorCode::USER_LOGOUT_FAILED, "退出登录失败"},
        {ErrorCode::USER_EMAIL_EMPTY, "邮箱不能为空"},
        {ErrorCode::USER_EMAIL_EXIST, "邮箱已存在"},
        {ErrorCode::USER_REGISTER_PARAM_ERROR, "注册参数错误，昵称或邮箱不能为空或已存在，密码长度必须大于等于6位，且不能包含空格"},
        {ErrorCode::USER_GET_USER_INFO_PARAM_ERROR, "获取用户信息参数错误，用户Id不能为空或已过期"},
        {ErrorCode::USER_UNKNOWN_ERROR, "用户子服务未知错误"},
        {ErrorCode::USER_CACHE_ERROR, "用户缓存操作失败"},
        
        // 文件子服务错误码描述符
        {ErrorCode::FILE_PARAM_INVALID, "参数无效"},
        {ErrorCode::FILE_NOT_FOUND, "文件不存在"},
        {ErrorCode::FILE_UPLOAD_FASTDFS_FAILED, "文件上传FastDFS失败"},
        {ErrorCode::FILE_SAVE_LOCAL_FAILED, "文件保存本地失败"},
        {ErrorCode::FILE_DELETE_FAILED, "文件删除失败"},
        {ErrorCode::FILE_INFO_SAVE_FAILED, "文件信息保存失败"},
        {ErrorCode::FILE_EXCEL_PARSE_FAILED, "Excel文件解析失败"},
        {ErrorCode::FILE_SQLITE_UPLOAD_FAILED, "SQLite文件上传失败"},
        {ErrorCode::FILE_PERMISSION_DENIED, "文件权限被拒绝"},
        {ErrorCode::FILE_WORKSHEET_SAVE_FAILED, "Worksheet信息保存失败"},
        {ErrorCode::FILE_UPLOAD_EXCEED_MAX_SIZE, "文件大小超过限制"},
        {ErrorCode::FILE_SESSION_INVALID, "会话无效或已过期"},
        {ErrorCode::FILE_FILEID_EMPTY, "文件ID为空"},
        {ErrorCode::FILE_SQLITE_FILE_NOT_FOUND, "SQLite文件不存在"},
        {ErrorCode::FILE_EXCEL_WORKSHEETS_GET_FAILED, "获取Excel工作表列表失败"},
        {ErrorCode::FILE_STORAGE_SERVICE_FAILED, "存储子服务调用失败"},
        
        // 数据库子服务错误码描述符
        {ErrorCode::DB_PARAM_INVALID, "参数无效"},
        {ErrorCode::DB_CONNECTION_FAILED, "数据库连接失败"},
        {ErrorCode::DB_CONNECTION_NOT_EXISTS, "数据库连接不存在"},
        {ErrorCode::DB_CONNECTION_INVALID, "数据库连接无效"},
        {ErrorCode::DB_SQL_EXECUTE_FAILED, "SQL语句执行失败"},
        {ErrorCode::DB_TABLE_NOT_FOUND, "表不存在"},
        {ErrorCode::DB_TEMP_TABLE_CREATE_FAILED, "临时表创建失败"},
        {ErrorCode::DB_BACKUP_TABLE_FAILED, "备份表失败"},
        {ErrorCode::DB_SQLITE_DOWNLOAD_FAILED, "SQLite文件下载失败"},
        {ErrorCode::DB_TEMP_TABLE_NOT_FOUND, "临时表不存在"},
        {ErrorCode::DB_TEMP_TABLE_DELETE_FAILED, "临时表删除失败"},
        {ErrorCode::DB_SAMPLE_DATA_GET_FAILED, "获取采样数据失败"},
        {ErrorCode::DB_TABLE_DATA_GET_FAILED, "获取表数据失败"},
        {ErrorCode::DB_TABLE_STRUCT_GET_FAILED, "获取表结构失败"},
        {ErrorCode::DB_IMPORT_DATA_FAILED, "导入数据失败"},
        {ErrorCode::DB_DROP_TABLE_FAILED, "删除表失败"},
        {ErrorCode::DB_USER_ALL_CONN_DELETE_FAILED, "删除用户所有连接失败"},

        // Excel解析子服务错误码描述符
        {ErrorCode::EXCEL_FILE_PATH_INVALID, "文件路径无效"},
        {ErrorCode::EXCEL_FILE_OPEN_FAILED, "文件打开失败"},
        {ErrorCode::EXCEL_WORKSHEET_NOT_FOUND, "工作表不存在"},
        {ErrorCode::EXCEL_PARSE_FAILED, "Excel解析失败"},
        {ErrorCode::EXCEL_DATA_INVALID, "Excel数据无效"},
        
        // 通知子服务错误码描述符
        {ErrorCode::NOTIFY_PARAM_EMPTY, "参数不能为空"},
        {ErrorCode::NOTIFY_SEND_FAILED, "邮件发送失败"},
        {ErrorCode::NOTIFY_EMAIL_INVALID, "邮箱地址无效"},
        {ErrorCode::NOTIFY_CODE_INVALID, "验证码无效"},
        {ErrorCode::NOTIFY_SUBJECT_INVALID, "邮件主题无效"},
        
        // AI子服务错误码描述符
        {ErrorCode::AI_PARAM_INVALID, "参数无效"},
        {ErrorCode::AI_SESSION_CREATE_ERROR, "会话创建失败"},
        {ErrorCode::AI_SESSION_SAVE_ERROR, "会话保存失败"},
        {ErrorCode::AI_SESSION_NOT_FOUND, "会话不存在"},
        {ErrorCode::AI_SESSION_PERMISSION_DENIED, "会话权限被拒绝"},
        {ErrorCode::AI_GET_MODELS_FAILED, "获取模型列表失败"},
        {ErrorCode::AI_SEND_MESSAGE_FAILED, "发送消息失败"},
        {ErrorCode::AI_SEND_MESSAGE_NOT_IMPLEMENTED, "发送消息功能未实现"},
        {ErrorCode::AI_MODEL_NOT_AVAILABLE, "模型不可用"},
        {ErrorCode::AI_UPDATE_SESSION_FILE_FAILED, "更新Excel文件和聊天会话关联失败"},
        {ErrorCode::AI_CHATSESSION_NOT_OWNED_BY_USER, "聊天会话不属于当前用户"}
        
        // 网关子服务错误码描述符
        // {ErrorCode::GATEWAY_CONNECTION_FAILED, "网关连接失败"},
    };
    
    auto it = errorMap.find(code);
    if (it != errorMap.end()) {
        return it->second;
    }
    return "未知错误";
}

std::string getServiceName(ErrorCode code) {
    int codeValue = static_cast<int>(code);
    
    if (codeValue == SUCCESS_CODE_VALUE) {
        return "请求操作成功";
    }
    
    if (codeValue >= USER_SERVICE_CODE_BASE && codeValue < USER_SERVICE_CODE_BASE + SERVICE_CODE_RANGE_SIZE) {
        return "userService";
    } else if (codeValue >= FILE_SERVICE_CODE_BASE &&
               codeValue < FILE_SERVICE_CODE_BASE + SERVICE_CODE_RANGE_SIZE) {
        return "fileService";
    } else if (codeValue >= DB_SERVICE_CODE_BASE && codeValue < DB_SERVICE_CODE_BASE + SERVICE_CODE_RANGE_SIZE) {
        return "dbService";
    } else if (codeValue >= EXCEL_SERVICE_CODE_BASE &&
               codeValue < EXCEL_SERVICE_CODE_BASE + SERVICE_CODE_RANGE_SIZE) {
        return "excelService";
    } else if (codeValue >= NOTIFY_SERVICE_CODE_BASE &&
               codeValue < NOTIFY_SERVICE_CODE_BASE + SERVICE_CODE_RANGE_SIZE) {
        return "notifyService";
    } else if (codeValue >= AI_SERVICE_CODE_BASE && codeValue < AI_SERVICE_CODE_BASE + SERVICE_CODE_RANGE_SIZE) {
        return "aiService";
    } else if (codeValue >= GATEWAY_SERVICE_CODE_BASE &&
               codeValue < GATEWAY_SERVICE_CODE_BASE + SERVICE_CODE_RANGE_SIZE) {
        return "gatewayService";
    }
    
    return "未知服务";
}

Chat2DataException::Chat2DataException(ErrorCode code)
    : _errorCode(code), _errorMessage(error2String(code)) {
}

const char* Chat2DataException::what() const noexcept {
    static std::string fullMessage;
    fullMessage = "[" + getServiceName(_errorCode) + "]: " + _errorMessage;
    return fullMessage.c_str();
}

ErrorCode Chat2DataException::getErrorCode() const {
    return _errorCode;
}

} // namespace chat2Data
