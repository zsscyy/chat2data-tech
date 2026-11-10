#pragma once

#include <memory>
#include <string>
#include <vector>

#include <bite_scaffold/odb.h>
#include <bite_scaffold/rpc.h>
#include <bite_scaffold/redis.h>

#include "userServer.h"

namespace userService {

/**
 * @brief 注册中心配置：etcd地址、服务名称、服务地址
 */
struct registerCenterConfig {
    std::string _etcdAddr;    // etcd地址
    std::string _serviceName; // 服务名称
    std::string _serviceAddr; // 服务地址
};

/**
 * @brief 用户子服务构建器
 *
 * 使用建造者模式逐项设置配置，由 build() 统一完成 MySQL/Redis/数据层/业务层/
 * RPC接口/RPC服务器/服务发现/服务注册的组装。
 */
class UserServerBuilder {
public:
    /**
     * @brief 设置 MySQL 配置
     */
    UserServerBuilder& setMysqlConfig(const biteodb::mysql_settings& mysqlConfig);

    /**
     * @brief 设置 Redis 配置
     */
    UserServerBuilder& setRedisConfig(const biteredis::redis_settings& redisConfig);

    /**
     * @brief 设置 RPC 服务器监听端口
     */
    UserServerBuilder& setPort(int port);

    /**
     * @brief 设置 etcd 地址
     */
    UserServerBuilder& setEtcdAddr(const std::string& etcdAddr);

    /**
     * @brief 设置注册中心配置（etcd地址、服务名称、服务地址）
     */
    UserServerBuilder& setRegisterCenterConfig(const registerCenterConfig& config);

    /**
     * @brief 设置需要监听的子服务名称
     */
    UserServerBuilder& setWatchServices(const std::vector<std::string>& serviceNames);

    /**
     * @brief 组装并返回 RPC 服务器
     * @return 构建成功返回服务器实例，失败返回空
     */
    std::shared_ptr<UserServer> build();

private:
    biteodb::mysql_settings _mysqlConfig;        // MySQL配置
    biteredis::redis_settings _redisConfig;      // Redis配置
    int _port = 0;                               // RPC服务器端口
    registerCenterConfig _registerCenterConfig;  // 注册中心配置
    std::string _etcdAddr;                       // etcd地址
    std::vector<std::string> _watchServices;     // 需要监听的子服务名称
    biterpc::SvcChannels::ptr _svcChannels;      // 服务通道
};

} // namespace userService
