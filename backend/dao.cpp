#include "dao.h"
#include "teacher.h"
#include "adminstaff.h"
#include "student.h"
#include "graduate.h"
#include "worker.h"
#include <sqlite3.h>
#include <memory>
#include <set>

using namespace std;

namespace {

runtime_error storageError(sqlite3* db, const string& operation) {
    return runtime_error("SQLite " + operation + ": " +
                         (db ? sqlite3_errmsg(db) : "database is not open"));
}

sqlite3* requireOpen(void* handle, bool opened) {
    if (!handle || !opened) throw runtime_error("SQLite database is not open");
    return static_cast<sqlite3*>(handle);
}

class Statement {
    sqlite3* db;
    sqlite3_stmt* stmt;
public:
    Statement(sqlite3* handle, const char* sql) : db(handle), stmt(NULL) {
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
            runtime_error error = storageError(db, "prepare failed");
            if (stmt) sqlite3_finalize(stmt);
            throw error;
        }
    }
    ~Statement() { sqlite3_finalize(stmt); }
    sqlite3_stmt* get() const { return stmt; }
    void checkBind(int result) {
        if (result != SQLITE_OK) throw storageError(db, "bind failed");
    }
    void text(int index, const string& value) {
        checkBind(sqlite3_bind_text(stmt, index, value.data(),
                                  static_cast<int>(value.size()), SQLITE_TRANSIENT));
    }
    void integer(int index, int value) {
        checkBind(sqlite3_bind_int(stmt, index, value));
    }
    void real(int index, double value) {
        checkBind(sqlite3_bind_double(stmt, index, value));
    }
private:
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;
};

void execute(sqlite3* handle, const char* sql) {
    char* error = NULL;
    if (sqlite3_exec(handle, sql, NULL, NULL, &error) != SQLITE_OK) {
        string detail = error ? error : sqlite3_errmsg(handle);
        sqlite3_free(error);
        throw runtime_error("SQLite initialization failed: " + detail);
    }
}

string colText(sqlite3_stmt* stmt, int column) {
    const unsigned char* value = sqlite3_column_text(stmt, column);
    return value ? string(reinterpret_cast<const char*>(value),
                          sqlite3_column_bytes(stmt, column)) : string();
}

Person* rowToPerson(sqlite3_stmt* stmt) {
    string type = colText(stmt, 0);
    string id = colText(stmt, 1);
    string name = colText(stmt, 2);
    string gender = colText(stmt, 3);
    int age = sqlite3_column_int(stmt, 4);
    string phone = colText(stmt, 5);
    string department = colText(stmt, 6);
    double salary = sqlite3_column_double(stmt, 9);
    if (type == "Teacher")
        return new Teacher(id, name, gender, age, phone, department,
                           colText(stmt, 7), salary);
    if (type == "Admin")
        return new AdminStaff(id, name, gender, age, phone, department,
                              colText(stmt, 8), salary);
    if (type == "Student")
        return new Undergraduate(id, name, gender, age, phone, department,
                           colText(stmt, 10), colText(stmt, 11), sqlite3_column_int(stmt, 12));
    if (type == "Worker")
        return new Worker(id, name, gender, age, phone, department, colText(stmt, 13), salary);
    if (type == "Graduate")
        return new GraduateStudent(id, name, gender, age, phone, department, colText(stmt, 10),
            colText(stmt, 11), sqlite3_column_int(stmt, 12), colText(stmt, 14), colText(stmt, 15));
    throw runtime_error("SQLite contains an unsupported personnel type");
}

const char* SELECT_COLS =
    "SELECT type,id,name,gender,age,phone,department,title,position,salary,"
    "major,class_name,admission_year,job,research_direction,supervisor FROM people";

void migrateSchema(sqlite3* handle) {
    // 先核对旧字段，再在事务内补充新字段；原有记录和主键不变。
    {
        Statement originalSchema(handle,
            "SELECT type,id,name,gender,age,phone,department,title,position,salary FROM people");
    }
    execute(handle, "BEGIN IMMEDIATE");
    try {
        set<string> columns;
        {
            Statement info(handle, "PRAGMA table_info(people)");
            int status;
            while ((status = sqlite3_step(info.get())) == SQLITE_ROW)
                columns.insert(colText(info.get(), 1));
            if (status != SQLITE_DONE) throw storageError(handle, "schema read failed");
        }
        if (!columns.count("major"))
            execute(handle, "ALTER TABLE people ADD COLUMN major TEXT NOT NULL DEFAULT ''");
        if (!columns.count("class_name"))
            execute(handle, "ALTER TABLE people ADD COLUMN class_name TEXT NOT NULL DEFAULT ''");
        if (!columns.count("admission_year"))
            execute(handle, "ALTER TABLE people ADD COLUMN admission_year INTEGER NOT NULL DEFAULT 0");
        if (!columns.count("job"))
            execute(handle, "ALTER TABLE people ADD COLUMN job TEXT NOT NULL DEFAULT ''");
        if (!columns.count("research_direction"))
            execute(handle, "ALTER TABLE people ADD COLUMN research_direction TEXT NOT NULL DEFAULT ''");
        if (!columns.count("supervisor"))
            execute(handle, "ALTER TABLE people ADD COLUMN supervisor TEXT NOT NULL DEFAULT ''");
        Statement schemaCheck(handle, SELECT_COLS);
        execute(handle, "COMMIT");
    } catch (...) {
        sqlite3_exec(handle, "ROLLBACK", NULL, NULL, NULL);
        throw;
    }
}

void bindPerson(Statement& stmt, Person* person) {
    if (!person) throw invalid_argument("Cannot persist a null person");
    const Teacher* teacher = dynamic_cast<const Teacher*>(person);
    const AdminStaff* admin = dynamic_cast<const AdminStaff*>(person);
    const Student* student = dynamic_cast<const Student*>(person);
    const Worker* worker = dynamic_cast<const Worker*>(person);
    const GraduateStudent* graduate = dynamic_cast<const GraduateStudent*>(person);
    if (!teacher && !admin && !student && !worker) throw invalid_argument("Unsupported personnel type");
    stmt.text(1, graduate ? "Graduate" : teacher ? "Teacher" : admin ? "Admin" : student ? "Student" : "Worker");
    stmt.text(2, person->getId());
    stmt.text(3, person->getName());
    stmt.text(4, person->getGender());
    stmt.integer(5, person->getAge());
    stmt.text(6, person->getPhone());
    stmt.text(7, graduate ? graduate->getDepartment() : teacher ? teacher->getDepartment() : admin ? admin->getDepartment() :
                 student ? student->getDepartment() : worker->getDepartment());
    stmt.text(8, teacher && !graduate ? teacher->getTitle() : "");
    stmt.text(9, admin ? admin->getPosition() : "");
    stmt.real(10, person->getSalary());
    stmt.text(11, student ? student->getMajor() : "");
    stmt.text(12, student ? student->getClassName() : "");
    stmt.integer(13, student ? student->getAdmissionYear() : 0);
    stmt.text(14, worker ? worker->getJob() : "");
    stmt.text(15, graduate ? graduate->getResearchDirection() : "");
    stmt.text(16, graduate ? graduate->getSupervisor() : "");
}

void checkWrite(sqlite3* handle, int result) {
    if (result == SQLITE_DONE) return;
    int extended = sqlite3_extended_errcode(handle);
    if (extended == SQLITE_CONSTRAINT_PRIMARYKEY || extended == SQLITE_CONSTRAINT_UNIQUE)
        throw DaoDuplicateError("Personnel ID already exists");
    throw storageError(handle, "write failed");
}

} // namespace

Dao::Dao() : db(NULL), opened(false) {}
Dao::~Dao() { close(); }

bool Dao::open(const string& path) {
    lock_guard<mutex> lock(dbMutex);
    sqlite3* handle = NULL;
    if (sqlite3_open_v2(path.c_str(), &handle,
                        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
                        NULL) != SQLITE_OK) {
        runtime_error error = storageError(handle, "open failed");
        if (handle) sqlite3_close_v2(handle);
        throw error;
    }
    try {
        if (sqlite3_extended_result_codes(handle, 1) != SQLITE_OK ||
            sqlite3_busy_timeout(handle, 5000) != SQLITE_OK)
            throw storageError(handle, "configuration failed");
        execute(handle, "PRAGMA journal_mode=WAL;");
        execute(handle, "PRAGMA synchronous=NORMAL;");
        execute(handle, "PRAGMA foreign_keys=ON;");
        execute(handle,
            "CREATE TABLE IF NOT EXISTS people ("
            "type TEXT NOT NULL,id TEXT PRIMARY KEY,name TEXT,gender TEXT,"
            "age INTEGER,phone TEXT,department TEXT,title TEXT,position TEXT,salary REAL);");
        // 提前验证已有数据库的列兼容性，防止服务启动后才发现损坏/旧表问题。
        migrateSchema(handle);
    } catch (...) {
        sqlite3_close_v2(handle);
        throw;
    }
    if (db) sqlite3_close_v2(static_cast<sqlite3*>(db));
    db = handle;
    opened = true;
    return true;
}

void Dao::close() {
    lock_guard<mutex> lock(dbMutex);
    if (db) sqlite3_close_v2(static_cast<sqlite3*>(db));
    db = NULL;
    opened = false;
}

vector<Person*> Dao::all() {
    lock_guard<mutex> lock(dbMutex);
    sqlite3* handle = requireOpen(db, opened);
    string sql = string(SELECT_COLS) + " ORDER BY id";
    Statement stmt(handle, sql.c_str());
    vector<Person*> result;
    try {
        int status;
        while ((status = sqlite3_step(stmt.get())) == SQLITE_ROW) {
            unique_ptr<Person> person(rowToPerson(stmt.get()));
            result.push_back(person.get());
            person.release();
        }
        if (status != SQLITE_DONE) throw storageError(handle, "read failed");
    } catch (...) {
        for (Person* person : result) delete person;
        throw;
    }
    return result;
}

Person* Dao::findById(const string& id) {
    lock_guard<mutex> lock(dbMutex);
    sqlite3* handle = requireOpen(db, opened);
    string sql = string(SELECT_COLS) + " WHERE id=?1";
    Statement stmt(handle, sql.c_str());
    stmt.text(1, id);
    int status = sqlite3_step(stmt.get());
    if (status == SQLITE_DONE) return NULL;
    if (status != SQLITE_ROW) throw storageError(handle, "read failed");
    unique_ptr<Person> person(rowToPerson(stmt.get()));
    if (sqlite3_step(stmt.get()) != SQLITE_DONE)
        throw storageError(handle, "read failed");
    return person.release();
}

bool Dao::exists(const string& id) {
    lock_guard<mutex> lock(dbMutex);
    sqlite3* handle = requireOpen(db, opened);
    Statement stmt(handle, "SELECT 1 FROM people WHERE id=?1");
    stmt.text(1, id);
    int status = sqlite3_step(stmt.get());
    if (status == SQLITE_ROW) return true;
    if (status == SQLITE_DONE) return false;
    throw storageError(handle, "read failed");
}

bool Dao::insert(Person* person) {
    lock_guard<mutex> lock(dbMutex);
    return insertUnlocked(person);
}
bool Dao::insertUnlocked(Person* person) {
    sqlite3* handle = requireOpen(db, opened);
    Statement stmt(handle,
        "INSERT INTO people(type,id,name,gender,age,phone,department,title,position,salary,"
        "major,class_name,admission_year,job,research_direction,supervisor) "
        "VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,?14,?15,?16)");
    bindPerson(stmt, person);
    checkWrite(handle, sqlite3_step(stmt.get()));
    return true;
}

bool Dao::update(const string& originalId, Person* person) {
    lock_guard<mutex> lock(dbMutex);
    sqlite3* handle = requireOpen(db, opened);
    execute(handle, "BEGIN IMMEDIATE");
    try {
    Statement stmt(handle,
        "UPDATE people SET type=?1,id=?2,name=?3,gender=?4,age=?5,phone=?6,"
        "department=?7,title=?8,position=?9,salary=?10,major=?11,class_name=?12,"
        "admission_year=?13,job=?14,research_direction=?15,supervisor=?16 WHERE id=?17");
    bindPerson(stmt, person);
    stmt.text(17, originalId);
    checkWrite(handle, sqlite3_step(stmt.get()));
    bool changed = sqlite3_changes(handle) > 0;
    // 变更编号由外键级联；变更身份时清理不适用的课程关联。
    Statement academicSchema(handle,"SELECT 1 FROM sqlite_master WHERE type='table' AND name='grades'");
    if (sqlite3_step(academicSchema.get()) == SQLITE_ROW && changed) {
        if (!dynamic_cast<Student*>(person)) {
            Statement grades(handle,"DELETE FROM grades WHERE student_id=?1");
            grades.text(1,person->getId()); checkWrite(handle,sqlite3_step(grades.get()));
        }
        if (!dynamic_cast<Teacher*>(person)) {
            Statement courses(handle,"UPDATE courses SET teacher_id=NULL WHERE teacher_id=?1");
            courses.text(1,person->getId()); checkWrite(handle,sqlite3_step(courses.get()));
        }
    }
    execute(handle,"COMMIT"); return changed;
    } catch (...) { sqlite3_exec(handle,"ROLLBACK",NULL,NULL,NULL); throw; }
}

bool Dao::remove(const string& id) {
    lock_guard<mutex> lock(dbMutex);
    sqlite3* handle = requireOpen(db, opened);
    Statement stmt(handle, "DELETE FROM people WHERE id=?1");
    stmt.text(1, id);
    checkWrite(handle, sqlite3_step(stmt.get()));
    return sqlite3_changes(handle) > 0;
}
