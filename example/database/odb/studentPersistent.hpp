#include <odb/database.hxx>               // 定义odb::database抽象类，定义数据库连接、查询、持久化等核心方法
#include <odb/mysql/transaction.hxx>      // 定义odb::transaction抽象类，定义事务相关方法
#include <odb/mysql/database.hxx>         // 定义odb::mysql::database类，管理MySQL连接池，处理MySQL的连接参数(主机地址、端口、用户名、密码、数据库名称)
#include <odb/result.hxx>                 // 定义查询结果集的处理接口
#include <odb/query.hxx>                  // 提供类型安全的查询构造器，定义odb::query<T>类模板
#include "./build/student-odb.hxx"

#include <bite_scaffold/odb.h>

#include <iostream>
#include "student.hpp"

// 1. 通过odb框架连接数据库
// 2. 封装插入学生对象到数据库的方法
// 3. 封装查询所有学生的方法
class StudentPersistent{
public:
    StudentPersistent(const biteodb::mysql_settings &settings){
        _db = biteodb::DBFactory::mysql(settings);
    }

    /*
    * odb接口封装
    *  1. 创建并开启事务
    *  2. 进行CURD操作
    *  3. 提交事务
    *  4. 返回结果
    *  注意：MySQL的C API在进行数据库操作的时，可能会操作失败，odb将操作失败的情况封装成了异常，因此odb接口可能会抛出异常
    */
    bool insert(Student &student){
        try{
            odb::transaction tx(_db->begin());
            _db->persist(student);
            tx.commit();
            return true;
        } catch(odb::exception &e){
            std::cerr << "insert student failed: " << e.what() << std::endl;
            return false;
        }
    }

    std::vector<Student> selectAll(){
        std::vector<Student> students;
        try{
            odb::transaction tx(_db->begin());
            typedef odb::result<Student> Result;
            Result rst(_db->query<Student>());

            // 遍历Result结果，将学生逐个放置到students中
            for(auto &student : rst){
                students.push_back(student);
            }

            tx.commit();
        } catch(odb::exception &e){
            std::cerr << "select all students failed: " << e.what() << std::endl;
        }

        return students;
    }

private:
    std::shared_ptr<odb::database> _db;  // 数据库连接对象
};

