#ifndef STUDENT_H
#define STUDENT_H

#include "person.h"

class Student : virtual public Person {
    string department;
    string major;
    string className;
    int admissionYear;
public:
    Student(string id = "", string name = "", string gender = "", int age = 0,
            string phone = "", string department = "", string major = "",
            string className = "", int admissionYear = 0);
    ~Student() override = default;
    string getDepartment() const { return department; }
    string getMajor() const { return major; }
    string getClassName() const { return className; }
    int getAdmissionYear() const { return admissionYear; }
    string getType() const override { return "学生"; }
    // 兼容统一人员接口；学生不参与工资统计。
    double getSalary() const override { return 0; }
    void input() override;
    void display() const override;
    void displayRow() const override;
    void save(ostream& os) const override;
};

// 本科生与研究生共享学生信息；旧 Student 文件记录仍可读取。
class Undergraduate : public Student {
public:
    Undergraduate(string id = "", string name = "", string gender = "", int age = 0,
                  string phone = "", string department = "", string major = "",
                  string className = "", int admissionYear = 0);
    ~Undergraduate() override = default;
    string getType() const override { return "本科生"; }
};

#endif
