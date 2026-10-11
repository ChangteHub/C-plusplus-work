#ifndef SCHOOL_ACADEMY_H
#define SCHOOL_ACADEMY_H
#include "dao.h"
#include "json.hpp"
struct Identity {
    bool authenticated = false;
    bool admin = false;
    std::string id;
};
class Academy {
    Dao &dao;
    std::mutex exportMutex;

  public:
    static constexpr int SessionSeconds = 8 * 60 * 60;
    explicit Academy(Dao &database);
    void checkStorage();
    void registerPerson(Person &person, const std::string &password);
    void createAccount(const std::string &id, const std::string &password);
    bool verify(const std::string &id, const std::string &password);
    nlohmann::json accounts();
    bool removeAccount(const std::string &id);
    bool cancel(const std::string &id, const std::string &password);
    void changePassword(const std::string &id, const std::string &oldPassword,
                        const std::string &newPassword);
    std::string createSession(const std::string &id, bool admin = false);
    Identity session(const std::string &token);
    void logout(const std::string &token);
    nlohmann::json courses();
    void saveCourse(const nlohmann::json &course, bool update = false);
    bool removeCourse(const std::string &id);
    void saveGrade(const std::string &courseId, const nlohmann::json &grade);
    bool removeGrade(const std::string &courseId, const std::string &studentId);
    nlohmann::json ranking(const std::string &courseId);
    nlohmann::json studentCourses(const std::string &studentId);
    bool teaches(const std::string &courseId, const std::string &personId);
    nlohmann::json exportFile(const std::string &filename);
};
#endif
