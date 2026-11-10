#pragma once

#include <string>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <httplib.h>
#include <ai_chat_sdk/ChatSDK.h>
#include "execlParser.h"

namespace chat2excel {

/**
 * @brief Chat2Excel HTTP服务器类
 * 
 * 该类负责搭建HTTP服务器，提供与前端web页面交互的功能。
 * 支持服务器的启动、停止和状态查询，以及健康检查等功能。
 */
class Chat2ExcelServer {
public:
    /**
     * @brief 构造函数
     * 
     * @param host 服务器绑定的地址，默认为"0.0.0.0"
     * @param port 服务器绑定的端口号，默认为9000
     */
    Chat2ExcelServer(const std::string& host = "0.0.0.0", int port = 9000);
    
    /**
     * @brief 析构函数
     */
    ~Chat2ExcelServer();
    
    /**
     * @brief 启动服务器
     * 
     * @return true 启动成功
     * @return false 启动失败
     * 
     * @note 启动前会检测服务器是否正在运行，如果正在运行则提示用户并返回false
     * @note 服务器在单独线程中启动，并与主线程分离，在后台运行
     */
    bool start();
    
    /**
     * @brief 停止服务器
     * 
     * @note 停止前会检测服务器是否正在运行，如果没有运行则提示用户并返回
     * @note 停止后会标记服务器的运行状态为未运行
     */
    void stop();
    
    /**
     * @brief 获取服务器运行状态
     * 
     * @return true 服务器正在运行
     * @return false 服务器未运行
     */
    bool isRunning() const;

private:
    /**
     * @brief 设置HTTP路由规则
     * 
     * 将前端请求路由到对应的处理函数
     * 包括健康检查路由等
     */
    void setRoutes();
    
    /**
     * @brief 处理健康检查请求
     * 
     * @param req HTTP请求对象
     * @param res HTTP响应对象
     * 
     * @note 响应内容：{"status": "healthy"}
     * @note 响应状态码：200 OK
     */
    void handleHealthCheck(const httplib::Request& req, httplib::Response& res);

    /**
     * @brief 处理文件上传请求
     * 
     * @param req HTTP请求对象
     * @param res HTTP响应对象
     * 
     * @note 接口：POST /api/uploadFile?fileName=xxx
     * @note 请求头：Content-Type: application/octet-stream
     * @note 请求体：文件二进制内容
     * @note 响应：{"success": true, "result": {"fileId": "1711234567"}}
     * @note 文件保存路径：chat2ExcelEasy/source/excelFile/
     * @note 文件ID：基于当前时间戳生成的唯一标识符
     */
    void handleUploadFile(const httplib::Request& req, httplib::Response& res);

    /**
     * @brief 发送错误响应
     * 
     * @param res HTTP响应对象
     * @param errorMessage 错误信息
     * @param statusCode HTTP状态码
     * 
     * @note 响应格式：{"success": false, "error": "错误信息"}
     */
    void sendErrorResponse(httplib::Response& res, const std::string& errorMessage, int statusCode);

    /**
     * @brief 解析Excel文件内容
     * 
     * @param fileId 文件唯一标识符
     * @return std::string 解析后的数据字符串
     *         - 格式：每个worksheet以"worksheetN: 工作表名"开头
     *         - 每个worksheet的数据以CSV格式展示
     *         - 多个worksheet之间用空行分隔
     * 
     * @note 如果fileId不存在、文件不存在或解析失败，返回空字符串
     * @note 使用bite_scaffold库的日志系统记录错误信息
     */
    std::string parseExcelFile(const std::string& fileId);

    /**
     * @brief 构建提示词
     * 
     * @param filename Excel文件名
     * @param excelData Excel文件解析后的数据内容
     * @param userQuestion 用户提出的问题
     * @return std::string 构建好的完整提示词
     * 
     * @note 根据预定义的提示词模板，将文件名、Excel数据和用户问题填充到模板中
     * @note 模板包含角色定义、输出要求、重要约束、数据上下文和用户问题等部分
     */
    std::string buildPrompt(const std::string& filename, const std::string& excelData, const std::string& userQuestion);

    /**
     * @brief 处理发送消息请求
     * 
     * @param req HTTP请求对象
     * @param res HTTP响应对象
     * 
     * @note 接口：POST /api/sendMessage
     * @note 请求头：Content-Type: application/json
     * @note 请求体：{"fileId": "file_Id", "message": "分析这个Excel文件"}
     * @note 响应：SSE流式响应，实时推送AI生成的文本片段
     * 
     * @note 流程：
     *   1. 解析请求体获取fileId和message
     *   2. 根据fileId解析Excel文件内容
     *   3. 构建提示词
     *   4. 通过ChatSDK发送消息给大模型
     *   5. 以SSE方式实时推送模型返回的流式结果
     */
    void handleSendMessage(const httplib::Request& req, httplib::Response& res);

    /**
     * @brief 从文件路径中提取文件名
     * 
     * @param filePath 文件完整路径
     * @return std::string 文件名（不含路径）
     */
    std::string getFileNameFromPath(const std::string& filePath);

    /**
     * @brief 初始化ChatSDK
     * 
     * @return true 初始化成功
     * @return false 初始化失败
     * 
     * @note 注册deepseek-chat模型，设置模型参数
     */
    bool initChatSDK();

private:
    std::string _host;                                      // 服务器绑定地址
    int _port;                                              // 服务器绑定端口
    std::unique_ptr<httplib::Server> _server;               // HTTP服务器实例
    std::atomic<bool> _isRunning;                           // 服务器运行状态标志
    std::unordered_map<std::string, std::string> _fileIdToPath;  // 文件ID到路径的映射
    std::mutex _fileMutex;                                  // 保护文件映射的互斥锁
    std::shared_ptr<ai_chat_sdk::ChatSDK> _chatSDK;         // ChatSDK实例，用于与大模型交互
    std::string _sessionId;                                 // 当前会话ID
};

}
