#ifndef ADMINSTAFF_H
#define ADMINSTAFF_H

#include "person.h"

// 行政人员类
class AdminStaff : public Person {
private:
    string department;  // 所属部门
    string position;    // 职务
    double salary;      // 工资

public:
    AdminStaff(string id = "", string name = "", string gender = "",
               int age = 0, string phone = "", string department = "",
               string position = "", double salary = 0.0);
    virtual ~AdminStaff();

    string getDepartment() const { return department; }
    string getPosition() const { return position; }
    void setDepartment(const string& v) { department = v; }
    void setPosition(const string& v) { position = v; }
    void setSalary(double s) { salary = s; }

    virtual string getType() const { return "行政人员"; }
    virtual double getSalary() const { return salary; }

    virtual void input();
    virtual void display() const;
    virtual void displayRow() const;
    virtual void save(ostream& os) const;
};

#endif // ADMINSTAFF_H
