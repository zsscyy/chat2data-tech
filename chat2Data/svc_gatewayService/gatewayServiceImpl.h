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
 * 该类负责HTTP接口的定义，与HTTP服务器搭建解耦。
 * 当前已实现健康检测接口与用户子服务的9个业务接口，其余业务接口待实现后再注册路由。
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
     * @note 绑定路由前先设置前端静态资源目录，未实现的业务接口不注册
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
     * @brief 发送JSON响应
     * @param res HTTP响应对象
     * @param jsonData JSON字符串
     * @param statusCode HTTP状态码
     */
    void sendJsonResponse(httplib::Response& res, const std::string& jsonData, int statusCode = 200);

    /**
     * @brief 发送错误响应
     * @param res HTTP响应对象
     * @param requestId 请求ID
     * @param errorCode 业务错误码
     * @param errorMsg 错误描述
     * @param statusCode HTTP状态码
     */
    void sendErrorResponse(httplib::Response& res, const std::string& requestId, int errorCode,
                           const std::string& errorMsg, int statusCode = 200);

    /**
     * @brief 通过用户子服务检测会话是否有效
     * @param requestId 请求ID
     * @param sessionId 会话ID
     * @param res HTTP响应对象，鉴权失败时直接写入错误响应
     * @param userId 鉴权成功时通过引用参数带出用户ID
     * @return 鉴权成功返回true，失败返回false
     */
    bool validateSession(const std::string& requestId, const std::string& sessionId, httplib::Response& res,
                         std::string& userId);

    /**
     * @brief 处理健康检测请求
     * @param req HTTP请求对象
     * @param res HTTP响应对象
     */
    void handleHealthCheck(const httplib::Request& req, httplib::Response& res);

    // 用户子服务接口（9个）
    /**
     * @brief 检测用户昵称是否唯一
     * @param req HTTP请求对象
     * @param res HTTP响应对象
     * @note 接口：POST /api/user/valid/nickname
     */
    void handleValidNickname(const httplib::Request& req, httplib::Response& res);

    /**
     * @brief 检测用户邮箱是否唯一
     * @param req HTTP请求对象
     * @param res HTTP响应对象
     * @note 接口：POST /api/user/valid/email
     */
    void handleValidEmail(const httplib::Request& req, httplib::Response& res);

    /**
     * @brief 用户注册
     * @param req HTTP请求对象
     * @param res HTTP响应对象
     * @note 接口：POST /api/user/register
     */
    void handleUserRegister(const httplib::Request& req, httplib::Response& res);

    /**
     * @brief 密码登录
     * @param req HTTP请求对象
     * @param res HTTP响应对象
     * @note 接口：POST /api/user/passwd/login
     */
    void handlePasswordLogin(const httplib::Request& req, httplib::Response& res);

    /**
     * @brief 获取验证码
     * @param req HTTP请求对象
     * @param res HTTP响应对象
     * @note 接口：POST /api/user/code
     */
    void handleGetVerifyCode(const httplib::Request& req, httplib::Response& res);

    /**
     * @brief 验证码登录
     * @param req HTTP请求对象
     * @param res HTTP响应对象
     * @note 接口：POST /api/user/vcode/login
     */
    void handleVerifyCodeLogin(const httplib::Request& req, httplib::Response& res);

    /**
     * @brief 会话登录
     * @param req HTTP请求对象
     * @param res HTTP响应对象
     * @note 接口：POST /api/user/session/login
     */
    void handleSessionLogin(const httplib::Request& req, httplib::Response& res);

    /**
     * @brief 退出登录
     * @param req HTTP请求对象
     * @param res HTTP响应对象
     * @note 接口：POST /api/user/logout，调用RPC前先进行会话鉴权
     */
    void handleLogout(const httplib::Request& req, httplib::Response& res);

    /**
     * @brief 获取用户信息
     * @param req HTTP请求对象
     * @param res HTTP响应对象
     * @note 接口：/api/user/info?requestId={requestId}&sessionId={sessionId}&userId={userId}
     *       调用RPC前先进行会话鉴权
     */
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
    std::string _staticResourcesPath;       // 前端静态资源目录
};

} // namespace GatewayService
