@echo off
chcp 65001 > nul
title Irufemi Benchmark Suite - 10,000 Objects Stress Test

echo ========================================================
echo  Irufemi Benchmark Suite (10,000 オブジェクト負荷検証)
echo ========================================================
echo.

set "GAME_DIR=%~dp0project\Application_solo"
set "GAME_EXE=%~dp0generated\outputs\Release\Application_solo.exe"
set "TELEMETRY_EXE=%~dp0project\Binaries\TelemetryMonitor\TelemetryMonitor.exe"

REM --- 1. TelemetryMonitor の準備 ---
if not exist "%TELEMETRY_EXE%" (
    echo [1/3] TelemetryMonitor.exe をビルドしています...
    pushd "%~dp0project\Tools\TelemetryMonitor"
    dotnet publish TelemetryMonitor.csproj -c Release -r win-x64 --self-contained false -p:PublishSingleFile=true -o ../../Binaries/TelemetryMonitor > nul
    popd
    if not exist "%TELEMETRY_EXE%" (
        echo [ERROR] TelemetryMonitor のビルドに失敗しました。
        pause
        exit /b 1
    )
) else (
    echo [1/3] TelemetryMonitor.exe: 準備完了
)

REM --- 2. ゲーム本体 (Release構成 / /O3) のビルド・確認 ---
echo [2/3] Application_solo (Release) のビルド状態を確認・同期しています...
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "$ErrorActionPreference = 'Stop'; [Console]::OutputEncoding = [System.Text.Encoding]::UTF8; [Console]::InputEncoding = [System.Text.Encoding]::UTF8; $scriptPath = '%~dp0scripts\build_and_run.ps1'; $content = [System.IO.File]::ReadAllText($scriptPath, [System.Text.Encoding]::UTF8); & ([scriptblock]::Create($content)) -Configuration Release -BuildOnly"

if %ERRORLEVEL% neq 0 (
    echo.
    echo [ERROR] ゲーム本体のビルドに失敗しました。
    pause
    exit /b %ERRORLEVEL%
)

REM --- 3. 連動起動 ---
echo [3/3] ベンチマーク環境を起動します...
echo   - Telemetry Monitor (左側推奨) を起動中...
start "" "%TELEMETRY_EXE%"

timeout /t 1 /nobreak > nul

echo   - ゲーム本体 (右側推奨) を起動中...
start /D "%GAME_DIR%" "" "%GAME_EXE%"

echo.
echo ========================================================
echo  [検証手順]
echo  1. 画面上の Telemetry Monitor 下部にある
echo     【💥 10,000 Debris】ボタンをクリックしてください。
echo  2. ゲーム画面内で 10,000 個のガレキが一括展開され、
echo     Telemetry Monitor 上の "System/VirtualEntities_Active" が
echo     即座に 10,000 に跳ね上がり、60FPS を維持する様子を確認できます。
echo  3. フィールドをクリアしたい場合は【🧹 Clear Debris】を押してください。
echo  ※ ゲーム内でも [F1] キーを押すことでコンソールから操作可能です。
echo ========================================================
echo.
exit /b 0
