#ifndef SERVICE_H
#define SERVICE_H

#include <string>
#include <vector>
#include "dao.h"
#include "person.h"

using namespace std;

enum ServiceResult {
    SVC_OK = 0,
    SVC_BAD_REQUEST,
    SVC_DUPLICATE,
    SVC_NOT_FOUND,
    SVC_STORAGE_ERROR // Controller 应返回 503；不要当作参数错误或成功。
};

class Service {
private:
    Dao& dao;

public:
    explicit Service(Dao& d) : dao(d) {}

    // 数据库错误抛 runtime_error；返回值里的对象由调用方释放。
    vector<Person*> list();
    ServiceResult create(const Person& proto);
    ServiceResult update(const string& originalId, const Person& proto);
    ServiceResult remove(const string& id);
};

#endif // SERVICE_H