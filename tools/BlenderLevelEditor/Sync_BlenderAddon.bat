@echo off
chcp 65001 > nul
title Blender Level Editor - Addon Sync Tool

echo ========================================================
echo  Blender Level Editor - アドオン同期ツール
echo ========================================================
echo.

set "SCRIPT_DIR=%~dp0"
set "ADDON_SRC=%SCRIPT_DIR%addon"

if not exist "%ADDON_SRC%" (
    echo [エラー] アドオン元ディレクトリが見つかりません: %ADDON_SRC%
    pause
    exit /b 1
)

REM Blender AppData の最新バージョンフォルダを自動検出
set "BLENDER_BASE=%APPDATA%\Blender Foundation\Blender"
set "LATEST_VER="

if exist "%BLENDER_BASE%" (
    for /f "tokens=*" %%v in ('dir /b /ad /o-n "%BLENDER_BASE%" 2^>nul') do (
        if exist "%BLENDER_BASE%\%%v" (
            set "LATEST_VER=%%v"
            goto :VER_FOUND
        )
    )
)

:VER_FOUND
if "%LATEST_VER%"=="" (
    set "LATEST_VER=4.5"
    echo [INFO] インストール済みバージョンが検出されなかったため、デフォルト 4.5 を使用します。
) else (
    echo [OK] Blender AppData を検出しました (バージョン: %LATEST_VER%)
)

set "BLENDER_ADDON_DIR=%BLENDER_BASE%\%LATEST_VER%\scripts\addons\level_editor"
echo      同期先: %BLENDER_ADDON_DIR%
echo.

echo ========================================================
echo  1: 【Deploy】 ワークスペース (tools) から Blender アドオンへ反映
echo  2: 【Pull】   Blender アドオン から ワークスペース (tools) へ取り込み
echo  0: キャンセル
echo ========================================================
set /p choice="実行する処理の番号を入力してください (1/2/0): "

if "%choice%"=="1" (
    echo.
    echo ワークスペースから Blender へアドオンを配置しています...
    if not exist "%BLENDER_ADDON_DIR%" mkdir "%BLENDER_ADDON_DIR%"
    robocopy "%ADDON_SRC%" "%BLENDER_ADDON_DIR%" /E /NJH /NJS /NDL /NC /NS /NP
    if %ERRORLEVEL% leq 7 (
        echo.
        echo [成功] Blender側にアドオンを反映しました！
        echo        Blender上でアドオンをリロード、または再起動してください。
    ) else (
        echo [失敗] 同期に失敗しました。Blenderが起動中でファイルがロックされている可能性があります。
    )
) else if "%choice%"=="2" (
    echo.
    echo Blender からワークスペースへアドオンを取り込んでいます...
    if not exist "%BLENDER_ADDON_DIR%" (
        echo [エラー] Blender側のアドオンフォルダが存在しません: %BLENDER_ADDON_DIR%
        pause
        exit /b 1
    )
    robocopy "%BLENDER_ADDON_DIR%" "%ADDON_SRC%" /E /NJH /NJS /NDL /NC /NS /NP
    if %ERRORLEVEL% leq 7 (
        echo.
        echo [成功] ワークスペースに取り込みました！
    ) else (
        echo [失敗] 取り込みに失敗しました。
    )
) else (
    echo.
    echo 処理をキャンセルしました。
)

echo.
pause
exit /b 0
