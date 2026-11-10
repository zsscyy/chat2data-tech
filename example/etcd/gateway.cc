#include <iostream>
#include <bite_scaffold/etcd.h>
#include <bite_scaffold/log.h>

void online(const std::string& serviceName, const std::string& serviceAddr){
    std::cout << "服务: " << serviceName << " 上线, 地址: " << serviceAddr << std::endl;
}

void offline(const std::string& serviceName, const std::string& serviceAddr){
    std::cout << "服务: " << serviceName << " 下线, 地址: " << serviceAddr << std::endl;
}

int main(){
    bitelog::bitelog_init();
    // 1. 实例化服务监控对象
    bitesvc::SvcWatcher watcher("http://dev-etcd:2379", online, offline);
    // 2. 调用watch开启监控
    watcher.watch();
    getchar();
    return 0;
}