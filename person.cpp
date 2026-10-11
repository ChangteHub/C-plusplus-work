#include "person.h"
#include "utils.h"
#include <iomanip>

Person::Person(string id, string name, string gender, int age, string phone)
    : id(id), name(name), gender(gender), age(age), phone(phone) {}

Person::~Person() {}

// 录入基类字段
void Person::inputBase() {
    cout << "工号: ";
    cin >> id;
    cout << "姓名: ";
    cin >> name;
    cout << "性别: ";
    cin >> gender;
    age = readInt("年龄: ");
    cout << "联系电话: ";
    cin >> phone;
}

// 表格中的一行（基类公共字段 + 类别）
void Person::displayRow() const {
    cout << left << setw(16) << getType()
         << setw(12) << id
         << setw(12) << name
         << setw(8) << gender
         << setw(6) << age
         << setw(16) << phone;
}
