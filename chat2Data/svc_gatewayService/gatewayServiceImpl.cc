#include <limits.h>
#include <unistd.h>

#include <chrono>
#include <thread>
#include <utility>

#include <gflags/gflags.h>
#include <jsoncpp/json/json.h>
#include <log.h>
#include <util.h>

#include "errorHandler.h"
#include "fileService.pb.h"
#include "gatewayServiceImpl.h"
#include "userService.pb.h"

// 声明main.cc中定义的gflags变量，用于获取用户子服务名称
DECLARE_string(userServiceName);
// 声明main.cc中定义的gflags变量，用于获取文件子服务名称
DECLARE_string(fileServiceName);

namespace GatewayService {

namespace {
// HTTP成功状态码
constexpr int HTTP_STATUS_OK = 200;
// HTTP请求参数错误状态码
constexpr int HTTP_STATUS_BAD_REQUEST = 400;
// HTTP服务内部错误状态码
constexpr int HTTP_STATUS_INTERNAL_SERVER_ERROR = 500;
// 未实现接口的HTTP状态码
constexpr int HTTP_STATUS_NOT_IMPLEMENTED = 501;

// 业务错误码：请求参数错误
constexpr int ERROR_CODE_PARAM_INVALID = 400;
// 业务错误码：会话无效
constexpr int ERROR_CODE_SESSION_INVALID = 401;
// 业务错误码：服务内部错误
constexpr int ERROR_CODE_INTERNAL_ERROR = 500;
// 业务错误码：后端服务不可用
constexpr int ERROR_CODE_SERVICE_UNAVAILABLE = 503;

// JSON响应内容类型
const char* const JSON_CONTENT_TYPE = "application/json";
// 请求体解析失败提示
const char* const INVALID_BODY_MESSAGE = "请求体格式错误";
// 缺少必要参数提示
const char* const MISSING_PARAM_MESSAGE = "缺少必要参数";
// 后端服务不可用提示
const char* const SERVICE_UNAVAILABLE_MESSAGE = "后端服务不可用";
// 会话无效提示
const char* const SESSION_INVALID_MESSAGE = "会话无效或已过期";
// 服务内部错误提示
const char* const INTERNAL_ERROR_MESSAGE = "服务内部错误";
// 未实现接口的错误提示
const char* const NOT_IMPLEMENTED_MESSAGE = "接口暂未实现";
// 未实现接口的兜底JSON响应
const char* const NOT_IMPLEMENTED_RESPONSE_JSON = R"({"errorMsg":"接口暂未实现"})";
// 服务内部错误的兜底JSON响应
const char* const INTERNAL_ERROR_RESPONSE_JSON = R"({"requestId":"","errorCode":500,"errorMsg":"服务内部错误"})";
// 前端静态资源目录名称
const char* const STATIC_RESOURCE_DIR_NAME = "www";
// 服务刚启动时 etcd 节点同步有短暂延迟，获取信道允许短重试
constexpr int CHANNEL_RETRY_TIMES = 3;
constexpr int CHANNEL_RETRY_INTERVAL_MS = 100;
} // namespace

GatewayServiceImpl::GatewayServiceImpl(biterpc::SvcChannels::ptr svcChannels)
    : _svcChannels(std::move(svcChannels)) {
    if (!_svcChannels) {
        ERR("GatewayServiceImpl constructed with empty SvcChannels!");
        return;
    }

    // 静态资源目录与可执行程序同级的www目录，即可执行程序位于build目录时对应build/www
    char executablePath[PATH_MAX] = {0};
    const ssize_t pathLength = readlink("/proc/self/exe", executablePath, sizeof(executablePath) - 1);
    if (pathLength > 0) {
        std::string executableDir(executablePath, static_cast<std::size_t>(pathLength));
        const auto slashPos = executableDir.find_last_of('/');
        if (slashPos != std::string::npos) {
            executableDir = executableDir.substr(0, slashPos);
        }
        _staticResourcesPath = executableDir + "/" + STATIC_RESOURCE_DIR_NAME;
    } else {
        _staticResourcesPath = std::string("./") + STATIC_RESOURCE_DIR_NAME;
        WRN("Failed to get executable path, use default static resources path: {}", _staticResourcesPath);
    }

    INF("GatewayServiceImpl initialized successfully, static resources path: {}", _staticResourcesPath);
}

void GatewayServiceImpl::bindRoutes(httplib::Server& server) {
    // 1. 绑定路由前设置前端静态资源目录，将www目录下的文件映射到根路径/
    if (!server.set_mount_point("/", _staticResourcesPath)) {
        WRN("Failed to set static resources mount point, path: {}", _staticResourcesPath);
    } else {
        INF("Static resources mounted: / -> {}", _staticResourcesPath);
    }

    // 2. 注册健康检测接口
    server.Get("/health", [this](const httplib::Request& req, httplib::Response& res) {
        handleHealthCheck(req, res);
    });
    INF("Gateway HTTP route bound: GET /health");

    // 3. 注册用户子服务9个业务接口
    server.Post("/api/user/valid/nickname", [this](const httplib::Request& req, httplib::Response& res) {
        handleValidNickname(req, res);
    });
    server.Post("/api/user/valid/email", [this](const httplib::Request& req, httplib::Response& res) {
        handleValidEmail(req, res);
    });
    server.Post("/api/user/register", [this](const httplib::Request& req, httplib::Response& res) {
        handleUserRegister(req, res);
    });
    server.Post("/api/user/passwd/login", [this](const httplib::Request& req, httplib::Response& res) {
        handlePasswordLogin(req, res);
    });
    server.Post("/api/user/code", [this](const httplib::Request& req, httplib::Response& res) {
        handleGetVerifyCode(req, res);
    });
    server.Post("/api/user/vcode/login", [this](const httplib::Request& req, httplib::Response& res) {
        handleVerifyCodeLogin(req, res);
    });
    server.Post("/api/user/session/login", [this](const httplib::Request& req, httplib::Response& res) {
        handleSessionLogin(req, res);
    });
    server.Post("/api/user/logout", [this](const httplib::Request& req, httplib::Response& res) {
        handleLogout(req, res);
    });
    // 接口文档定义请求方法为POST且参数位于URL中，同时兼容前端使用GET查询用户信息
    server.Post("/api/user/info", [this](const httplib::Request& req, httplib::Response& res) {
        handleGetUserInfo(req, res);
    });
    server.Get("/api/user/info", [this](const httplib::Request& req, httplib::Response& res) {
        handleGetUserInfo(req, res);
    });
    INF("Gateway HTTP routes bound: 9 user service interfaces");

    // 4. 注册文件子服务9个业务接口
    server.Post("/api/file/upload/info", [this](const httplib::Request& req, httplib::Response& res) {
        handleUploadFileInfo(req, res);
    });
    server.Get("/api/file/info", [this](const httplib::Request& req, httplib::Response& res) {
        handleGetFileInfo(req, res);
    });
    server.Post("/api/file/upload", [this](const httplib::Request& req, httplib::Response& res) {
        handleUploadFile(req, res);
    });
    server.Get("/api/file/download", [this](const httplib::Request& req, httplib::Response& res) {
        handleDownloadFile(req, res);
    });
    // 文件ID为路径参数，使用正则捕获组提取
    server.Delete(R"(/api/file/([\w-]+))", [this](const httplib::Request& req, httplib::Response& res) {
        handleDeleteFile(req, res);
    });
    server.Post("/api/file/preview", [this](const httplib::Request& req, httplib::Response& res) {
        handlePreviewExcel(req, res);
    });
    server.Post("/api/file/list", [this](const httplib::Request& req, httplib::Response& res) {
        handleGetFileList(req, res);
    });
    server.Post("/api/file/chat/map", [this](const httplib::Request& req, httplib::Response& res) {
        handleMapFileChatSession(req, res);
    });
    server.Post("/api/file/sqlite/upload", [this](const httplib::Request& req, httplib::Response& res) {
        handleUploadSqliteFile(req, res);
    });
    INF("Gateway HTTP routes bound: 9 file service interfaces");

    // 其余12个业务接口待实现后再注册，路由清单如下：
    // 数据库: POST /api/db/connect、/disconnect、/table/data、/connection/status
    //         GET /api/db/tables
    // AI: POST /api/ai/models、/session/create、/chatSessionLists、/history、/delete、/sendStreamMessage
    WRN("12 business interfaces are not implemented yet, routes are not bound");
}

biterpc::ChannelPtr GatewayServiceImpl::getChannel(const std::string& serviceName) {
    if (!_svcChannels) {
        ERR("SvcChannels is null, cannot get channel for service: {}", serviceName);
        return nullptr;
    }

    // 服务刚重启时，etcd 节点可能还未同步到 SvcChannels，
    // 这里做短重试，避免一次瞬时不可用直接返回 503。
    biterpc::ChannelPtr channel;
    for (int attempt = 0; attempt < CHANNEL_RETRY_TIMES; ++attempt) {
        channel = _svcChannels->getNode(serviceName);
        if (channel) {
            return channel;
        }
        if (attempt + 1 < CHANNEL_RETRY_TIMES) {
            std::this_thread::sleep_for(std::chrono::milliseconds(CHANNEL_RETRY_INTERVAL_MS));
        }
    }

    WRN("No available node for service: {}", serviceName);
    return nullptr;
}

std::optional<std::string> GatewayServiceImpl::getNodeAddr(const std::string& serviceName) {
    if (!_svcChannels) {
        ERR("SvcChannels is null, cannot get node address for service: {}", serviceName);
        return std::nullopt;
    }

    return _svcChannels->getNodeAddr(serviceName);
}

void GatewayServiceImpl::sendJsonResponse(httplib::Response& res, const std::string& jsonData, int statusCode) {
    res.status = statusCode;
    res.set_content(jsonData, JSON_CONTENT_TYPE);
}

void GatewayServiceImpl::sendErrorResponse(httplib::Response& res, const std::string& requestId, int errorCode,
                                           const std::string& errorMsg, int statusCode) {
    Json::Value response;
    response["requestId"] = requestId;
    response["errorCode"] = errorCode;
    response["errorMsg"] = errorMsg;

    auto jsonStr = biteutil::JSON::serialize(response);
    if (!jsonStr) {
        ERR("Failed to serialize error response, requestId: {}, errorCode: {}", requestId, errorCode);
        sendJsonResponse(res, INTERNAL_ERROR_RESPONSE_JSON, HTTP_STATUS_INTERNAL_SERVER_ERROR);
        return;
    }
    sendJsonResponse(res, jsonStr.value(), statusCode);
}

bool GatewayServiceImpl::validateRequiredFields(
    const std::string& requestId, const std::vector<std::pair<std::string, std::string>>& fields,
    httplib::Response& res) {
    for (const auto& field : fields) {
        if (field.second.empty()) {
            WRN("Missing required field: {}, requestId: {}", field.first, requestId);
            sendErrorResponse(res, requestId, ERROR_CODE_PARAM_INVALID, "缺少必要参数: " + field.first,
                              HTTP_STATUS_BAD_REQUEST);
            return false;
        }
    }
    return true;
}

bool GatewayServiceImpl::validateSession(const std::string& requestId, const std::string& sessionId,
                                         httplib::Response& res, std::string& userId) {
    // 1. 获取用户服务信道
    auto channel = getChannel(FLAGS_userServiceName);
    if (!channel) {
        sendErrorResponse(res, requestId, ERROR_CODE_SERVICE_UNAVAILABLE, SERVICE_UNAVAILABLE_MESSAGE);
        return false;
    }

    // 2. 构建RPC请求并创建RPC客户端
    chat2Data::userService::IsSessionValidRequest request;
    request.set_request_id(requestId);
    request.set_session_id(sessionId);

    chat2Data::userService::IsSessionValidResponse response;
    brpc::Controller controller;
    chat2Data::userService::UserService_Stub stub(channel.get());

    // 3. 调用用户子服务会话有效性检测接口
    stub.IsSessionValid(&controller, &request, &response, nullptr);
    if (controller.Failed()) {
        ERR("IsSessionValid RPC failed, requestId: {}, reason: {}", requestId, controller.ErrorText());
        sendErrorResponse(res, requestId, ERROR_CODE_SERVICE_UNAVAILABLE, SERVICE_UNAVAILABLE_MESSAGE);
        return false;
    }
    // 用户服务在会话不存在时返回 USER_SESSION_INVALID(105)，
    // 网关统一转换为 401，避免同一“会话无效”出现两套错误码。
    if (response.error_code() ==
        static_cast<int>(chat2Data::ErrorCode::USER_SESSION_INVALID)) {
        sendErrorResponse(res, response.request_id(), ERROR_CODE_SESSION_INVALID, SESSION_INVALID_MESSAGE);
        return false;
    }
    if (response.error_code() != 0) {
        sendErrorResponse(res, response.request_id(), response.error_code(), response.error_msg());
        return false;
    }
    if (!response.is_valid()) {
        WRN("Session is invalid, requestId: {}, sessionId: {}", requestId, sessionId);
        sendErrorResponse(res, requestId, ERROR_CODE_SESSION_INVALID, SESSION_INVALID_MESSAGE);
        return false;
    }

    // 4. 通过引用参数带出鉴权得到的用户ID
    userId = response.user_id();
    return true;
}

void GatewayServiceImpl::handleHealthCheck(const httplib::Request& req, httplib::Response& res) {
    // 服务器能够处理该请求即说明服务健康
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

    Json::Value response;
    response["status"] = "healthy";
    response["service"] = "GatewayService";
    response["timestamp"] = Json::Int64(timestamp);

    auto jsonStr = biteutil::JSON::serialize(response);
    if (!jsonStr.has_value()) {
        ERR("Failed to serialize health check response");
        res.status = HTTP_STATUS_INTERNAL_SERVER_ERROR;
        res.set_content(R"({"status":"unhealthy"})", JSON_CONTENT_TYPE);
        return;
    }

    res.status = HTTP_STATUS_OK;
    res.set_content(jsonStr.value(), JSON_CONTENT_TYPE);
    DBG("Health check request handled, path: {}", req.path);
}

void GatewayServiceImpl::handleValidNickname(const httplib::Request& req, httplib::Response& res) {
    // 1. 解析HTTP请求参数
    auto requestJson = biteutil::JSON::unserialize(req.body);
    if (!requestJson) {
        ERR("Failed to parse valid nickname request body");
        sendErrorResponse(res, "", ERROR_CODE_PARAM_INVALID, INVALID_BODY_MESSAGE, HTTP_STATUS_BAD_REQUEST);
        return;
    }
    const std::string requestId = requestJson->get("requestId", "").asString();
    const std::string nickname = requestJson->get("nickname", "").asString();
    if (!validateRequiredFields(requestId, {{"nickname", nickname}}, res)) {
        return;
    }

    // 2. 调用用户子服务检测昵称是否唯一
    auto channel = getChannel(FLAGS_userServiceName);
    if (!channel) {
        sendErrorResponse(res, requestId, ERROR_CODE_SERVICE_UNAVAILABLE, SERVICE_UNAVAILABLE_MESSAGE);
        return;
    }

    chat2Data::userService::ValidNicknameRequest rpcRequest;
    rpcRequest.set_request_id(requestId);
    rpcRequest.set_nickname(nickname);

    chat2Data::userService::ValidNicknameResponse rpcResponse;
    brpc::Controller controller;
    chat2Data::userService::UserService_Stub stub(channel.get());
    stub.ValidNickname(&controller, &rpcRequest, &rpcResponse, nullptr);

    if (controller.Failed()) {
        ERR("ValidNickname RPC failed, requestId: {}, reason: {}", requestId, controller.ErrorText());
        sendErrorResponse(res, requestId, ERROR_CODE_SERVICE_UNAVAILABLE, SERVICE_UNAVAILABLE_MESSAGE);
        return;
    }
    if (rpcResponse.error_code() != 0) {
        sendErrorResponse(res, rpcResponse.request_id(), rpcResponse.error_code(), rpcResponse.error_msg());
        return;
    }

    // 3. 解析RPC响应并构建HTTP响应体
    Json::Value responseJson;
    responseJson["requestId"] = rpcResponse.request_id();
    responseJson["errorCode"] = rpcResponse.error_code();
    responseJson["errorMsg"] = rpcResponse.error_msg();

    auto jsonStr = biteutil::JSON::serialize(responseJson);
    if (!jsonStr) {
        ERR("Failed to serialize valid nickname response, requestId: {}", requestId);
        sendErrorResponse(res, requestId, ERROR_CODE_INTERNAL_ERROR, INTERNAL_ERROR_MESSAGE,
                          HTTP_STATUS_INTERNAL_SERVER_ERROR);
        return;
    }
    sendJsonResponse(res, jsonStr.value());
    INF("Valid nickname request handled, requestId: {}", requestId);
}

void GatewayServiceImpl::handleValidEmail(const httplib::Request& req, httplib::Response& res) {
    // 1. 解析HTTP请求参数
    auto requestJson = biteutil::JSON::unserialize(req.body);
    if (!requestJson) {
        ERR("Failed to parse valid email request body");
        sendErrorResponse(res, "", ERROR_CODE_PARAM_INVALID, INVALID_BODY_MESSAGE, HTTP_STATUS_BAD_REQUEST);
        return;
    }
    const std::string requestId = requestJson->get("requestId", "").asString();
    const std::string email = requestJson->get("email", "").asString();
    if (!validateRequiredFields(requestId, {{"email", email}}, res)) {
        return;
    }

    // 2. 调用用户子服务检测邮箱是否唯一
    auto channel = getChannel(FLAGS_userServiceName);
    if (!channel) {
        sendErrorResponse(res, requestId, ERROR_CODE_SERVICE_UNAVAILABLE, SERVICE_UNAVAILABLE_MESSAGE);
        return;
    }

    chat2Data::userService::ValidEmailRequest rpcRequest;
    rpcRequest.set_request_id(requestId);
    rpcRequest.set_email(email);

    chat2Data::userService::ValidEmailResponse rpcResponse;
    brpc::Controller controller;
    chat2Data::userService::UserService_Stub stub(channel.get());
    stub.ValidEmail(&controller, &rpcRequest, &rpcResponse, nullptr);

    if (controller.Failed()) {
        ERR("ValidEmail RPC failed, requestId: {}, reason: {}", requestId, controller.ErrorText());
        sendErrorResponse(res, requestId, ERROR_CODE_SERVICE_UNAVAILABLE, SERVICE_UNAVAILABLE_MESSAGE);
        return;
    }
    if (rpcResponse.error_code() != 0) {
        sendErrorResponse(res, rpcResponse.request_id(), rpcResponse.error_code(), rpcResponse.error_msg());
        return;
    }

    // 3. 解析RPC响应并构建HTTP响应体
    Json::Value responseJson;
    responseJson["requestId"] = rpcResponse.request_id();
    responseJson["errorCode"] = rpcResponse.error_code();
    responseJson["errorMsg"] = rpcResponse.error_msg();

    auto jsonStr = biteutil::JSON::serialize(responseJson);
    if (!jsonStr) {
        ERR("Failed to serialize valid email response, requestId: {}", requestId);
        sendErrorResponse(res, requestId, ERROR_CODE_INTERNAL_ERROR, INTERNAL_ERROR_MESSAGE,
                          HTTP_STATUS_INTERNAL_SERVER_ERROR);
        return;
    }
    sendJsonResponse(res, jsonStr.value());
    INF("Valid email request handled, requestId: {}", requestId);
}

void GatewayServiceImpl::handleUserRegister(const httplib::Request& req, httplib::Response& res) {
    // 1. 解析HTTP请求参数
    auto requestJson = biteutil::JSON::unserialize(req.body);
    if (!requestJson) {
        ERR("Failed to parse user register request body");
        sendErrorResponse(res, "", ERROR_CODE_PARAM_INVALID, INVALID_BODY_MESSAGE, HTTP_STATUS_BAD_REQUEST);
        return;
    }
    const std::string requestId = requestJson->get("requestId", "").asString();
    const std::string nickname = requestJson->get("nickname", "").asString();
    const std::string password = requestJson->get("password", "").asString();
    const std::string email = requestJson->get("email", "").asString();
    if (!validateRequiredFields(requestId,
                                {{"nickname", nickname}, {"password", password}, {"email", email}},
                                res)) {
        return;
    }

    // 2. 调用用户子服务注册接口
    auto channel = getChannel(FLAGS_userServiceName);
    if (!channel) {
        sendErrorResponse(res, requestId, ERROR_CODE_SERVICE_UNAVAILABLE, SERVICE_UNAVAILABLE_MESSAGE);
        return;
    }

    chat2Data::userService::UserRegisterRequest rpcRequest;
    rpcRequest.set_request_id(requestId);
    rpcRequest.set_nickname(nickname);
    rpcRequest.set_password(password);
    rpcRequest.set_email(email);

    chat2Data::userService::UserRegisterResponse rpcResponse;
    brpc::Controller controller;
    chat2Data::userService::UserService_Stub stub(channel.get());
    stub.UserRegister(&controller, &rpcRequest, &rpcResponse, nullptr);

    if (controller.Failed()) {
        ERR("UserRegister RPC failed, requestId: {}, reason: {}", requestId, controller.ErrorText());
        sendErrorResponse(res, requestId, ERROR_CODE_SERVICE_UNAVAILABLE, SERVICE_UNAVAILABLE_MESSAGE);
        return;
    }
    if (rpcResponse.error_code() != 0) {
        sendErrorResponse(res, rpcResponse.request_id(), rpcResponse.error_code(), rpcResponse.error_msg());
        return;
    }

    // 3. 解析RPC响应并构建HTTP响应体
    Json::Value responseJson;
    responseJson["requestId"] = rpcResponse.request_id();
    responseJson["errorCode"] = rpcResponse.error_code();
    responseJson["errorMsg"] = rpcResponse.error_msg();

    auto jsonStr = biteutil::JSON::serialize(responseJson);
    if (!jsonStr) {
        ERR("Failed to serialize user register response, requestId: {}", requestId);
        sendErrorResponse(res, requestId, ERROR_CODE_INTERNAL_ERROR, INTERNAL_ERROR_MESSAGE,
                          HTTP_STATUS_INTERNAL_SERVER_ERROR);
        return;
    }
    sendJsonResponse(res, jsonStr.value());
    INF("User register request handled, requestId: {}", requestId);
}

void GatewayServiceImpl::handlePasswordLogin(const httplib::Request& req, httplib::Response& res) {
    // 1. 解析HTTP请求参数
    auto requestJson = biteutil::JSON::unserialize(req.body);
    if (!requestJson) {
        ERR("Failed to parse password login request body");
        sendErrorResponse(res, "", ERROR_CODE_PARAM_INVALID, INVALID_BODY_MESSAGE, HTTP_STATUS_BAD_REQUEST);
        return;
    }
    const std::string requestId = requestJson->get("requestId", "").asString();
    const std::string username = requestJson->get("username", "").asString();
    const std::string password = requestJson->get("password", "").asString();
    if (!validateRequiredFields(requestId, {{"username", username}, {"password", password}}, res)) {
        return;
    }

    // 2. 调用用户子服务密码登录接口
    auto channel = getChannel(FLAGS_userServiceName);
    if (!channel) {
        sendErrorResponse(res, requestId, ERROR_CODE_SERVICE_UNAVAILABLE, SERVICE_UNAVAILABLE_MESSAGE);
        return;
    }

    chat2Data::userService::PasswdLoginRequest rpcRequest;
    rpcRequest.set_request_id(requestId);
    rpcRequest.set_username(username);
    rpcRequest.set_password(password);

    chat2Data::userService::PasswdLoginResponse rpcResponse;
    brpc::Controller controller;
    chat2Data::userService::UserService_Stub stub(channel.get());
    stub.PasswdLogin(&controller, &rpcRequest, &rpcResponse, nullptr);

    if (controller.Failed()) {
        ERR("PasswdLogin RPC failed, requestId: {}, reason: {}", requestId, controller.ErrorText());
        sendErrorResponse(res, requestId, ERROR_CODE_SERVICE_UNAVAILABLE, SERVICE_UNAVAILABLE_MESSAGE);
        return;
    }
    if (rpcResponse.error_code() != 0) {
        sendErrorResponse(res, rpcResponse.request_id(), rpcResponse.error_code(), rpcResponse.error_msg());
        return;
    }

    // 3. 解析RPC响应并构建HTTP响应体
    Json::Value responseJson;
    responseJson["requestId"] = rpcResponse.request_id();
    responseJson["errorCode"] = rpcResponse.error_code();
    responseJson["errorMsg"] = rpcResponse.error_msg();
    if (rpcResponse.has_result()) {
        Json::Value resultJson;
        resultJson["sessionId"] = rpcResponse.result().session_id();
        responseJson["result"] = resultJson;
    }

    auto jsonStr = biteutil::JSON::serialize(responseJson);
    if (!jsonStr) {
        ERR("Failed to serialize password login response, requestId: {}", requestId);
        sendErrorResponse(res, requestId, ERROR_CODE_INTERNAL_ERROR, INTERNAL_ERROR_MESSAGE,
                          HTTP_STATUS_INTERNAL_SERVER_ERROR);
        return;
    }
    sendJsonResponse(res, jsonStr.value());
    INF("Password login request handled, requestId: {}", requestId);
}

void GatewayServiceImpl::handleGetVerifyCode(const httplib::Request& req, httplib::Response& res) {
    // 1. 解析HTTP请求参数
    auto requestJson = biteutil::JSON::unserialize(req.body);
    if (!requestJson) {
        ERR("Failed to parse get verify code request body");
        sendErrorResponse(res, "", ERROR_CODE_PARAM_INVALID, INVALID_BODY_MESSAGE, HTTP_STATUS_BAD_REQUEST);
        return;
    }
    const std::string requestId = requestJson->get("requestId", "").asString();
    const std::string email = requestJson->get("email", "").asString();
    if (!validateRequiredFields(requestId, {{"email", email}}, res)) {
        return;
    }

    // 2. 调用用户子服务获取验证码接口
    auto channel = getChannel(FLAGS_userServiceName);
    if (!channel) {
        sendErrorResponse(res, requestId, ERROR_CODE_SERVICE_UNAVAILABLE, SERVICE_UNAVAILABLE_MESSAGE);
        return;
    }

    chat2Data::userService::GetCodeRequest rpcRequest;
    rpcRequest.set_request_id(requestId);
    rpcRequest.set_email(email);

    chat2Data::userService::GetCodeResponse rpcResponse;
    brpc::Controller controller;
    chat2Data::userService::UserService_Stub stub(channel.get());
    stub.GetCode(&controller, &rpcRequest, &rpcResponse, nullptr);

    if (controller.Failed()) {
        ERR("GetCode RPC failed, requestId: {}, reason: {}", requestId, controller.ErrorText());
        sendErrorResponse(res, requestId, ERROR_CODE_SERVICE_UNAVAILABLE, SERVICE_UNAVAILABLE_MESSAGE);
        return;
    }
    if (rpcResponse.error_code() != 0) {
        sendErrorResponse(res, rpcResponse.request_id(), rpcResponse.error_code(), rpcResponse.error_msg());
        return;
    }

    // 3. 解析RPC响应并构建HTTP响应体
    Json::Value responseJson;
    responseJson["requestId"] = rpcResponse.request_id();
    responseJson["errorCode"] = rpcResponse.error_code();
    responseJson["errorMsg"] = rpcResponse.error_msg();
    if (rpcResponse.has_result()) {
        Json::Value resultJson;
        resultJson["codeId"] = rpcResponse.result().code_id();
        responseJson["result"] = resultJson;
    }

    auto jsonStr = biteutil::JSON::serialize(responseJson);
    if (!jsonStr) {
        ERR("Failed to serialize get verify code response, requestId: {}", requestId);
        sendErrorResponse(res, requestId, ERROR_CODE_INTERNAL_ERROR, INTERNAL_ERROR_MESSAGE,
                          HTTP_STATUS_INTERNAL_SERVER_ERROR);
        return;
    }
    sendJsonResponse(res, jsonStr.value());
    INF("Get verify code request handled, requestId: {}", requestId);
}

void GatewayServiceImpl::handleVerifyCodeLogin(const httplib::Request& req, httplib::Response& res) {
    // 1. 解析HTTP请求参数
    auto requestJson = biteutil::JSON::unserialize(req.body);
    if (!requestJson) {
        ERR("Failed to parse verify code login request body");
        sendErrorResponse(res, "", ERROR_CODE_PARAM_INVALID, INVALID_BODY_MESSAGE, HTTP_STATUS_BAD_REQUEST);
        return;
    }
    const std::string requestId = requestJson->get("requestId", "").asString();
    const std::string email = requestJson->get("email", "").asString();
    const std::string verifyCode = requestJson->get("verifyCode", "").asString();
    const std::string codeId = requestJson->get("codeId", "").asString();
    if (!validateRequiredFields(
            requestId,
            {{"email", email}, {"verifyCode", verifyCode}, {"codeId", codeId}}, res)) {
        return;
    }

    // 2. 调用用户子服务验证码登录接口
    auto channel = getChannel(FLAGS_userServiceName);
    if (!channel) {
        sendErrorResponse(res, requestId, ERROR_CODE_SERVICE_UNAVAILABLE, SERVICE_UNAVAILABLE_MESSAGE);
        return;
    }

    chat2Data::userService::VcodeLoginRequest rpcRequest;
    rpcRequest.set_request_id(requestId);
    rpcRequest.set_email(email);
    rpcRequest.set_verify_code(verifyCode);
    rpcRequest.set_code_id(codeId);

    chat2Data::userService::VcodeLoginResponse rpcResponse;
    brpc::Controller controller;
    chat2Data::userService::UserService_Stub stub(channel.get());
    stub.VcodeLogin(&controller, &rpcRequest, &rpcResponse, nullptr);

    if (controller.Failed()) {
        ERR("VcodeLogin RPC failed, requestId: {}, reason: {}", requestId, controller.ErrorText());
        sendErrorResponse(res, requestId, ERROR_CODE_SERVICE_UNAVAILABLE, SERVICE_UNAVAILABLE_MESSAGE);
        return;
    }
    if (rpcResponse.error_code() != 0) {
        sendErrorResponse(res, rpcResponse.request_id(), rpcResponse.error_code(), rpcResponse.error_msg());
        return;
    }

    // 3. 解析RPC响应并构建HTTP响应体
    Json::Value responseJson;
    responseJson["requestId"] = rpcResponse.request_id();
    responseJson["errorCode"] = rpcResponse.error_code();
    responseJson["errorMsg"] = rpcResponse.error_msg();
    if (rpcResponse.has_result()) {
        Json::Value resultJson;
        resultJson["sessionId"] = rpcResponse.result().session_id();
        responseJson["result"] = resultJson;
    }

    auto jsonStr = biteutil::JSON::serialize(responseJson);
    if (!jsonStr) {
        ERR("Failed to serialize verify code login response, requestId: {}", requestId);
        sendErrorResponse(res, requestId, ERROR_CODE_INTERNAL_ERROR, INTERNAL_ERROR_MESSAGE,
                          HTTP_STATUS_INTERNAL_SERVER_ERROR);
        return;
    }
    sendJsonResponse(res, jsonStr.value());
    INF("Verify code login request handled, requestId: {}", requestId);
}

void GatewayServiceImpl::handleSessionLogin(const httplib::Request& req, httplib::Response& res) {
    // 1. 解析HTTP请求参数
    auto requestJson = biteutil::JSON::unserialize(req.body);
    if (!requestJson) {
        ERR("Failed to parse session login request body");
        sendErrorResponse(res, "", ERROR_CODE_PARAM_INVALID, INVALID_BODY_MESSAGE, HTTP_STATUS_BAD_REQUEST);
        return;
    }
    const std::string requestId = requestJson->get("requestId", "").asString();
    const std::string sessionId = requestJson->get("sessionId", "").asString();
    if (!validateRequiredFields(requestId, {{"sessionId", sessionId}}, res)) {
        return;
    }

    // 2. 调用用户子服务会话登录接口
    auto channel = getChannel(FLAGS_userServiceName);
    if (!channel) {
        sendErrorResponse(res, requestId, ERROR_CODE_SERVICE_UNAVAILABLE, SERVICE_UNAVAILABLE_MESSAGE);
        return;
    }

    chat2Data::userService::SessionLoginRequest rpcRequest;
    rpcRequest.set_request_id(requestId);
    rpcRequest.set_session_id(sessionId);

    chat2Data::userService::SessionLoginResponse rpcResponse;
    brpc::Controller controller;
    chat2Data::userService::UserService_Stub stub(channel.get());
    stub.SessionLogin(&controller, &rpcRequest, &rpcResponse, nullptr);

    if (controller.Failed()) {
        ERR("SessionLogin RPC failed, requestId: {}, reason: {}", requestId, controller.ErrorText());
        sendErrorResponse(res, requestId, ERROR_CODE_SERVICE_UNAVAILABLE, SERVICE_UNAVAILABLE_MESSAGE);
        return;
    }
    if (rpcResponse.error_code() != 0) {
        sendErrorResponse(res, rpcResponse.request_id(), rpcResponse.error_code(), rpcResponse.error_msg());
        return;
    }

    // 3. 解析RPC响应并构建HTTP响应体
    Json::Value responseJson;
    responseJson["requestId"] = rpcResponse.request_id();
    responseJson["errorCode"] = rpcResponse.error_code();
    responseJson["errorMsg"] = rpcResponse.error_msg();

    auto jsonStr = biteutil::JSON::serialize(responseJson);
    if (!jsonStr) {
        ERR("Failed to serialize session login response, requestId: {}", requestId);
        sendErrorResponse(res, requestId, ERROR_CODE_INTERNAL_ERROR, INTERNAL_ERROR_MESSAGE,
                          HTTP_STATUS_INTERNAL_SERVER_ERROR);
        return;
    }
    sendJsonResponse(res, jsonStr.value());
    INF("Session login request handled, requestId: {}", requestId);
}

void GatewayServiceImpl::handleLogout(const httplib::Request& req, httplib::Response& res) {
    // 1. 解析HTTP请求参数
    auto requestJson = biteutil::JSON::unserialize(req.body);
    if (!requestJson) {
        ERR("Failed to parse logout request body");
        sendErrorResponse(res, "", ERROR_CODE_PARAM_INVALID, INVALID_BODY_MESSAGE, HTTP_STATUS_BAD_REQUEST);
        return;
    }
    const std::string requestId = requestJson->get("requestId", "").asString();
    const std::string sessionId = requestJson->get("sessionId", "").asString();
    if (!validateRequiredFields(requestId, {{"sessionId", sessionId}}, res)) {
        return;
    }

    // 2. 调用用户子服务前先进行会话鉴权
    std::string userId;
    if (!validateSession(requestId, sessionId, res, userId)) {
        return;
    }

    // 3. 调用用户子服务退出登录接口
    auto channel = getChannel(FLAGS_userServiceName);
    if (!channel) {
        sendErrorResponse(res, requestId, ERROR_CODE_SERVICE_UNAVAILABLE, SERVICE_UNAVAILABLE_MESSAGE);
        return;
    }

    chat2Data::userService::LogoutRequest rpcRequest;
    rpcRequest.set_request_id(requestId);
    rpcRequest.set_session_id(sessionId);

    chat2Data::userService::LogoutResponse rpcResponse;
    brpc::Controller controller;
    chat2Data::userService::UserService_Stub stub(channel.get());
    stub.Logout(&controller, &rpcRequest, &rpcResponse, nullptr);

    if (controller.Failed()) {
        ERR("Logout RPC failed, requestId: {}, reason: {}", requestId, controller.ErrorText());
        sendErrorResponse(res, requestId, ERROR_CODE_SERVICE_UNAVAILABLE, SERVICE_UNAVAILABLE_MESSAGE);
        return;
    }
    if (rpcResponse.error_code() != 0) {
        sendErrorResponse(res, rpcResponse.request_id(), rpcResponse.error_code(), rpcResponse.error_msg());
        return;
    }

    // 4. 解析RPC响应并构建HTTP响应体
    Json::Value responseJson;
    responseJson["requestId"] = rpcResponse.request_id();
    responseJson["errorCode"] = rpcResponse.error_code();
    responseJson["errorMsg"] = rpcResponse.error_msg();

    auto jsonStr = biteutil::JSON::serialize(responseJson);
    if (!jsonStr) {
        ERR("Failed to serialize logout response, requestId: {}", requestId);
        sendErrorResponse(res, requestId, ERROR_CODE_INTERNAL_ERROR, INTERNAL_ERROR_MESSAGE,
                          HTTP_STATUS_INTERNAL_SERVER_ERROR);
        return;
    }
    sendJsonResponse(res, jsonStr.value());
    INF("Logout request handled, requestId: {}, userId: {}", requestId, userId);
}

void GatewayServiceImpl::handleGetUserInfo(const httplib::Request& req, httplib::Response& res) {
    // 1. 解析HTTP请求参数（参数位于URL中）
    if (!req.has_param("requestId") || !req.has_param("sessionId")) {
        ERR("Missing required parameters for get user info");
        sendErrorResponse(res, "", ERROR_CODE_PARAM_INVALID, MISSING_PARAM_MESSAGE, HTTP_STATUS_BAD_REQUEST);
        return;
    }
    const std::string requestId = req.get_param_value("requestId");
    const std::string sessionId = req.get_param_value("sessionId");
    const std::string requestedUserId = req.has_param("userId") ? req.get_param_value("userId") : "";

    // 2. 调用用户子服务前先进行会话鉴权，鉴权成功后可获取当前登录用户ID
    std::string authenticatedUserId;
    if (!validateSession(requestId, sessionId, res, authenticatedUserId)) {
        return;
    }
    // 以会话鉴权结果为准，不信任 URL 中的 userId，避免越权查询其他用户
    if (!requestedUserId.empty() && requestedUserId != authenticatedUserId) {
        WRN("Requested userId differs from authenticated user, requestId: {}, requested: {}, "
            "authenticated: {}",
            requestId, requestedUserId, authenticatedUserId);
    }
    const std::string userId = authenticatedUserId;

    // 3. 调用用户子服务获取用户信息接口
    auto channel = getChannel(FLAGS_userServiceName);
    if (!channel) {
        sendErrorResponse(res, requestId, ERROR_CODE_SERVICE_UNAVAILABLE, SERVICE_UNAVAILABLE_MESSAGE);
        return;
    }

    chat2Data::userService::GetUserInfoRequest rpcRequest;
    rpcRequest.set_request_id(requestId);
    rpcRequest.set_session_id(sessionId);
    rpcRequest.set_user_id(userId);

    chat2Data::userService::GetUserInfoResponse rpcResponse;
    brpc::Controller controller;
    chat2Data::userService::UserService_Stub stub(channel.get());
    stub.GetUserInfo(&controller, &rpcRequest, &rpcResponse, nullptr);

    if (controller.Failed()) {
        ERR("GetUserInfo RPC failed, requestId: {}, reason: {}", requestId, controller.ErrorText());
        sendErrorResponse(res, requestId, ERROR_CODE_SERVICE_UNAVAILABLE, SERVICE_UNAVAILABLE_MESSAGE);
        return;
    }
    if (rpcResponse.error_code() != 0) {
        sendErrorResponse(res, rpcResponse.request_id(), rpcResponse.error_code(), rpcResponse.error_msg());
        return;
    }

    // 4. 解析RPC响应并构建HTTP响应体
    Json::Value responseJson;
    responseJson["requestId"] = rpcResponse.request_id();
    responseJson["errorCode"] = rpcResponse.error_code();
    responseJson["errorMsg"] = rpcResponse.error_msg();
    if (rpcResponse.has_result()) {
        Json::Value userInfoJson;
        userInfoJson["userId"] = rpcResponse.result().user_info().user_id();
        userInfoJson["nickname"] = rpcResponse.result().user_info().nickname();
        userInfoJson["email"] = rpcResponse.result().user_info().email();

        Json::Value resultJson;
        resultJson["userInfo"] = userInfoJson;
        responseJson["result"] = resultJson;
    }

    auto jsonStr = biteutil::JSON::serialize(responseJson);
    if (!jsonStr) {
        ERR("Failed to serialize get user info response, requestId: {}", requestId);
        sendErrorResponse(res, requestId, ERROR_CODE_INTERNAL_ERROR, INTERNAL_ERROR_MESSAGE,
                          HTTP_STATUS_INTERNAL_SERVER_ERROR);
        return;
    }
    sendJsonResponse(res, jsonStr.value());
    INF("Get user info request handled, requestId: {}, userId: {}", requestId, userId);
}

void GatewayServiceImpl::sendNotImplemented(httplib::Response& res) {
    Json::Value response;
    response["errorMsg"] = NOT_IMPLEMENTED_MESSAGE;

    auto jsonStr = biteutil::JSON::serialize(response);
    res.status = HTTP_STATUS_NOT_IMPLEMENTED;
    res.set_content(jsonStr.value_or(NOT_IMPLEMENTED_RESPONSE_JSON), JSON_CONTENT_TYPE);
    WRN("Request reached an unimplemented interface");
}

// 文件子服务接口
void GatewayServiceImpl::handleUploadFileInfo(const httplib::Request& req, httplib::Response& res) {
    // 1. 解析HTTP请求参数
    auto requestJson = biteutil::JSON::unserialize(req.body);
    if (!requestJson) {
        ERR("Failed to parse file upload info request body");
        sendErrorResponse(res, "", ERROR_CODE_PARAM_INVALID, INVALID_BODY_MESSAGE, HTTP_STATUS_BAD_REQUEST);
        return;
    }
    const std::string requestId = requestJson->get("requestId", "").asString();
    const std::string sessionId = requestJson->get("sessionId", "").asString();
    if (!requestJson->isMember("fileInfo")) {
        ERR("Missing fileInfo for file upload info, requestId: {}", requestId);
        sendErrorResponse(res, requestId, ERROR_CODE_PARAM_INVALID, MISSING_PARAM_MESSAGE,
                          HTTP_STATUS_BAD_REQUEST);
        return;
    }
    const Json::Value& fileInfoJson = (*requestJson)["fileInfo"];

    // 2. 会话鉴权，鉴权成功后可获取当前登录用户ID
    std::string userId;
    if (!validateSession(requestId, sessionId, res, userId)) {
        return;
    }

    // 3. 调用文件子服务上传文件信息接口
    auto channel = getChannel(FLAGS_fileServiceName);
    if (!channel) {
        sendErrorResponse(res, requestId, ERROR_CODE_SERVICE_UNAVAILABLE, SERVICE_UNAVAILABLE_MESSAGE);
        return;
    }

    chat2Data::fileService::UploadFileInfoRequest rpcRequest;
    rpcRequest.set_request_id(requestId);
    rpcRequest.set_session_id(sessionId);
    rpcRequest.set_user_id(userId);
    auto* fileInfo = rpcRequest.mutable_file_info();
    fileInfo->set_filename(fileInfoJson.get("filename", "").asString());
    fileInfo->set_file_size(fileInfoJson.get("fileSize", 0).asInt64());
    fileInfo->set_file_ext(fileInfoJson.get("fileExt", "").asString());

    chat2Data::fileService::UploadFileInfoResponse rpcResponse;
    brpc::Controller controller;
    chat2Data::fileService::FileService_Stub stub(channel.get());
    stub.UploadFileInfo(&controller, &rpcRequest, &rpcResponse, nullptr);

    if (controller.Failed()) {
        ERR("UploadFileInfo RPC failed, requestId: {}, reason: {}", requestId, controller.ErrorText());
        sendErrorResponse(res, requestId, ERROR_CODE_SERVICE_UNAVAILABLE, SERVICE_UNAVAILABLE_MESSAGE);
        return;
    }
    if (rpcResponse.error_code() != 0) {
        sendErrorResponse(res, rpcResponse.request_id(), rpcResponse.error_code(), rpcResponse.error_msg());
        return;
    }

    // 4. 解析RPC响应并构建HTTP响应体
    Json::Value responseJson;
    responseJson["requestId"] = rpcResponse.request_id();
    responseJson["errorCode"] = rpcResponse.error_code();
    responseJson["errorMsg"] = rpcResponse.error_msg();
    if (rpcResponse.has_result()) {
        Json::Value resultJson;
        resultJson["fileId"] = rpcResponse.result().file_id();
        responseJson["result"] = resultJson;
    }

    auto jsonStr = biteutil::JSON::serialize(responseJson);
    if (!jsonStr) {
        ERR("Failed to serialize file upload info response, requestId: {}", requestId);
        sendErrorResponse(res, requestId, ERROR_CODE_INTERNAL_ERROR, INTERNAL_ERROR_MESSAGE,
                          HTTP_STATUS_INTERNAL_SERVER_ERROR);
        return;
    }
    sendJsonResponse(res, jsonStr.value());
    INF("File upload info request handled, requestId: {}, userId: {}", requestId, userId);
}

void GatewayServiceImpl::handleGetFileInfo(const httplib::Request& req, httplib::Response& res) {
    // 1. 解析HTTP请求参数（参数位于URL中）
    if (!req.has_param("requestId") || !req.has_param("sessionId") || !req.has_param("fileId")) {
        ERR("Missing required parameters for get file info");
        sendErrorResponse(res, "", ERROR_CODE_PARAM_INVALID, MISSING_PARAM_MESSAGE, HTTP_STATUS_BAD_REQUEST);
        return;
    }
    const std::string requestId = req.get_param_value("requestId");
    const std::string sessionId = req.get_param_value("sessionId");
    const std::string fileId = req.get_param_value("fileId");

    // 2. 会话鉴权，鉴权成功后可获取当前登录用户ID
    std::string userId;
    if (!validateSession(requestId, sessionId, res, userId)) {
        return;
    }

    // 3. 调用文件子服务获取文件信息接口
    auto channel = getChannel(FLAGS_fileServiceName);
    if (!channel) {
        sendErrorResponse(res, requestId, ERROR_CODE_SERVICE_UNAVAILABLE, SERVICE_UNAVAILABLE_MESSAGE);
        return;
    }

    chat2Data::fileService::GetFileInfoRequest rpcRequest;
    rpcRequest.set_request_id(requestId);
    rpcRequest.set_session_id(sessionId);
    rpcRequest.set_file_id(fileId);
    rpcRequest.set_user_id(userId);

    chat2Data::fileService::GetFileInfoResponse rpcResponse;
    brpc::Controller controller;
    chat2Data::fileService::FileService_Stub stub(channel.get());
    stub.GetFileInfo(&controller, &rpcRequest, &rpcResponse, nullptr);

    if (controller.Failed()) {
        ERR("GetFileInfo RPC failed, requestId: {}, reason: {}", requestId, controller.ErrorText());
        sendErrorResponse(res, requestId, ERROR_CODE_SERVICE_UNAVAILABLE, SERVICE_UNAVAILABLE_MESSAGE);
        return;
    }
    if (rpcResponse.error_code() != 0) {
        sendErrorResponse(res, rpcResponse.request_id(), rpcResponse.error_code(), rpcResponse.error_msg());
        return;
    }

    // 4. 解析RPC响应并构建HTTP响应体
    Json::Value responseJson;
    responseJson["requestId"] = rpcResponse.request_id();
    responseJson["errorCode"] = rpcResponse.error_code();
    responseJson["errorMsg"] = rpcResponse.error_msg();
    if (rpcResponse.has_result()) {
        Json::Value resultJson;
        resultJson["fileId"] = rpcResponse.result().file_id();
        resultJson["fileName"] = rpcResponse.result().file_name();
        resultJson["fileSize"] = Json::Int64(rpcResponse.result().file_size());
        resultJson["uploadTime"] = Json::Int64(rpcResponse.result().upload_time());
        resultJson["fileExt"] = rpcResponse.result().file_ext();
        responseJson["result"] = resultJson;
    }

    auto jsonStr = biteutil::JSON::serialize(responseJson);
    if (!jsonStr) {
        ERR("Failed to serialize get file info response, requestId: {}", requestId);
        sendErrorResponse(res, requestId, ERROR_CODE_INTERNAL_ERROR, INTERNAL_ERROR_MESSAGE,
                          HTTP_STATUS_INTERNAL_SERVER_ERROR);
        return;
    }
    sendJsonResponse(res, jsonStr.value());
    INF("Get file info request handled, requestId: {}, fileId: {}", requestId, fileId);
}

void GatewayServiceImpl::handleUploadFile(const httplib::Request& req, httplib::Response& res) {
    // 1. 解析HTTP请求参数（参数位于URL中，文件数据位于请求体）
    if (!req.has_param("requestId") || !req.has_param("sessionId") || !req.has_param("fileId")) {
        ERR("Missing required parameters for file upload");
        sendErrorResponse(res, "", ERROR_CODE_PARAM_INVALID, MISSING_PARAM_MESSAGE, HTTP_STATUS_BAD_REQUEST);
        return;
    }
    const std::string requestId = req.get_param_value("requestId");
    const std::string sessionId = req.get_param_value("sessionId");
    const std::string fileId = req.get_param_value("fileId");
    if (req.body.empty()) {
        ERR("Empty file data for file upload, requestId: {}", requestId);
        sendErrorResponse(res, requestId, ERROR_CODE_PARAM_INVALID, INVALID_BODY_MESSAGE,
                          HTTP_STATUS_BAD_REQUEST);
        return;
    }

    // 2. 会话鉴权，鉴权成功后可获取当前登录用户ID
    std::string userId;
    if (!validateSession(requestId, sessionId, res, userId)) {
        return;
    }

    // 3. 调用文件子服务上传文件数据接口，文件数据通过attachment传输
    auto channel = getChannel(FLAGS_fileServiceName);
    if (!channel) {
        sendErrorResponse(res, requestId, ERROR_CODE_SERVICE_UNAVAILABLE, SERVICE_UNAVAILABLE_MESSAGE);
        return;
    }

    chat2Data::fileService::UploadFileRequest rpcRequest;
    rpcRequest.set_request_id(requestId);
    rpcRequest.set_session_id(sessionId);
    rpcRequest.set_file_id(fileId);
    rpcRequest.set_user_id(userId);

    chat2Data::fileService::UploadFileResponse rpcResponse;
    brpc::Controller controller;
    controller.request_attachment().append(req.body);
    chat2Data::fileService::FileService_Stub stub(channel.get());
    stub.UploadFile(&controller, &rpcRequest, &rpcResponse, nullptr);

    if (controller.Failed()) {
        ERR("UploadFile RPC failed, requestId: {}, reason: {}", requestId, controller.ErrorText());
        sendErrorResponse(res, requestId, ERROR_CODE_SERVICE_UNAVAILABLE, SERVICE_UNAVAILABLE_MESSAGE);
        return;
    }
    if (rpcResponse.error_code() != 0) {
        sendErrorResponse(res, rpcResponse.request_id(), rpcResponse.error_code(), rpcResponse.error_msg());
        return;
    }

    // 4. 解析RPC响应并构建HTTP响应体，RPC响应无result，使用请求中的文件ID填充
    Json::Value responseJson;
    responseJson["requestId"] = rpcResponse.request_id();
    responseJson["errorCode"] = rpcResponse.error_code();
    responseJson["errorMsg"] = rpcResponse.error_msg();
    Json::Value resultJson;
    resultJson["fileId"] = fileId;
    responseJson["result"] = resultJson;

    auto jsonStr = biteutil::JSON::serialize(responseJson);
    if (!jsonStr) {
        ERR("Failed to serialize file upload response, requestId: {}", requestId);
        sendErrorResponse(res, requestId, ERROR_CODE_INTERNAL_ERROR, INTERNAL_ERROR_MESSAGE,
                          HTTP_STATUS_INTERNAL_SERVER_ERROR);
        return;
    }
    sendJsonResponse(res, jsonStr.value());
    INF("File upload request handled, requestId: {}, fileId: {}", requestId, fileId);
}

void GatewayServiceImpl::handleDownloadFile(const httplib::Request& req, httplib::Response& res) {
    // 1. 解析HTTP请求参数（参数位于URL中）
    if (!req.has_param("requestId") || !req.has_param("sessionId") || !req.has_param("fileId")) {
        ERR("Missing required parameters for file download");
        sendErrorResponse(res, "", ERROR_CODE_PARAM_INVALID, MISSING_PARAM_MESSAGE, HTTP_STATUS_BAD_REQUEST);
        return;
    }
    const std::string requestId = req.get_param_value("requestId");
    const std::string sessionId = req.get_param_value("sessionId");
    const std::string fileId = req.get_param_value("fileId");

    // 2. 会话鉴权，鉴权成功后可获取当前登录用户ID
    std::string userId;
    if (!validateSession(requestId, sessionId, res, userId)) {
        return;
    }

    auto channel = getChannel(FLAGS_fileServiceName);
    if (!channel) {
        sendErrorResponse(res, requestId, ERROR_CODE_SERVICE_UNAVAILABLE, SERVICE_UNAVAILABLE_MESSAGE);
        return;
    }
    chat2Data::fileService::FileService_Stub stub(channel.get());

    // 3. 先获取文件信息，用于HTTP响应头中的下载文件名
    chat2Data::fileService::GetFileInfoRequest infoRequest;
    infoRequest.set_request_id(requestId);
    infoRequest.set_session_id(sessionId);
    infoRequest.set_file_id(fileId);
    infoRequest.set_user_id(userId);

    chat2Data::fileService::GetFileInfoResponse infoResponse;
    brpc::Controller infoController;
    stub.GetFileInfo(&infoController, &infoRequest, &infoResponse, nullptr);

    if (infoController.Failed()) {
        ERR("GetFileInfo for download failed, requestId: {}, reason: {}", requestId,
            infoController.ErrorText());
        sendErrorResponse(res, requestId, ERROR_CODE_SERVICE_UNAVAILABLE, SERVICE_UNAVAILABLE_MESSAGE);
        return;
    }
    if (infoResponse.error_code() != 0) {
        sendErrorResponse(res, infoResponse.request_id(), infoResponse.error_code(),
                          infoResponse.error_msg());
        return;
    }
    const std::string fileName = infoResponse.result().file_name();

    // 4. 调用文件子服务下载文件接口，文件数据通过attachment返回
    chat2Data::fileService::DownloadFileRequest rpcRequest;
    rpcRequest.set_request_id(requestId);
    rpcRequest.set_session_id(sessionId);
    rpcRequest.set_file_id(fileId);
    rpcRequest.set_user_id(userId);

    chat2Data::fileService::DownloadFileResponse rpcResponse;
    brpc::Controller controller;
    stub.DownloadFile(&controller, &rpcRequest, &rpcResponse, nullptr);

    if (controller.Failed()) {
        ERR("DownloadFile RPC failed, requestId: {}, reason: {}", requestId, controller.ErrorText());
        sendErrorResponse(res, requestId, ERROR_CODE_SERVICE_UNAVAILABLE, SERVICE_UNAVAILABLE_MESSAGE);
        return;
    }
    if (rpcResponse.error_code() != 0) {
        sendErrorResponse(res, rpcResponse.request_id(), rpcResponse.error_code(), rpcResponse.error_msg());
        return;
    }

    // 5. 从attachment中取出文件数据，构建二进制HTTP响应
    std::string fileData;
    controller.response_attachment().copy_to(&fileData);
    res.status = HTTP_STATUS_OK;
    res.set_header("Content-Disposition", "attachment; filename=\"" + fileName + "\"");
    res.set_content(fileData, "application/octet-stream");
    INF("File download request handled, requestId: {}, fileId: {}, size: {}", requestId, fileId,
        fileData.size());
}

void GatewayServiceImpl::handleDeleteFile(const httplib::Request& req, httplib::Response& res) {
    // 1. 解析HTTP请求参数（文件ID为路径参数，请求ID与会话ID位于URL中）
    if (!req.has_param("requestId") || !req.has_param("sessionId")) {
        ERR("Missing required parameters for file delete");
        sendErrorResponse(res, "", ERROR_CODE_PARAM_INVALID, MISSING_PARAM_MESSAGE, HTTP_STATUS_BAD_REQUEST);
        return;
    }
    const std::string fileId = req.matches[1];
    const std::string requestId = req.get_param_value("requestId");
    const std::string sessionId = req.get_param_value("sessionId");

    // 2. 会话鉴权，鉴权成功后可获取当前登录用户ID
    std::string userId;
    if (!validateSession(requestId, sessionId, res, userId)) {
        return;
    }

    // 3. 调用文件子服务删除文件接口
    auto channel = getChannel(FLAGS_fileServiceName);
    if (!channel) {
        sendErrorResponse(res, requestId, ERROR_CODE_SERVICE_UNAVAILABLE, SERVICE_UNAVAILABLE_MESSAGE);
        return;
    }

    chat2Data::fileService::DeleteFileRequest rpcRequest;
    rpcRequest.set_request_id(requestId);
    rpcRequest.set_session_id(sessionId);
    rpcRequest.set_file_id(fileId);
    rpcRequest.set_user_id(userId);

    chat2Data::fileService::DeleteFileResponse rpcResponse;
    brpc::Controller controller;
    chat2Data::fileService::FileService_Stub stub(channel.get());
    stub.DeleteFile(&controller, &rpcRequest, &rpcResponse, nullptr);

    if (controller.Failed()) {
        ERR("DeleteFile RPC failed, requestId: {}, reason: {}", requestId, controller.ErrorText());
        sendErrorResponse(res, requestId, ERROR_CODE_SERVICE_UNAVAILABLE, SERVICE_UNAVAILABLE_MESSAGE);
        return;
    }
    if (rpcResponse.error_code() != 0) {
        sendErrorResponse(res, rpcResponse.request_id(), rpcResponse.error_code(), rpcResponse.error_msg());
        return;
    }

    // 4. 解析RPC响应并构建HTTP响应体
    Json::Value responseJson;
    responseJson["requestId"] = rpcResponse.request_id();
    responseJson["errorCode"] = rpcResponse.error_code();
    responseJson["errorMsg"] = rpcResponse.error_msg();

    auto jsonStr = biteutil::JSON::serialize(responseJson);
    if (!jsonStr) {
        ERR("Failed to serialize file delete response, requestId: {}", requestId);
        sendErrorResponse(res, requestId, ERROR_CODE_INTERNAL_ERROR, INTERNAL_ERROR_MESSAGE,
                          HTTP_STATUS_INTERNAL_SERVER_ERROR);
        return;
    }
    sendJsonResponse(res, jsonStr.value());
    INF("File delete request handled, requestId: {}, fileId: {}", requestId, fileId);
}

void GatewayServiceImpl::handlePreviewExcel(const httplib::Request& req, httplib::Response& res) {
    // 1. 解析HTTP请求参数
    auto requestJson = biteutil::JSON::unserialize(req.body);
    if (!requestJson) {
        ERR("Failed to parse file preview request body");
        sendErrorResponse(res, "", ERROR_CODE_PARAM_INVALID, INVALID_BODY_MESSAGE, HTTP_STATUS_BAD_REQUEST);
        return;
    }
    const std::string requestId = requestJson->get("requestId", "").asString();
    const std::string sessionId = requestJson->get("sessionId", "").asString();
    const std::string fileId = requestJson->get("fileId", "").asString();
    if (fileId.empty()) {
        ERR("Missing fileId for file preview, requestId: {}", requestId);
        sendErrorResponse(res, requestId, ERROR_CODE_PARAM_INVALID, MISSING_PARAM_MESSAGE,
                          HTTP_STATUS_BAD_REQUEST);
        return;
    }

    // 2. 会话鉴权，鉴权成功后可获取当前登录用户ID
    std::string userId;
    if (!validateSession(requestId, sessionId, res, userId)) {
        return;
    }

    // 3. 调用文件子服务预览Excel文件接口
    auto channel = getChannel(FLAGS_fileServiceName);
    if (!channel) {
        sendErrorResponse(res, requestId, ERROR_CODE_SERVICE_UNAVAILABLE, SERVICE_UNAVAILABLE_MESSAGE);
        return;
    }

    chat2Data::fileService::PreviewExcelRequest rpcRequest;
    rpcRequest.set_request_id(requestId);
    rpcRequest.set_session_id(sessionId);
    rpcRequest.set_file_id(fileId);
    rpcRequest.set_user_id(userId);
    // 分页参数可选，未传时由文件子服务使用默认值
    if (requestJson->isMember("pageNumber")) {
        rpcRequest.set_page_number(requestJson->get("pageNumber", 0).asInt());
    }
    if (requestJson->isMember("pageSize")) {
        rpcRequest.set_page_size(requestJson->get("pageSize", 0).asInt());
    }

    chat2Data::fileService::PreviewExcelResponse rpcResponse;
    brpc::Controller controller;
    chat2Data::fileService::FileService_Stub stub(channel.get());
    stub.PreviewExcel(&controller, &rpcRequest, &rpcResponse, nullptr);

    if (controller.Failed()) {
        ERR("PreviewExcel RPC failed, requestId: {}, reason: {}", requestId, controller.ErrorText());
        sendErrorResponse(res, requestId, ERROR_CODE_SERVICE_UNAVAILABLE, SERVICE_UNAVAILABLE_MESSAGE);
        return;
    }
    if (rpcResponse.error_code() != 0) {
        sendErrorResponse(res, rpcResponse.request_id(), rpcResponse.error_code(), rpcResponse.error_msg());
        return;
    }

    // 4. 解析RPC响应并构建HTTP响应体
    Json::Value responseJson;
    responseJson["requestId"] = rpcResponse.request_id();
    responseJson["errorCode"] = rpcResponse.error_code();
    responseJson["errorMsg"] = rpcResponse.error_msg();
    if (rpcResponse.has_result()) {
        const auto& result = rpcResponse.result();
        Json::Value resultJson;
        resultJson["fileId"] = result.file_id();
        resultJson["fileName"] = result.file_name();
        resultJson["fileSize"] = Json::Int64(result.file_size());
        resultJson["fileExt"] = result.file_ext();

        Json::Value sheetsJson(Json::arrayValue);
        for (int i = 0; i < result.excel_data().sheets_size(); ++i) {
            const auto& sheet = result.excel_data().sheets(i);
            Json::Value sheetJson;
            sheetJson["name"] = sheet.name();
            sheetJson["totalRows"] = sheet.total_rows();
            sheetJson["colCount"] = sheet.col_count();
            sheetJson["currentPage"] = sheet.current_page();
            sheetJson["totalPages"] = sheet.total_pages();
            sheetJson["pageSize"] = sheet.page_size();

            Json::Value columnsJson(Json::arrayValue);
            for (int j = 0; j < sheet.columns_size(); ++j) {
                columnsJson.append(sheet.columns(j));
            }
            sheetJson["columns"] = columnsJson;

            Json::Value dataJson(Json::arrayValue);
            for (int j = 0; j < sheet.data_size(); ++j) {
                Json::Value rowJson(Json::arrayValue);
                const auto& row = sheet.data(j);
                for (int k = 0; k < row.cells_size(); ++k) {
                    rowJson.append(row.cells(k));
                }
                dataJson.append(rowJson);
            }
            sheetJson["data"] = dataJson;
            sheetsJson.append(sheetJson);
        }
        resultJson["excelData"]["sheets"] = sheetsJson;
        responseJson["result"] = resultJson;
    }

    auto jsonStr = biteutil::JSON::serialize(responseJson);
    if (!jsonStr) {
        ERR("Failed to serialize file preview response, requestId: {}", requestId);
        sendErrorResponse(res, requestId, ERROR_CODE_INTERNAL_ERROR, INTERNAL_ERROR_MESSAGE,
                          HTTP_STATUS_INTERNAL_SERVER_ERROR);
        return;
    }
    sendJsonResponse(res, jsonStr.value());
    INF("File preview request handled, requestId: {}, fileId: {}", requestId, fileId);
}

void GatewayServiceImpl::handleGetFileList(const httplib::Request& req, httplib::Response& res) {
    // 1. 解析HTTP请求参数
    auto requestJson = biteutil::JSON::unserialize(req.body);
    if (!requestJson) {
        ERR("Failed to parse file list request body");
        sendErrorResponse(res, "", ERROR_CODE_PARAM_INVALID, INVALID_BODY_MESSAGE, HTTP_STATUS_BAD_REQUEST);
        return;
    }
    const std::string requestId = requestJson->get("requestId", "").asString();
    const std::string sessionId = requestJson->get("sessionId", "").asString();

    // 2. 会话鉴权，鉴权成功后可获取当前登录用户ID
    std::string userId;
    if (!validateSession(requestId, sessionId, res, userId)) {
        return;
    }

    // 3. 调用文件子服务获取文件列表接口
    auto channel = getChannel(FLAGS_fileServiceName);
    if (!channel) {
        sendErrorResponse(res, requestId, ERROR_CODE_SERVICE_UNAVAILABLE, SERVICE_UNAVAILABLE_MESSAGE);
        return;
    }

    chat2Data::fileService::GetFileListRequest rpcRequest;
    rpcRequest.set_request_id(requestId);
    rpcRequest.set_session_id(sessionId);
    rpcRequest.set_user_id(userId);

    chat2Data::fileService::GetFileListResponse rpcResponse;
    brpc::Controller controller;
    chat2Data::fileService::FileService_Stub stub(channel.get());
    stub.GetFileList(&controller, &rpcRequest, &rpcResponse, nullptr);

    if (controller.Failed()) {
        ERR("GetFileList RPC failed, requestId: {}, reason: {}", requestId, controller.ErrorText());
        sendErrorResponse(res, requestId, ERROR_CODE_SERVICE_UNAVAILABLE, SERVICE_UNAVAILABLE_MESSAGE);
        return;
    }
    if (rpcResponse.error_code() != 0) {
        sendErrorResponse(res, rpcResponse.request_id(), rpcResponse.error_code(), rpcResponse.error_msg());
        return;
    }

    // 4. 解析RPC响应并构建HTTP响应体
    Json::Value responseJson;
    responseJson["requestId"] = rpcResponse.request_id();
    responseJson["errorCode"] = rpcResponse.error_code();
    responseJson["errorMsg"] = rpcResponse.error_msg();
    if (rpcResponse.has_result()) {
        Json::Value fileListJson(Json::arrayValue);
        const auto& fileList = rpcResponse.result().file_list();
        for (int i = 0; i < fileList.size(); ++i) {
            const auto& fileItem = fileList.Get(i);
            Json::Value itemJson;
            itemJson["fileId"] = fileItem.file_id();
            itemJson["fileName"] = fileItem.file_name();
            itemJson["fileSize"] = Json::Int64(fileItem.file_size());
            itemJson["uploadTime"] = Json::Int64(fileItem.upload_time());
            itemJson["chatSessionId"] = fileItem.chat_session_id();
            fileListJson.append(itemJson);
        }
        responseJson["result"]["fileList"] = fileListJson;
    }

    auto jsonStr = biteutil::JSON::serialize(responseJson);
    if (!jsonStr) {
        ERR("Failed to serialize file list response, requestId: {}", requestId);
        sendErrorResponse(res, requestId, ERROR_CODE_INTERNAL_ERROR, INTERNAL_ERROR_MESSAGE,
                          HTTP_STATUS_INTERNAL_SERVER_ERROR);
        return;
    }
    sendJsonResponse(res, jsonStr.value());
    INF("Get file list request handled, requestId: {}, userId: {}", requestId, userId);
}

void GatewayServiceImpl::handleMapFileChatSession(const httplib::Request& req, httplib::Response& res) {
    // 1. 解析HTTP请求参数
    auto requestJson = biteutil::JSON::unserialize(req.body);
    if (!requestJson) {
        ERR("Failed to parse file chat map request body");
        sendErrorResponse(res, "", ERROR_CODE_PARAM_INVALID, INVALID_BODY_MESSAGE, HTTP_STATUS_BAD_REQUEST);
        return;
    }
    const std::string requestId = requestJson->get("requestId", "").asString();
    const std::string sessionId = requestJson->get("sessionId", "").asString();
    const std::string fileId = requestJson->get("fileId", "").asString();
    const std::string chatSessionId = requestJson->get("chatSessionId", "").asString();
    if (fileId.empty() || chatSessionId.empty()) {
        ERR("Missing fileId or chatSessionId for file chat map, requestId: {}", requestId);
        sendErrorResponse(res, requestId, ERROR_CODE_PARAM_INVALID, MISSING_PARAM_MESSAGE,
                          HTTP_STATUS_BAD_REQUEST);
        return;
    }

    // 2. 会话鉴权，鉴权成功后可获取当前登录用户ID
    std::string userId;
    if (!validateSession(requestId, sessionId, res, userId)) {
        return;
    }

    // 3. 调用文件子服务关联文件和聊天会话接口
    auto channel = getChannel(FLAGS_fileServiceName);
    if (!channel) {
        sendErrorResponse(res, requestId, ERROR_CODE_SERVICE_UNAVAILABLE, SERVICE_UNAVAILABLE_MESSAGE);
        return;
    }

    chat2Data::fileService::HandleFileChatSessionMapRequest rpcRequest;
    rpcRequest.set_request_id(requestId);
    rpcRequest.set_session_id(sessionId);
    rpcRequest.set_file_id(fileId);
    rpcRequest.set_chat_session_id(chatSessionId);
    rpcRequest.set_user_id(userId);

    chat2Data::fileService::HandleFileChatSessionMapResponse rpcResponse;
    brpc::Controller controller;
    chat2Data::fileService::FileService_Stub stub(channel.get());
    stub.HandleFileChatSessionMap(&controller, &rpcRequest, &rpcResponse, nullptr);

    if (controller.Failed()) {
        ERR("HandleFileChatSessionMap RPC failed, requestId: {}, reason: {}", requestId,
            controller.ErrorText());
        sendErrorResponse(res, requestId, ERROR_CODE_SERVICE_UNAVAILABLE, SERVICE_UNAVAILABLE_MESSAGE);
        return;
    }
    if (rpcResponse.error_code() != 0) {
        sendErrorResponse(res, rpcResponse.request_id(), rpcResponse.error_code(), rpcResponse.error_msg());
        return;
    }

    // 4. 解析RPC响应并构建HTTP响应体
    Json::Value responseJson;
    responseJson["requestId"] = rpcResponse.request_id();
    responseJson["errorCode"] = rpcResponse.error_code();
    responseJson["errorMsg"] = rpcResponse.error_msg();

    auto jsonStr = biteutil::JSON::serialize(responseJson);
    if (!jsonStr) {
        ERR("Failed to serialize file chat map response, requestId: {}", requestId);
        sendErrorResponse(res, requestId, ERROR_CODE_INTERNAL_ERROR, INTERNAL_ERROR_MESSAGE,
                          HTTP_STATUS_INTERNAL_SERVER_ERROR);
        return;
    }
    sendJsonResponse(res, jsonStr.value());
    INF("File chat map request handled, requestId: {}, fileId: {}", requestId, fileId);
}

void GatewayServiceImpl::handleUploadSqliteFile(const httplib::Request& req, httplib::Response& res) {
    // 1. 解析HTTP请求参数（参数位于URL中，文件数据位于请求体）
    if (!req.has_param("requestId") || !req.has_param("sessionId") || !req.has_param("filename")) {
        ERR("Missing required parameters for sqlite upload");
        sendErrorResponse(res, "", ERROR_CODE_PARAM_INVALID, MISSING_PARAM_MESSAGE, HTTP_STATUS_BAD_REQUEST);
        return;
    }
    const std::string requestId = req.get_param_value("requestId");
    const std::string sessionId = req.get_param_value("sessionId");
    const std::string filename = req.get_param_value("filename");
    if (req.body.empty()) {
        ERR("Empty file data for sqlite upload, requestId: {}", requestId);
        sendErrorResponse(res, requestId, ERROR_CODE_PARAM_INVALID, INVALID_BODY_MESSAGE,
                          HTTP_STATUS_BAD_REQUEST);
        return;
    }

    // 2. 会话鉴权，鉴权成功后可获取当前登录用户ID
    std::string userId;
    if (!validateSession(requestId, sessionId, res, userId)) {
        return;
    }

    // 3. 调用文件子服务上传SQLite文件接口，文件数据通过attachment传输
    auto channel = getChannel(FLAGS_fileServiceName);
    if (!channel) {
        sendErrorResponse(res, requestId, ERROR_CODE_SERVICE_UNAVAILABLE, SERVICE_UNAVAILABLE_MESSAGE);
        return;
    }

    chat2Data::fileService::UploadSQLiteFileRequest rpcRequest;
    rpcRequest.set_request_id(requestId);
    rpcRequest.set_session_id(sessionId);
    rpcRequest.set_filename(filename);
    rpcRequest.set_user_id(userId);

    chat2Data::fileService::UploadSQLiteFileResponse rpcResponse;
    brpc::Controller controller;
    controller.request_attachment().append(req.body);
    chat2Data::fileService::FileService_Stub stub(channel.get());
    stub.UploadSQLiteFile(&controller, &rpcRequest, &rpcResponse, nullptr);

    if (controller.Failed()) {
        ERR("UploadSQLiteFile RPC failed, requestId: {}, reason: {}", requestId, controller.ErrorText());
        sendErrorResponse(res, requestId, ERROR_CODE_SERVICE_UNAVAILABLE, SERVICE_UNAVAILABLE_MESSAGE);
        return;
    }
    if (rpcResponse.error_code() != 0) {
        sendErrorResponse(res, rpcResponse.request_id(), rpcResponse.error_code(), rpcResponse.error_msg());
        return;
    }

    // 4. 解析RPC响应并构建HTTP响应体
    Json::Value responseJson;
    responseJson["requestId"] = rpcResponse.request_id();
    responseJson["errorCode"] = rpcResponse.error_code();
    responseJson["errorMsg"] = rpcResponse.error_msg();
    if (rpcResponse.has_result()) {
        Json::Value resultJson;
        resultJson["fileId"] = rpcResponse.result().file_id();
        responseJson["result"] = resultJson;
    }

    auto jsonStr = biteutil::JSON::serialize(responseJson);
    if (!jsonStr) {
        ERR("Failed to serialize sqlite upload response, requestId: {}", requestId);
        sendErrorResponse(res, requestId, ERROR_CODE_INTERNAL_ERROR, INTERNAL_ERROR_MESSAGE,
                          HTTP_STATUS_INTERNAL_SERVER_ERROR);
        return;
    }
    sendJsonResponse(res, jsonStr.value());
    INF("SQLite upload request handled, requestId: {}, userId: {}", requestId, userId);
}

// 数据库子服务接口（待实现）
void GatewayServiceImpl::handleConnectDatabase(const httplib::Request&, httplib::Response& res) {
    sendNotImplemented(res);
}

void GatewayServiceImpl::handleDisconnectDatabase(const httplib::Request&, httplib::Response& res) {
    sendNotImplemented(res);
}

void GatewayServiceImpl::handleGetTableList(const httplib::Request&, httplib::Response& res) {
    sendNotImplemented(res);
}

void GatewayServiceImpl::handleGetTableData(const httplib::Request&, httplib::Response& res) {
    sendNotImplemented(res);
}

void GatewayServiceImpl::handleGetConnectionStatus(const httplib::Request&, httplib::Response& res) {
    sendNotImplemented(res);
}

// AI子服务接口（待实现）
void GatewayServiceImpl::handleGetModelList(const httplib::Request&, httplib::Response& res) {
    sendNotImplemented(res);
}

void GatewayServiceImpl::handleCreateChatSession(const httplib::Request&, httplib::Response& res) {
    sendNotImplemented(res);
}

void GatewayServiceImpl::handleGetChatSessionList(const httplib::Request&, httplib::Response& res) {
    sendNotImplemented(res);
}

void GatewayServiceImpl::handleGetChatHistory(const httplib::Request&, httplib::Response& res) {
    sendNotImplemented(res);
}

void GatewayServiceImpl::handleDeleteChatSession(const httplib::Request&, httplib::Response& res) {
    sendNotImplemented(res);
}

void GatewayServiceImpl::handleSendStreamMessage(const httplib::Request&, httplib::Response& res) {
    sendNotImplemented(res);
}

} // namespace GatewayService
