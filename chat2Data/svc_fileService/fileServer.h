#pragma once

#include <atomic>
#include <memory>
#include <string>

namespace brpc {
class Server;
} // namespace brpc

namespace bitesvc {
class SvcWatcher;
class SvcProvider;
} // namespace bitesvc

namespace fileService {

class FileServiceImpl;

/**
 * @brief 文件子服务 RPC 服务器
 *
 * 只负责 RPC 服务器的启动、停止，以及 etcd 服务的注册与注销；
 * 业务对象与服务器实例均由 FileServerBuilder 组装后注入。
 */
class FileServer {
public:
    /**
     * @brief 构造文件 RPC 服务器
     * @param serviceImpl RPC 接口实现对象（生命周期由 brpc 服务器接管，这里只保留非拥有指针）
     * @param server      brpc 服务器实例（已由 RpcServerFactory 创建并启动）
     * @param svcWatcher  服务发现对象
     * @param svcProvider 服务注册对象
     */
    FileServer(FileServiceImpl* serviceImpl, std::shared_ptr<brpc::Server> server,
               std::shared_ptr<bitesvc::SvcWatcher> svcWatcher,
               std::shared_ptr<bitesvc::SvcProvider> svcProvider);

    ~FileServer();

    /**
     * @brief 注册服务到 etcd，标记服务器进入运行状态
     * @return 注册成功返回 true
     */
    bool start();

    /**
     * @brief 注销服务并停止 RPC 服务器
     */
    void stop();

    /**
     * @brief 查询服务器是否处于运行状态
     * @return 运行中返回 true
     */
    bool isRunning() const;

private:
    FileServiceImpl* _serviceImpl;                      // RPC 接口实现（brpc 服务器拥有所有权）
    std::shared_ptr<brpc::Server> _server;              // brpc 服务器
    std::shared_ptr<bitesvc::SvcWatcher> _svcWatcher;   // 服务发现对象
    std::shared_ptr<bitesvc::SvcProvider> _svcProvider; // 服务注册对象
    std::atomic<bool> _isRunning;                       // 运行状态
};

} // namespace fileService
