#pragma once

#include <memory>
#include <optional>
#include <string>

#include <httplib.h>
#include <bite_scaffold/rpc.h>

namespace GatewayService {

/**
 * @brief 网关HTTP接口定义与路由绑定类
 *
 * 该类负责30个HTTP接口的定义，与HTTP服务器搭建解耦。
 * 骨架阶段仅注册已实现的健康检测接口，其余业务接口待实现后再注册路由。
 * 同时持有服务信道管理对象，供各业务接口实现时获取RPC服务Channel。
 */
class GatewayServiceImpl {
public:
    /**
     * @brief 构造函数
     * @param svcChannels 服务信道管理对象，按服务名管理RPC服务节点
     */
    explicit GatewayServiceImpl(biterpc::SvcChannels::ptr svcChannels);

    /**
     * @brief 析构函数
     */
    ~GatewayServiceImpl() = default;

    /**
     * @brief 绑定HTTP路由
     * @param server cpp-httplib的HTTP服务器对象
     *
     * @note 骨架阶段仅绑定健康检测接口，未实现的业务接口不注册
     */
    void bindRoutes(httplib::Server& server);

    /**
     * @brief 获取指定服务的RPC信道
     * @param serviceName 服务名称
     * @return 服务可用时返回信道，否则返回空指针
     */
    biterpc::ChannelPtr getChannel(const std::string& serviceName);

    /**
     * @brief 获取指定服务的一个节点地址
     * @param serviceName 服务名称
     * @return 服务可用时返回节点地址，否则返回空
     */
    std::optional<std::string> getNodeAddr(const std::string& serviceName);

private:
    /**
     * @brief 处理健康检测请求
     * @param req HTTP请求对象
     * @param res HTTP响应对象
     */
    void handleHealthCheck(const httplib::Request& req, httplib::Response& res);

    // 用户子服务接口（9个，待实现）
    // POST /api/user/valid/nickname
    void handleCheckNicknameUnique(const httplib::Request& req, httplib::Response& res);
    // POST /api/user/valid/email
    void handleCheckEmailUnique(const httplib::Request& req, httplib::Response& res);
    // POST /api/user/register
    void handleRegisterUser(const httplib::Request& req, httplib::Response& res);
    // POST /api/user/passwd/login
    void handlePasswordLogin(const httplib::Request& req, httplib::Response& res);
    // POST /api/user/code
    void handleGetVerifyCode(const httplib::Request& req, httplib::Response& res);
    // POST /api/user/vcode/login
    void handleVerifyCodeLogin(const httplib::Request& req, httplib::Response& res);
    // POST /api/user/session/login
    void handleSessionLogin(const httplib::Request& req, httplib::Response& res);
    // POST /api/user/logout
    void handleLogout(const httplib::Request& req, httplib::Response& res);
    // POST /api/user/info
    void handleGetUserInfo(const httplib::Request& req, httplib::Response& res);

    // 文件子服务接口（9个，待实现）
    // POST /api/file/upload/info
    void handleUploadFileInfo(const httplib::Request& req, httplib::Response& res);
    // GET /api/file/info
    void handleGetFileInfo(const httplib::Request& req, httplib::Response& res);
    // POST /api/file/upload
    void handleUploadFile(const httplib::Request& req, httplib::Response& res);
    // GET /api/file/download
    void handleDownloadFile(const httplib::Request& req, httplib::Response& res);
    // DELETE /api/file/{fileId}
    void handleDeleteFile(const httplib::Request& req, httplib::Response& res);
    // POST /api/file/preview
    void handlePreviewExcel(const httplib::Request& req, httplib::Response& res);
    // POST /api/file/list
    void handleGetFileList(const httplib::Request& req, httplib::Response& res);
    // POST /api/file/chat/map
    void handleMapFileChatSession(const httplib::Request& req, httplib::Response& res);
    // POST /api/file/sqlite/upload
    void handleUploadSqliteFile(const httplib::Request& req, httplib::Response& res);

    // 数据库子服务接口（5个，待实现）
    // POST /api/db/connect
    void handleConnectDatabase(const httplib::Request& req, httplib::Response& res);
    // POST /api/db/disconnect
    void handleDisconnectDatabase(const httplib::Request& req, httplib::Response& res);
    // GET /api/db/tables
    void handleGetTableList(const httplib::Request& req, httplib::Response& res);
    // POST /api/db/table/data
    void handleGetTableData(const httplib::Request& req, httplib::Response& res);
    // POST /api/db/connection/status
    void handleGetConnectionStatus(const httplib::Request& req, httplib::Response& res);

    // AI子服务接口（6个，待实现）
    // POST /api/ai/models
    void handleGetModelList(const httplib::Request& req, httplib::Response& res);
    // POST /api/ai/session/create
    void handleCreateChatSession(const httplib::Request& req, httplib::Response& res);
    // POST /api/ai/chatSessionLists
    void handleGetChatSessionList(const httplib::Request& req, httplib::Response& res);
    // POST /api/ai/history
    void handleGetChatHistory(const httplib::Request& req, httplib::Response& res);
    // POST /api/ai/delete
    void handleDeleteChatSession(const httplib::Request& req, httplib::Response& res);
    // POST /api/ai/sendStreamMessage
    void handleSendStreamMessage(const httplib::Request& req, httplib::Response& res);

    /**
     * @brief 返回未实现占位响应
     * @param res HTTP响应对象
     *
     * @note 响应状态码501，响应体仅包含errorMsg，不占用业务错误码号段
     */
    void sendNotImplemented(httplib::Response& res);

private:
    biterpc::SvcChannels::ptr _svcChannels; // 服务信道管理对象
};

} // namespace GatewayService
