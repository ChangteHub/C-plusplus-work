#include "student.h"
#include "utils.h"
#include <iomanip>

Student::Student(string id, string name, string gender, int age, string phone,
                 string department, string major, string className, int admissionYear)
    : Person(id, name, gender, age, phone), department(department), major(major),
      className(className), admissionYear(admissionYear) {}

void Student::input() {
    inputBase("学号");
    cout << "所属学院: "; cin >> department;
    cout << "专业: "; cin >> major;
    cout << "班级: "; cin >> className;
    admissionYear = readInt("入学年份: ");
}

void Student::display() const {
    cout << "----------------------------------------\n"
         << "人员类别: 学生\n学号: " << id << "\n姓名: " << name
         << "\n性别: " << gender << "\n年龄: " << age << "\n联系电话: " << phone
         << "\n所属学院: " << department << "\n专业: " << major
         << "\n班级: " << className << "\n入学年份: " << admissionYear
         << "\n----------------------------------------\n";
}

void Student::displayRow() const {
    Person::displayRow();
    cout << left << setw(18) << department << setw(12) << major << setw(12) << "—"
         << "  " << className << " / " << admissionYear << "年入学";
}

void Student::save(ostream& os) const {
    os << "Student|" << id << '|' << name << '|' << gender << '|' << age << '|'
       << phone << '|' << department << '|' << major << '|' << className
       << '|' << admissionYear << '\n';
}
