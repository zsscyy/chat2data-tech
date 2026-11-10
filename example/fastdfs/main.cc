#include <bite_scaffold/log.h>
#include <bite_scaffold/fdfs.h>
#include <bite_scaffold/util.h>
#include<string>

// 文件上传/下载：本地文件
std::string test_fastdfs_loacl(){
    // 1. 调用比特脚手架中封装的通过本地上传文件的方式上传文件
    auto id = bitefdfs::FDFSClient::upload_from_file("../main.cc");
    if(!id){
        ERR("upload_from_file failed");
        abort();
    }
    INF("upload_from_file success, id: {}", id.value());

    // 2. 调用比特脚手架中封装的通过本地下载文件的方式下载文件
    bool ret = bitefdfs::FDFSClient::download_to_file(id.value(), "./main1.cc");
    if(!ret){
        ERR("download_to_file failed");
        abort();
    }
    INF("download_to_file success, id: {}", id.value());

    // 3. 可以检测文件内容是否相同---自己写个简单的工具逐字节进行对比
    return id.value();
}


// 文件上传/下载：buff
std::string test_fastdfs_buff(){
    // 1. 从磁盘中读取文件数据
    std::string body;
    if(!biteutil::FUTIL::read("../main.cc", body)){
        ERR("read file failed");
        abort();
    }
    // 2. 调用比特脚手架中封装的通过buff上传文件的方式上传文件
    auto id = bitefdfs::FDFSClient::upload_from_buff(body);
    if(!id){
        ERR("upload_from_buff failed");
        abort();
    }
    INF("upload_from_buff success, id: {}", id.value());

    // 3. 调用比特脚手架中封装的通过buff下载文件的方式下载文件
    bool ret = bitefdfs::FDFSClient::download_to_buff(id.value(), body);
    if(!ret){
        ERR("download_to_buff failed");
        abort();
    }
    INF("download_to_buff success, id: {}", id.value());
    // 4. 可以检测文件内容是否相同---自己写个简单的工具逐字节进行对比
    return id.value();
}

// 文件删除
void test_fastdfs_delete(std::vector<std::string> ids){
    // 1. 调用比特脚手架中封装的通过buff删除文件的方式删除文件
    for(const auto& id : ids){
        bool ret = bitefdfs::FDFSClient::remove(id);
        if(!ret){
            ERR("delete_file failed");
            abort();
        }
        INF("delete_file success, id: {}", id);
    }
}

int main(){
    // 初始化日志
    bitelog::bitelog_init();

    // 初始化fastdfs的客户端
    bitefdfs::fdfs_settings settings{
        .trackers = {"192.168.150.129:22122"}
    };
    bitefdfs::FDFSClient::init(settings);

    // 文件上传、文件下载、文件删除等操作
    std::string id1 = test_fastdfs_loacl();
    std::string id2 = test_fastdfs_buff();
    std::vector<std::string> ids = {id1, id2};
    test_fastdfs_delete(ids);
    // 资源销毁
    bitefdfs::FDFSClient::destroy();
    return 0;
}