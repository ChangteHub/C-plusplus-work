#include "academy.h"
#include "academic.h"
#include "crypto.h"
#include "sqlite3.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
using nlohmann::json;
namespace {
long long now() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}
struct Query {
    sqlite3 *db;
    sqlite3_stmt *stmt = nullptr;
    Query(sqlite3 *database, const char *sql) : db(database) {
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
            throw std::runtime_error(sqlite3_errmsg(db));
    }
    ~Query() {
        if (stmt)
            sqlite3_finalize(stmt);
    }
    void text(int i, const std::string &value) {
        if (sqlite3_bind_text(stmt, i, value.data(), static_cast<int>(value.size()),
                              SQLITE_TRANSIENT) != SQLITE_OK)
            throw std::runtime_error(sqlite3_errmsg(db));
    }
    void number(int i, long long value) {
        if (sqlite3_bind_int64(stmt, i, value) != SQLITE_OK)
            throw std::runtime_error(sqlite3_errmsg(db));
    }
    void null(int i) { sqlite3_bind_null(stmt, i); }
    bool row() {
        int s = sqlite3_step(stmt);
        if (s == SQLITE_ROW)
            return true;
        if (s == SQLITE_DONE)
            return false;
        throw std::runtime_error(sqlite3_errmsg(db));
    }
    std::string string(int i) {
        const auto *p = sqlite3_column_text(stmt, i);
        return p ? std::string(reinterpret_cast<const char *>(p), sqlite3_column_bytes(stmt, i))
                 : "";
    }
    long long integer(int i) { return sqlite3_column_int64(stmt, i); }
    void done() {
        int s = sqlite3_step(stmt);
        if (s == SQLITE_DONE)
            return;
        int error = sqlite3_extended_errcode(db);
        if (error == SQLITE_CONSTRAINT_UNIQUE || error == SQLITE_CONSTRAINT_PRIMARYKEY)
            throw DaoDuplicateError("记录已存在");
        if (error == SQLITE_CONSTRAINT_FOREIGNKEY)
            throw std::invalid_argument("关联的人员或课程不存在");
        throw std::runtime_error(sqlite3_errmsg(db));
    }
};
void execute(sqlite3 *db, const char *sql) {
    char *error = nullptr;
    int status = sqlite3_exec(db, sql, nullptr, nullptr, &error);
    std::string message = error ? error : "";
    sqlite3_free(error);
    if (status != SQLITE_OK)
        throw std::runtime_error(message);
}
struct Transaction {
    sqlite3 *db;
    bool committed = false;
    explicit Transaction(sqlite3 *handle) : db(handle) { execute(db, "BEGIN IMMEDIATE"); }
    ~Transaction() {
        if (!committed)
            sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
    }
    void commit() {
        execute(db, "COMMIT");
        committed = true;
    }
};
bool validId(const std::string &id) {
    if (id.empty() || id.size() > 32)
        return false;
    for (unsigned char c : id)
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
              c == '_' || c == '-'))
            return false;
    return true;
}
std::string field(const json &value, const char *key, bool optional = false) {
    if (optional && !value.contains(key))
        return "";
    if (!value.contains(key) || !value.at(key).is_string())
        throw std::invalid_argument(std::string("无效字段：") + key);
    std::string text = value.at(key).get<std::string>();
    auto first = text.find_first_not_of(" \t\r\n"), last = text.find_last_not_of(" \t\r\n");
    text = first == std::string::npos ? "" : text.substr(first, last - first + 1);
    if ((text.empty() && !optional) || text.size() > 512)
        throw std::invalid_argument(std::string("字段为空或过长：") + key);
    for (unsigned char c : text)
        if (c < 32 || c == 127)
            throw std::invalid_argument("字段包含控制字符");
    return text;
}
std::string passwordSalt(const std::string &password) {
    if (!schoolcrypto::validPassword(password))
        throw std::invalid_argument("密码需要 8–128 个字符，不能包含控制字符");
    return schoolcrypto::randomHex(16);
}
std::string personType(sqlite3 *db, const std::string &id) {
    Query q(db, "SELECT type FROM people WHERE id=?1");
    q.text(1, id);
    if (!q.row())
        throw std::invalid_argument("人员不存在");
    return q.string(0);
}
void accountInsert(sqlite3 *db, const std::string &id, const std::string &salt,
                   const std::string &hash) {
    Query q(db,
            "INSERT INTO accounts(person_id,salt,password_hash,iterations) VALUES(?1,?2,?3,?4)");
    q.text(1, id);
    q.text(2, salt);
    q.text(3, hash);
    q.number(4, schoolcrypto::PasswordIterations);
    q.done();
}
} // namespace
Academy::Academy(Dao &database) : dao(database) {
    std::lock_guard<std::mutex> guard(dao.dbMutex);
    auto *db = static_cast<sqlite3 *>(dao.db);
    Transaction transaction(db);
    execute(db, "CREATE TABLE IF NOT EXISTS accounts(person_id TEXT PRIMARY KEY REFERENCES "
                "people(id) ON UPDATE CASCADE ON DELETE CASCADE,salt TEXT NOT NULL,password_hash "
                "TEXT NOT NULL,iterations INTEGER NOT NULL);");
    execute(db, "CREATE TABLE IF NOT EXISTS sessions(token_hash TEXT PRIMARY KEY,person_id TEXT "
                "REFERENCES accounts(person_id) ON UPDATE CASCADE ON DELETE CASCADE,is_admin "
                "INTEGER NOT NULL,expires INTEGER NOT NULL);");
    execute(db, "CREATE TABLE IF NOT EXISTS courses(id TEXT PRIMARY KEY,name TEXT NOT "
                "NULL,credit_hundredths INTEGER NOT NULL CHECK(credit_hundredths>0),teacher_id "
                "TEXT REFERENCES people(id) ON UPDATE CASCADE ON DELETE SET NULL);");
    execute(db, "CREATE TABLE IF NOT EXISTS grades(course_id TEXT REFERENCES courses(id) ON UPDATE "
                "CASCADE ON DELETE CASCADE,student_id TEXT REFERENCES people(id) ON UPDATE CASCADE "
                "ON DELETE CASCADE,score_hundredths INTEGER NOT NULL CHECK(score_hundredths "
                "BETWEEN 0 AND 10000),PRIMARY KEY(course_id,student_id));");
    execute(db, "CREATE INDEX IF NOT EXISTS grades_student ON grades(student_id);");
    // 管理员凭据来自配置；重启后旧管理员会话需重新认证。
    execute(db, "DELETE FROM sessions WHERE is_admin=1;");
    transaction.commit();
}
void Academy::registerPerson(Person &person, const std::string &password) {
    auto salt = passwordSalt(password), hash = schoolcrypto::derive(password, salt);
    std::lock_guard<std::mutex> guard(dao.dbMutex);
    auto *db = static_cast<sqlite3 *>(dao.db);
    Transaction transaction(db);
    dao.insertUnlocked(&person);
    accountInsert(db, person.getId(), salt, hash);
    transaction.commit();
}
void Academy::checkStorage() {
    std::lock_guard<std::mutex> guard(dao.dbMutex);
    auto *db = static_cast<sqlite3 *>(dao.db);
    for (const char *sql : {"SELECT salt,password_hash,iterations FROM accounts LIMIT 1",
                            "SELECT token_hash,expires FROM sessions LIMIT 1",
                            "SELECT id,name,credit_hundredths,teacher_id FROM courses LIMIT 1",
                            "SELECT course_id,student_id,score_hundredths FROM grades LIMIT 1"}) {
        Query q(db, sql);
        q.row();
    }
}
void Academy::createAccount(const std::string &id, const std::string &password) {
    auto salt = passwordSalt(password), hash = schoolcrypto::derive(password, salt);
    std::lock_guard<std::mutex> guard(dao.dbMutex);
    auto *db = static_cast<sqlite3 *>(dao.db);
    personType(db, id);
    accountInsert(db, id, salt, hash);
}
bool Academy::verify(const std::string &id, const std::string &password) {
    if (!schoolcrypto::validPassword(password))
        return false;
    std::string salt(32, '0'), hash(64, '0');
    int iterations = schoolcrypto::PasswordIterations;
    bool found = false;
    {
        std::lock_guard<std::mutex> guard(dao.dbMutex);
        Query q(static_cast<sqlite3 *>(dao.db),
                "SELECT salt,password_hash,iterations FROM accounts WHERE person_id=?1");
        q.text(1, id);
        if (q.row()) {
            salt = q.string(0);
            hash = q.string(1);
            iterations = static_cast<int>(q.integer(2));
            found = true;
        }
    }
    bool matched = schoolcrypto::equal(hash, schoolcrypto::derive(password, salt, iterations));
    return found && matched;
}
json Academy::accounts() {
    std::lock_guard<std::mutex> guard(dao.dbMutex);
    Query q(static_cast<sqlite3 *>(dao.db), "SELECT a.person_id,p.name,p.type FROM accounts a JOIN "
                                            "people p ON p.id=a.person_id ORDER BY a.person_id");
    json result = json::array();
    while (q.row())
        result.push_back({{"personId", q.string(0)}, {"name", q.string(1)}, {"type", q.string(2)}});
    return result;
}
bool Academy::removeAccount(const std::string &id) {
    std::lock_guard<std::mutex> guard(dao.dbMutex);
    auto *db = static_cast<sqlite3 *>(dao.db);
    Query q(db, "DELETE FROM accounts WHERE person_id=?1");
    q.text(1, id);
    q.done();
    return sqlite3_changes(db) > 0;
}
bool Academy::cancel(const std::string &id, const std::string &password) {
    if (!verify(id, password))
        return false;
    return dao.remove(id);
}
void Academy::changePassword(const std::string &id, const std::string &oldPassword,
                             const std::string &newPassword) {
    if (!verify(id, oldPassword))
        throw std::invalid_argument("原密码不正确");
    auto salt = passwordSalt(newPassword), hash = schoolcrypto::derive(newPassword, salt);
    std::lock_guard<std::mutex> guard(dao.dbMutex);
    auto *db = static_cast<sqlite3 *>(dao.db);
    Transaction transaction(db);
    Query q(db, "UPDATE accounts SET salt=?2,password_hash=?3,iterations=?4 WHERE person_id=?1");
    q.text(1, id);
    q.text(2, salt);
    q.text(3, hash);
    q.number(4, schoolcrypto::PasswordIterations);
    q.done();
    if (sqlite3_changes(db) == 0)
        throw std::invalid_argument("账号不存在");
    Query revoke(db, "DELETE FROM sessions WHERE person_id=?1");
    revoke.text(1, id);
    revoke.done();
    transaction.commit();
}
std::string Academy::createSession(const std::string &id, bool admin) {
    auto token = schoolcrypto::randomHex();
    auto hash = schoolcrypto::digest(token);
    std::lock_guard<std::mutex> guard(dao.dbMutex);
    auto *db = static_cast<sqlite3 *>(dao.db);
    Query cleanup(db, "DELETE FROM sessions WHERE expires<=?1");
    cleanup.number(1, now());
    cleanup.done();
    Query q(db, "INSERT INTO sessions(token_hash,person_id,is_admin,expires) VALUES(?1,?2,?3,?4)");
    q.text(1, hash);
    if (admin)
        q.null(2);
    else
        q.text(2, id);
    q.number(3, admin);
    q.number(4, now() + SessionSeconds);
    q.done();
    return token;
}
Identity Academy::session(const std::string &token) {
    if (token.size() != 64)
        return {};
    auto hash = schoolcrypto::digest(token);
    std::lock_guard<std::mutex> guard(dao.dbMutex);
    Query q(static_cast<sqlite3 *>(dao.db),
            "SELECT person_id,is_admin FROM sessions WHERE token_hash=?1 AND expires>?2");
    q.text(1, hash);
    q.number(2, now());
    if (!q.row())
        return {};
    return {true, q.integer(1) != 0, q.string(0)};
}
void Academy::logout(const std::string &token) {
    if (token.empty())
        return;
    auto hash = schoolcrypto::digest(token);
    std::lock_guard<std::mutex> guard(dao.dbMutex);
    Query q(static_cast<sqlite3 *>(dao.db), "DELETE FROM sessions WHERE token_hash=?1");
    q.text(1, hash);
    q.done();
}
json Academy::courses() {
    std::lock_guard<std::mutex> guard(dao.dbMutex);
    Query q(static_cast<sqlite3 *>(dao.db),
            "SELECT c.id,c.name,c.credit_hundredths,c.teacher_id,p.name FROM courses c LEFT JOIN "
            "people p ON p.id=c.teacher_id ORDER BY c.id");
    json result = json::array();
    while (q.row())
        result.push_back({{"id", q.string(0)},
                          {"name", q.string(1)},
                          {"credit", q.integer(2) / 100.0},
                          {"teacherId", q.string(3)},
                          {"teacherName", q.string(4)}});
    return result;
}
void Academy::saveCourse(const json &course, bool update) {
    auto id = field(course, "id"), name = field(course, "name"),
         teacherId = field(course, "teacherId", true);
    if (!validId(id) || (!teacherId.empty() && !validId(teacherId)))
        throw std::invalid_argument("课程编号或教师编号不合法");
    if (!course.contains("credit") || !course.at("credit").is_number())
        throw std::invalid_argument("学分必须是数字");
    double credit = course.at("credit").get<double>();
    if (!std::isfinite(credit) || credit <= 0 || credit > 30 ||
        std::abs(credit * 100 - std::round(credit * 100)) > 1e-7)
        throw std::invalid_argument("学分须大于 0、不超过 30，最多两位小数");
    std::lock_guard<std::mutex> guard(dao.dbMutex);
    auto *db = static_cast<sqlite3 *>(dao.db);
    if (!teacherId.empty()) {
        auto type = personType(db, teacherId);
        if (type != "Teacher" && type != "Graduate")
            throw std::invalid_argument("授课教师必须是教师或研究生");
    }
    Query q(db,
            update
                ? "UPDATE courses SET name=?2,credit_hundredths=?3,teacher_id=?4 WHERE id=?1"
                : "INSERT INTO courses(id,name,credit_hundredths,teacher_id) VALUES(?1,?2,?3,?4)");
    q.text(1, id);
    q.text(2, name);
    q.number(3, static_cast<int>(std::round(credit * 100)));
    if (teacherId.empty())
        q.null(4);
    else
        q.text(4, teacherId);
    q.done();
    if (update && sqlite3_changes(db) == 0)
        throw std::invalid_argument("课程不存在");
}
bool Academy::removeCourse(const std::string &id) {
    std::lock_guard<std::mutex> guard(dao.dbMutex);
    auto *db = static_cast<sqlite3 *>(dao.db);
    Query q(db, "DELETE FROM courses WHERE id=?1");
    q.text(1, id);
    q.done();
    return sqlite3_changes(db) > 0;
}
void Academy::saveGrade(const std::string &courseId, const json &grade) {
    auto id = field(grade, "studentId");
    if (!validId(id) || !validId(courseId) || !grade.contains("score") ||
        !grade.at("score").is_number())
        throw std::invalid_argument("学号或成绩格式不正确");
    double score = grade.at("score").get<double>();
    if (!std::isfinite(score) || score < 0 || score > 100 ||
        std::abs(score * 100 - std::round(score * 100)) > 1e-7)
        throw std::invalid_argument("成绩须为 0–100 分，最多两位小数");
    std::lock_guard<std::mutex> guard(dao.dbMutex);
    auto *db = static_cast<sqlite3 *>(dao.db);
    auto type = personType(db, id);
    if (type != "Student" && type != "Graduate")
        throw std::invalid_argument("只能为本科生或研究生录入成绩");
    Query q(
        db,
        "INSERT INTO grades(course_id,student_id,score_hundredths) VALUES(?1,?2,?3) ON "
        "CONFLICT(course_id,student_id) DO UPDATE SET score_hundredths=excluded.score_hundredths");
    q.text(1, courseId);
    q.text(2, id);
    q.number(3, static_cast<int>(std::round(score * 100)));
    q.done();
}
bool Academy::removeGrade(const std::string &courseId, const std::string &studentId) {
    std::lock_guard<std::mutex> guard(dao.dbMutex);
    auto *db = static_cast<sqlite3 *>(dao.db);
    Query q(db, "DELETE FROM grades WHERE course_id=?1 AND student_id=?2");
    q.text(1, courseId);
    q.text(2, studentId);
    q.done();
    return sqlite3_changes(db) > 0;
}
json Academy::ranking(const std::string &courseId) {
    std::lock_guard<std::mutex> guard(dao.dbMutex);
    Query q(static_cast<sqlite3 *>(dao.db),
            "SELECT g.student_id,p.name,g.score_hundredths FROM grades g JOIN people p ON "
            "p.id=g.student_id WHERE g.course_id=?1 AND p.type IN ('Student','Graduate')");
    q.text(1, courseId);
    std::vector<Grade> values;
    std::map<std::string, std::string> names;
    while (q.row()) {
        values.emplace_back(q.string(0), static_cast<int>(q.integer(2)));
        names[q.string(0)] = q.string(1);
    }
    std::sort(values.begin(), values.end());
    json result = json::array();
    int rank = 0, previous = -1;
    for (size_t i = 0; i < values.size(); ++i) {
        const auto &value = values[i];
        if (value.getHundredths() != previous) {
            rank = static_cast<int>(i) + 1;
            previous = value.getHundredths();
        }
        result.push_back({{"studentId", value.getStudentId()},
                          {"name", names[value.getStudentId()]},
                          {"score", value.getHundredths() / 100.0},
                          {"rank", rank}});
    }
    return result;
}
json Academy::studentCourses(const std::string &studentId) {
    auto all = courses();
    json result = json::array();
    for (const auto &course : all) {
        auto grades = ranking(course.at("id"));
        json item = course;
        item["score"] = nullptr;
        item["rank"] = nullptr;
        item["participants"] = grades.size();
        for (const auto &grade : grades)
            if (grade.at("studentId") == studentId) {
                item["score"] = grade.at("score");
                item["rank"] = grade.at("rank");
                break;
            }
        result.push_back(item);
    }
    return result;
}
bool Academy::teaches(const std::string &courseId, const std::string &personId) {
    std::lock_guard<std::mutex> guard(dao.dbMutex);
    Query q(static_cast<sqlite3 *>(dao.db),
            "SELECT 1 FROM courses c JOIN people p ON p.id=c.teacher_id WHERE c.id=?1 AND "
            "c.teacher_id=?2 AND p.type IN ('Teacher','Graduate')");
    q.text(1, courseId);
    q.text(2, personId);
    return q.row();
}
json Academy::exportFile(const std::string &filename) {
    std::lock_guard<std::mutex> exportGuard(exportMutex);
    json result = {{"format", "school-personnel-backup"}, {"version", 1}, {"exportedAt", now()}};
    {
        std::lock_guard<std::mutex> guard(dao.dbMutex);
        auto *db = static_cast<sqlite3 *>(dao.db);
        Transaction snapshot(db);
        for (const char *table : {"people", "accounts", "courses", "grades"}) {
            std::string sql = std::string("SELECT * FROM ") + table;
            Query q(db, sql.c_str());
            json rows = json::array();
            while (q.row()) {
                json row = json::object();
                for (int i = 0; i < sqlite3_column_count(q.stmt); ++i) {
                    std::string name = sqlite3_column_name(q.stmt, i);
                    switch (sqlite3_column_type(q.stmt, i)) {
                    case SQLITE_NULL:
                        row[name] = nullptr;
                        break;
                    case SQLITE_INTEGER:
                        row[name] = q.integer(i);
                        break;
                    case SQLITE_FLOAT:
                        row[name] = sqlite3_column_double(q.stmt, i);
                        break;
                    default:
                        row[name] = q.string(i);
                    }
                }
                rows.push_back(row);
            }
            result[table] = rows;
        }
        snapshot.commit();
    }
    // 文件流直接保存所有业务信息；包含密码散列，不保存明文或会话令牌。
    auto temporary = filename + ".tmp";
    {
        std::ofstream stream(temporary, std::ios::binary);
        if (!stream)
            throw std::runtime_error("无法创建导出文件");
        stream << result.dump(2);
        stream.close();
        if (!stream)
            throw std::runtime_error("导出文件写入失败");
    }
    std::filesystem::permissions(temporary, std::filesystem::perms::owner_read |
                                                std::filesystem::perms::owner_write);
#ifdef _WIN32
    std::filesystem::remove(filename);
#endif
    std::filesystem::rename(temporary, filename);
    // 回读校验避免把失败的写入报告为成功。
    std::ifstream input(filename, std::ios::binary);
    json verified;
    input >> verified;
    if (verified != result)
        throw std::runtime_error("导出文件校验失败");
    return verified;
}
