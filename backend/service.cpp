#include "service.h"
#include "teacher.h"
#include "adminstaff.h"
#include <cmath>
#include <iostream>
#include <memory>

using namespace std;

namespace {

bool validId(const string& id) {
    if (id.empty() || id.size() > 32) return false;
    for (unsigned char c : id)
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-')) return false;
    return true;
}

bool whitespace(unsigned int cp) {
    return cp == ' ' || (cp >= 9 && cp <= 13) || cp == 0x85 || cp == 0xA0 ||
           cp == 0x1680 || (cp >= 0x2000 && cp <= 0x200A) || cp == 0x2028 ||
           cp == 0x2029 || cp == 0x202F || cp == 0x205F || cp == 0x3000;
}

// 检查严格 UTF-8、去除首尾空白，按字符数限制长度（中文不会按字节截断）。
bool normalizedText(const string& input, size_t maximum, bool required, string& output) {
    if (input.size() > maximum * 4 + 64) return false;
    size_t first = string::npos;
    size_t last = 0;
    size_t count = 0;
    size_t contentCount = 0;
    bool pendingControlWhitespace = false;
    for (size_t i = 0; i < input.size();) {
        size_t start = i;
        unsigned char lead = static_cast<unsigned char>(input[i++]);
        unsigned int cp;
        size_t continuation;
        unsigned int minimum;
        if (lead < 0x80) { cp = lead; continuation = 0; minimum = 0; }
        else if (lead >= 0xC2 && lead <= 0xDF) { cp = lead & 0x1F; continuation = 1; minimum = 0x80; }
        else if (lead >= 0xE0 && lead <= 0xEF) { cp = lead & 0x0F; continuation = 2; minimum = 0x800; }
        else if (lead >= 0xF0 && lead <= 0xF4) { cp = lead & 0x07; continuation = 3; minimum = 0x10000; }
        else return false;
        if (continuation > input.size() - i) return false;
        while (continuation--) {
            unsigned char next = static_cast<unsigned char>(input[i++]);
            if ((next & 0xC0) != 0x80) return false;
            cp = (cp << 6) | (next & 0x3F);
        }
        if (cp < minimum || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return false;
        ++count;
        if (!whitespace(cp)) {
            if (cp < 0x20 || (cp >= 0x7F && cp <= 0x9F)) return false;
            if (pendingControlWhitespace) return false;
            if (first == string::npos) { first = start; contentCount = count - 1; }
            last = i;
            // 包括中间空白；首尾空白不计入最终长度。
            if (count - contentCount > maximum) return false;
        } else if (first != string::npos && cp != ' ' &&
                   (cp < 0x20 || (cp >= 0x7F && cp <= 0x9F) ||
                    cp == 0x2028 || cp == 0x2029)) {
            // 尾部控制空白会被 trim；若后面还有正文则属于非法内部控制字符。
            pendingControlWhitespace = true;
        }
    }
    output = first == string::npos ? string() : input.substr(first, last - first);
    return !required || !output.empty();
}

bool validPhone(const string& phone) {
    if (phone.empty()) return true;
    if (phone.size() > 32) return false;
    int digits = 0;
    int parentheses = 0;
    for (size_t i = 0; i < phone.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(phone[i]);
        if (c >= '0' && c <= '9') ++digits;
        else if (c == '+') { if (i != 0) return false; }
        else if (c == '(') { if (parentheses != 0) return false; ++parentheses; }
        else if (c == ')') { if (parentheses != 1) return false; --parentheses; }
        else if (c != ' ' && c != '-' && c != '.') return false;
    }
    return digits >= 6 && digits <= 20 && parentheses == 0;
}

unique_ptr<Person> validatedClone(const Person& proto) {
    if (!validId(proto.getId()) || proto.getAge() < 18 || proto.getAge() > 100 ||
        !isfinite(proto.getSalary()) || proto.getSalary() < 0 || proto.getSalary() > 100000000)
        return unique_ptr<Person>();
    string name, gender, phone, department, role;
    if (!normalizedText(proto.getName(), 64, true, name) ||
        !normalizedText(proto.getGender(), 2, true, gender) ||
        (gender != "男" && gender != "女" && gender != "其他") ||
        !normalizedText(proto.getPhone(), 32, false, phone) || !validPhone(phone))
        return unique_ptr<Person>();
    const Teacher* teacher = dynamic_cast<const Teacher*>(&proto);
    const AdminStaff* admin = dynamic_cast<const AdminStaff*>(&proto);
    if (teacher && teacher->getType() == "教师") {
        if (!normalizedText(teacher->getDepartment(), 128, true, department) ||
            !normalizedText(teacher->getTitle(), 128, true, role)) return unique_ptr<Person>();
        return unique_ptr<Person>(new Teacher(proto.getId(), name, gender, proto.getAge(),
                                             phone, department, role, proto.getSalary()));
    }
    if (admin && admin->getType() == "行政人员") {
        if (!normalizedText(admin->getDepartment(), 128, true, department) ||
            !normalizedText(admin->getPosition(), 128, true, role)) return unique_ptr<Person>();
        return unique_ptr<Person>(new AdminStaff(proto.getId(), name, gender, proto.getAge(),
                                                phone, department, role, proto.getSalary()));
    }
    return unique_ptr<Person>();
}

ServiceResult storageFailure(const runtime_error& error) {
    cerr << "[Service] " << error.what() << endl;
    return SVC_STORAGE_ERROR;
}

} // namespace

vector<Person*> Service::list() { return dao.all(); }

ServiceResult Service::create(const Person& proto) {
    unique_ptr<Person> person = validatedClone(proto);
    if (!person) return SVC_BAD_REQUEST;
    try {
        return dao.insert(person.get()) ? SVC_OK : SVC_STORAGE_ERROR;
    } catch (const DaoDuplicateError&) {
        return SVC_DUPLICATE;
    } catch (const runtime_error& error) {
        return storageFailure(error);
    }
}

ServiceResult Service::update(const string& originalId, const Person& proto) {
    if (!validId(originalId)) return SVC_BAD_REQUEST;
    unique_ptr<Person> person = validatedClone(proto);
    if (!person) return SVC_BAD_REQUEST;
    try {
        return dao.update(originalId, person.get()) ? SVC_OK : SVC_NOT_FOUND;
    } catch (const DaoDuplicateError&) {
        return SVC_DUPLICATE;
    } catch (const runtime_error& error) {
        return storageFailure(error);
    }
}

ServiceResult Service::remove(const string& id) {
    if (!validId(id)) return SVC_BAD_REQUEST;
    try {
        return dao.remove(id) ? SVC_OK : SVC_NOT_FOUND;
    } catch (const runtime_error& error) {
        return storageFailure(error);
    }
}
