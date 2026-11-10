#include <bite_scaffold/rpc.h>
#include <bite_scaffold/log.h>
#include "cal.pb.h"

class CalServiceImpl : public cal::CalService{
public:
    CalServiceImpl() = default;
    ~CalServiceImpl() = default;
    void Add(::google::protobuf::RpcController* controller,
                       const ::cal::AddReq* request,
                       ::cal::AddRsp* response,
                       ::google::protobuf::Closure* done) override{
        // 0. 构造ClosureGuard对象,该对象是以RAII的机制，来管理done->Run()方法的调用
        brpc::ClosureGuard done_guard(done); 

        // 1. 解析RPC请求中参数
        int32_t num1 = request->num1();
        int32_t num2 = request->num2();
        // 2. 业务逻辑处理
        int32_t result = num1 + num2;
        // 3. 设置RPC响应
        response->set_result(result);
        // 4. 调用 done 回调函数---直接调用不太好，有些时候会忘记，比如中间抛出异常时，done->Run()方法不会被调用，响应就不会发送给客户端
        // done->Run();
    }
};


int main(){
    bitelog::bitelog_init(bitelog::log_settings());
    // 1. 创建服务对象
    CalServiceImpl* cal_service = new CalServiceImpl();
    // 2. 创建RPC服务器: 将服务对象添加到RPC服务器，设置RPC服务器配置，并启动服务器
    std::shared_ptr<brpc::Server> server = biterpc::RpcServerFactory::create(9000, cal_service);
    // 3. 等待客户端连接，并持续为客户端提供服务，直到用户按下ctrl+C终止服务器的运行
    server->RunUntilAskedToQuit();
    delete cal_service;
    return 0;
}
