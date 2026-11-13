#include <limits.h>
#include <unistd.h>

#include <chrono>
#include <utility>

#include <gflags/gflags.h>
#include <jsoncpp/json/json.h>
#include <log.h>
#include <util.h>

#include "gatewayServiceImpl.h"
#include "userService.pb.h"

// 声明main.cc中定义的gflags变量，用于获取用户子服务名称
DECLARE_string(userServiceName);

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

    // 其余21个业务接口待实现后再注册，路由清单如下：
    // 文件: POST /api/file/upload/info、/upload、/preview、/list、/chat/map、/sqlite/upload
    //       GET /api/file/info、/download；DELETE /api/file/{fileId}
    // 数据库: POST /api/db/connect、/disconnect、/table/data、/connection/status
    //         GET /api/db/tables
    // AI: POST /api/ai/models、/session/create、/chatSessionLists、/history、/delete、/sendStreamMessage
    WRN("21 business interfaces are not implemented yet, routes are not bound");
}

biterpc::ChannelPtr GatewayServiceImpl::getChannel(const std::string& serviceName) {
    if (!_svcChannels) {
        ERR("SvcChannels is null, cannot get channel for service: {}", serviceName);
        return nullptr;
    }

    auto channel = _svcChannels->getNode(serviceName);
    if (!channel) {
        WRN("No available node for service: {}", serviceName);
        return nullptr;
    }

    return channel;
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
    std::string userId = req.has_param("userId") ? req.get_param_value("userId") : "";

    // 2. 调用用户子服务前先进行会话鉴权，鉴权成功后可获取当前登录用户ID
    std::string authenticatedUserId;
    if (!validateSession(requestId, sessionId, res, authenticatedUserId)) {
        return;
    }
    if (userId.empty()) {
        userId = authenticatedUserId;
    }

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

// 文件子服务接口（待实现）
void GatewayServiceImpl::handleUploadFileInfo(const httplib::Request&, httplib::Response& res) {
    sendNotImplemented(res);
}

void GatewayServiceImpl::handleGetFileInfo(const httplib::Request&, httplib::Response& res) {
    sendNotImplemented(res);
}

void GatewayServiceImpl::handleUploadFile(const httplib::Request&, httplib::Response& res) {
    sendNotImplemented(res);
}

void GatewayServiceImpl::handleDownloadFile(const httplib::Request&, httplib::Response& res) {
    sendNotImplemented(res);
}

void GatewayServiceImpl::handleDeleteFile(const httplib::Request&, httplib::Response& res) {
    sendNotImplemented(res);
}

void GatewayServiceImpl::handlePreviewExcel(const httplib::Request&, httplib::Response& res) {
    sendNotImplemented(res);
}

void GatewayServiceImpl::handleGetFileList(const httplib::Request&, httplib::Response& res) {
    sendNotImplemented(res);
}

void GatewayServiceImpl::handleMapFileChatSession(const httplib::Request&, httplib::Response& res) {
    sendNotImplemented(res);
}

void GatewayServiceImpl::handleUploadSqliteFile(const httplib::Request&, httplib::Response& res) {
    sendNotImplemented(res);
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
