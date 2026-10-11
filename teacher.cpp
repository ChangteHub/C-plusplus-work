#include "teacher.h"
#include "utils.h"
#include <iomanip>

Teacher::Teacher(string id, string name, string gender, int age, string phone,
                 string department, string title, double salary)
    : Person(id, name, gender, age, phone),
      department(department), title(title), salary(salary) {}

Teacher::~Teacher() {}

void Teacher::input() {
    inputBase();
    cout << "所属院系: ";
    cin >> department;
    cout << "职称: ";
    cin >> title;
    salary = readDouble("工资: ");
}

void Teacher::display() const {
    cout << "----------------------------------------\n";
    cout << "人员类别: " << getType() << "\n";
    cout << "工号: " << id << "\n";
    cout << "姓名: " << name << "\n";
    cout << "性别: " << gender << "\n";
    cout << "年龄: " << age << "\n";
    cout << "联系电话: " << phone << "\n";
    cout << "所属院系: " << department << "\n";
    cout << "职称: " << title << "\n";
    cout << "工资: " << fixed << setprecision(2) << salary << "\n";
    cout << "----------------------------------------\n";
}

void Teacher::displayRow() const {
    Person::displayRow();
    cout << left << setw(18) << department
         << setw(12) << title
         << setw(12) << fixed << setprecision(2) << salary;
}

void Teacher::save(ostream& os) const {
    os << "Teacher|" << id << "|" << name << "|" << gender << "|"
       << age << "|" << phone << "|" << department << "|" << title
       << "|" << salary << "\n";
}
