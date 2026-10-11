#include "person.h"
#include "utils.h"
#include <iomanip>

Person::Person(string id, string name, string gender, int age, string phone)
    : id(id), name(name), gender(gender), age(age), phone(phone) { ++liveCount; }

atomic<int> Person::liveCount{0};
Person::Person(const Person& other)
    : id(other.id), name(other.name), gender(other.gender), age(other.age), phone(other.phone) { ++liveCount; }
Person::~Person() { --liveCount; }
ostream& operator<<(ostream& os, const Person& person) {
    return os << person.getType() << " " << person.id << " " << person.name
              << " " << person.gender << " " << person.age << "岁";
}

// 录入基类字段
void Person::inputBase(const string& idLabel) {
    cout << idLabel << ": ";
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
