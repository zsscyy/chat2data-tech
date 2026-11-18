#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <vector>

namespace brpc {
class Server;
} // namespace brpc

namespace bitesvc {
class SvcProvider;
} // namespace bitesvc

namespace excelParserService {

class ExcelParserServiceImpl;

/**
 * @brief 注册中心配置：etcd地址、服务名称、服务地址
 */
struct RegisterCenterConfig {
    std::string _etcdAddr;    // etcd地址
    std::string _serviceName; // 服务名称
    std::string _serviceAddr; // 服务地址
};

/**
 * @brief FastDFS 配置：tracker 列表以及连接、网络超时
 */
struct FdfsConfig {
    std::vector<std::string> _trackers; // tracker服务器列表
    int _connectTimeout = 30;           // 连接超时时间（秒）
    int _networkTimeout = 30;           // 网络超时时间（秒）
};

/**
 * @brief Excel 解析子服务 RPC 服务器
 *
 * 负责 RPC 服务器的启动、停止以及 etcd 服务注册；
 * 各对象均由 ExcelParserServerBuilder 组装后注入。
 */
class ExcelParserServer {
public:
    /**
     * @brief 构造 Excel 解析 RPC 服务器
     * @param serviceImpl RPC 接口实现对象（生命周期由 brpc 服务器接管，这里只保留非拥有指针）
     * @param server      brpc 服务器实例（已由 RpcServerFactory 创建并启动）
     * @param svcProvider 服务注册对象
     */
    ExcelParserServer(ExcelParserServiceImpl* serviceImpl, std::shared_ptr<brpc::Server> server,
                      std::shared_ptr<bitesvc::SvcProvider> svcProvider);

    ~ExcelParserServer();

    /**
     * @brief 注册服务到 etcd
     * @return 注册成功返回 true
     */
    bool start();

    /**
     * @brief 停止 RPC 服务器
     */
    void stop();

    /**
     * @brief 查询服务器是否处于运行状态
     * @return 运行中返回 true
     */
    bool isRunning() const;

private:
    ExcelParserServiceImpl* _serviceImpl;               // RPC 接口实现（brpc 服务器拥有所有权）
    std::shared_ptr<brpc::Server> _server;              // brpc 服务器
    std::shared_ptr<bitesvc::SvcProvider> _svcProvider; // 服务注册对象
    std::atomic<bool> _isRunning;                       // 运行状态
};

/**
 * @brief Excel 解析子服务构建器
 *
 * 使用建造者模式设置端口、FastDFS 配置与注册中心配置，
 * 由 build() 统一完成 FastDFS 初始化、业务层、RPC 接口、RPC 服务器与服务注册对象的组装。
 */
class ExcelParserServerBuilder {
public:
    /**
     * @brief 设置 RPC 服务器监听端口
     */
    ExcelParserServerBuilder& setPort(int port);

    /**
     * @brief 设置 FastDFS 配置
     */
    ExcelParserServerBuilder& setFdfsConfig(const FdfsConfig& config);

    /**
     * @brief 设置注册中心配置（etcd地址、服务名称、服务地址）
     */
    ExcelParserServerBuilder& setRegisterCenterConfig(const RegisterCenterConfig& config);

    /**
     * @brief 组装并返回 RPC 服务器
     * @return 构建成功返回服务器实例，失败返回空
     */
    std::shared_ptr<ExcelParserServer> build();

private:
    int _port = 0;                              // RPC服务器端口
    FdfsConfig _fdfsConfig;                     // FastDFS配置
    RegisterCenterConfig _registerCenterConfig; // 注册中心配置
};

} // namespace excelParserService
