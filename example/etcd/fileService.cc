#include <bite_scaffold/etcd.h>
#include <bite_scaffold/log.h>

int main(){
    bitelog::bitelog_init();
    // 1. 实例化服务注册对象
    bitesvc::SvcProvider svc_provider("192.168.150.129:2379", "fileService", "192.168.150.129:9001");
    // 2. 调用registry注册服务
    svc_provider.registry();
    getchar();
    return 0;
}