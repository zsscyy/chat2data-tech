#include <mysql/mysql.h>
#include <iostream>
#include <string>
#include <cstring>
int main() {
    // 1. 初始化 MySQL 连接
    MYSQL* mysql = mysql_init(nullptr);
    if (!mysql) {
        std::cerr << "初始化 MySQL 失败" << std::endl;
        return 1;
    }
    
    // 2. 设置连接选项
    // 设置超时时间
    unsigned int timeout = 10;
    mysql_options(mysql, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
    // 设置字符集
    mysql_options(mysql, MYSQL_SET_CHARSET_NAME, "utf8mb4");
    
    // 3. 连接数据库
    MYSQL* conn = mysql_real_connect(mysql,
        "dev-mysql",               // 主机
        "root",                          // 用户名
        "123456",                      // 密码
        "testdb",                          // 数据库名
        3306,                            // 端口
        nullptr,                  // Unix socket
        CLIENT_MULTI_STATEMENTS);  // 客户端标志
    if (!conn) {
        std::cerr << "连接失败: " << mysql_error(mysql) << std::endl;
        mysql_close(mysql);
        return 1;
    }
    
    // 4. 创建表
    const std::string createTableSQL = 
        R"(CREATE TABLE IF NOT EXISTS users (
            id BIGINT PRIMARY KEY AUTO_INCREMENT,
            name VARCHAR(100) NOT NULL,
            age INT,
            score DOUBLE))";
    
    if (mysql_real_query(mysql, createTableSQL.c_str(), createTableSQL.length() + 1) != 0) {
        std::cerr << "创建表失败: " << mysql_error(mysql) << std::endl;
        mysql_close(mysql);
        return 1;
    }
    
    // 5. 插入数据（使用预编译语句）
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        std::cerr << "初始化预编译语句失败" << std::endl;
        mysql_close(mysql);
        return 1;
    }
    
    const std::string insertSQL = R"(INSERT INTO users (name, age, score) VALUES (?, ?, ?))";
    if (mysql_stmt_prepare(stmt, insertSQL.c_str(), insertSQL.length() + 1) != 0) {
        std::cerr << "预编译失败: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        mysql_close(mysql);
        return 1;
    }
    
    // 绑定参数
    MYSQL_BIND binds[3];
    memset(binds, 0, sizeof(binds));
    
    std::string name = "张三";
    int age = 25;
    double score = 95.5;
    // 绑定用户名
    binds[0].buffer_type = MYSQL_TYPE_STRING;
    binds[0].buffer = const_cast<char*>(name.c_str());
    binds[0].buffer_length = name.length();
    binds[0].is_null = nullptr;
    
    binds[1].buffer_type = MYSQL_TYPE_LONG;
    binds[1].buffer = &age;
    binds[1].is_null = nullptr;
    
    binds[2].buffer_type = MYSQL_TYPE_DOUBLE;
    binds[2].buffer = &score;
    binds[2].is_null = nullptr;
    if (mysql_stmt_bind_param(stmt, binds) != 0) {
        std::cerr << "绑定参数失败: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        mysql_close(mysql);
        return 1;
    }
    
    if (mysql_stmt_execute(stmt) != 0) {
        std::cerr << "插入失败: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        mysql_close(mysql);
        return 1;
    }
    
    mysql_stmt_close(stmt);
    
    // 6. 查询数据
    stmt = mysql_stmt_init(mysql);
    const std::string selectSQL = R"(SELECT id, name, age, score FROM users WHERE age > ?)";
    if (mysql_stmt_prepare(stmt, selectSQL.c_str(), selectSQL.length() + 1) != 0) {
        std::cerr << "预编译失败: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        mysql_close(mysql);
        return 1;
    }
    
    // 绑定查询参数
    int minAge = 20;
    MYSQL_BIND paramBind;
    memset(&paramBind, 0, sizeof(paramBind));
    paramBind.buffer_type = MYSQL_TYPE_LONG;
    paramBind.buffer = &minAge;
    paramBind.is_null = nullptr;
    
    if (mysql_stmt_bind_param(stmt, &paramBind) != 0) {
        std::cerr << "绑定参数失败: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        mysql_close(mysql);
        return 1;
    }
    
    if (mysql_stmt_execute(stmt) != 0) {
        std::cerr << "执行失败: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        mysql_close(mysql);
        return 1;
    }
    
    // 将查询结果集从MySQL服务器端获取到客户端内存中，并存储在 stmt 句柄内部管理的缓冲区里
    if (mysql_stmt_store_result(stmt) != 0) {
        std::cerr << "存储结果失败: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        mysql_close(mysql);
        return 1;
    }
       
    // 绑定结果
    MYSQL_BIND resultBinds[4];
    memset(resultBinds, 0, sizeof(resultBinds));
    
    long long id;
    char nameBuf[256];
    unsigned long nameLength;
    int ageResult;
    double scoreResult;
    
    resultBinds[0].buffer_type = MYSQL_TYPE_LONGLONG;
    resultBinds[0].buffer = &id;
    resultBinds[0].is_null = nullptr;
    
    resultBinds[1].buffer_type = MYSQL_TYPE_STRING;
    resultBinds[1].buffer = nameBuf;
    resultBinds[1].buffer_length = sizeof(nameBuf);
    resultBinds[1].length = &nameLength;
    resultBinds[1].is_null = nullptr;
    
    resultBinds[2].buffer_type = MYSQL_TYPE_LONG;
    resultBinds[2].buffer = &ageResult;
    resultBinds[2].is_null = nullptr;
    
    resultBinds[3].buffer_type = MYSQL_TYPE_DOUBLE;
    resultBinds[3].buffer = &scoreResult;
    resultBinds[3].is_null = nullptr;
    
    if (mysql_stmt_bind_result(stmt, resultBinds) != 0) {
        std::cerr << "绑定结果失败: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        mysql_close(mysql);
        return 1;
    }
    
    // 获取结果
    std::cout << "查询结果:" << std::endl;
    while (mysql_stmt_fetch(stmt) == 0) {
        nameBuf[nameLength] = '\0';
        std::cout << "ID: " << id << ", 姓名: " << nameBuf 
                  << ", 年龄: " << ageResult << ", 分数: " << scoreResult << std::endl;
    }
    
    mysql_stmt_close(stmt);
    
    // 7. 关闭连接
    mysql_close(mysql);
    return 0;
}