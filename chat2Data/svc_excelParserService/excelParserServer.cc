#include "excelParserServer.h"

#include <memory>
#include <utility>

#include <brpc/server.h>

#include <bite_scaffold/log.h>
#include <bite_scaffold/etcd.h>
#include <bite_scaffold/rpc.h>

// fdfs.h 引入的 fastcommon 会定义 byte 宏，必须放到最后包含并立即取消定义
#include <bite_scaffold/fdfs.h>
#ifdef byte
#undef byte
#endif

#include "excelParserBusiness.h"
#include "excelParserServiceImpl.h"

namespace excelParserService {

namespace {

// brpc停止时等待在途请求完成的毫秒数
const int SERVER_STOP_WAIT_MILLISECONDS = 1000;

} // namespace

ExcelParserServer::ExcelParserServer(ExcelParserServiceImpl* serviceImpl, std::shared_ptr<brpc::Server> server,
                                     std::shared_ptr<bitesvc::SvcProvider> svcProvider)
    : _serviceImpl(serviceImpl)
    , _server(std::move(server))
    , _svcProvider(std::move(svcProvider))
    , _isRunning(false) {
    INF("ExcelParserServer initialized");
}

ExcelParserServer::~ExcelParserServer() {
    if (_isRunning.load()) {
        stop();
    }
}

bool ExcelParserServer::start() {
    if (_isRunning.load()) {
        WRN("ExcelParserServer is already running");
        return false;
    }
    if (!_server) {
        ERR("ExcelParserServer start failed, rpc server is null");
        return false;
    }

    // 注册服务到 etcd；RPC 服务器已由 RpcServerFactory 启动
    if (_svcProvider) {
        if (!_svcProvider->registry()) {
            ERR("register to etcd failed");
            // 注册失败时立即停止服务器，避免留下不可用服务
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

void ExcelParserServer::stop() {
    // 先释放注册对象，脚手架没有显式注销接口，key 会在租约（30s）到期后自动消失
    if (_svcProvider) {
        _svcProvider.reset();
        INF("etcd keepalive stopped, key expires within lease TTL");
    }

    if (_server) {
        _server->Stop(SERVER_STOP_WAIT_MILLISECONDS);
        _server.reset();
        INF("brpc server stopped");
    }
    _isRunning.store(false);
}

bool ExcelParserServer::isRunning() const {
    return _isRunning.load();
}

ExcelParserServerBuilder& ExcelParserServerBuilder::setPort(int port) {
    _port = port;
    return *this;
}

ExcelParserServerBuilder& ExcelParserServerBuilder::setFdfsConfig(const FdfsConfig& config) {
    _fdfsConfig = config;
    return *this;
}

ExcelParserServerBuilder& ExcelParserServerBuilder::setRegisterCenterConfig(const RegisterCenterConfig& config) {
    _registerCenterConfig = config;
    return *this;
}

std::shared_ptr<ExcelParserServer> ExcelParserServerBuilder::build() {
    // 1. 参数校验
    if (_port <= 0) {
        ERR("invalid rpc port: {}", _port);
        return nullptr;
    }
    if (_fdfsConfig._trackers.empty()) {
        ERR("fdfs trackers is empty");
        return nullptr;
    }

    // 2. 初始化FastDFS客户端全局配置
    bitefdfs::fdfs_settings fdfsSettings;
    fdfsSettings.trackers = _fdfsConfig._trackers;
    fdfsSettings.connect_timeout = _fdfsConfig._connectTimeout;
    fdfsSettings.network_timeout = _fdfsConfig._networkTimeout;
    bitefdfs::FDFSClient::init(fdfsSettings);
    INF("FastDFS client initialized, tracker count: {}", _fdfsConfig._trackers.size());

    // 3. 组装业务层
    std::shared_ptr<ExcelParserBusiness> excelParserBusiness = std::make_shared<ExcelParserBusiness>();

    // 4. 组装RPC接口实现；RpcServerFactory 会以 SERVER_OWNS_SERVICE 接管服务对象，因此这里移交所有权
    auto serviceImpl = std::make_unique<ExcelParserServiceImpl>(excelParserBusiness);
    ExcelParserServiceImpl* rawServiceImpl = serviceImpl.release();

    // 5. 创建RPC服务器（RpcServerFactory 内部完成 AddService 与 Start）
    std::shared_ptr<brpc::Server> rpcServer = biterpc::RpcServerFactory::create(_port, rawServiceImpl);
    if (!rpcServer) {
        ERR("Failed to create RPC server, port: {}", _port);
        delete rawServiceImpl;
        return nullptr;
    }
    INF("RPC server started, port: {}", _port);

    // 6. 创建服务注册对象，实际注册由 ExcelParserServer::start() 执行
    auto provider = std::make_shared<bitesvc::SvcProvider>(
        _registerCenterConfig._etcdAddr, _registerCenterConfig._serviceName, _registerCenterConfig._serviceAddr);

    // 7. 组装并返回 ExcelParserServer
    return std::make_shared<ExcelParserServer>(rawServiceImpl, rpcServer, provider);
}

} // namespace excelParserService
