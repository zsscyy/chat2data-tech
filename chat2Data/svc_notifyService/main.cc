#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <bite_scaffold/log.h>
#include <gflags/gflags.h>

#include "notifyServer.h"

// ---------- 配置文件 ----------
DEFINE_string(conf, "", "配置文件路径（gflags flagfile 格式）");

// ---------- 通知子服务配置 ----------
DEFINE_int32(notify_listen_port, 9002, "通知子服务监听端口");
DEFINE_string(notify_service_name, "NotifyService", "通知子服务名称，用于etcd注册");
DEFINE_string(notify_service_addr, "192.168.150.129", "通知子服务注册地址（必须是其他服务可达的地址）");
DEFINE_string(etcd_addr, "http://dev-etcd:2379", "ETCD注册中心地址");

// ---------- 邮箱配置 ----------
DEFINE_string(mail_username, "", "邮箱用户名");
DEFINE_string(mail_password, "", "邮箱密码/授权码");
DEFINE_string(mail_url, "", "邮箱服务器地址");
DEFINE_string(mail_from, "", "发件人邮箱");

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
 * @brief 校验邮箱配置是否完整
 * @return 四项配置均非空返回 true
 */
bool isMailConfigValid() {
    return !FLAGS_mail_username.empty() && !FLAGS_mail_password.empty() && !FLAGS_mail_url.empty() &&
           !FLAGS_mail_from.empty();
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
    INF("NotifyService log initialized, level: {}, path: {}", FLAGS_log_level, FLAGS_log_path);

    // 3. 注册SIGINT/SIGTERM信号处理函数
    std::signal(SIGINT, handleExitSignal);
    std::signal(SIGTERM, handleExitSignal);

    // 4. 校验配置
    if (!isMailConfigValid()) {
        ERR("邮箱配置不完整, username: {}, url: {}, from: {}", FLAGS_mail_username, FLAGS_mail_url, FLAGS_mail_from);
        return PROGRAM_EXIT_FAILED;
    }
    if (FLAGS_notify_service_addr.empty() || FLAGS_notify_service_addr == "0.0.0.0") {
        ERR("服务注册地址必须是其他服务可达的地址，不能使用空值或0.0.0.0");
        return PROGRAM_EXIT_FAILED;
    }
    if (FLAGS_notify_listen_port <= 0) {
        ERR("通知子服务监听端口非法: {}", FLAGS_notify_listen_port);
        return PROGRAM_EXIT_FAILED;
    }

    // 5. 组装邮箱配置与注册中心配置
    notifyService::MailSettings mailSettings;
    mailSettings._username = FLAGS_mail_username;
    mailSettings._password = FLAGS_mail_password;
    mailSettings._url = FLAGS_mail_url;
    mailSettings._from = FLAGS_mail_from;

    const std::string registeredAddress =
        FLAGS_notify_service_addr + ":" + std::to_string(FLAGS_notify_listen_port);
    notifyService::registerCenterConfig registerConfig{FLAGS_etcd_addr, FLAGS_notify_service_name, registeredAddress};

    // 6. 使用建造者模式组装并启动RPC服务器
    std::shared_ptr<notifyService::NotifyServer> server;
    try {
        server = notifyService::NotifyServerBuilder()
                     .setPort(FLAGS_notify_listen_port)
                     .setMailConfig(mailSettings)
                     .setRegisterCenterConfig(registerConfig)
                     .build();
    } catch (const std::exception& e) {
        ERR("Failed to build NotifyServer: {}", e.what());
        return PROGRAM_EXIT_FAILED;
    }
    if (!server) {
        ERR("Failed to build NotifyServer");
        return PROGRAM_EXIT_FAILED;
    }
    if (!server->start()) {
        ERR("NotifyServer start failed");
        return PROGRAM_EXIT_FAILED;
    }
    INF("NotifyService is running on {}, press Ctrl+C to stop", registeredAddress);

    // 7. 主循环等待退出信号，退出时停止服务器
    while (server->isRunning() && !g_exitFlag.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(LOOP_CHECK_INTERVAL_SECONDS));
    }
    INF("exit signal received, stopping notify service...");
    server->stop();

    return PROGRAM_EXIT_SUCCESS;
}
