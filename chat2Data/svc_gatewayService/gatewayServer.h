#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <httplib.h>

#include "gatewayServiceImpl.h"

namespace GatewayService {

/**
 * @brief 网关HTTP服务器类
 *
 * 该类只负责HTTP服务器的生命周期管理（启动、停止、运行状态查询），
 * 不承载业务逻辑；接口定义与路由绑定由GatewayServiceImpl负责。
 */
class GatewayServer {
public:
    /**
     * @brief 构造函数
     * @param host 服务器绑定地址
     * @param port 服务器绑定端口
     * @param server cpp-httplib的HTTP服务器对象
     * @param serviceImpl HTTP接口定义对象
     *
     * @note 构造方法中完成服务器实例与接口定义实例的初始化
     */
    GatewayServer(const std::string& host, int port, std::shared_ptr<httplib::Server> server,
                  std::shared_ptr<GatewayServiceImpl> serviceImpl);

    /**
     * @brief 析构函数
     *
     * @note 对象销毁时自动调用stop()停止服务器
     */
    ~GatewayServer();

    /**
     * @brief 启动服务器
     * @return 启动成功返回true，服务器已在运行或实例无效返回false
     *
     * @note 服务器在独立线程中启动，避免阻塞主线程
     */
    bool start();

    /**
     * @brief 停止服务器
     *
     * @note 服务器未运行时直接返回，运行中则停止并回收服务器线程
     */
    void stop();

    /**
     * @brief 获取服务器运行状态
     * @return 服务器正在运行返回true，否则返回false
     */
    bool isRunning() const;

private:
    std::string _host;                                       // 服务器绑定地址
    int _port;                                               // 服务器绑定端口
    std::shared_ptr<httplib::Server> _server;                // HTTP服务器实例
    std::shared_ptr<GatewayServiceImpl> _serviceImpl;        // HTTP接口定义实例
    std::atomic<bool> _isRunning;                            // 服务器运行状态标志
    std::thread _serverThread;                               // 服务器运行线程
};

/**
 * @brief 网关服务器构建器
 *
 * 负责服务发现组件与网关服务器的构建，采用Builder模式封装构建流程。
 */
class GatewayServerBuilder {
public:
    /**
     * @brief 构造函数
     * @param etcdAddr 注册中心ETCD地址
     * @param host 网关服务器绑定地址
     * @param port 网关服务器绑定端口
     * @param serviceNames 需要监控的子服务名称列表
     */
    GatewayServerBuilder(const std::string& etcdAddr, const std::string& host, int port,
                         const std::vector<std::string>& serviceNames);

    /**
     * @brief 构建并启动网关服务器
     * @return 构建并启动成功返回网关服务器对象，失败返回空指针
     */
    std::shared_ptr<GatewayServer> build();

private:
    std::string _etcdAddr;                       // 注册中心ETCD地址
    std::string _host;                           // 网关服务器绑定地址
    int _port;                                   // 网关服务器绑定端口
    std::vector<std::string> _serviceNames;      // 需要监控的子服务名称列表
};

} // namespace GatewayService
