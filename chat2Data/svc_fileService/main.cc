#include <atomic>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <bite_scaffold/log.h>
#include <gflags/gflags.h>

#include "fileServer.h"
#include "fileServerBuilder.h"

// ---------- 配置文件 ----------
DEFINE_string(conf, "", "配置文件路径（gflags flagfile 格式）");

// ---------- 文件子服务配置 ----------
DEFINE_int32(file_listen_port, 9004, "文件子服务监听端口");
DEFINE_string(file_service_name, "FileService", "文件子服务名称，用于etcd注册");
DEFINE_string(file_service_addr, "dev-env-service",
              "文件子服务注册地址（必须是其他容器可达的地址，不要填127.0.0.1或0.0.0.0）");
DEFINE_string(excel_service, "ExcelParserService", "Excel解析子服务名称");
DEFINE_string(db_service, "DatabaseService", "数据库子服务名称");
DEFINE_string(ai_service, "AIService", "AI子服务名称");
DEFINE_string(etcd_addr, "http://dev-etcd:2379", "ETCD注册中心地址");

// ---------- MySQL配置 ----------
DEFINE_string(mysql_host, "dev-mysql", "MySQL主机");
DEFINE_int32(mysql_port, 3306, "MySQL端口");
DEFINE_string(mysql_user, "root", "MySQL用户名");
DEFINE_string(mysql_passwd, "123456", "MySQL密码");
DEFINE_string(mysql_db, "chat2Data", "MySQL数据库名");
DEFINE_string(mysql_charset, "utf8mb4", "MySQL字符集");

// ---------- Redis配置 ----------
DEFINE_string(redis_host, "dev-redis", "Redis主机");
DEFINE_int32(redis_port, 6379, "Redis端口");
DEFINE_string(redis_passwd, "123456", "Redis密码");
DEFINE_int32(redis_db, 0, "Redis数据库编号");
DEFINE_int32(redis_pool_size, 16, "Redis连接池大小");

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
// 数据库连接池大小
const unsigned int MYSQL_POOL_SIZE = 3;
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
 * @note 共享配置文件中其他子服务的flag很多，这里使用非致命模式解析，自动忽略未知flag
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
 * @brief 解析FastDFS tracker地址列表
 * @param trackers 逗号分隔的tracker地址
 * @return tracker地址列表
 */
std::vector<std::string> parseTrackers(const std::string& trackers) {
    std::vector<std::string> result;
    std::stringstream stream(trackers);
    std::string tracker;
    while (std::getline(stream, tracker, ',')) {
        if (!tracker.empty()) {
            result.push_back(tracker);
        }
    }
    return result;
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
    INF("FileService log initialized, level: {}, path: {}", FLAGS_log_level, FLAGS_log_path);

    // 3. 注册SIGINT/SIGTERM信号处理函数
    std::signal(SIGINT, handleExitSignal);
    std::signal(SIGTERM, handleExitSignal);

    // 4. 校验必要配置
    if (FLAGS_mysql_host.empty() || FLAGS_redis_host.empty()) {
        ERR("MySQL或Redis主机未配置, mysql: {}, redis: {}", FLAGS_mysql_host, FLAGS_redis_host);
        return PROGRAM_EXIT_FAILED;
    }
    if (FLAGS_file_service_addr.empty() || FLAGS_file_service_addr == "0.0.0.0") {
        ERR("服务注册地址必须是其他服务可达的地址，不能使用空值或0.0.0.0");
        return PROGRAM_EXIT_FAILED;
    }

    std::vector<std::string> trackers = parseTrackers(FLAGS_fdfs_trackers);
    if (trackers.empty()) {
        ERR("FastDFS tracker地址未配置");
        return PROGRAM_EXIT_FAILED;
    }

    biteodb::mysql_settings mysqlSettings;
    mysqlSettings.host = FLAGS_mysql_host;
    mysqlSettings.port = static_cast<unsigned int>(FLAGS_mysql_port);
    mysqlSettings.user = FLAGS_mysql_user;
    mysqlSettings.passwd = FLAGS_mysql_passwd;
    mysqlSettings.db = FLAGS_mysql_db;
    mysqlSettings.cset = FLAGS_mysql_charset;
    mysqlSettings.connection_pool_size = MYSQL_POOL_SIZE;

    biteredis::redis_settings redisSettings;
    redisSettings.host = FLAGS_redis_host;
    redisSettings.port = FLAGS_redis_port;
    redisSettings.passwd = FLAGS_redis_passwd;
    redisSettings.db = FLAGS_redis_db;
    redisSettings.connection_pool_size = static_cast<std::size_t>(FLAGS_redis_pool_size);

    bitefdfs::fdfs_settings fdfsSettings;
    fdfsSettings.trackers = trackers;
    fdfsSettings.connect_timeout = FLAGS_fdfs_connect_timeout;
    fdfsSettings.network_timeout = FLAGS_fdfs_network_timeout;

    // 5. 组装需要监听的子服务
    std::vector<std::string> watchServices;
    if (!FLAGS_excel_service.empty()) {
        watchServices.push_back(FLAGS_excel_service);
    }
    if (!FLAGS_db_service.empty()) {
        watchServices.push_back(FLAGS_db_service);
    }
    if (!FLAGS_ai_service.empty()) {
        watchServices.push_back(FLAGS_ai_service);
    }

    // 6. 使用建造者模式组装并启动RPC服务器
    const std::string registeredAddress =
        FLAGS_file_service_addr + ":" + std::to_string(FLAGS_file_listen_port);
    fileService::registerCenterConfig registerConfig{FLAGS_etcd_addr, FLAGS_file_service_name,
                                                     registeredAddress};
    std::shared_ptr<fileService::FileServer> server =
        fileService::FileServerBuilder()
            .setMysqlConfig(mysqlSettings)
            .setRedisConfig(redisSettings)
            .setFdfsConfig(fdfsSettings)
            .setPort(FLAGS_file_listen_port)
            .setEtcdAddr(FLAGS_etcd_addr)
            .setRegisterCenterConfig(registerConfig)
            .setWatchServices(watchServices)
            .build();
    if (!server) {
        ERR("Failed to build FileServer");
        return PROGRAM_EXIT_FAILED;
    }
    if (!server->start()) {
        ERR("FileServer start failed");
        return PROGRAM_EXIT_FAILED;
    }
    INF("FileService is running on {}, press Ctrl+C to stop", registeredAddress);

    // 7. 主循环等待退出信号，退出时停止服务器
    while (server->isRunning() && !g_exitFlag.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(LOOP_CHECK_INTERVAL_SECONDS));
    }
    INF("exit signal received, stopping file service...");
    server->stop();

    return PROGRAM_EXIT_SUCCESS;
}
