#pragma once

#include <atomic>
#include <memory>
#include <string>

#include "emailSender.h"

namespace brpc {
class Server;
} // namespace brpc

namespace bitesvc {
class SvcProvider;
} // namespace bitesvc

namespace notifyService {

class NotifyBusiness;
class NotifyServiceImpl;

/**
 * @brief 注册中心配置：etcd地址、服务名称、服务地址
 */
struct registerCenterConfig {
    std::string _etcdAddr;    // etcd地址
    std::string _serviceName; // 服务名称
    std::string _serviceAddr; // 服务地址
};

/**
 * @brief 通知子服务 RPC 服务器
 *
 * 负责 RPC 服务器的启动、停止、etcd 服务注册，
 * 以及邮件发送线程的启停；各对象均由 NotifyServerBuilder 组装后注入。
 */
class NotifyServer {
public:
    /**
     * @brief 构造通知 RPC 服务器
     * @param serviceImpl    RPC 接口实现对象（生命周期由 brpc 服务器接管，这里只保留非拥有指针）
     * @param server         brpc 服务器实例（已由 RpcServerFactory 创建并启动）
     * @param svcProvider    服务注册对象
     * @param notifyBusiness 通知业务对象，用于控制邮件发送线程
     */
    NotifyServer(NotifyServiceImpl* serviceImpl, std::shared_ptr<brpc::Server> server,
                 std::shared_ptr<bitesvc::SvcProvider> svcProvider,
                 std::shared_ptr<NotifyBusiness> notifyBusiness);

    ~NotifyServer();

    /**
     * @brief 启动邮件发送线程并注册服务到 etcd
     * @return 启动并注册成功返回 true
     */
    bool start();

    /**
     * @brief 停止邮件发送线程并停止 RPC 服务器
     */
    void stop();

    /**
     * @brief 查询服务器是否处于运行状态
     * @return 运行中返回 true
     */
    bool isRunning() const;

private:
    NotifyServiceImpl* _serviceImpl;                      // RPC 接口实现（brpc 服务器拥有所有权）
    std::shared_ptr<brpc::Server> _server;                // brpc 服务器
    std::shared_ptr<bitesvc::SvcProvider> _svcProvider;   // 服务注册对象
    std::shared_ptr<NotifyBusiness> _notifyBusiness;      // 通知业务对象
    std::atomic<bool> _isRunning;                         // 运行状态
};

/**
 * @brief 通知子服务构建器
 *
 * 使用建造者模式设置端口、邮箱配置与注册中心配置，
 * 由 build() 统一完成业务层、RPC 接口、RPC 服务器与服务注册对象的组装。
 */
class NotifyServerBuilder {
public:
    /**
     * @brief 设置 RPC 服务器监听端口
     */
    NotifyServerBuilder& setPort(int port);

    /**
     * @brief 设置邮箱配置
     */
    NotifyServerBuilder& setMailConfig(const MailSettings& mailConfig);

    /**
     * @brief 设置注册中心配置（etcd地址、服务名称、服务地址）
     */
    NotifyServerBuilder& setRegisterCenterConfig(const registerCenterConfig& config);

    /**
     * @brief 组装并返回 RPC 服务器
     * @return 构建成功返回服务器实例，失败返回空
     */
    std::shared_ptr<NotifyServer> build();

private:
    int _port = 0;                              // RPC服务器端口
    MailSettings _mailConfig;                   // 邮箱配置
    registerCenterConfig _registerCenterConfig; // 注册中心配置
};

} // namespace notifyService
