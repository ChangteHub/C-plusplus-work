#include "routes.h"
#include "crypto.h"
#include "person_codec.h"
#include "service.h"
#include "student.h"
#include <chrono>
#include <map>
#include <memory>
using nlohmann::json;
namespace {
void reply(httplib::Response &res, int status, const json &value) {
    res.status = status;
    res.set_content(value.dump(), "application/json; charset=utf-8");
}
template <class Action> void action(httplib::Response &res, Action fn) {
    try {
        fn();
    } catch (const DaoDuplicateError &) {
        reply(res, 409, {{"error", "编号或账号已存在，请更换编号；已有档案请由管理员开通账号"}});
    } catch (const std::invalid_argument &e) {
        reply(res, 400, {{"error", e.what()}});
    } catch (const json::exception &) {
        reply(res, 400, {{"error", "字段格式不正确"}});
    } catch (const std::exception &) {
        reply(res, 503, {{"error", "数据暂时不可用，请稍后重试"}});
    }
}
json body(const httplib::Request &req) {
    try {
        auto value = json::parse(req.body);
        if (!value.is_object())
            throw std::invalid_argument("请求体必须为 JSON 对象");
        return value;
    } catch (const json::exception &) {
        throw std::invalid_argument("JSON 格式不正确");
    }
}
std::string token(const httplib::Request &req) {
    std::string cookies = req.get_header_value("Cookie");
    size_t offset = 0;
    while (offset < cookies.size()) {
        auto end = cookies.find(';', offset);
        auto part = cookies.substr(offset, end == std::string::npos ? end : end - offset);
        auto first = part.find_first_not_of(' ');
        if (first != std::string::npos && part.compare(first, 15, "school_session=") == 0)
            return part.substr(first + 15);
        if (end == std::string::npos)
            break;
        offset = end + 1;
    }
    return "";
}
bool secure(const httplib::Request &req) {
    return (req.remote_addr == "127.0.0.1" || req.remote_addr == "::1") &&
           req.get_header_value("X-Forwarded-Proto") == "https";
}
void cookie(httplib::Response &res, const httplib::Request &req, const std::string &value) {
    res.set_header("Set-Cookie",
                   "school_session=" + value + "; Path=/; HttpOnly; SameSite=Strict; Max-Age=" +
                       (value.empty() ? "0" : std::to_string(Academy::SessionSeconds)) +
                       (secure(req) ? "; Secure" : ""));
}
} // namespace
void registerAcademyRoutes(httplib::Server &server, Dao &dao, Academy &academy,
                           const std::string &username, const std::string &password,
                           const std::string &dbPath) {
    auto basic = username.empty()
                     ? ""
                     : httplib::make_basic_authentication_header(username, password).second;
    auto identify = [&academy, basic, username](const httplib::Request &req) {
        auto current = academy.session(token(req));
        if (current.authenticated) {
            if (current.admin)
                current.id = username;
            return current;
        }
        if (!basic.empty() && schoolcrypto::equal(req.get_header_value("Authorization"), basic))
            return Identity{true, true, username};
        // 兼容原有本机管理模式；公网监听仍要求配置管理员凭据。
        if (username.empty())
            return Identity{true, true, "local-admin"};
        return Identity{};
    };
    server.set_pre_routing_handler([identify](const httplib::Request &req, httplib::Response &res) {
        if (req.path.compare(0, 5, "/api/") != 0)
            return httplib::Server::HandlerResponse::Unhandled;
        res.set_header("Cache-Control", "no-store");
        if (req.method == "POST" || req.method == "PUT" || req.method == "DELETE") {
            auto origin = req.get_header_value("Origin");
            auto expected =
                std::string(secure(req) ? "https://" : "http://") + req.get_header_value("Host");
            if (!origin.empty() && origin != expected) {
                reply(res, 403, {{"error", "请求来源不受信任"}});
                return httplib::Server::HandlerResponse::Handled;
            }
            if (req.method != "DELETE") {
                if (req.body.size() > 65536) {
                    reply(res, 413, {{"error", "请求信息过长"}});
                    return httplib::Server::HandlerResponse::Handled;
                }
                if (req.get_header_value("Content-Type").rfind("application/json", 0) != 0) {
                    reply(res, 415, {{"error", "请使用 application/json 请求体"}});
                    return httplib::Server::HandlerResponse::Handled;
                }
            }
        }
        if (req.path == "/api/health" || req.path == "/api/auth/login" ||
            req.path == "/api/auth/register" || req.path == "/api/auth/logout")
            return httplib::Server::HandlerResponse::Unhandled;
        try {
            auto current = identify(req);
            if (!current.authenticated) {
                reply(res, 401, {{"error", "请先登录"}});
                return httplib::Server::HandlerResponse::Handled;
            }
            bool adminOnly = req.path.rfind("/api/people", 0) == 0 ||
                             req.path.rfind("/api/accounts", 0) == 0 ||
                             req.path.rfind("/api/storage", 0) == 0 ||
                             (req.path.rfind("/api/courses", 0) == 0 &&
                              req.path.find("/grades") == std::string::npos &&
                              req.method != "GET" && req.method != "HEAD");
            if (adminOnly && !current.admin) {
                reply(res, 403, {{"error", "此操作需要管理员权限"}});
                return httplib::Server::HandlerResponse::Handled;
            }
        } catch (const std::exception &) {
            reply(res, 503, {{"error", "认证服务暂时不可用"}});
            return httplib::Server::HandlerResponse::Handled;
        }
        return httplib::Server::HandlerResponse::Unhandled;
    });
    auto description = [&dao, username](Identity current) {
        json out = {{"authenticated", current.authenticated},
                    {"role", current.admin ? "admin" : "person"},
                    {"username", current.id},
                    {"developmentMode", username.empty()}};
        if (!current.admin) {
            std::unique_ptr<Person> person(dao.findById(current.id));
            if (!person)
                throw std::invalid_argument("人员档案不存在");
            out["person"] = personJson(*person);
        }
        return out;
    };
    auto attempts = std::make_shared<std::map<std::string, std::pair<long long, int>>>();
    auto attemptMutex = std::make_shared<std::mutex>();
    server.Post(
        "/api/auth/login", [&academy, username, password, description, attempts,
                            attemptMutex](const httplib::Request &req, httplib::Response &res) {
            action(res, [&] {
                auto value = body(req);
                auto id = textField(value, "username"), supplied = textField(value, "password");
                auto time = std::chrono::duration_cast<std::chrono::seconds>(
                                std::chrono::system_clock::now().time_since_epoch())
                                .count();
                auto key = req.remote_addr + ":" + id;
                {
                    std::lock_guard<std::mutex> guard(*attemptMutex);
                    if (attempts->size() > 10000)
                        attempts->clear();
                    auto &entry = (*attempts)[key];
                    if (time - entry.first > 60)
                        entry = {time, 0};
                    if (++entry.second > 10) {
                        reply(res, 429, {{"error", "登录尝试过多，请一分钟后重试"}});
                        return;
                    }
                }
                bool admin =
                    !username.empty() && id == username && schoolcrypto::equal(supplied, password);
                if (!admin && !academy.verify(id, supplied)) {
                    reply(res, 401, {{"error", "账号或密码不正确"}});
                    return;
                }
                {
                    std::lock_guard<std::mutex> guard(*attemptMutex);
                    attempts->erase(key);
                }
                academy.logout(token(req));
                auto session = academy.createSession(id, admin);
                cookie(res, req, session);
                reply(res, 200, description({true, admin, id}));
            });
        });
    server.Post("/api/auth/register", [&academy, username, description](const httplib::Request &req,
                                                                        httplib::Response &res) {
        action(res, [&] {
            auto value = body(req);
            if (!value.contains("person") || !value.at("person").is_object())
                throw std::invalid_argument("请填写人员信息");
            auto raw = parsePerson(value.at("person").dump());
            auto person = Service::normalize(*raw);
            if (!person)
                throw std::invalid_argument("人员信息不合法");
            if (!username.empty() && person->getId() == username)
                throw std::invalid_argument("此编号保留给系统管理员");
            academy.registerPerson(*person, textField(value, "password"));
            academy.logout(token(req));
            auto session = academy.createSession(person->getId());
            cookie(res, req, session);
            reply(res, 201, description({true, false, person->getId()}));
        });
    });
    server.Post("/api/auth/logout",
                [&academy](const httplib::Request &req, httplib::Response &res) {
                    action(res, [&] {
                        academy.logout(token(req));
                        cookie(res, req, "");
                        reply(res, 200, {{"ok", true}});
                    });
                });
    server.Get("/api/me",
               [identify, description](const httplib::Request &req, httplib::Response &res) {
                   action(res, [&] { reply(res, 200, description(identify(req))); });
               });
    server.Post("/api/auth/password",
                [identify, &academy](const httplib::Request &req, httplib::Response &res) {
                    action(res, [&] {
                        auto current = identify(req);
                        if (current.admin) {
                            reply(res, 403, {{"error", "系统管理员密码由服务器配置管理"}});
                            return;
                        }
                        auto value = body(req);
                        academy.changePassword(current.id, textField(value, "oldPassword"),
                                               textField(value, "newPassword"));
                        cookie(res, req, "");
                        reply(res, 200, {{"ok", true}});
                    });
                });
    server.Post("/api/auth/cancel",
                [identify, &academy](const httplib::Request &req, httplib::Response &res) {
                    action(res, [&] {
                        auto current = identify(req);
                        if (current.admin) {
                            reply(res, 403, {{"error", "系统管理员不能在个人端销户"}});
                            return;
                        }
                        auto value = body(req);
                        if (!academy.cancel(current.id, textField(value, "password"))) {
                            reply(res, 401, {{"error", "密码不正确，未执行销户"}});
                            return;
                        }
                        cookie(res, req, "");
                        reply(res, 200, {{"ok", true}});
                    });
                });
    server.Get("/api/accounts", [&academy](const httplib::Request &, httplib::Response &res) {
        action(res, [&] { reply(res, 200, academy.accounts()); });
    });
    server.Post("/api/accounts",
                [&academy, username](const httplib::Request &req, httplib::Response &res) {
                    action(res, [&] {
                        auto value = body(req);
                        auto id = textField(value, "personId");
                        if (id == username)
                            throw std::invalid_argument("此编号保留给系统管理员");
                        academy.createAccount(id, textField(value, "password"));
                        reply(res, 201, {{"ok", true}});
                    });
                });
    server.Delete(R"(/api/accounts/([A-Za-z0-9_-]{1,32}))",
                  [&academy](const httplib::Request &req, httplib::Response &res) {
                      action(res, [&] {
                          if (academy.removeAccount(req.matches[1]))
                              reply(res, 200, {{"ok", true}});
                          else
                              reply(res, 404, {{"error", "账号不存在"}});
                      });
                  });
    server.Get("/api/courses", [&academy](const httplib::Request &, httplib::Response &res) {
        action(res, [&] { reply(res, 200, academy.courses()); });
    });
    server.Post("/api/courses", [&academy](const httplib::Request &req, httplib::Response &res) {
        action(res, [&] {
            academy.saveCourse(body(req));
            reply(res, 201, {{"ok", true}});
        });
    });
    server.Put(R"(/api/courses/([A-Za-z0-9_-]{1,32}))",
               [&academy](const httplib::Request &req, httplib::Response &res) {
                   action(res, [&] {
                       auto value = body(req);
                       value["id"] = req.matches[1].str();
                       academy.saveCourse(value, true);
                       reply(res, 200, {{"ok", true}});
                   });
               });
    server.Delete(R"(/api/courses/([A-Za-z0-9_-]{1,32}))",
                  [&academy](const httplib::Request &req, httplib::Response &res) {
                      action(res, [&] {
                          if (academy.removeCourse(req.matches[1]))
                              reply(res, 200, {{"ok", true}});
                          else
                              reply(res, 404, {{"error", "课程不存在"}});
                      });
                  });
    auto allowGrades = [identify, &academy](const httplib::Request &req, httplib::Response &res) {
        auto current = identify(req);
        if (!current.admin && !academy.teaches(req.matches[1], current.id)) {
            reply(res, 403, {{"error", "仅管理员或该课程的授课人员可维护成绩"}});
            return false;
        }
        return true;
    };
    server.Get(R"(/api/courses/([A-Za-z0-9_-]{1,32})/grades)",
               [&academy, allowGrades](const httplib::Request &req, httplib::Response &res) {
                   action(res, [&] {
                       if (allowGrades(req, res))
                           reply(res, 200, academy.ranking(req.matches[1]));
                   });
               });
    server.Post(R"(/api/courses/([A-Za-z0-9_-]{1,32})/grades)",
                [&academy, allowGrades](const httplib::Request &req, httplib::Response &res) {
                    action(res, [&] {
                        if (!allowGrades(req, res))
                            return;
                        academy.saveGrade(req.matches[1], body(req));
                        reply(res, 200, {{"ok", true}});
                    });
                });
    server.Delete(R"(/api/courses/([A-Za-z0-9_-]{1,32})/grades/([A-Za-z0-9_-]{1,32}))",
                  [&academy, allowGrades](const httplib::Request &req, httplib::Response &res) {
                      action(res, [&] {
                          if (!allowGrades(req, res))
                              return;
                          if (academy.removeGrade(req.matches[1], req.matches[2]))
                              reply(res, 200, {{"ok", true}});
                          else
                              reply(res, 404, {{"error", "成绩记录不存在"}});
                      });
                  });
    server.Get("/api/my/courses",
               [identify, &academy, &dao](const httplib::Request &req, httplib::Response &res) {
                   action(res, [&] {
                       auto current = identify(req);
                       if (current.admin) {
                           reply(res, 200, academy.courses());
                           return;
                       }
                       std::unique_ptr<Person> person(dao.findById(current.id));
                       if (!person)
                           throw std::invalid_argument("人员档案不存在");
                       if (dynamic_cast<Student *>(person.get()))
                           reply(res, 200, academy.studentCourses(current.id));
                       else {
                           auto list = academy.courses();
                           json selected = json::array();
                           for (const auto &course : list)
                               if (course.at("teacherId") == current.id)
                                   selected.push_back(course);
                           reply(res, 200, selected);
                       }
                   });
               });
    server.Get("/api/storage/export",
               [&academy, dbPath](const httplib::Request &, httplib::Response &res) {
                   action(res, [&] {
                       auto value = academy.exportFile(dbPath + ".export.json");
                       res.set_header("Content-Disposition",
                                      "attachment; filename=school-personnel-backup.json");
                       reply(res, 200, value);
                   });
               });
}
