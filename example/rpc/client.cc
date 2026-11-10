#include <bite_scaffold/rpc.h>
#include <bite_scaffold/log.h>
#include "cal.pb.h"

int main(){
    bitelog::bitelog_init(bitelog::log_settings());
    // 1. 创建服务管理对象--bite_scaffold中svcChannels
    biterpc::SvcChannels scs;
    // 2. 客户端进行服务发现--但是现在用于服务发现的ETCD还未实现，因此只能手动模拟
    // 2.1 添加服务
    scs.setWatch("calService");
    // 2.2 添加服务对应的节点
    scs.addNode("calService", "192.168.150.129:9000");
    // 3. 获取节点服务节点对应的信道Channel
    auto channel = scs.getNode("calService");
    if(channel == nullptr){
        std::cerr << "getNode failed" << std::endl;
        return -1;
    }
    // 4. 发起RPC调用
    // 4.1 构造RPC请求
    cal::AddReq request;
    request.set_num1(10);
    request.set_num2(20);
    // 4.2 创建RPC的客户端
    brpc::Controller controller;
    cal::CalService_Stub stub(channel.get());
    cal::AddRsp response;
    stub.Add(&controller, &request, &response, nullptr);
    // 4.3 检测RPC调用是否成功
    if(controller.Failed()){
        std::cerr << "Add failed failed" << std::endl;
        return -1;
    }

    // 4.4 解析RPC响应
    std::cout << "Add response: " << response.result() << std::endl;
    return 0;
}
