#include "studentPersistent.hpp"
#include <bite_scaffold/odb.h>


int main(){
    // 初始化MySQL的连接配置
    biteodb::mysql_settings settings;
    settings.host = "192.168.150.129";
    settings.port = 3306;
    settings.user = "root";
    settings.passwd = "123456";
    settings.db = "testdb";
    settings.cset = "utf8mb4";

    // 创建持久化对象，建立MySQL连接
    StudentPersistent sp(settings);

    // 创建学生对象
    Student stu1("张三", 18, "男", 90.0);
    Student stu2("李四", 19, "女", 85.0);

    // 插入学生对象
    if(!sp.insert(stu1)){
        std::cerr << "Insert  s1 failed: "  << std::endl;
        return 1;
    }

    // 插入学生对象
    if(!sp.insert(stu2)){
        std::cerr << "Insert  s2  failed: "  << std::endl;
        return 1;
    }
    
    sp.selectAll();
}

