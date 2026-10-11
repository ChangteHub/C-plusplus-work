#include "graduate.h"
GraduateStudent::GraduateStudent(string id, string name, string gender, int age, string phone,
                                 string department, string major, string className,
                                 int admissionYear, string researchDirection, string supervisor)
    : Person(id, name, gender, age, phone),
      Teacher(id, name, gender, age, phone, department, "研究助教", 0),
      Student(id, name, gender, age, phone, department, major, className, admissionYear),
      researchDirection(researchDirection), supervisor(supervisor) {}
void GraduateStudent::input() {
    Student::input();
    Teacher::setDepartment(Student::getDepartment());
    std::cout << "研究方向: ";
    std::cin >> researchDirection;
    std::cout << "导师: ";
    std::cin >> supervisor;
}
void GraduateStudent::display() const {
    Student::display();
    std::cout << "研究方向: " << researchDirection << "\n导师: " << supervisor << '\n';
}
void GraduateStudent::displayRow() const {
    Student::displayRow();
    std::cout << " / " << researchDirection << " / 导师 " << supervisor;
}
void GraduateStudent::save(ostream &os) const {
    os << "Graduate|" << id << '|' << name << '|' << gender << '|' << age << '|' << phone << '|'
       << getDepartment() << '|' << getMajor() << '|' << getClassName() << '|' << getAdmissionYear()
       << '|' << researchDirection << '|' << supervisor << '\n';
}
