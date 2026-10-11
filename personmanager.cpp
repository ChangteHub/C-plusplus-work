#include "personmanager.h"
#include "teacher.h"
#include "adminstaff.h"
#include "utils.h"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <cstdlib>

PersonManager::PersonManager() {}

PersonManager::~PersonManager() {
    clearPeople();
}

void PersonManager::clearPeople() {
    for (size_t i = 0; i < people.size(); ++i) {
        delete people[i];
    }
    people.clear();
}

int PersonManager::findIndexById(const string& id) const {
    for (size_t i = 0; i < people.size(); ++i) {
        if (people[i]->getId() == id) return (int)i;
    }
    return -1;
}

// 添加人员
void PersonManager::addPerson() {
    cout << "\n===== 添加人员 =====\n";
    cout << "1. 教师\n";
    cout << "2. 行政人员\n";
    cout << "0. 返回\n";
    int choice = readInt("请选择类别: ");

    Person* p = NULL;
    if (choice == 1) {
        Teacher* t = new Teacher();
        t->input();
        p = t;
    } else if (choice == 2) {
        AdminStaff* a = new AdminStaff();
        a->input();
        p = a;
    } else if (choice == 0) {
        return;
    } else {
        cout << "无效选择！\n";
        return;
    }

    // 检查工号是否重复
    if (findIndexById(p->getId()) != -1) {
        cout << "工号已存在，添加失败！\n";
        delete p;
        return;
    }

    people.push_back(p);
    cout << "添加成功！\n";
}

// 删除人员
void PersonManager::deletePerson() {
    cout << "\n===== 删除人员 =====\n";
    if (people.empty()) {
        cout << "当前没有任何人员记录！\n";
        return;
    }
    string id;
    cout << "请输入要删除的工号: ";
    cin >> id;

    int idx = findIndexById(id);
    if (idx == -1) {
        cout << "未找到该工号对应的人员！\n";
        return;
    }

    cout << "找到以下人员:\n";
    people[idx]->display();
    string confirm;
    cout << "确认删除？(y/n): ";
    cin >> confirm;
    if (confirm == "y" || confirm == "Y") {
        delete people[idx];
        people.erase(people.begin() + idx);
        cout << "删除成功！\n";
    } else {
        cout << "已取消删除。\n";
    }
}

// 修改人员
void PersonManager::modifyPerson() {
    cout << "\n===== 修改人员 =====\n";
    if (people.empty()) {
        cout << "当前没有任何人员记录！\n";
        return;
    }
    string id;
    cout << "请输入要修改的工号: ";
    cin >> id;

    int idx = findIndexById(id);
    if (idx == -1) {
        cout << "未找到该工号对应的人员！\n";
        return;
    }

    cout << "当前信息:\n";
    people[idx]->display();

    cout << "请重新录入该人员的信息:\n";
    string type = people[idx]->getType();
    Person* p = NULL;
    if (type == "教师") {
        Teacher* t = new Teacher();
        t->input();
        p = t;
    } else {
        AdminStaff* a = new AdminStaff();
        a->input();
        p = a;
    }

    if (p->getId() != id && findIndexById(p->getId()) != -1) {
        cout << "该工号已被其他人使用，修改失败！\n";
        delete p;
        return;
    }

    delete people[idx];
    people[idx] = p;
    cout << "修改成功！\n";
}


// 查询人员
void PersonManager::searchPerson() const {
    cout << "\n===== 查询人员 =====\n";
    if (people.empty()) {
        cout << "当前没有任何人员记录！\n";
        return;
    }
    cout << "1. 按工号查询\n";
    cout << "2. 按姓名查询\n";
    cout << "3. 按类别查询\n";
    cout << "0. 返回\n";
    int choice = readInt("请选择查询方式: ");

    if (choice == 1) {
        string id;
        cout << "请输入工号: ";
        cin >> id;
        int idx = findIndexById(id);
        if (idx == -1) {
            cout << "未找到该人员！\n";
        } else {
            people[idx]->display();
        }
    } else if (choice == 2) {
        string name;
        cout << "请输入姓名: ";
        cin >> name;
        bool found = false;
        for (size_t i = 0; i < people.size(); ++i) {
            if (people[i]->getName() == name) {
                people[i]->display();
                found = true;
            }
        }
        if (!found) cout << "未找到该姓名的人员！\n";
    } else if (choice == 3) {
        cout << "1. 教师\n2. 行政人员\n";
        int t = readInt("请选择类别: ");
        string type = (t == 1) ? "教师" : "行政人员";
        bool found = false;
        for (size_t i = 0; i < people.size(); ++i) {
            if (people[i]->getType() == type) {
                people[i]->display();
                found = true;
            }
        }
        if (!found) cout << "未找到该类别的人员！\n";
    } else if (choice == 0) {
        return;
    } else {
        cout << "无效选择！\n";
    }
}

// 打印表头
static void printHeader() {
    cout << left << setw(16) << "类别"
         << setw(12) << "工号"
         << setw(12) << "姓名"
         << setw(8) << "性别"
         << setw(6) << "年龄"
         << setw(16) << "电话"
         << setw(18) << "院系/部门"
         << setw(12) << "职称/职务"
         << setw(12) << "工资" << "\n";
    cout << string(112, '-') << "\n";
}

// 显示所有人员
void PersonManager::displayAll() const {
    cout << "\n===== 全部人员列表 =====\n";
    if (people.empty()) {
        cout << "当前没有任何人员记录！\n";
        return;
    }
    printHeader();
    for (size_t i = 0; i < people.size(); ++i) {
        people[i]->displayRow();
        cout << "\n";
    }
    cout << "共 " << people.size() << " 条记录。\n";
}

// 统计信息
void PersonManager::statistics() const {
    cout << "\n===== 统计信息 =====\n";
    int total = (int)people.size();
    int teacherCount = 0, adminCount = 0;
    double totalSalary = 0;
    int ageSum = 0;

    for (size_t i = 0; i < people.size(); ++i) {
        if (people[i]->getType() == "教师") teacherCount++;
        else adminCount++;
        totalSalary += people[i]->getSalary();
        ageSum += people[i]->getAge();
    }

    cout << "总人数: " << total << "\n";
    cout << "教师人数: " << teacherCount << "\n";
    cout << "行政人员数: " << adminCount << "\n";
    cout << fixed << setprecision(2);
    cout << "工资总额: " << totalSalary << "\n";
    if (total > 0) {
        cout << "平均年龄: " << fixed << setprecision(1)
             << (double)ageSum / total << "\n";
        cout << "平均工资: " << fixed << setprecision(2)
             << totalSalary / total << "\n";
    }
}

// 按年龄排序(升序)
void PersonManager::sortByAge() const {
    cout << "\n===== 按年龄排序(升序) =====\n";
    if (people.empty()) {
        cout << "当前没有任何人员记录！\n";
        return;
    }
    vector<Person*> tmp = people;
    std::sort(tmp.begin(), tmp.end(),
              [](Person* a, Person* b) { return a->getAge() < b->getAge(); });
    printHeader();
    for (size_t i = 0; i < tmp.size(); ++i) {
        tmp[i]->displayRow();
        cout << "\n";
    }
}

// 按工资排序(降序)
void PersonManager::sortBySalary() const {
    cout << "\n===== 按工资排序(降序) =====\n";
    if (people.empty()) {
        cout << "当前没有任何人员记录！\n";
        return;
    }
    vector<Person*> tmp = people;
    std::sort(tmp.begin(), tmp.end(),
              [](Person* a, Person* b) { return a->getSalary() > b->getSalary(); });
    printHeader();
    for (size_t i = 0; i < tmp.size(); ++i) {
        tmp[i]->displayRow();
        cout << "\n";
    }
}

// 保存到文件
void PersonManager::saveToFile(const string& filename) const {
    ofstream ofs(filename.c_str());
    if (!ofs) {
        cout << "无法打开文件进行写入: " << filename << "\n";
        return;
    }
    for (size_t i = 0; i < people.size(); ++i) {
        people[i]->save(ofs);
    }
    ofs.close();
    cout << "已成功保存 " << people.size() << " 条记录到 " << filename << "\n";
}

// 从文件加载
void PersonManager::loadFromFile(const string& filename) {
    ifstream ifs(filename.c_str());
    if (!ifs) {
        cout << "无法打开文件: " << filename << "\n";
        return;
    }
    clearPeople();  // 加载前清空现有数据
    string line;
    while (getline(ifs, line)) {
        if (line.empty()) continue;
        Person* p = createPersonFromLine(line);
        if (p != NULL) {
            // 跳过重复工号
            if (findIndexById(p->getId()) == -1) {
                people.push_back(p);
            } else {
                delete p;
            }
        }
    }
    ifs.close();
    cout << "已从 " << filename << " 加载 " << people.size() << " 条记录。\n";
}

// 根据一行文本创建人员对象
Person* PersonManager::createPersonFromLine(const string& line) const {
    vector<string> f = split(line, '|');
    if (f.empty()) return NULL;

    if (f[0] == "Teacher" && f.size() >= 9) {
        return new Teacher(f[1], f[2], f[3], atoi(f[4].c_str()),
                           f[5], f[6], f[7], atof(f[8].c_str()));
    } else if (f[0] == "Admin" && f.size() >= 9) {
        return new AdminStaff(f[1], f[2], f[3], atoi(f[4].c_str()),
                              f[5], f[6], f[7], atof(f[8].c_str()));
    }
    return NULL;
}

// 主菜单
void PersonManager::menu() {
    const string dataFile = "school_data.txt";
    int choice;
    do {
        cout << "\n========================================\n";
        cout << "        学校人员信息管理系统            \n";
        cout << "========================================\n";
        cout << "   1. 添加人员\n";
        cout << "   2. 删除人员\n";
        cout << "   3. 修改人员\n";
        cout << "   4. 查询人员\n";
        cout << "   5. 显示全部人员\n";
        cout << "   6. 统计信息\n";
        cout << "   7. 按年龄排序\n";
        cout << "   8. 按工资排序\n";
        cout << "   9. 保存到文件\n";
        cout << "  10. 从文件加载\n";
        cout << "   0. 退出系统\n";
        cout << "========================================\n";
        choice = readInt("请选择操作: ");

        switch (choice) {
            case 1: addPerson(); break;
            case 2: deletePerson(); break;
            case 3: modifyPerson(); break;
            case 4: searchPerson(); break;
            case 5: displayAll(); break;
            case 6: statistics(); break;
            case 7: sortByAge(); break;
            case 8: sortBySalary(); break;
            case 9: saveToFile(dataFile); break;
            case 10: loadFromFile(dataFile); break;
            case 0:
                cout << "感谢使用，再见！\n";
                break;
            default:
                cout << "无效选择，请重新输入！\n";
        }
    } while (choice != 0);
}
