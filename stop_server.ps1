[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$pidFilePath = Join-Path $PSScriptRoot 'build\local-server.pid'
$expectedServerPath = Join-Path $PSScriptRoot 'build\server.exe'
if (-not (Test-Path -LiteralPath $pidFilePath)) {
    Write-Host '没有后台启动记录。如果服务在终端中运行，请在对应终端按 Ctrl+C。'
    exit 0
}
$localServerId = [int](Get-Content -LiteralPath $pidFilePath -Raw)
$localServerProcess = Get-Process -Id $localServerId -ErrorAction SilentlyContinue
if ($localServerProcess) {
    if ($localServerProcess.Path -ne $expectedServerPath) {
        throw '进程路径与本项目服务不一致，已取消停止操作。'
    }
    Stop-Process -Id $localServerId -ErrorAction Stop
    Write-Host '本项目后台服务已停止。'
} else {
    Write-Host '后台服务已经退出。'
}
Remove-Item -LiteralPath $pidFilePath -ErrorAction SilentlyContinue
