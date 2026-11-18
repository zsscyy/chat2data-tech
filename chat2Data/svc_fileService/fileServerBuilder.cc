#include "fileServerBuilder.h"

#include <memory>
#include <thread>
#include <utility>
#include <vector>

#include <bite_scaffold/etcd.h>
#include <bite_scaffold/log.h>
#include <bite_scaffold/odb.h>
#include <bite_scaffold/rpc.h>

#include "fileBusiness.h"
#include "fileInfoData.h"
#include "fileServer.h"
#include "fileServiceImpl.h"
#include "worksheetData.h"

namespace fileService {

FileServerBuilder& FileServerBuilder::setMysqlConfig(const biteodb::mysql_settings& mysqlConfig) {
    _mysqlConfig = mysqlConfig;
    return *this;
}

FileServerBuilder& FileServerBuilder::setRedisConfig(const biteredis::redis_settings& redisConfig) {
    _redisConfig = redisConfig;
    return *this;
}

FileServerBuilder& FileServerBuilder::setFdfsConfig(const bitefdfs::fdfs_settings& fdfsConfig) {
    _fdfsConfig = fdfsConfig;
    return *this;
}

FileServerBuilder& FileServerBuilder::setPort(int port) {
    _port = port;
    return *this;
}

FileServerBuilder& FileServerBuilder::setEtcdAddr(const std::string& etcdAddr) {
    _etcdAddr = etcdAddr;
    return *this;
}

FileServerBuilder& FileServerBuilder::setRegisterCenterConfig(const registerCenterConfig& config) {
    _registerCenterConfig = config;
    return *this;
}

FileServerBuilder& FileServerBuilder::setWatchServices(const std::vector<std::string>& serviceNames) {
    _watchServices = serviceNames;
    return *this;
}

std::shared_ptr<FileServer> FileServerBuilder::build() {
    // 1. 创建服务通道管理器，并设置需要监听的子服务
    _svcChannels = std::make_shared<biterpc::SvcChannels>();
    for (const std::string& serviceName : _watchServices) {
        if (serviceName.empty()) {
            continue;
        }
        _svcChannels->setWatch(serviceName);
        INF("watching service: {}", serviceName);
    }

    // 2. 初始化MySQL、Redis与FastDFS
    std::shared_ptr<odb::database> mysql = biteodb::DBFactory::mysql(_mysqlConfig);
    std::shared_ptr<sw::redis::Redis> redis = biteredis::RedisFactory::create(_redisConfig);
    bitefdfs::FDFSClient::init(_fdfsConfig);
    INF("MySQL/Redis/FastDFS connected, fdfs trackers: {}", _fdfsConfig.trackers.size());

    // 3. 组装数据访问层、业务层与RPC接口实现
    FileInfoData::ptr fileInfoData = std::make_shared<FileInfoData>(mysql, redis);
    WorksheetData::ptr worksheetData = std::make_shared<WorksheetData>(mysql, redis);
    FileBusiness::ptr fileBusiness =
        std::make_shared<FileBusiness>(fileInfoData, worksheetData, _svcChannels);
    // RpcServerFactory 会以 SERVER_OWNS_SERVICE 接管服务对象，因此这里移交所有权
    auto serviceImpl = std::make_unique<FileServiceImpl>(fileBusiness);

    // 4. 创建RPC服务器（RpcServerFactory 内部完成 AddService 与 Start）
    FileServiceImpl* rawServiceImpl = serviceImpl.release();
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

    // 6. 创建服务注册对象，实际注册由 FileServer::start() 执行
    auto provider = std::make_shared<bitesvc::SvcProvider>(
        _registerCenterConfig._etcdAddr, _registerCenterConfig._serviceName,
        _registerCenterConfig._serviceAddr);

    // 7. 组装并返回 FileServer
    return std::make_shared<FileServer>(rawServiceImpl, rpcServer, watcher, provider);
}

} // namespace fileService
