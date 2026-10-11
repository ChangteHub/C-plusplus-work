[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Push-Location -LiteralPath $PSScriptRoot
try {
    & $env:ComSpec /d /c 'build_server.bat'
    if ($LASTEXITCODE -ne 0) {
        throw '后端编译失败，服务未启动。请检查编译输出。'
    }

    $serverHost = if ($env:SCHOOL_HOST) { $env:SCHOOL_HOST } else { '127.0.0.1' }
    $serverPort = if ($env:SCHOOL_PORT) { $env:SCHOOL_PORT } else { '8080' }
    Write-Host "本机访问: http://localhost:$serverPort"
    Write-Host "监听地址: ${serverHost}:$serverPort"
    Write-Host '数据默认保存在项目目录下的 school.db。按 Ctrl+C 停止服务。'
    & '.\build\server.exe'
    $serverExitCode = $LASTEXITCODE
} finally {
    Pop-Location
}
exit $serverExitCode
