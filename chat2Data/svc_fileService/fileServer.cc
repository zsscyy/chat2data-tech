#include "fileServer.h"

#include <utility>

// 必须放在 fileServer.h 之后、其他 Redis 相关头文件之前：
// brpc 的 redis_reply.h 与 hiredis 的 REDIS_REPLY_* 宏同名，brpc 头必须先解析
#include <brpc/server.h>

#include <bite_scaffold/etcd.h>
#include <bite_scaffold/log.h>

namespace fileService {

namespace {

// brpc停止时等待在途请求完成的毫秒数
const int SERVER_STOP_WAIT_MILLISECONDS = 1000;

} // namespace

FileServer::FileServer(FileServiceImpl* serviceImpl, std::shared_ptr<brpc::Server> server,
                       std::shared_ptr<bitesvc::SvcWatcher> svcWatcher,
                       std::shared_ptr<bitesvc::SvcProvider> svcProvider)
    : _serviceImpl(serviceImpl)
    , _server(std::move(server))
    , _svcWatcher(std::move(svcWatcher))
    , _svcProvider(std::move(svcProvider))
    , _isRunning(false) {
    INF("FileServer initialized");
}

FileServer::~FileServer() {
    if (_isRunning.load()) {
        stop();
    }
}

bool FileServer::start() {
    if (_isRunning.load()) {
        WRN("FileServer is already running");
        return false;
    }
    if (!_server) {
        ERR("FileServer start failed, rpc server is null");
        return false;
    }

    // RPC 服务器由 RpcServerFactory 创建并启动，这里只负责服务注册
    if (_svcProvider) {
        if (!_svcProvider->registry()) {
            ERR("register to etcd failed");
            // RpcServerFactory 已经启动 brpc；注册失败时立即关闭，避免留下未注册服务。
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

void FileServer::stop() {
    // 先停掉 etcd 租约保活；脚手架没有显式注销接口，key 会在租约（30s）到期后自动消失
    if (_svcProvider) {
        _svcProvider.reset();
        INF("etcd keepalive stopped, key expires within lease TTL");
    }
    _svcWatcher.reset();

    if (_server) {
        _server->Stop(SERVER_STOP_WAIT_MILLISECONDS);
        _server.reset();
        INF("brpc server stopped");
    }
    _isRunning.store(false);
}

bool FileServer::isRunning() const {
    return _isRunning.load();
}

} // namespace fileService
