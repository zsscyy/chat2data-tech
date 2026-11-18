#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <bite_scaffold/log.h>
#include <gflags/gflags.h>

#include "excelParserServer.h"

// ---------- 配置文件 ----------
DEFINE_string(conf, "", "配置文件路径（gflags flagfile 格式）");

// ---------- Excel解析子服务配置 ----------
DEFINE_int32(excel_listen_port, 9003, "Excel解析子服务监听端口");
DEFINE_string(excel_service_name, "ExcelParserService", "Excel解析子服务名称，用于etcd注册");
DEFINE_string(excel_service_addr, "dev-env-service", "Excel解析子服务注册地址（必须是其他容器可达的地址）");
DEFINE_string(etcd_addr, "http://dev-etcd:2379", "ETCD注册中心地址");

// ---------- FastDFS配置 ----------
DEFINE_string(fdfs_trackers, "", "FastDFS tracker服务器列表，多个地址使用逗号分隔");
DEFINE_int32(fdfs_connect_timeout, 30, "FastDFS连接超时时间（秒）");
DEFINE_int32(fdfs_network_timeout, 30, "FastDFS网络超时时间（秒）");

// ---------- 日志配置 ----------
DEFINE_bool(log_async, false, "是否启用异步日志");
DEFINE_int32(log_level, 2, "日志级别: 1-debug;2-info;3-warn;4-error;6-off");
DEFINE_string(log_format, "[%H:%M:%S][%-7l]: %v", "日志格式");
DEFINE_string(log_path, "stdout", "日志输出目标");

namespace {

// 主循环检查退出信号的间隔，单位秒
const int LOOP_CHECK_INTERVAL_SECONDS = 1;
// 程序正常退出返回值
const int PROGRAM_EXIT_SUCCESS = 0;
// 程序异常退出返回值
const int PROGRAM_EXIT_FAILED = -1;

// 程序退出标志，由信号处理函数置位，主循环轮询检测
std::atomic<bool> g_exitFlag(false);

/**
 * @brief 处理程序退出信号
 * @param 信号值（未使用）
 * @note 信号处理函数只置位退出标志，清理动作放在主循环中执行
 */
void handleExitSignal(int) {
    g_exitFlag.store(true);
}

/**
 * @brief 加载gflags配置文件
 * @param programName 程序名，用于错误提示
 * @param loadedPath  输出参数，加载成功的配置文件路径，未找到时置为空串
 * @return 加载成功或未找到配置文件返回 true，解析失败返回 false
 * @note 日志尚未初始化，这里不能使用日志宏，错误信息直接输出到标准错误
 * @note 使用非致命模式解析，共享配置文件中其他子服务的flag会被自动忽略
 */
bool loadConfFile(const char* programName, std::string* loadedPath) {
    std::vector<std::string> candidates;
    if (!FLAGS_conf.empty()) {
        candidates.push_back(FLAGS_conf);
    } else {
        candidates = {"chat2Data.conf", "../chat2Data.conf", "../../chat2Data.conf"};
    }

    for (const std::string& path : candidates) {
        std::ifstream confFile(path);
        if (!confFile.good()) {
            continue;
        }
        confFile.close();
        if (!gflags::ReadFromFlagsFile(path, programName, false)) {
            std::fprintf(stderr, "配置文件解析失败: %s\n", path.c_str());
            return false;
        }
        *loadedPath = path;
        return true;
    }

    *loadedPath = std::string();
    return true;
}

/**
 * @brief 按逗号拆分FastDFS tracker列表
 * @param trackers 逗号分隔的tracker字符串
 * @return 去除空白和空项后的tracker列表
 */
std::vector<std::string> splitTrackers(const std::string& trackers) {
    std::vector<std::string> result;
    std::stringstream stream(trackers);
    std::string item;
    while (std::getline(stream, item, ',')) {
        const std::size_t begin = item.find_first_not_of(" \t\r\n");
        if (begin == std::string::npos) {
            continue;
        }
        const std::size_t end = item.find_last_not_of(" \t\r\n");
        result.push_back(item.substr(begin, end - begin + 1));
    }
    return result;
}

/**
 * @brief 校验服务配置是否合法
 * @return 配置合法返回 true
 */
bool isServiceConfigValid() {
    if (FLAGS_excel_service_addr.empty() || FLAGS_excel_service_addr == "0.0.0.0") {
        ERR("服务注册地址必须是其他服务可达的地址，不能使用空值或0.0.0.0");
        return false;
    }
    if (FLAGS_excel_listen_port <= 0) {
        ERR("Excel解析子服务监听端口非法: {}", FLAGS_excel_listen_port);
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char* argv[]) {
    // 1. 解析命令行参数并加载配置文件
    google::ParseCommandLineFlags(&argc, &argv, true);
    std::string loadedConfPath;
    if (!loadConfFile(argv[0], &loadedConfPath)) {
        return PROGRAM_EXIT_FAILED;
    }

    // 2. 初始化日志
    bitelog::log_settings logSettings;
    logSettings.async = FLAGS_log_async;
    logSettings.level = FLAGS_log_level;
    logSettings.format = FLAGS_log_format;
    logSettings.path = FLAGS_log_path;
    bitelog::bitelog_init(logSettings);
    if (loadedConfPath.empty()) {
        WRN("未找到配置文件，使用命令行参数与默认值");
    } else {
        INF("配置文件加载成功: {}", loadedConfPath);
    }
    INF("ExcelParserService log initialized, level: {}, path: {}", FLAGS_log_level, FLAGS_log_path);

    // 3. 注册SIGINT/SIGTERM信号处理函数
    std::signal(SIGINT, handleExitSignal);
    std::signal(SIGTERM, handleExitSignal);

    // 4. 校验配置
    if (!isServiceConfigValid()) {
        return PROGRAM_EXIT_FAILED;
    }

    // 5. 解析FastDFS tracker列表
    const std::vector<std::string> trackers = splitTrackers(FLAGS_fdfs_trackers);
    if (trackers.empty()) {
        ERR("FastDFS tracker配置为空，请通过 --fdfs_trackers 指定");
        return PROGRAM_EXIT_FAILED;
    }

    // 6. 组装FastDFS配置与注册中心配置
    excelParserService::FdfsConfig fdfsConfig;
    fdfsConfig._trackers = trackers;
    fdfsConfig._connectTimeout = FLAGS_fdfs_connect_timeout;
    fdfsConfig._networkTimeout = FLAGS_fdfs_network_timeout;

    const std::string registeredAddress =
        FLAGS_excel_service_addr + ":" + std::to_string(FLAGS_excel_listen_port);
    excelParserService::RegisterCenterConfig registerConfig{
        FLAGS_etcd_addr, FLAGS_excel_service_name, registeredAddress};

    // 7. 使用建造者模式组装并启动RPC服务器
    std::shared_ptr<excelParserService::ExcelParserServer> server;
    try {
        server = excelParserService::ExcelParserServerBuilder()
                     .setPort(FLAGS_excel_listen_port)
                     .setFdfsConfig(fdfsConfig)
                     .setRegisterCenterConfig(registerConfig)
                     .build();
    } catch (const std::exception& e) {
        ERR("Failed to build ExcelParserServer: {}", e.what());
        return PROGRAM_EXIT_FAILED;
    }
    if (!server) {
        ERR("Failed to build ExcelParserServer");
        return PROGRAM_EXIT_FAILED;
    }
    if (!server->start()) {
        ERR("ExcelParserServer start failed");
        return PROGRAM_EXIT_FAILED;
    }
    INF("ExcelParserService is running on {}, press Ctrl+C to stop", registeredAddress);

    // 8. 主循环等待退出信号，退出时停止服务器
    while (server->isRunning() && !g_exitFlag.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(LOOP_CHECK_INTERVAL_SECONDS));
    }
    INF("exit signal received, stopping excel parser service...");
    server->stop();

    return PROGRAM_EXIT_SUCCESS;
}
