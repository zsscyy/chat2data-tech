#pragma once
#include <string>

class Student {
public:
    Student() = default;
    Student(int id, const std::string& name, int age,
            const std::string& gender, float score);
    // 获取和设置学生Id
    void setId(int id);
    int getId() const;
    // 获取和设置学生姓名
    void setName(const std::string& name);
    std::string getName() const;
    // 获取和设置学生年龄
    void setAge(int age);
    int getAge() const;
    // 获取和设置学生性别
    void setGender(const std::string& gender);
    std::string getGender() const;
    // 获取和设置学生成绩
    void setScore(float score);  
    float getScore() const;
private:
    int _id = 0;
    std::string _name;
    int _age = 0;
    std::string _gender;
    float _score = 0.0f;
};