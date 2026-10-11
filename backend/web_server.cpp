// Controller: HTTP / JSON. Business rules are in Service; SQL is in Dao.
#include "httplib.h"
#include "json.hpp"
#include "dao.h"
#include "service.h"
#include "teacher.h"
#include "adminstaff.h"
#include "student.h"
#include "worker.h"
#include "person_codec.h"
#include "routes.h"
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
// Owns every returned pointer even when serialization throws.
struct OwnedPeople {
    std::vector<Person*> people;
    explicit OwnedPeople(std::vector<Person*> value) : people(std::move(value)) {}
    ~OwnedPeople() { for (auto* person : people) delete person; }
    OwnedPeople(const OwnedPeople&) = delete;
    OwnedPeople& operator=(const OwnedPeople&) = delete;
};
static void sendServiceResult(httplib::Response& res, ServiceResult result, int success = 200) {
    switch (result) {
        case SVC_OK: sendJson(res, success, {{"ok", true}}); break;
        case SVC_DUPLICATE: sendJson(res, 409, {{"error", "工号或学号已存在"}}); break;
        case SVC_NOT_FOUND: sendJson(res, 404, {{"error", "未找到该人员"}}); break;
        case SVC_BAD_REQUEST: sendJson(res, 400, {{"error", "人员信息不合法，请检查编号、年龄、电话和类别对应的必填项"}}); break;
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
        Academy academy(dao);
        registerAcademyRoutes(server, dao, academy, username, password, dbPath);
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
        server.Get("/api/health", [&service,&academy](const httplib::Request&, httplib::Response& res) {
            try {
                academy.checkStorage();
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
