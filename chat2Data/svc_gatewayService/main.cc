#include <atomic>
#include <chrono>
#include <csignal>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <gflags/gflags.h>
#include <log.h>

#include "gatewayServer.h"

DEFINE_int32(logLevel, 2, "bitelog日志级别: 1-debug;2-info;3-warn;4-error;6-off");
DEFINE_string(logPath, "stdout", "bitelog日志输出目标");
DEFINE_string(logFormat, "[%H:%M:%S][%-7l]: %v", "bitelog日志格式");
DEFINE_bool(logAsync, false, "bitelog是否启用异步日志");
DEFINE_string(gatewayHost, "0.0.0.0", "网关服务器绑定地址");
DEFINE_int32(gatewayPort, 9000, "网关服务器绑定端口");
DEFINE_string(etcdAddr, "http://dev-etcd:2379", "ETCD注册中心地址");
DEFINE_string(userServiceName, "UserService", "用户子服务名称");
DEFINE_string(fileServiceName, "FileService", "文件子服务名称");
DEFINE_string(databaseServiceName, "DatabaseService", "数据库子服务名称");
DEFINE_string(aiServiceName, "AIService", "AI子服务名称");

namespace {
// 主线程轮询退出信号的间隔，单位秒
const int LOOP_CHECK_INTERVAL_SECONDS = 1;
// 程序异常退出返回值
const int PROGRAM_EXIT_FAILED = -1;
// 程序正常退出返回值
const int PROGRAM_EXIT_SUCCESS = 0;

// 程序退出标志，由信号处理函数置位，主线程轮询检测
std::atomic<bool> g_exitFlag(false);

/**
 * @brief 处理程序退出信号
 * @param 信号值（未使用）
 *
 * @note 信号处理函数只置位退出标志，避免在信号上下文执行非异步信号安全操作
 */
void handleExitSignal(int) {
    g_exitFlag.store(true);
}
} // namespace

int main(int argc, char* argv[]) {
    // 1. 解析gflags参数
    google::ParseCommandLineFlags(&argc, &argv, true);

    // 2. 初始化日志记录
    bitelog::log_settings logSettings;
    logSettings.level = FLAGS_logLevel;
    logSettings.path = FLAGS_logPath;
    logSettings.format = FLAGS_logFormat;
    logSettings.async = FLAGS_logAsync;
    bitelog::bitelog_init(logSettings);
    INF("Gateway service log initialized, level: {}, path: {}", FLAGS_logLevel, FLAGS_logPath);

    // 3. 注册SIGINT和SIGTERM信号处理函数
    std::signal(SIGINT, handleExitSignal);
    std::signal(SIGTERM, handleExitSignal);
    INF("SIGINT and SIGTERM signal handlers registered");

    // 4. 配置需要监控的子服务，构建网关服务器对象并启动
    std::vector<std::string> serviceNames = {
        FLAGS_userServiceName, FLAGS_fileServiceName, FLAGS_databaseServiceName, FLAGS_aiServiceName
    };
    GatewayService::GatewayServerBuilder builder(FLAGS_etcdAddr, FLAGS_gatewayHost, FLAGS_gatewayPort,
                                                 serviceNames);
    auto gatewayServer = builder.build();
    if (!gatewayServer) {
        ERR("Failed to build gateway server");
        return PROGRAM_EXIT_FAILED;
    }

    INF("Gateway service is running on {}:{}, press Ctrl+C to stop", FLAGS_gatewayHost, FLAGS_gatewayPort);

    // 5. 循环检测程序退出信号，服务器停止或收到退出信号时结束循环
    while (gatewayServer->isRunning() && !g_exitFlag.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(LOOP_CHECK_INTERVAL_SECONDS));
    }

    // 6. 程序退出时如果服务器在运行则停止服务器
    if (gatewayServer->isRunning()) {
        INF("Exit signal received, stopping gateway server...");
        gatewayServer->stop();
    }
    INF("Gateway service stopped, program exit");
    return PROGRAM_EXIT_SUCCESS;
}
