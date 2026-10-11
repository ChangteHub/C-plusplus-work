#include "adminstaff.h"
#include "utils.h"
#include <iomanip>

AdminStaff::AdminStaff(string id, string name, string gender, int age,
                       string phone, string department, string position,
                       double salary)
    : Person(id, name, gender, age, phone),
      department(department), position(position), salary(salary) {}

AdminStaff::~AdminStaff() {}

void AdminStaff::input() {
    inputBase();
    cout << "所属部门: ";
    cin >> department;
    cout << "职务: ";
    cin >> position;
    salary = readDouble("工资: ");
}

void AdminStaff::display() const {
    cout << "----------------------------------------\n";
    cout << "人员类别: " << getType() << "\n";
    cout << "工号: " << id << "\n";
    cout << "姓名: " << name << "\n";
    cout << "性别: " << gender << "\n";
    cout << "年龄: " << age << "\n";
    cout << "联系电话: " << phone << "\n";
    cout << "所属部门: " << department << "\n";
    cout << "职务: " << position << "\n";
    cout << "工资: " << fixed << setprecision(2) << salary << "\n";
    cout << "----------------------------------------\n";
}

void AdminStaff::displayRow() const {
    Person::displayRow();
    cout << left << setw(18) << department
         << setw(12) << position
         << setw(12) << fixed << setprecision(2) << salary;
}

void AdminStaff::save(ostream& os) const {
    os << "Admin|" << id << "|" << name << "|" << gender << "|"
       << age << "|" << phone << "|" << department << "|" << position
       << "|" << salary << "\n";
}
