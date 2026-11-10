#include <chrono>
#include <utility>

#include <jsoncpp/json/json.h>
#include <log.h>
#include <util.h>

#include "gatewayServiceImpl.h"

namespace GatewayService {

namespace {
// HTTP成功状态码
const int HTTP_STATUS_OK = 200;
// HTTP服务内部错误状态码
const int HTTP_STATUS_INTERNAL_SERVER_ERROR = 500;
// 未实现接口的HTTP状态码
const int HTTP_STATUS_NOT_IMPLEMENTED = 501;
} // namespace

GatewayServiceImpl::GatewayServiceImpl(biterpc::SvcChannels::ptr svcChannels)
    : _svcChannels(std::move(svcChannels)) {
    if (!_svcChannels) {
        ERR("GatewayServiceImpl constructed with empty SvcChannels!");
        return;
    }
    INF("GatewayServiceImpl initialized successfully");
}

void GatewayServiceImpl::bindRoutes(httplib::Server& server) {
    // 骨架阶段仅注册已实现的健康检测接口
    server.Get("/health", [this](const httplib::Request& req, httplib::Response& res) {
        handleHealthCheck(req, res);
    });
    INF("Gateway HTTP route bound: GET /health");

    // 其余29个业务接口待实现后再注册，路由清单如下：
    // 用户: POST /api/user/valid/nickname、/valid/email、/register、/passwd/login、/code、
    //       /vcode/login、/session/login、/logout、/info
    // 文件: POST /api/file/upload/info、/upload、/preview、/list、/chat/map、/sqlite/upload
    //       GET /api/file/info、/download；DELETE /api/file/{fileId}
    // 数据库: POST /api/db/connect、/disconnect、/table/data、/connection/status
    //         GET /api/db/tables
    // AI: POST /api/ai/models、/session/create、/chatSessionLists、/history、/delete、/sendStreamMessage
    WRN("29 business interfaces are not implemented yet, routes are not bound");
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
        res.set_content(R"({"status":"unhealthy"})", "application/json");
        return;
    }

    res.status = HTTP_STATUS_OK;
    res.set_content(jsonStr.value(), "application/json");
    DBG("Health check request handled, path: {}", req.path);
}

void GatewayServiceImpl::sendNotImplemented(httplib::Response& res) {
    Json::Value response;
    response["errorMsg"] = "接口暂未实现";

    auto jsonStr = biteutil::JSON::serialize(response);
    res.status = HTTP_STATUS_NOT_IMPLEMENTED;
    res.set_content(jsonStr.value_or(R"({"errorMsg":"接口暂未实现"})"), "application/json");
    WRN("Request reached an unimplemented interface");
}

// 用户子服务接口（待实现）
void GatewayServiceImpl::handleCheckNicknameUnique(const httplib::Request&, httplib::Response& res) {
    sendNotImplemented(res);
}

void GatewayServiceImpl::handleCheckEmailUnique(const httplib::Request&, httplib::Response& res) {
    sendNotImplemented(res);
}

void GatewayServiceImpl::handleRegisterUser(const httplib::Request&, httplib::Response& res) {
    sendNotImplemented(res);
}

void GatewayServiceImpl::handlePasswordLogin(const httplib::Request&, httplib::Response& res) {
    sendNotImplemented(res);
}

void GatewayServiceImpl::handleGetVerifyCode(const httplib::Request&, httplib::Response& res) {
    sendNotImplemented(res);
}

void GatewayServiceImpl::handleVerifyCodeLogin(const httplib::Request&, httplib::Response& res) {
    sendNotImplemented(res);
}

void GatewayServiceImpl::handleSessionLogin(const httplib::Request&, httplib::Response& res) {
    sendNotImplemented(res);
}

void GatewayServiceImpl::handleLogout(const httplib::Request&, httplib::Response& res) {
    sendNotImplemented(res);
}

void GatewayServiceImpl::handleGetUserInfo(const httplib::Request&, httplib::Response& res) {
    sendNotImplemented(res);
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
