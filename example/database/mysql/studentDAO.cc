#include "studentDAO.h"
#include <iostream>

StudentDAO::StudentDAO(MySQLConn& conn)
    : _conn(conn) {}

bool StudentDAO::insert(const Student& stu) {
    // 1. 构建SQL语句
    char sql[512];
    snprintf(sql, sizeof(sql),
             "INSERT INTO student(id, name, age, gender, score) "
             "VALUES(%d, '%s', %d, '%s', %f)",
             stu.getId(),
             stu.getName().c_str(),
             stu.getAge(),
             stu.getGender().c_str(),
             stu.getScore());

    // 2. 执行SQL语句
    if (mysql_query(_conn.get(), sql) != 0) {
        std::cerr << "Insert failed: "
                  << mysql_error(_conn.get()) << std::endl;
        return false;
    }
    return true;
}

std::vector<Student> StudentDAO::queryAll() {
    std::vector<Student> result;
    // 1. 构建SQL语句
    const char* sql = "SELECT id, name, age, gender, score FROM student";
    if (mysql_query(_conn.get(), sql) != 0) {
        std::cerr << "Query failed: "
                  << mysql_error(_conn.get()) << std::endl;
        return result;
    }

    // 2. 获取结果集
    MYSQL_RES* res = mysql_store_result(_conn.get());
    if (!res) return result;

    // 3. 从结果集中解析查询结果
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        Student stu;
        stu.setId(std::stoi(row[0]));
        stu.setName(row[1]);
        stu.setAge(std::stoi(row[2]));
        stu.setGender(row[3]);
        stu.setScore(std::stof(row[4]));
        result.push_back(stu);
    }

    // 4. 释放结果集空间
    mysql_free_result(res);
    return result;
}