#include <iostream>
#include <gflags/gflags.h>

/*
 * gflags 库的使用
 * 1. 包含<gflags/gflags.h>
 * 2. 通过DEFINE_xxx(xx,xx,xx)定义参数
 * 3. 在main方法中通过google::ParseCommandLineFlags解析命令函参数
 * 4. 通过FLAGS_xx访问参数值
 */

DEFINE_string(ip, "0.0.0.0", "ip address");
DEFINE_int32(port, 8080, "port number");
DEFINE_bool(reuseAddr, true, "reuse address");
DEFINE_string(conf, "./main.conf", "配置文件的路径");


int main(int argc, char *argv[])
{
    // 先解析命令行参数
    google::ParseCommandLineFlags(&argc, &argv, true);
    std::cout << "ip: " << FLAGS_ip << std::endl;
    std::cout << "port: " << FLAGS_port << std::endl;
    std::cout << "reuseAddr: " << FLAGS_reuseAddr << std::endl;

    if (!FLAGS_conf.empty())
    {
        std::cout << "conf: " << FLAGS_conf << std::endl;
        // 从配置文件中进行解析
        google::ReadFromFlagsFile(FLAGS_conf, argv[0],true);
        std::cout << "ip: " << FLAGS_ip << std::endl;
        std::cout << "port: " << FLAGS_port << std::endl;
        std::cout << "reuseAddr: " << FLAGS_reuseAddr << std::endl;
    }
    return 0;
}
