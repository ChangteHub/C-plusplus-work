#ifndef GRADUATE_H
#define GRADUATE_H
#include "student.h"
#include "teacher.h"

// 菱形继承：Teacher/Student 虚继承 Person，研究生只有一份人员身份。
class GraduateStudent : public Teacher, public Student {
    string researchDirection;
    string supervisor;

  public:
    GraduateStudent(string id = "", string name = "", string gender = "", int age = 0,
                    string phone = "", string department = "", string major = "",
                    string className = "", int admissionYear = 0, string researchDirection = "",
                    string supervisor = "");
    ~GraduateStudent() override = default;
    string getType() const override { return "研究生"; }
    double getSalary() const override { return 0; }
    string getDepartment() const { return Student::getDepartment(); }
    string getResearchDirection() const { return researchDirection; }
    string getSupervisor() const { return supervisor; }
    void input() override;
    void display() const override;
    void displayRow() const override;
    void save(ostream &os) const override;
};
#endif
