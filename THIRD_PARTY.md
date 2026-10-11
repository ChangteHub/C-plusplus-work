# 第三方源码

为方便 C++ 作业在本地编译，项目包含以下依赖源码，不需要运行时联网下载。

| 依赖 | 版本 | 文件 | 来源与许可 |
| --- | --- | --- | --- |
| cpp-httplib | 0.60.1（项目原有版本） | `backend/httplib.h` | [官方仓库](https://github.com/yhirose/cpp-httplib)，MIT |
| JSON for Modern C++ | 3.12.0 | `backend/json.hpp` | [固定版本源码](https://github.com/nlohmann/json/blob/v3.12.0/single_include/nlohmann/json.hpp)，MIT |
| SQLite | 3.51.3 | `backend/sqlite3.c`、`backend/sqlite3.h` | [官方源码归档](https://www.sqlite.org/2026/sqlite-amalgamation-3510300.zip)，[公有领域声明](https://sqlite.org/copyright.html) |

JSON 单头文件下载时的 SHA-256：`aaf127c04cb31c406e5b04a63f1ae89369fccde6d8fa7cdda1ed4f32dfc5de63`。

个人账号密码派生与随机数使用系统加密库，未复制其实现源码：Windows 链接 BCrypt（系统组件），Ubuntu 链接 OpenSSL libcrypto（需 libssl-dev）。接口依据 [Microsoft BCrypt 文档](https://learn.microsoft.com/en-us/windows/win32/api/bcrypt/nf-bcrypt-bcryptderivekeypbkdf2) 与 [OpenSSL PBKDF2 文档](https://docs.openssl.org/3.5/man3/PKCS5_PBKDF2_HMAC/)。Windows 与 Linux 使用相同参数，测试通过 Python hashlib 校验。

SQLite 从原有 3.51.1 更新至 3.51.3，包含官方 [WAL 并发写入修复](https://sqlite.org/wal.html)。依赖自身的版权声明保留在源码中；MIT 许可文本保存在 `licenses/`。
