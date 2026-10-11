#ifndef SCHOOL_ROUTES_H
#define SCHOOL_ROUTES_H
// 先读取平台网络声明，再读取使用人员类的业务头文件。
#include "httplib.h"
#include "academy.h"
void registerAcademyRoutes(httplib::Server &server, Dao &dao, Academy &academy,
                           const std::string &username, const std::string &password,
                           const std::string &dbPath);
#endif
