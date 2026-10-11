@echo off
setlocal
chcp 65001 >nul
pushd "%~dp0" || exit /b 1

where gcc >nul 2>nul
if errorlevel 1 (
    echo [错误] 未找到 gcc。请安装 MinGW，并将其 bin 目录加入 PATH。
    popd
    exit /b 1
)
where g++ >nul 2>nul
if errorlevel 1 (
    echo [错误] 未找到 g++。请安装 MinGW，并将其 bin 目录加入 PATH。
    popd
    exit /b 1
)

if not exist build mkdir build
if not exist build (
    echo [错误] 无法创建 build 目录。
    popd
    exit /b 1
)

echo [1/2] 编译 SQLite C 源码...
gcc -O2 -c backend\sqlite3.c -o build\sqlite3.o
if errorlevel 1 (
    echo [错误] SQLite 编译失败，请检查上方信息。
    popd
    exit /b 1
)

echo [2/2] 编译 C++ HTTP 服务...
g++ -std=c++17 -O2 -Wall -I. -Ibackend ^
    backend\web_server.cpp backend\service.cpp backend\dao.cpp ^
    person.cpp teacher.cpp adminstaff.cpp build\sqlite3.o ^
    -lws2_32 -o build\server.exe
if errorlevel 1 (
    echo [错误] 后端编译失败，请检查上方信息。
    popd
    exit /b 1
)

echo 编译成功: build\server.exe
popd
exit /b 0
