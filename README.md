# 学校人员管理系统

一个以 C++ 为核心的学校人员管理作业项目。Web 界面由 C++ HTTP 服务托管，人员数据保存在 SQLite 数据库中；原有 C++ 控制台程序也保留。支持 Windows 本地开发，以及 Ubuntu 上通过 Nginx、HTTPS 和 systemd 部署。

## 服务器部署

Ubuntu 配置、HTTPS 证书续期、数据库备份和更新步骤见 [deploy/README.md](deploy/README.md)。服务器部署使用独立管理员登录；数据库和实际凭据只保存在服务器，不进入 Git 仓库。

当前运行地址：[https://swustmarket.xyz](https://swustmarket.xyz)。浏览器会弹出管理员登录框，凭据由管理员单独提供。2026 年 10 月 11 日已在 Ubuntu 24.04 上完成部署，验证页面、API、增删改查、重启后的数据库持久化和证书模拟续期。

## 本地启动

电脑需要安装支持 C++17 的 MinGW，并确保 `gcc`、`g++` 可以在终端中调用。项目已包含 HTTP、JSON 和 SQLite 源码，无需安装数据库服务或 Node.js。

在项目目录打开 PowerShell，运行：

```powershell
.\start_server.bat
```

脚本会编译 SQLite 与 C++ 后端，然后在前台启动服务。浏览器访问 **http://localhost:8080**。窗口需保持运行，按 `Ctrl+C` 停止服务。也可以双击 `start_server.bat` 启动；如果出现错误，建议在 PowerShell 中运行以保留编译输出。

只编译后端：

```powershell
.\build_server.bat
.\build\server.exe
```

可选 PowerShell 启动入口：

```powershell
.\start_server.ps1
```

如果本机的执行策略禁止 `.ps1`，使用 `.bat` 入口即可。所有脚本会自动切换到项目目录，编译产物统一写入 `build/`。启动时读取现有 `school.db`；数据库不存在时自动创建。

如果服务由助手在后台启动，直接打开网页即可。重新编译或改为前台启动前，先在项目目录运行 `./stop_server.ps1` 停止该后台进程。脚本会核对进程路径，只处理本项目的后台服务。前台启动的服务仍在对应终端按 `Ctrl+C` 停止。

**不要直接双击 `web/index.html` 使用系统。** 页面应通过 C++ 服务提供的 HTTP 地址访问，以便与后端和数据库连接。后端不可用时，页面会显示连接错误；不会将修改保存到浏览器 `localStorage` 或自动换用示例数据。

## 功能

- 管理教师和行政人员，支持新增、编辑、删除与查询。
- 记录工号、姓名、性别、年龄、联系电话、院系或部门、职称或职务、工资。
- 按人员类别筛选，搜索人员，查看人数和工资统计。
- 后端校验字段格式、工号唯一性和数值范围。
- SQLite 实际持久化：关闭服务后，已保存的数据仍保留在数据库中。
- 前端显示后端连接状态，操作失败时保留输入并提示错误。

## 架构与作业设计

```text
浏览器 web/index.html + styles.css + app.js
                    │ HTTP / JSON
                    ▼
C++ Controller：backend/web_server.cpp
                    │
                    ▼
C++ Service：backend/service.cpp（业务规则和字段校验）
                    │
                    ▼
C++ DAO：backend/dao.cpp（参数化 SQL 和数据映射）
                    │
                    ▼
SQLite：school.db / people 表
```

`Person` 是包含公共属性和虚函数的抽象基类；`Teacher`、`AdminStaff` 通过继承扩展各自字段，借助多态提供统一的显示、工资统计与保存接口。Web 后端分为 HTTP 接口、业务逻辑、数据库访问三层，使协议处理和业务规则分开。

SQLite 在 C++ 程序内部运行，因此数据库随后端一起启用。DAO 使用 SQL 参数绑定写入字段，工号为主键。Web 与控制台复用人员类，但**两者的数据各自独立**：Web 保存到 `school.db`，控制台保存到 `school_data.txt`，没有自动同步或导入功能。

## 项目文件

```text
project6/
├── person.h / person.cpp                 人员基类
├── teacher.h / teacher.cpp               教师类
├── adminstaff.h / adminstaff.cpp         行政人员类
├── main.cpp                             控制台入口
├── personmanager.h / personmanager.cpp   控制台增删改查、统计与文本存储
├── utils.h                              控制台输入工具
├── backend/
│   ├── web_server.cpp                    HTTP 路由、JSON 与静态页面服务
│   ├── service.h / service.cpp           业务校验
│   ├── dao.h / dao.cpp                   SQLite 数据访问
│   ├── httplib.h                        cpp-httplib
│   ├── json.hpp                         nlohmann/json
│   └── sqlite3.h / sqlite3.c             SQLite C 源码
├── web/
│   ├── index.html                       页面结构
│   ├── styles.css                       页面样式
│   └── app.js                           前端交互和 API 请求
├── CMakeLists.txt                        跨平台构建配置
├── deploy/                              Ubuntu 服务、Nginx 与运维配置
├── build.bat                            编译控制台程序
├── build_server.bat                     编译 Web 后端
├── start_server.bat                     编译并启动 Web 系统
├── start_server.ps1                     可选 PowerShell 启动入口
├── stop_server.ps1                      停止助手启动的本项目后台服务
├── tests/test_api.py                     标准库 HTTP / 数据库集成测试
├── build/                               新编译的可执行文件和中间产物
├── school.db                            Web 数据库（按需创建，原有数据保留）
├── school_data.txt                      控制台数据（保存后创建）
└── README.md
```

根目录原有 `server.exe`、`school_system.exe` 是旧版本产物；本次脚本运行的是 `build/` 中新编译的程序。提交作业源码时可不提交可执行文件、数据库和中间产物，`.gitignore` 已提供对应规则。

## 配置

后端通过环境变量读取配置。默认只监听本机，无需登录即可在本地开发使用。

| 环境变量 | 默认值 | 用途 |
| --- | --- | --- |
| `SCHOOL_HOST` | `127.0.0.1` | 监听地址 |
| `SCHOOL_PORT` | `8080` | HTTP 端口 |
| `SCHOOL_DB_PATH` | `school.db` | SQLite 文件路径 |
| `SCHOOL_WEB_ROOT` | `web` | 前端静态文件目录 |
| `SCHOOL_ADMIN_USER` | 未设置 | 可选 HTTP Basic 登录用户名 |
| `SCHOOL_ADMIN_PASSWORD` | 未设置 | 可选 HTTP Basic 登录密码 |

相对路径以服务的当前工作目录为准；启动脚本会确保工作目录为项目根目录。如果端口被占用，可以在同一个 PowerShell 窗口先设置端口：

```powershell
$env:SCHOOL_PORT = '8081'
.\start_server.bat
```

此时访问 `http://localhost:8081`。若要使用另一个数据库，设置 `SCHOOL_DB_PATH` 为文件路径，路径的父目录需已存在。

用户名和密码必须成对设置。启用后浏览器会弹出登录框；健康接口仍可用于检查服务状态。示例仅用于本地测试，请将示例密码换成自己的值：

```powershell
$env:SCHOOL_ADMIN_USER = 'admin'
$env:SCHOOL_ADMIN_PASSWORD = 'replace-this-example-password'
.\start_server.bat
```

服务拒绝在未配置登录凭据时监听非本机地址。公网部署时使用 Nginx 提供 HTTPS，C++ 后端仅监听本机，并设置独立的管理员凭据；具体配置见服务器部署文档。

## HTTP API

| 方法 | 路径 | 成功响应 | 用途 |
| --- | --- | --- | --- |
| `GET` | `/api/health` | `200`，健康状态对象 | 检查服务及数据库，公开接口 |
| `GET` | `/api/people` | `200`，人员 JSON 数组 | 查询全部人员 |
| `POST` | `/api/people` | `201`，`{"ok":true}` | 新增人员 |
| `PUT` | `/api/people/{工号}` | `200`，`{"ok":true}` | 修改对应人员，路径为原工号 |
| `DELETE` | `/api/people/{工号}` | `200`，`{"ok":true}` | 删除对应人员 |

健康响应：

```json
{"status":"ok","service":"school-personnel","database":"ok"}
```

数据库不可用时健康接口返回 `503`，`status` 为 `error`、`database` 为 `unavailable`。业务错误响应为 `{"error":"错误说明"}`；常见状态码为 `400`（JSON 或字段不合法）、`401`（未通过登录）、`404`（人员不存在）、`409`（工号重复）、`503`（数据库操作失败）、`500`（其他服务异常）。

新增或修改教师的请求体示例：

```json
{
  "type": "teacher",
  "id": "T001",
  "name": "张三",
  "gender": "男",
  "age": 35,
  "phone": "13800138000",
  "department": "计算机学院",
  "title": "教授",
  "salary": 12000
}
```

行政人员使用 `"type":"admin"`，以 `position` 表示职务。API 中类别使用小写 `teacher` / `admin`；SQLite 表内保存的类别标识为 `Teacher` / `Admin`。字符串字段需符合后端规则；`age` 必须为 `18` 至 `100` 的整数，`salary` 必须为 `0` 至 `100000000` 的数值，不能以 JSON 字符串代替数字。编辑时路径中的工号定位原记录，请求体中的 `id` 可以提供新工号。

在另一 PowerShell 窗口检查服务：

```powershell
Invoke-RestMethod 'http://localhost:8080/api/health'
Invoke-RestMethod 'http://localhost:8080/api/people'
```

启用 Basic 登录后可使用 `curl.exe` 检查受保护的 API（密码由终端提示输入）：

```powershell
curl.exe --user admin http://localhost:8080/api/people
```

## 控制台程序

编译并运行：

```powershell
.\build.bat
.\build\school_system.exe
```

按菜单进行新增、删除、修改、查询、排序、统计与保存。控制台数据文件为 `school_data.txt`，与 Web 数据库独立。

## CMake 构建

CMake 是可选构建方式；已有 MinGW 时，上面的 `.bat` 脚本即可用于 Windows 本地开发。CMake 配置包含 `server` 和 `school_system` 两个目标。

安装 CMake 和 C/C++ 编译器后，Windows MinGW 可运行：

```powershell
cmake -S . -B build-cmake -G 'MinGW Makefiles'
cmake --build build-cmake --config Release
.\build-cmake\server.exe
```

Ubuntu 上可使用同一份源码构建；公网运行还需按服务器部署文档配置服务与 HTTPS：

```bash
cmake -S . -B build-cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-cmake -j1
./build-cmake/server
```

运行时仍应保持当前工作目录为项目根目录，以正确读取 `web/` 和数据库路径。Windows 的批处理构建已提供直接使用 GCC/G++ 的入口；Ubuntu 24.04 已使用 GCC 13.3 和 CMake 3.28 编译并通过集成测试。

## 集成测试

测试脚本仅使用 Python 标准库，需要 Python 3.8 或更高版本。先编译后端，再从项目目录运行：

```powershell
.\build_server.bat
python tests/test_api.py
```

如果使用 CMake 或其他产物路径，可通过参数指定服务程序：

```powershell
python tests/test_api.py --server build-cmake/server.exe
```

Linux 上同样支持 `python3 tests/test_api.py --server build-cmake/server`。测试自行选取空闲本机端口，使用临时目录和临时数据库，不访问项目现有 `school.db`；退出时关闭自己启动的服务进程。Windows 测试进程不显示额外控制台窗口。

已在 Windows 和 Ubuntu 24.04 环境编译并通过 13 组后端集成测试，覆盖健康检查、静态页面、教师与行政人员增删改查、工号重复和修改、人员类别切换、字段与 JSON 校验、超过 64 KiB 的请求被拒绝、Unicode 转义姓名和工资小数保存、并发重复工号、进程重启后持久化、Basic 登录、非法配置与损坏数据库拒绝启动、外部数据库锁导致 `503` 后恢复，以及数据库表或记录编码损坏时健康接口和列表接口返回 `503`。服务器还通过了公网 HTTPS 登录、静态资源、实际增删改查、systemd 重启后持久化、SQLite 在线备份及证书模拟续期验证；部署临时测试记录已删除。

## 本地问题排查

- 提示找不到 `gcc` 或 `g++`：检查 MinGW 的 `bin` 目录是否在 `PATH`，重开终端后运行 `gcc --version` 和 `g++ --version`。
- 服务提示监听失败：检查是否有另一个服务使用相同端口，或更换 `SCHOOL_PORT`。
- 页面提示后端断开：确认启动窗口仍在运行，并通过 `/api/health` 查看健康状态。恢复服务后刷新页面。
- 数据库打开失败：检查 `SCHOOL_DB_PATH` 所指父目录存在、文件可读写，且没有指定为目录。
- 运行后数据看起来不同：检查是否从项目根目录启动，以及是否配置了另一个数据库路径。

备份 Web 数据时，使用项目提供的 SQLite 备份脚本。它会将已提交的 WAL 数据一起生成到独立备份文件中，服务运行时也可使用；需要 Python 3：

```powershell
python tools/backup_database.py --source school.db --output backups/school-backup.db
```

脚本不会覆盖已有备份，再次备份时换一个文件名。不要只复制正在使用的 `school.db`；即使按 `Ctrl+C` 停止服务，仍可能存在包含新记录的 `school.db-wal`。不要删除已有数据库或 WAL 文件来处理连接问题，以免丢失人员记录。

## 第三方依赖

- `cpp-httplib`：提供 C++ HTTP 服务，源码在 `backend/httplib.h`。
- `nlohmann/json` 3.12.0：提供 JSON 解析和序列化，源码在 `backend/json.hpp`。
- SQLite 3.51.3：提供本地持久化，C 源码在 `backend/sqlite3.c`、`backend/sqlite3.h`。

第三方代码保留各自源文件中的版权和许可说明，版本来源与许可见 [THIRD_PARTY.md](THIRD_PARTY.md)。
