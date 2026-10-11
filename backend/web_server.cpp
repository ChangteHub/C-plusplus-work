// Controller: HTTP / JSON. Business rules are in Service; SQL is in Dao.
#include "httplib.h"
#include "json.hpp"
#include "dao.h"
#include "service.h"
#include "teacher.h"
#include "adminstaff.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using nlohmann::json;

static std::string envOr(const char* key, const char* fallback) {
    const char* value = std::getenv(key);
    return value && *value ? value : fallback;
}
static void sendJson(httplib::Response& res, int status, const json& body) {
    res.status = status;
    res.set_content(body.dump(), "application/json; charset=utf-8");
}
static json personJson(const Person& person) {
    json out = {{"id", person.getId()}, {"name", person.getName()},
                {"gender", person.getGender()}, {"age", person.getAge()},
                {"phone", person.getPhone()}, {"salary", person.getSalary()}};
    if (const auto* teacher = dynamic_cast<const Teacher*>(&person)) {
        out["type"] = "teacher";
        out["department"] = teacher->getDepartment();
        out["title"] = teacher->getTitle();
    } else if (const auto* admin = dynamic_cast<const AdminStaff*>(&person)) {
        out["type"] = "admin";
        out["department"] = admin->getDepartment();
        out["position"] = admin->getPosition();
    } else throw std::runtime_error("Unsupported personnel type in storage");
    return out;
}
// Owns every returned pointer even when serialization throws.
struct OwnedPeople {
    std::vector<Person*> people;
    explicit OwnedPeople(std::vector<Person*> value) : people(std::move(value)) {}
    ~OwnedPeople() { for (auto* person : people) delete person; }
    OwnedPeople(const OwnedPeople&) = delete;
    OwnedPeople& operator=(const OwnedPeople&) = delete;
};
static std::string textField(const json& body, const char* key, bool optional = false) {
    if (optional && !body.contains(key)) return "";
    if (!body.contains(key) || !body.at(key).is_string())
        throw std::invalid_argument(std::string("字段必须是字符串：") + key);
    return body.at(key).get<std::string>();
}
static std::unique_ptr<Person> parsePerson(const std::string& raw) {
    json body;
    try { body = json::parse(raw); }
    catch (const json::exception&) { throw std::invalid_argument("JSON 格式或字段类型不正确"); }
    if (!body.is_object()) throw std::invalid_argument("请求体必须是 JSON 对象");
    const auto type = textField(body, "type");
    if (type != "teacher" && type != "admin")
        throw std::invalid_argument("人员类别必须为 teacher 或 admin");
    if (!body.contains("age") || !body.at("age").is_number_integer() ||
        body.at("age") < 18 || body.at("age") > 100)
        throw std::invalid_argument("年龄必须是 18 到 100 的整数");
    if (!body.contains("salary") || !body.at("salary").is_number() ||
        body.at("salary") < 0 || body.at("salary") > 100000000)
        throw std::invalid_argument("工资必须是 0 到 100000000 的数字");
    const auto id = textField(body, "id");
    const auto name = textField(body, "name");
    const auto gender = textField(body, "gender");
    const auto phone = textField(body, "phone", true);
    const auto department = textField(body, "department");
    const int age = body.at("age").get<int>();
    const double salary = body.at("salary").get<double>();
    if (type == "teacher") return std::make_unique<Teacher>(id, name, gender, age,
        phone, department, textField(body, "title"), salary);
    return std::make_unique<AdminStaff>(id, name, gender, age,
        phone, department, textField(body, "position"), salary);
}
static void sendServiceResult(httplib::Response& res, ServiceResult result, int success = 200) {
    switch (result) {
        case SVC_OK: sendJson(res, success, {{"ok", true}}); break;
        case SVC_DUPLICATE: sendJson(res, 409, {{"error", "工号已存在"}}); break;
        case SVC_NOT_FOUND: sendJson(res, 404, {{"error", "未找到该人员"}}); break;
        case SVC_BAD_REQUEST: sendJson(res, 400, {{"error", "人员信息不合法，请检查必填项、工号、年龄、电话和工资"}}); break;
        case SVC_STORAGE_ERROR: sendJson(res, 503, {{"error", "数据库暂时不可用，请稍后重试"}}); break;
        default: sendJson(res, 500, {{"error", "服务器内部错误"}}); break;
    }
}
template <typename Action>
static void apiAction(httplib::Response& res, Action action) {
    try { action(); }
    catch (const json::exception& error) {
        std::cerr << "[Storage JSON] " << error.what() << '\n';
        sendJson(res, 503, {{"error", "数据库记录无法读取，请检查数据完整性"}});
    }
    catch (const std::invalid_argument& error) { sendJson(res, 400, {{"error", error.what()}}); }
    catch (const std::runtime_error& error) {
        std::cerr << "[Storage] " << error.what() << '\n';
        sendJson(res, 503, {{"error", "数据库暂时不可用，请稍后重试"}});
    }
}
static bool constantTimeEqual(const std::string& actual, const std::string& expected) {
    if (actual.size() != expected.size()) return false;
    unsigned char mismatch = 0;
    for (size_t i = 0; i < actual.size(); ++i)
        mismatch |= static_cast<unsigned char>(actual[i] ^ expected[i]);
    return mismatch == 0;
}
int main() {
    try {
        const auto host = envOr("SCHOOL_HOST", "127.0.0.1");
        const auto portText = envOr("SCHOOL_PORT", "8080");
        size_t consumed = 0;
        const int port = std::stoi(portText, &consumed);
        if (consumed != portText.size() || port < 1 || port > 65535)
            throw std::invalid_argument("SCHOOL_PORT 必须是 1 到 65535 的整数");
        const auto dbPath = envOr("SCHOOL_DB_PATH", "school.db");
        const auto webRoot = envOr("SCHOOL_WEB_ROOT", "web");
        const auto username = envOr("SCHOOL_ADMIN_USER", "");
        const auto password = envOr("SCHOOL_ADMIN_PASSWORD", "");
        if (username.empty() != password.empty())
            throw std::invalid_argument("管理员用户名和密码必须同时配置");
        if (username.find(':') != std::string::npos)
            throw std::invalid_argument("管理员用户名不能包含冒号");
        if (host != "127.0.0.1" && host != "localhost" && host != "::1" && username.empty())
            throw std::invalid_argument("监听非本机地址时，请先配置管理员用户名和密码");
        std::ifstream index(webRoot + "/index.html");
        if (!index) throw std::invalid_argument("找不到前端 index.html，请从项目目录启动或配置 SCHOOL_WEB_ROOT");
        Dao dao;
        if (!dao.open(dbPath)) throw std::runtime_error("数据库初始化失败");
        Service service(dao);
        httplib::Server server;
        server.new_task_queue = [] { return new httplib::ThreadPool(8); };
        server.set_payload_max_length(64 * 1024);
        server.set_read_timeout(10);
        server.set_write_timeout(10);
        server.set_default_headers({{"Cache-Control", "no-cache"}, {"X-Content-Type-Options", "nosniff"},
            {"X-Frame-Options", "DENY"}, {"Referrer-Policy", "same-origin"}});
        const auto authHeader = username.empty() ? "" :
            httplib::make_basic_authentication_header(username, password).second;
        server.set_pre_routing_handler([authHeader](const httplib::Request& req, httplib::Response& res) {
            if (req.path.compare(0, 5, "/api/") == 0) res.set_header("Cache-Control", "no-store");
            if (!authHeader.empty() && !(req.path == "/api/health" && (req.method == "GET" || req.method == "HEAD")) &&
                !constantTimeEqual(req.get_header_value("Authorization"), authHeader)) {
                res.set_header("WWW-Authenticate", "Basic realm=\"School Personnel\", charset=\"UTF-8\"");
                sendJson(res, 401, {{"error", "请使用管理员账号登录"}});
                return httplib::Server::HandlerResponse::Handled;
            }
            if (req.path.compare(0, 11, "/api/people") == 0 && (req.method == "POST" || req.method == "PUT")) {
                const auto contentType = req.get_header_value("Content-Type");
                if (contentType.substr(0, contentType.find(';')) != "application/json") {
                    sendJson(res, 415, {{"error", "请使用 application/json 请求体"}});
                    return httplib::Server::HandlerResponse::Handled;
                }
            }
            return httplib::Server::HandlerResponse::Unhandled;
        });
        server.set_exception_handler([](const httplib::Request&, httplib::Response& res, std::exception_ptr error) {
            try { if (error) std::rethrow_exception(error); }
            catch (const std::exception& exception) { std::cerr << "[HTTP] " << exception.what() << '\n'; }
            catch (...) { std::cerr << "[HTTP] Unexpected exception\n"; }
            sendJson(res, 500, {{"error", "服务器内部错误"}});
        });
        server.set_error_handler([](const httplib::Request& req, httplib::Response& res) {
            if (res.body.empty() && req.path.compare(0, 5, "/api/") == 0)
                sendJson(res, res.status, {{"error", res.status == 404 ? "接口不存在" : "请求无法处理"}});
        });
        server.Get("/api/health", [&service](const httplib::Request&, httplib::Response& res) {
            try {
                OwnedPeople people(service.list());
                for (const auto* person : people.people) personJson(*person).dump();
                sendJson(res, 200, {{"status", "ok"}, {"service", "school-personnel"}, {"database", "ok"}});
            } catch (const std::exception& error) {
                std::cerr << "[Health] " << error.what() << '\n';
                sendJson(res, 503, {{"status", "error"}, {"service", "school-personnel"}, {"database", "unavailable"}});
            }
        });
        server.Get("/api/people", [&service](const httplib::Request&, httplib::Response& res) {
            apiAction(res, [&] {
                OwnedPeople people(service.list());
                json output = json::array();
                for (const auto* person : people.people) output.push_back(personJson(*person));
                sendJson(res, 200, output);
            });
        });
        server.Post("/api/people", [&service](const httplib::Request& req, httplib::Response& res) {
            apiAction(res, [&] { const auto person = parsePerson(req.body); sendServiceResult(res, service.create(*person), 201); });
        });
        server.Put(R"(/api/people/([A-Za-z0-9_-]{1,32}))", [&service](const httplib::Request& req, httplib::Response& res) {
            apiAction(res, [&] { const auto person = parsePerson(req.body); sendServiceResult(res, service.update(req.matches[1], *person)); });
        });
        server.Delete(R"(/api/people/([A-Za-z0-9_-]{1,32}))", [&service](const httplib::Request& req, httplib::Response& res) {
            apiAction(res, [&] { sendServiceResult(res, service.remove(req.matches[1])); });
        });
        if (!server.set_mount_point("/", webRoot)) throw std::runtime_error("无法托管前端目录");
        std::cout << "Aurora 学校人员信息管理系统\n"
                  << "访问：http://" << (host == "::1" ? "[::1]" : host) << ':' << port << '\n'
                  << "数据库：" << dbPath << "\n前端目录：" << webRoot << '\n'
                  << "管理员认证：" << (username.empty() ? "本机模式" : "已启用") << "\n按 Ctrl+C 停止服务" << std::endl;
        if (!server.listen(host, port)) throw std::runtime_error("无法监听端口，可能已被占用");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "启动失败：" << error.what() << std::endl;
        return 1;
    }
}
