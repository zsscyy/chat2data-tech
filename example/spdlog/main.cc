#include <iostream>
#include <bite_scaffold/log.h>

/*
 * bite_scaffold中日志 库的使用
 * 1. 包含<bite_scaffold/log.h>
 * 2. 定义并初始化bitelog::log_settings结构体
 * 3. 调用bitelog::bitelog_init初始化日志器
 * 4. 通过DBG,INF,WRN,ERR宏输出日志
 */

int main(int argc, char *argv[])
{
    // 1. 定义并初始化bitelog::log_settings结构体
    bitelog::log_settings settings;
    settings.async = false;
    settings.level = 4;   // ERROR
    settings.path = "stdout";

    // 2. 调用bitelog::bitelog_init初始化日志器
    bitelog::bitelog_init(settings);

    // 3. 通过DBG,INF,WRN,ERR宏输出日志
    DBG("debug log");
    INF("info log");
    WRN("warn log");
    ERR("error log");
    INF("host addr{}:{}", "127.0.0.1", 8080);
    return 0;
}
