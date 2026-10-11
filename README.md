# 学校人员管理系统

一个以 C++ 为核心的学校人员管理作业项目。Web 界面由 C++ HTTP 服务托管，人员数据保存在 SQLite 数据库中；原有 C++ 控制台程序也保留。支持 Windows 本地开发，以及 Ubuntu 上通过 Nginx、HTTPS 和 systemd 部署。

## 服务器部署

Ubuntu 配置、HTTPS 证书续期、数据库备份和更新步骤见 [deploy/README.md](deploy/README.md)。服务器部署使用独立管理员登录；数据库和实际凭据只保存在服务器，不进入 Git 仓库。

当前运行地址：[https://swustmarket.xyz](https://swustmarket.xyz)。浏览器会弹出管理员登录框，凭据由管理员单独提供。2026 年 10 月 11 日已在 Ubuntu 24.04 上完成四类人员部署，验证页面、API、增删改查、旧库数据保留、重启后的数据库持久化和证书模拟续期。

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

- 管理教师、行政人员、学生和普通工作者，四类均支持新增、编辑、删除与查询。
- 公共信息包括工号或学号、姓名、性别、年龄、联系电话和院系或部门。
- 学生记录专业、班级和入学年份；普通工作者记录岗位和工资，适用于保安、保洁、维修等人员。
- 工资统计只计算教师、行政人员和普通工作者，学生不设置工资；工资排序将学生放在末尾。
- 按人员类别筛选，搜索人员，查看人数和工资统计。
- 后端校验字段格式、工号与学号的全校唯一性和数值范围；可按专业、班级、部门或岗位搜索。
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

`Person` 是包含公共属性和虚函数的抽象基类；`Teacher`、`AdminStaff`、`Student` 和 `Worker` 通过继承扩展各自字段，借助多态提供统一的显示、统计与保存接口。学生的 `getSalary()` 在统一 C++ 接口中返回 `0`，但学生不计入教职工工资统计，Web API 也不返回学生工资字段。Web 后端分为 HTTP 接口、业务逻辑、数据库访问三层，使协议处理和业务规则分开。

SQLite 在 C++ 程序内部运行，因此数据库随后端一起启用。DAO 使用 SQL 参数绑定写入字段，工号为主键。Web 与控制台复用人员类，但**两者的数据各自独立**：Web 保存到 `school.db`，控制台保存到 `school_data.txt`，没有自动同步或导入功能。

已有教师、行政人员数据库首次启动新版本时，会在事务内自动补充 `major`、`class_name`、`admission_year` 和 `job` 列，不删除旧记录。人员切换类别时会清理不适用的字段，例如学生转为工作者后清空专业与班级。工号、学号共用主键，不能给不同类别使用同一个编号。

## 项目文件

```text
project6/
├── person.h / person.cpp                 人员基类
├── teacher.h / teacher.cpp               教师类
├── adminstaff.h / adminstaff.cpp         行政人员类
├── student.h / student.cpp               学生类：学院、专业、班级、入学年份
├── worker.h / worker.cpp                 普通工作者类：部门、岗位、工资
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
├── tests/test_console.py                 新类别控制台保存、加载与统计测试
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

API 中类别使用小写 `teacher` / `admin` / `student` / `worker`，SQLite 表内保存为 `Teacher` / `Admin` / `Student` / `Worker`。四类均需要 `id`、`name`、`gender`、`age` 和 `department`，`phone` 可省略；类别字段如下：

| 类别 | 专属必填字段 | 年龄范围 |
| --- | --- | --- |
| 教师 `teacher` | `title`、`salary` | 18–100 |
| 行政人员 `admin` | `position`、`salary` | 18–100 |
| 学生 `student` | `major`、`className`、`admissionYear` | 6–100 |
| 普通工作者 `worker` | `job`、`salary` | 18–100 |

`age` 必须为整数，`salary` 必须为 `0` 至 `100000000` 的数值，`admissionYear` 必须为 `1900` 至 `2100` 的整数，不能用字符串或布尔值代替数字。学生不提交工资字段；非零学生工资会被拒绝。编辑时路径中的编号定位原记录，请求体中的 `id` 可以提供新工号或学号，类别也可修改。

学生请求示例：

```json
{"type":"student","id":"202601001","name":"李小明","gender":"男","age":17,"phone":"","department":"计算机学院","major":"软件工程","className":"软件工程2026级1班","admissionYear":2026}
```

普通工作者请求示例：

```json
{"type":"worker","id":"W001","name":"王师傅","gender":"男","age":45,"phone":"","department":"后勤服务中心","job":"维修","salary":4800.50}
```

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

按菜单对四类人员进行新增、删除、修改、查询、排序、统计与保存。控制台数据文件为 `school_data.txt`，与 Web 数据库独立；旧教师、行政人员文本记录仍可加载。学生文本记录以 `Student|` 开头，普通工作者以 `Worker|` 开头。

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

后端集成测试共 16 组，覆盖四类人员的增删改查、编号跨类别唯一性、编号修改和类别切换、学生与工作者字段校验、旧数据库升级后保留原记录，以及重启后的持久化。还覆盖登录、Unicode、工资小数、并发、请求大小、数据库锁与损坏后的错误处理。测试只使用临时数据库。

上述 16 组接口测试和控制台测试已在 Windows 与 Ubuntu 24.04 上通过。网页已验证学生与工作者表单、编辑回填、班级搜索和排除学生后的工资统计。公网还验证了两类人员的新增、编辑、编号冲突、服务重启后的持久化和删除；临时测试记录已清理，原有正式记录完整保留。

新增类别的控制台验证可在编译控制台程序后运行：

```powershell
python tests/test_console.py
```

Ubuntu 可指定 CMake 产物：`python3 tests/test_console.py --console build-cmake/school_system`。测试覆盖学生与工作者录入、编辑、分类查询、保存与加载、删除，以及排除学生后的工资统计；不读取现有 `school_data.txt`。

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
