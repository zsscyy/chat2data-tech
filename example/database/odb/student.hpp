#pragma once
#include <string>
#include <odb/core.hxx>


#pragma db object table("tbl_student")
class Student{
public:
    Student() = default;
    Student(std::string name, int age, std::string gender, float score)
        : _name(name), _age(age), _gender(gender), _score(score) {}
        
    int setId(int id) { _id = id; return 0; }
    int getId() const { return _id; }
    
    int setName(std::string name) { _name = name; return 0; }
    std::string getName() const { return _name; }
    
    int setAge(int age) { _age = age; return 0; }
    int getAge() const { return _age; }
    
    int setGender(std::string gender) { _gender = gender; return 0; }
    std::string getGender() const { return _gender; }
    
    int setScore(float score) { _score = score; return 0; }
    float getScore() const { return _score; }

private:
    friend class odb::access;

    #pragma db id auto column("id")
    int _id;

    #pragma db column("name")
    std::string _name;

    #pragma db column("age")
    int _age;

    #pragma db column("gender")
    std::string _gender;

    #pragma db column("score")
    float _score;
};