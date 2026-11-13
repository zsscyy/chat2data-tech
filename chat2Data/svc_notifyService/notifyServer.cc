#include "notifyServer.h"

#include <memory>
#include <utility>

#include <brpc/server.h>

#include <bite_scaffold/etcd.h>
#include <bite_scaffold/log.h>
#include <bite_scaffold/rpc.h>

#include "notifyBusiness.h"
#include "notifyServiceImpl.h"

namespace notifyService {

namespace {

// brpc停止时等待在途请求完成的毫秒数
const int SERVER_STOP_WAIT_MILLISECONDS = 1000;

} // namespace

NotifyServer::NotifyServer(NotifyServiceImpl* serviceImpl, std::shared_ptr<brpc::Server> server,
                           std::shared_ptr<bitesvc::SvcProvider> svcProvider,
                           std::shared_ptr<NotifyBusiness> notifyBusiness)
    : _serviceImpl(serviceImpl)
    , _server(std::move(server))
    , _svcProvider(std::move(svcProvider))
    , _notifyBusiness(std::move(notifyBusiness))
    , _isRunning(false) {
    INF("NotifyServer initialized");
}

NotifyServer::~NotifyServer() {
    if (_isRunning.load()) {
        stop();
    }
}

bool NotifyServer::start() {
    if (_isRunning.load()) {
        WRN("NotifyServer is already running");
        return false;
    }
    if (!_server) {
        ERR("NotifyServer start failed, rpc server is null");
        return false;
    }

    // 1. 启动邮件发送线程，确保注册成功后即可受理邮件任务
    if (_notifyBusiness) {
        _notifyBusiness->start();
    }

    // 2. 注册服务到 etcd；RPC 服务器已由 RpcServerFactory 启动
    if (_svcProvider) {
        if (!_svcProvider->registry()) {
            ERR("register to etcd failed");
            // 注册失败时立即停止线程与服务器，避免留下不可用服务
            if (_notifyBusiness) {
                _notifyBusiness->stop();
            }
            _server->Stop(SERVER_STOP_WAIT_MILLISECONDS);
            _server.reset();
            _svcProvider.reset();
            return false;
        }
        INF("service registered to etcd");
    } else {
        WRN("svcProvider is null, skip etcd registration");
    }

    _isRunning.store(true);
    return true;
}

void NotifyServer::stop() {
    // 先释放注册对象，脚手架没有显式注销接口，key 会在租约（30s）到期后自动消失
    if (_svcProvider) {
        _svcProvider.reset();
        INF("etcd keepalive stopped, key expires within lease TTL");
    }

    // 停止邮件发送线程，队列中剩余任务会先发送完
    if (_notifyBusiness) {
        _notifyBusiness->stop();
    }

    if (_server) {
        _server->Stop(SERVER_STOP_WAIT_MILLISECONDS);
        _server.reset();
        INF("brpc server stopped");
    }
    _isRunning.store(false);
}

bool NotifyServer::isRunning() const {
    return _isRunning.load();
}

NotifyServerBuilder& NotifyServerBuilder::setPort(int port) {
    _port = port;
    return *this;
}

NotifyServerBuilder& NotifyServerBuilder::setMailConfig(const MailSettings& mailConfig) {
    _mailConfig = mailConfig;
    return *this;
}

NotifyServerBuilder& NotifyServerBuilder::setRegisterCenterConfig(const registerCenterConfig& config) {
    _registerCenterConfig = config;
    return *this;
}

std::shared_ptr<NotifyServer> NotifyServerBuilder::build() {
    // 1. 组装业务层（内部完成 curl 全局初始化与邮件发送器创建）
    std::shared_ptr<NotifyBusiness> notifyBusiness = std::make_shared<NotifyBusiness>(_mailConfig);

    // 2. 组装RPC接口实现；RpcServerFactory 会以 SERVER_OWNS_SERVICE 接管服务对象，因此这里移交所有权
    auto serviceImpl = std::make_unique<NotifyServiceImpl>(notifyBusiness);
    NotifyServiceImpl* rawServiceImpl = serviceImpl.release();

    // 3. 创建RPC服务器（RpcServerFactory 内部完成 AddService 与 Start）
    std::shared_ptr<brpc::Server> rpcServer = biterpc::RpcServerFactory::create(_port, rawServiceImpl);
    if (!rpcServer) {
        ERR("Failed to create RPC server, port: {}", _port);
        delete rawServiceImpl;
        return nullptr;
    }
    INF("RPC server started, port: {}", _port);

    // 4. 创建服务注册对象，实际注册由 NotifyServer::start() 执行
    auto provider = std::make_shared<bitesvc::SvcProvider>(
        _registerCenterConfig._etcdAddr, _registerCenterConfig._serviceName, _registerCenterConfig._serviceAddr);

    // 5. 组装并返回 NotifyServer
    return std::make_shared<NotifyServer>(rawServiceImpl, rpcServer, provider, notifyBusiness);
}

} // namespace notifyService
