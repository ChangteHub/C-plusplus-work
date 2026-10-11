#ifndef PERSON_H
#define PERSON_H

#include <string>
#include <iostream>
using namespace std;

// 人员基类（抽象类，不能直接实例化）
class Person {
protected:
    string id;      // 工号或学号，所有人员共用唯一编号
    string name;    // 姓名
    string gender;  // 性别
    int age;        // 年龄
    string phone;   // 联系电话

    // 录入基类字段（供派生类 input() 调用）
    void inputBase(const string& idLabel = "工号");

public:
    Person(string id = "", string name = "", string gender = "",
           int age = 0, string phone = "");
    virtual ~Person();

    // 访问器
    string getId() const { return id; }
    string getName() const { return name; }
    string getGender() const { return gender; }
    int getAge() const { return age; }
    string getPhone() const { return phone; }

    // 修改器
    void setId(const string& v) { id = v; }
    void setName(const string& v) { name = v; }
    void setGender(const string& v) { gender = v; }
    void setAge(int v) { age = v; }
    void setPhone(const string& v) { phone = v; }

    // 纯虚函数：由派生类实现
    virtual void display() const = 0;           // 显示详细信息
    virtual string getType() const = 0;         // 获取人员类别
    virtual double getSalary() const = 0;       // 获取工资
    virtual void input() = 0;                   // 从键盘录入全部信息
    virtual void save(ostream& os) const = 0;   // 保存到文件

    // 显示一行表格数据（多态）
    virtual void displayRow() const;
};

#endif // PERSON_H
