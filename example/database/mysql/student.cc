#include "student.h"

Student::Student(int id, const std::string& name, int age,
                 const std::string& gender, float score)
    : _id(id), _name(name), _age(age), _gender(gender), _score(score) {}

// 设置和获取学生id
void Student::setId(int id) {
     _id = id; 
}

int Student::getId() const {
    return _id; 
}

// 设置和获取学生姓名
void Student::setName(const std::string& name) { 
    _name = name; 
}

std::string Student::getName() const { 
    return _name; 
}

// 设置和获取学生年龄
void Student::setAge(int age) { 
    _age = age; 
}

int Student::getAge() const { 
    return _age; 
}

// 设置和获取学生性别
void Student::setGender(const std::string& gender) { 
    _gender = gender; 
}

std::string Student::getGender() const { 
    return _gender; 
}

// 设置和获取学生成绩
void Student::setScore(float score) { 
    _score = score; 
}

float Student::getScore() const { 
    return _score; 
}
