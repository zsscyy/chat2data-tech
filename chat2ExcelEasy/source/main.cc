#include <gflags/gflags.h>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include "chat2ExcelServer.h"
#include <log.h>
// bite_scaffold 与 ai_chat_sdk 都定义了 DBG/ERR 宏，
// 先解除前者的定义，避免重定义警告（后定义者生效，行为不变）
#undef DBG
#undef ERR
#include <ai_chat_sdk/util/myLog.h>

DEFINE_string(host, "0.0.0.0", "HTTP服务器绑定的地址");
DEFINE_int32(port, 9000, "HTTP服务器绑定的端口号");

int main(int argc, char* argv[]) {
    // 步骤1：解析命令行参数
    // 使用gflags库解析命令行参数，支持--host和--port选项
    google::ParseCommandLineFlags(&argc, &argv, true);

    // 步骤2：设置日志级别为INFO并初始化日志系统
    // 日志级别说明：1-debug; 2-info; 3-warn; 4-error; 6-off
    bitelog::log_settings logSettings;
    logSettings.level = 2;  // 设置为INFO级别
    bitelog::bitelog_init(logSettings);
    INF("Log level set to INFO");

    // 将ChatSDK内部的日志初始化完整
    bite::Logger::initLogger("ChatServer", "stdout", spdlog::level::info);

    // 步骤3：创建并初始化HTTP服务器
    // 使用命令行参数指定的地址和端口创建服务器实例
    chat2excel::Chat2ExcelServer server(FLAGS_host, FLAGS_port);

    // 步骤4：启动HTTP服务器
    // 服务器将在后台线程中运行，监听指定端口处理HTTP请求
    // 如果启动失败（如端口被占用），则输出错误信息并退出
    if (!server.start()) {
        ERR("Failed to start Chat2ExcelServer!");
        return -1;
    }

    INF("Chat2ExcelServer is running on {}:{}", FLAGS_host, FLAGS_port);
    INF("Press Ctrl+C to stop the server...");

    // 步骤5：循环等待前端请求，直到服务器被停止
    // 主线程通过轮询服务器状态来保持程序运行
    // 当服务器停止时（如调用stop()方法），循环退出
    while (server.isRunning()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    INF("Chat2ExcelServer has stopped.");
    return 0;
}
