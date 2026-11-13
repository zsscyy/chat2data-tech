#include <atomic>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdio>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <bite_scaffold/log.h>
#include <gflags/gflags.h>

#include "userServer.h"
#include "userServerBuilder.h"

// ---------- 配置文件 ----------
DEFINE_string(conf, "", "配置文件路径（gflags flagfile 格式）");

// ---------- 服务配置 ----------
DEFINE_int32(listen_port, 9100, "服务监听端口");
DEFINE_string(service_name, "UserService", "服务名称，用于etcd注册");
DEFINE_string(service_addr, "127.0.0.1", "服务注册地址（必须是其他服务可达的地址，不要填0.0.0.0）");
DEFINE_string(notify_service, "NotifyService", "通知子服务名称");
DEFINE_string(db_service, "DatabaseService", "数据库子服务名称");
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
        if (!gflags::ReadFromFlagsFile(path, programName, true)) {
            std::fprintf(stderr, "配置文件解析失败: %s\n", path.c_str());
            return false;
        }
        *loadedPath = path;
        return true;
    }

    *loadedPath = std::string();
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
    INF("UserService log initialized, level: {}, path: {}", FLAGS_log_level, FLAGS_log_path);

    // 3. 注册SIGINT/SIGTERM信号处理函数
    std::signal(SIGINT, handleExitSignal);
    std::signal(SIGTERM, handleExitSignal);

    // 4. 校验MySQL与Redis配置
    if (FLAGS_mysql_host.empty() || FLAGS_redis_host.empty()) {
        ERR("MySQL或Redis主机未配置, mysql: {}, redis: {}", FLAGS_mysql_host, FLAGS_redis_host);
        return PROGRAM_EXIT_FAILED;
    }
    if (FLAGS_service_addr.empty() || FLAGS_service_addr == "0.0.0.0") {
        ERR("服务注册地址必须是其他服务可达的地址，不能使用空值或0.0.0.0");
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

    // 5. 组装需要监听的子服务（本章通知服务、数据库服务未实现，仅预留信道）
    std::vector<std::string> watchServices;
    if (!FLAGS_notify_service.empty()) {
        watchServices.push_back(FLAGS_notify_service);
    }
    if (!FLAGS_db_service.empty()) {
        watchServices.push_back(FLAGS_db_service);
    }

    // 6. 使用建造者模式组装并启动RPC服务器
    const std::string registeredAddress = FLAGS_service_addr + ":" + std::to_string(FLAGS_listen_port);
    userService::registerCenterConfig registerConfig{FLAGS_etcd_addr, FLAGS_service_name, registeredAddress};
    std::shared_ptr<userService::UserServer> server =
        userService::UserServerBuilder()
            .setMysqlConfig(mysqlSettings)
            .setRedisConfig(redisSettings)
            .setPort(FLAGS_listen_port)
            .setEtcdAddr(FLAGS_etcd_addr)
            .setRegisterCenterConfig(registerConfig)
            .setWatchServices(watchServices)
            .build();
    if (!server) {
        ERR("Failed to build UserServer");
        return PROGRAM_EXIT_FAILED;
    }
    if (!server->start()) {
        ERR("UserServer start failed");
        return PROGRAM_EXIT_FAILED;
    }
    INF("UserService is running on {}, press Ctrl+C to stop", registeredAddress);

    // 7. 主循环等待退出信号，退出时停止服务器
    while (server->isRunning() && !g_exitFlag.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(LOOP_CHECK_INTERVAL_SECONDS));
    }
    INF("exit signal received, stopping user service...");
    server->stop();

    return PROGRAM_EXIT_SUCCESS;
}
