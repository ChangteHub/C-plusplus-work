@echo off
setlocal
chcp 65001 >nul
pushd "%~dp0" || exit /b 1

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

echo 正在编译 C++ 控制台程序...
g++ -std=c++17 -O2 -Wall -o build\school_system.exe ^
    main.cpp person.cpp teacher.cpp adminstaff.cpp student.cpp worker.cpp graduate.cpp personmanager.cpp
if errorlevel 1 (
    echo [错误] 控制台程序编译失败，请检查上方信息。
    popd
    exit /b 1
)

echo 编译成功: build\school_system.exe
echo 从项目根目录运行: .\build\school_system.exe
popd
exit /b 0
