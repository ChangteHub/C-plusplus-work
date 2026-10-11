#include "worker.h"
#include "utils.h"
#include <iomanip>

Worker::Worker(string id, string name, string gender, int age, string phone,
               string department, string job, double salary)
    : Person(id, name, gender, age, phone), department(department), job(job), salary(salary) {}

void Worker::input() {
    inputBase();
    cout << "所属部门: "; cin >> department;
    cout << "岗位: "; cin >> job;
    salary = readDouble("工资: ");
}

void Worker::display() const {
    cout << "----------------------------------------\n"
         << "人员类别: 普通工作者\n工号: " << id << "\n姓名: " << name
         << "\n性别: " << gender << "\n年龄: " << age << "\n联系电话: " << phone
         << "\n所属部门: " << department << "\n岗位: " << job
         << "\n工资: " << fixed << setprecision(2) << salary
         << "\n----------------------------------------\n";
}

void Worker::displayRow() const {
    Person::displayRow();
    cout << left << setw(18) << department << setw(12) << job
         << setw(12) << fixed << setprecision(2) << salary;
}

void Worker::save(ostream& os) const {
    os << "Worker|" << id << '|' << name << '|' << gender << '|' << age << '|'
       << phone << '|' << department << '|' << job << '|' << salary << '\n';
}
