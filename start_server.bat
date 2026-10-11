@echo off
setlocal
chcp 65001 >nul
pushd "%~dp0" || exit /b 1

call build_server.bat
if errorlevel 1 (
    echo [错误] 编译未成功，服务未启动。
    popd
    exit /b 1
)

if not defined SCHOOL_HOST set "SCHOOL_HOST=127.0.0.1"
if not defined SCHOOL_PORT set "SCHOOL_PORT=8080"
echo.
echo 正在启动学校人员管理系统...
echo 本机访问: http://localhost:%SCHOOL_PORT%
echo 监听地址: %SCHOOL_HOST%:%SCHOOL_PORT%
echo 数据保存到 SQLite 数据库，默认文件为项目目录下的 school.db。
echo 按 Ctrl+C 停止服务。请保持此窗口运行。
echo.
build\server.exe
set "SERVER_EXIT_CODE=%ERRORLEVEL%"
popd
exit /b %SERVER_EXIT_CODE%
