#include "userServerBuilder.h"

#include <memory>
#include <thread>
#include <utility>
#include <vector>

#include <bite_scaffold/etcd.h>
#include <bite_scaffold/log.h>
#include <bite_scaffold/odb.h>
#include <bite_scaffold/rpc.h>

#include "sessionData.h"
#include "sessionManager.h"
#include "userBusiness.h"
#include "userData.h"
#include "userServer.h"
#include "userServiceImpl.h"
#include "verifyCodeData.h"

namespace userService {

UserServerBuilder& UserServerBuilder::setMysqlConfig(const biteodb::mysql_settings& mysqlConfig) {
    _mysqlConfig = mysqlConfig;
    return *this;
}

UserServerBuilder& UserServerBuilder::setRedisConfig(const biteredis::redis_settings& redisConfig) {
    _redisConfig = redisConfig;
    return *this;
}

UserServerBuilder& UserServerBuilder::setPort(int port) {
    _port = port;
    return *this;
}

UserServerBuilder& UserServerBuilder::setEtcdAddr(const std::string& etcdAddr) {
    _etcdAddr = etcdAddr;
    return *this;
}

UserServerBuilder& UserServerBuilder::setRegisterCenterConfig(const registerCenterConfig& config) {
    _registerCenterConfig = config;
    return *this;
}

UserServerBuilder& UserServerBuilder::setWatchServices(const std::vector<std::string>& serviceNames) {
    _watchServices = serviceNames;
    return *this;
}

std::shared_ptr<UserServer> UserServerBuilder::build() {
    // 1. 创建服务通道管理器，并设置需要监听的子服务
    _svcChannels = std::make_shared<biterpc::SvcChannels>();
    for (const std::string& serviceName : _watchServices) {
        _svcChannels->setWatch(serviceName);
        INF("watching service: {}", serviceName);
    }

    // 2. 初始化MySQL与Redis连接
    std::shared_ptr<odb::database> mysql = biteodb::DBFactory::mysql(_mysqlConfig);
    std::shared_ptr<sw::redis::Redis> redis = biteredis::RedisFactory::create(_redisConfig);
    INF("MySQL/Redis connected");

    // 3. 组装数据访问层、业务层与RPC接口实现
    std::shared_ptr<SessionData> sessionData = std::make_shared<SessionData>(mysql, redis);
    std::shared_ptr<VerifyCodeData> verifyCodeData = std::make_shared<VerifyCodeData>(redis);
    std::shared_ptr<UserData> userData = std::make_shared<UserData>(mysql, redis);
    std::shared_ptr<SessionManager> sessionManager = std::make_shared<SessionManager>(sessionData);
    std::shared_ptr<UserBusiness> userBusiness = std::make_shared<UserBusiness>(
        sessionManager, verifyCodeData, userData, _svcChannels);
    // RpcServerFactory 会以 SERVER_OWNS_SERVICE 接管服务对象，因此这里移交所有权
    auto serviceImpl = std::make_unique<UserServiceImpl>(userBusiness);

    // 4. 创建RPC服务器（RpcServerFactory 内部完成 AddService 与 Start）
    UserServiceImpl* rawServiceImpl = serviceImpl.release();
    std::shared_ptr<brpc::Server> rpcServer = biterpc::RpcServerFactory::create(_port, rawServiceImpl);
    if (!rpcServer) {
        ERR("Failed to create RPC server");
        return nullptr;
    }
    INF("RPC server started, port: {}", _port);

    // 5. 创建服务发现：服务上线/下线时更新信道管理器
    biterpc::SvcChannels::ptr channels = _svcChannels;
    auto onlineCallback = [channels](const std::string& serviceName, const std::string& serviceAddr) {
        INF("service online: {}, addr: {}", serviceName, serviceAddr);
        channels->addNode(serviceName, serviceAddr);
    };
    auto offlineCallback = [channels](const std::string& serviceName, const std::string& serviceAddr) {
        INF("service offline: {}, addr: {}", serviceName, serviceAddr);
        channels->delNode(serviceName, serviceAddr);
    };

    std::shared_ptr<bitesvc::SvcWatcher> watcher =
        std::make_shared<bitesvc::SvcWatcher>(_etcdAddr, onlineCallback, offlineCallback);
    std::thread watcherThread([watcher]() { watcher->watch(); });
    watcherThread.detach();
    INF("service watcher started, etcd: {}", _etcdAddr);

    // 6. 创建服务注册对象，实际注册由 UserServer::start() 执行
    auto provider = std::make_shared<bitesvc::SvcProvider>(
        _registerCenterConfig._etcdAddr, _registerCenterConfig._serviceName, _registerCenterConfig._serviceAddr);
    // 服务注册由 UserServer::start() 统一负责，避免构建阶段重复注册。
    // 7. 组装并返回 UserServer
    return std::make_shared<UserServer>(rawServiceImpl, rpcServer, watcher, provider);
}

} // namespace userService
