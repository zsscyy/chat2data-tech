#pragma once

#include <memory>
#include <string>
#include <vector>

#include <bite_scaffold/odb.h>
#include <bite_scaffold/rpc.h>
#include <bite_scaffold/redis.h>

#include "fileServer.h"

// fdfs.h 必须放在最后包含，并清理 byte 宏
#include <bite_scaffold/fdfs.h>
#ifdef byte
#undef byte
#endif

namespace fileService {

/**
 * @brief 注册中心配置：etcd地址、服务名称、服务地址
 */
struct registerCenterConfig {
    std::string _etcdAddr;    // etcd地址
    std::string _serviceName; // 服务名称
    std::string _serviceAddr; // 服务地址
};

/**
 * @brief 文件子服务构建器
 *
 * 使用建造者模式逐项设置配置，由 build() 统一完成 MySQL/Redis/FastDFS/数据层/
 * 业务层/RPC接口/RPC服务器/服务发现/服务注册的组装。
 */
class FileServerBuilder {
public:
    /**
     * @brief 设置 MySQL 配置
     */
    FileServerBuilder& setMysqlConfig(const biteodb::mysql_settings& mysqlConfig);

    /**
     * @brief 设置 Redis 配置
     */
    FileServerBuilder& setRedisConfig(const biteredis::redis_settings& redisConfig);

    /**
     * @brief 设置 FastDFS 配置
     */
    FileServerBuilder& setFdfsConfig(const bitefdfs::fdfs_settings& fdfsConfig);

    /**
     * @brief 设置 RPC 服务器监听端口
     */
    FileServerBuilder& setPort(int port);

    /**
     * @brief 设置 etcd 地址
     */
    FileServerBuilder& setEtcdAddr(const std::string& etcdAddr);

    /**
     * @brief 设置注册中心配置（etcd地址、服务名称、服务地址）
     */
    FileServerBuilder& setRegisterCenterConfig(const registerCenterConfig& config);

    /**
     * @brief 设置需要监听的子服务名称
     */
    FileServerBuilder& setWatchServices(const std::vector<std::string>& serviceNames);

    /**
     * @brief 组装并返回 RPC 服务器
     * @return 构建成功返回服务器实例，失败返回空
     */
    std::shared_ptr<FileServer> build();

private:
    biteodb::mysql_settings _mysqlConfig;       // MySQL配置
    biteredis::redis_settings _redisConfig;     // Redis配置
    bitefdfs::fdfs_settings _fdfsConfig;        // FastDFS配置
    int _port = 0;                              // RPC服务器端口
    registerCenterConfig _registerCenterConfig; // 注册中心配置
    std::string _etcdAddr;                      // etcd地址
    std::vector<std::string> _watchServices;    // 需要监听的子服务名称
    biterpc::SvcChannels::ptr _svcChannels;     // 服务通道
};

} // namespace fileService
