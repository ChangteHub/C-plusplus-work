#include "person_codec.h"
#include "adminstaff.h"
#include "graduate.h"
#include "student.h"
#include "teacher.h"
#include "worker.h"
#include <stdexcept>
using nlohmann::json;
json personJson(const Person &person) {
    json out = {{"id", person.getId()},         {"name", person.getName()},
                {"gender", person.getGender()}, {"age", person.getAge()},
                {"phone", person.getPhone()},   {"salary", person.getSalary()}};
    if (const auto *graduate = dynamic_cast<const GraduateStudent *>(&person)) {
        out["type"] = "graduate";
        out["department"] = graduate->getDepartment();
        out["major"] = graduate->getMajor();
        out["className"] = graduate->getClassName();
        out["admissionYear"] = graduate->getAdmissionYear();
        out["researchDirection"] = graduate->getResearchDirection();
        out["supervisor"] = graduate->getSupervisor();
        out.erase("salary");
    } else if (const auto *teacher = dynamic_cast<const Teacher *>(&person)) {
        out["type"] = "teacher";
        out["department"] = teacher->getDepartment();
        out["title"] = teacher->getTitle();
    } else if (const auto *admin = dynamic_cast<const AdminStaff *>(&person)) {
        out["type"] = "admin";
        out["department"] = admin->getDepartment();
        out["position"] = admin->getPosition();
    } else if (const auto *student = dynamic_cast<const Student *>(&person)) {
        out["type"] = "student";
        out["department"] = student->getDepartment();
        out["major"] = student->getMajor();
        out["className"] = student->getClassName();
        out["admissionYear"] = student->getAdmissionYear();
        out.erase("salary");
    } else if (const auto *worker = dynamic_cast<const Worker *>(&person)) {
        out["type"] = "worker";
        out["department"] = worker->getDepartment();
        out["job"] = worker->getJob();
    } else
        throw std::runtime_error("Unsupported personnel type in storage");
    return out;
}
std::string textField(const json &body, const char *key, bool optional) {
    if (optional && !body.contains(key))
        return "";
    if (!body.contains(key) || !body.at(key).is_string())
        throw std::invalid_argument(std::string("字段必须是字符串：") + key);
    return body.at(key).get<std::string>();
}
std::unique_ptr<Person> parsePerson(const std::string &raw) {
    json body;
    try {
        body = json::parse(raw);
    } catch (const json::exception &) {
        throw std::invalid_argument("JSON 格式或字段类型不正确");
    }
    if (!body.is_object())
        throw std::invalid_argument("请求体必须是 JSON 对象");
    const auto type = textField(body, "type");
    if (type != "teacher" && type != "admin" && type != "student" && type != "worker" &&
        type != "graduate")
        throw std::invalid_argument("人员类别必须为 teacher、admin、student、graduate 或 worker");
    const int minimumAge = type == "student" ? 6 : 18;
    if (!body.contains("age") || !body.at("age").is_number_integer() ||
        body.at("age") < minimumAge || body.at("age") > 100)
        throw std::invalid_argument("年龄必须是 " + std::to_string(minimumAge) + " 到 100 的整数");
    if (type != "student" && type != "graduate" &&
        (!body.contains("salary") || !body.at("salary").is_number() || body.at("salary") < 0 ||
         body.at("salary") > 100000000))
        throw std::invalid_argument("工资必须是 0 到 100000000 的数字");
    const auto id = textField(body, "id");
    const auto name = textField(body, "name");
    const auto gender = textField(body, "gender");
    const auto phone = textField(body, "phone", true);
    const auto department = textField(body, "department");
    const int age = body.at("age").get<int>();
    if (type == "student" || type == "graduate") {
        if (!body.contains("admissionYear") || !body.at("admissionYear").is_number_integer() ||
            body.at("admissionYear") < 1900 || body.at("admissionYear") > 2100)
            throw std::invalid_argument("入学年份必须是 1900 到 2100 的整数");
        if (body.contains("salary") && (!body.at("salary").is_number() || body.at("salary") != 0))
            throw std::invalid_argument("学生不设置工资，请移除 salary 字段");
        if (type == "graduate")
            return std::make_unique<GraduateStudent>(
                id, name, gender, age, phone, department, textField(body, "major"),
                textField(body, "className"), body.at("admissionYear").get<int>(),
                textField(body, "researchDirection"), textField(body, "supervisor"));
        return std::make_unique<Undergraduate>(
            id, name, gender, age, phone, department, textField(body, "major"),
            textField(body, "className"), body.at("admissionYear").get<int>());
    }
    const double salary = body.at("salary").get<double>();
    if (type == "teacher")
        return std::make_unique<Teacher>(id, name, gender, age, phone, department,
                                         textField(body, "title"), salary);
    if (type == "worker")
        return std::make_unique<Worker>(id, name, gender, age, phone, department,
                                        textField(body, "job"), salary);
    return std::make_unique<AdminStaff>(id, name, gender, age, phone, department,
                                        textField(body, "position"), salary);
}
