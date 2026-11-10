#include <utility>

#include <bite_scaffold/etcd.h>
#include <log.h>

#include "gatewayServer.h"

namespace GatewayService {

namespace {
// HTTP服务器读写超时时间，5分钟，单位秒
const int HTTP_READ_WRITE_TIMEOUT_SECONDS = 300;
} // namespace

GatewayServer::GatewayServer(const std::string& host, int port, std::shared_ptr<httplib::Server> server,
                             std::shared_ptr<GatewayServiceImpl> serviceImpl)
    : _host(host)
    , _port(port)
    , _server(std::move(server))
    , _serviceImpl(std::move(serviceImpl))
    , _isRunning(false) {
    INF("GatewayServer initialized with host: {}, port: {}", _host, _port);
}

GatewayServer::~GatewayServer() {
    // 对象销毁时自动停止服务器；监听失败时线程可能已退出但仍需回收
    if (_isRunning.load() || _serverThread.joinable()) {
        stop();
    }
}

bool GatewayServer::start() {
    if (_isRunning.load()) {
        ERR("GatewayServer is already running, host: {}, port: {}", _host, _port);
        return false;
    }

    if (!_server) {
        ERR("GatewayServer instance is null, cannot start");
        return false;
    }

    // 回收上一次启动失败后残留的线程，避免线程对象重复赋值
    if (_serverThread.joinable()) {
        _serverThread.join();
    }

    _isRunning.store(true);
    _serverThread = std::thread([this]() {
        INF("GatewayServer listening on {}:{}", _host, _port);
        // listen()会阻塞，直到服务器停止
        bool listenResult = _server->listen(_host, _port);
        if (!listenResult) {
            if (_isRunning.load()) {
                ERR("GatewayServer failed to listen on {}:{}", _host, _port);
                _isRunning.store(false);
            } else {
                DBG("GatewayServer listen loop exited by stop");
            }
        }
    });

    INF("GatewayServer started successfully, host: {}, port: {}", _host, _port);
    return true;
}

void GatewayServer::stop() {
    if (!_isRunning.load()) {
        // 监听失败时线程可能已退出但尚未回收，这里一并处理
        if (_serverThread.joinable()) {
            _serverThread.join();
        }
        WRN("GatewayServer is not running, no need to stop");
        return;
    }

    _isRunning.store(false);
    if (_server) {
        _server->stop();
    }

    if (_serverThread.joinable()) {
        _serverThread.join();
    }

    INF("GatewayServer stopped successfully");
}

bool GatewayServer::isRunning() const {
    return _isRunning.load();
}

GatewayServerBuilder::GatewayServerBuilder(const std::string& etcdAddr, const std::string& host, int port,
                                           const std::vector<std::string>& serviceNames)
    : _etcdAddr(etcdAddr)
    , _host(host)
    , _port(port)
    , _serviceNames(serviceNames) {
    INF("GatewayServerBuilder initialized, etcd: {}, host: {}, port: {}, service count: {}",
        _etcdAddr, _host, _port, _serviceNames.size());
}

std::shared_ptr<GatewayServer> GatewayServerBuilder::build() {
    INF("Start building gateway server...");

    // 1. 构建服务信道管理对象，并设置需要监控的服务
    auto svcChannels = std::make_shared<biterpc::SvcChannels>();
    for (const auto& serviceName : _serviceNames) {
        svcChannels->setWatch(serviceName);
        INF("Service channel watch added: {}", serviceName);
    }

    // 2. 构造服务发现对象，服务上线时添加节点，下线时移除节点
    auto svcWatcher = std::make_shared<bitesvc::SvcWatcher>(
        _etcdAddr,
        [svcChannels](std::string serviceName, std::string serviceAddr) {
            INF("Service online: {}, addr: {}", serviceName, serviceAddr);
            svcChannels->addNode(serviceName, serviceAddr);
        },
        [svcChannels](std::string serviceName, std::string serviceAddr) {
            INF("Service offline: {}, addr: {}", serviceName, serviceAddr);
            svcChannels->delNode(serviceName, serviceAddr);
        });
    INF("Service watcher created, etcd: {}", _etcdAddr);

    // 3. 在独立线程中启动服务监控，避免阻塞构建流程
    std::thread watcherThread([svcWatcher]() {
        INF("Service watcher thread started");
        if (!svcWatcher->watch()) {
            ERR("Service watcher failed to watch services");
            return;
        }
        INF("Service watcher exited");
    });
    watcherThread.detach();

    // 4. 构建HTTP接口定义对象
    auto serviceImpl = std::make_shared<GatewayServiceImpl>(svcChannels);
    INF("GatewayServiceImpl built successfully");

    // 5. 构建HTTP服务器对象，设置读写超时时间为5分钟
    auto httpServer = std::make_shared<httplib::Server>();
    httpServer->set_read_timeout(HTTP_READ_WRITE_TIMEOUT_SECONDS, 0);
    httpServer->set_write_timeout(HTTP_READ_WRITE_TIMEOUT_SECONDS, 0);
    INF("HTTP server built, read/write timeout: {} seconds", HTTP_READ_WRITE_TIMEOUT_SECONDS);

    // 6. 绑定路由
    serviceImpl->bindRoutes(*httpServer);

    // 7. 构建网关服务器对象
    auto gatewayServer = std::make_shared<GatewayServer>(_host, _port, httpServer, serviceImpl);

    // 8. 启动服务器
    if (!gatewayServer->start()) {
        ERR("Failed to start gateway server, host: {}, port: {}", _host, _port);
        return nullptr;
    }

    // 9. 返回网关服务器对象
    INF("Gateway server built and started successfully");
    return gatewayServer;
}

} // namespace GatewayService
