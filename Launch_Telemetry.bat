@echo off
chcp 65001 > nul
title Telemetry Monitor - Performance Profiler

echo ========================================================
echo  Telemetry Monitor リアルタイムパフォーマンス監視ツール
echo ========================================================
echo.

set "EXE_PATH=%~dp0project\Binaries\TelemetryMonitor\TelemetryMonitor.exe"
set "CSPROJ_DIR=%~dp0project\Tools\TelemetryMonitor"

if exist "%EXE_PATH%" (
    echo [INFO] 既存の TelemetryMonitor.exe を起動します...
    start "" "%EXE_PATH%"
    exit /b 0
)

echo [INFO] TelemetryMonitor.exe が見つからないため、自動ビルドを実行します...
where dotnet >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo.
    echo [ERROR] .NET SDK (dotnet) がインストールされていません。
    echo 本ツールをビルドするには .NET SDK が必要です。
    pause
    exit /b 1
)

pushd "%CSPROJ_DIR%"
dotnet publish TelemetryMonitor.csproj -c Release -r win-x64 --self-contained false -p:PublishSingleFile=true -o ../../Binaries/TelemetryMonitor
popd

if exist "%EXE_PATH%" (
    echo.
    echo [OK] ビルド成功！ Telemetry Monitor を起動します...
    start "" "%EXE_PATH%"
) else (
    echo.
    echo [ERROR] Telemetry Monitor のビルドに失敗しました。
    pause
    exit /b 1
)
