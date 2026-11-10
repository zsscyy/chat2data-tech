#pragma once
#include "student.h"
#include "mysqlConn.h"
#include <vector>

class StudentDAO {
public:
    explicit StudentDAO(MySQLConn& conn);

    bool insert(const Student& stu);
    std::vector<Student> queryAll();

private:
    MySQLConn& _conn;
};
