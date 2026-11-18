#include <sqlite3.h>
#include <iostream>
#include <string>
int main() {
    sqlite3* db = nullptr;
    
    // 1. 打开数据库
    int rc = sqlite3_open_v2("test.db", &db, 
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "无法打开数据库: " << sqlite3_errmsg(db) << std::endl;
        return 1;
    }
    
    // 2. 创建表
    char* errMsg = nullptr;
    const std::string createTableSQL = 
        R"(CREATE TABLE IF NOT EXISTS users (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                name TEXT NOT NULL,
                age INTEGER,
                score REAL))";
    rc = sqlite3_exec(db, createTableSQL.c_str(), nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "创建表失败: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        sqlite3_close(db);
        return 1;
    }
    
    // 3. 插入数据（使用预编译语句）
    sqlite3_stmt* stmt = nullptr;
    const std::string insertSQL = R"(INSERT INTO users (name, age, score) VALUES (?, ?, ?))";
    rc = sqlite3_prepare_v2(db, insertSQL.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "预编译失败: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db);
        return 1;
    }
    
    // 4. 绑定参数并执行
    sqlite3_bind_text(stmt, 1, "张三", -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, 25);
    sqlite3_bind_double(stmt, 3, 95.5);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        std::cerr << "插入失败: " << sqlite3_errmsg(db) << std::endl;
    }
    sqlite3_finalize(stmt);
    
    // 4. 查询数据
    const std::string selectSQL = R"(SELECT id, name, age, score FROM users WHERE age > ?)";
    rc = sqlite3_prepare_v2(db, selectSQL.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "预编译失败: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db);
        return 1;
    }
    
    sqlite3_bind_int64(stmt, 1, 20);  // 查询年龄大于 20 的用户
    
    std::cout << "查询结果:" << std::endl;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int id = sqlite3_column_int64(stmt, 0);
        const std::string name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        int age = sqlite3_column_int64(stmt, 2);
        double score = sqlite3_column_double(stmt, 3);
        
        std::cout << "ID: " << id << ", 姓名: " << name 
                  << ", 年龄: " << age << ", 分数: " << score << std::endl;
    }
    
    sqlite3_finalize(stmt);
    
    // 5. 关闭数据库
    sqlite3_close(db);
    
    return 0;
}