#ifndef PERSONMANAGER_H
#define PERSONMANAGER_H

#include "person.h"
#include <vector>

// 人员信息管理系统
class PersonManager {
private:
    std::vector<Person*> people;  // 使用基类指针实现多态

    // 辅助函数
    void clearPeople();
    Person* createPersonFromLine(const string& line) const;
    int findIndexById(const string& id) const;

public:
    PersonManager();
    ~PersonManager();

    // 功能操作
    void addPerson();
    void deletePerson();
    void modifyPerson();
    void searchPerson() const;
    void displayAll() const;
    void statistics() const;
    void sortByAge() const;
    void sortBySalary() const;

    // 文件操作
    void saveToFile(const string& filename) const;
    void loadFromFile(const string& filename);

    // 主菜单
    void menu();
};

#endif // PERSONMANAGER_H
