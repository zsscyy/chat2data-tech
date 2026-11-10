#include "mysqlConn.h"
#include "studentDAO.h"
#include <iostream>

int main() {
    // 1. 连接MySQL数据库
    MySQLConn conn;
    if (!conn.connect("192.168.150.129", "root", "123456", "testdb", 3306)) {
        return -1;
    }

    // 定于学生表的持久化对象
    StudentDAO dao(conn);

    // 向学生表中插入学生信息
    Student s1(1, "张三", 18, "男", 89.5f);
    Student s2(2, "李四", 19, "女", 92.0f);
    dao.insert(s1);
    dao.insert(s2);

    // 查询所有学生信息
    auto students = dao.queryAll();
    for (const auto& s : students) {
        std::cout << "id=" << s.getId()
                  << ", name=" << s.getName()
                  << ", age=" << s.getAge()
                  << ", gender=" << s.getGender()
                  << ", score=" << s.getScore()
                  << std::endl;
    }
    return 0;
}