#include "academic.h"
#include "graduate.h"
#include <algorithm>
#include <cassert>
#include <sstream>
#include <vector>
int main() {
    const int before = Person::getLiveCount();
    {
        GraduateStudent graduate("G1", "研究生测试", "男", 24, "", "计算机学院", "软件工程", "研一",
                                 2026, "智能控制", "李老师");
        Teacher *teaching = &graduate;
        Student *studying = &graduate;
        assert(static_cast<Person *>(teaching) == static_cast<Person *>(studying));
        assert(teaching->getId() == "G1" && studying->getId() == "G1");
        assert(teaching->getType() == "研究生" && studying->getSalary() == 0);
        Undergraduate undergraduate("S1", "本科生测试", "女", 19, "", "计算机学院", "软件工程",
                                    "一班", 2026);
        assert(undergraduate.getId() == "S1" && undergraduate.getType() == "本科生");
        std::ostringstream text;
        text << graduate;
        assert(text.str().find("研究生 G1") != std::string::npos);
        assert(Person::getLiveCount() == before + 2);
        std::vector<Grade> grades = {Grade("S2", 8000), Grade("G1", 9050), Grade("S1", 9050)};
        std::sort(grades.begin(), grades.end());
        assert(grades[0].getStudentId() == "G1" && grades[2].getStudentId() == "S2");
    }
    assert(Person::getLiveCount() == before);
}
