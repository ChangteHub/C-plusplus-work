#ifndef WORKER_H
#define WORKER_H

#include "person.h"

class Worker : public Person {
    string department;
    string job;
    double salary;
public:
    Worker(string id = "", string name = "", string gender = "", int age = 0,
           string phone = "", string department = "", string job = "", double salary = 0);
    ~Worker() override = default;
    string getDepartment() const { return department; }
    string getJob() const { return job; }
    string getType() const override { return "普通工作者"; }
    double getSalary() const override { return salary; }
    void input() override;
    void display() const override;
    void displayRow() const override;
    void save(ostream& os) const override;
};

#endif
