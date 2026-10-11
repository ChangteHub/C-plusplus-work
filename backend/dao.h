/* SQLite 数据访问层：每个 SQL 写操作由 SQLite 原子执行。 */
#ifndef DAO_H
#define DAO_H

#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>
#include "person.h"

using namespace std;

// 仅代表工号主键/唯一约束冲突；其他数据库故障抛 runtime_error。
class DaoDuplicateError : public runtime_error {
public:
    explicit DaoDuplicateError(const string& message) : runtime_error(message) {}
};

class Dao {
    // Academy 使用同一把锁、同一连接，原子写入人员及账号，避免孤立账号。
    friend class Academy;
private:
    void* db;
    bool opened;
    mutex dbMutex;
    bool insertUnlocked(Person* person);

    Dao(const Dao&) = delete;
    Dao& operator=(const Dao&) = delete;

public:
    Dao();
    ~Dao();

    // 成功返回 true；初始化失败抛 runtime_error，调用方必须报告并停止启动。
    bool open(const string& path);
    void close();

    // 查询失败抛 runtime_error，不能把故障当作空表/不存在。
    // 所有返回的 Person* 均由调用方负责 delete。
    vector<Person*> all();
    Person* findById(const string& id);
    bool exists(const string& id);

    // insert 的重复工号抛 DaoDuplicateError。
    // update/remove 返回 false 仅表示原记录不存在。
    bool insert(Person* p);
    bool update(const string& origId, Person* p);
    bool remove(const string& id);
};

#endif // DAO_H
