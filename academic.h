#ifndef SCHOOL_ACADEMIC_H
#define SCHOOL_ACADEMIC_H
#include <string>
#include <utility>
// 成绩用百分之一分的整数存储，避免浮点比较影响并列名次。
class Grade {
    std::string studentId;
    int hundredths;

  public:
    Grade(std::string id, int score) : studentId(std::move(id)), hundredths(score) {}
    const std::string &getStudentId() const { return studentId; }
    int getHundredths() const { return hundredths; }
    bool operator<(const Grade &other) const {
        return hundredths != other.hundredths ? hundredths > other.hundredths
                                              : studentId < other.studentId;
    }
};
#endif
