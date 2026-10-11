#ifndef TEACHER_H
#define TEACHER_H

#include "person.h"

// 教师类
class Teacher : virtual public Person {
private:
    string department;  // 所属院系
    string title;       // 职称
    double salary;      // 工资

public:
    Teacher(string id = "", string name = "", string gender = "",
            int age = 0, string phone = "", string department = "",
            string title = "", double salary = 0.0);
    virtual ~Teacher();

    string getDepartment() const { return department; }
    string getTitle() const { return title; }
    void setDepartment(const string& v) { department = v; }
    void setTitle(const string& v) { title = v; }
    void setSalary(double s) { salary = s; }

    virtual string getType() const { return "教师"; }
    virtual double getSalary() const { return salary; }

    virtual void input();
    virtual void display() const;
    virtual void displayRow() const;
    virtual void save(ostream& os) const;
};

#endif // TEACHER_H
