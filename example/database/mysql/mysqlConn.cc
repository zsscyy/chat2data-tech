#include "mysqlConn.h"
#include <iostream>

// 构造函数：初始化数据库连接
MySQLConn::MySQLConn() {
    _conn = mysql_init(nullptr);
}

// 析构函数：关闭数据库连接
MySQLConn::~MySQLConn() {
    if (_conn) {
        mysql_close(_conn);
    }
}

// 连接数据库
bool MySQLConn::connect(const std::string& host,
                        const std::string& user,
                        const std::string& password,
                        const std::string& db,
                        unsigned int port) {
    // 连接数据库
    if (!mysql_real_connect(
            _conn,
            host.c_str(),
            user.c_str(),
            password.c_str(),
            db.c_str(),
            port,
            nullptr,
            0)) {
        std::cerr << "MySQL connect failed: "
                  << mysql_error(_conn) << std::endl;
        return false;
    }
    // 设置字符集为utf8mb4
    mysql_set_character_set(_conn, "utf8mb4");
    return true;
}

MYSQL* MySQLConn::get() {
    return _conn;
}