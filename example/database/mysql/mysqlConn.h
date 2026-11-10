#pragma once
#include <mysql/mysql.h>
#include <string>

class MySQLConn {
public:
    MySQLConn();
    ~MySQLConn();
    // 连接数据库
    bool connect(const std::string& host,      // 数据库主机地址
                 const std::string& user,      // 数据库用户名
                 const std::string& password,  // 数据库密码
                 const std::string& db,        // 数据库名称
                 unsigned int port = 3306);    // 数据库端口号

    // 获取数据库连接
    MYSQL* get();

private:
    MYSQL* _conn{nullptr};
};