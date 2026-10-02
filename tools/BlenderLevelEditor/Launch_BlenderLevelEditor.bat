@echo off
chcp 65001 > nul
title Blender Level Editor - Launcher

echo ========================================================
echo  Blender Level Editor - 起動ランチャー
echo ========================================================
echo.

set "BLENDER_EXE="

REM 1. PATH 上の blender を確認
where blender.exe >nul 2>&1
if %ERRORLEVEL% equ 0 (
    for /f "tokens=*" %%i in ('where blender.exe') do (
        set "BLENDER_EXE=%%i"
        goto :FOUND
    )
)

REM 2. ユーザー独自環境 (C:\Blender Foundation) の探索
if exist "C:\Blender Foundation\Blender 4.5\blender.exe" (
    set "BLENDER_EXE=C:\Blender Foundation\Blender 4.5\blender.exe"
    goto :FOUND
)
for /d %%d in ("C:\Blender Foundation\Blender*") do (
    if exist "%%d\blender.exe" (
        set "BLENDER_EXE=%%d\blender.exe"
        goto :FOUND
    )
)

REM 3. 公式標準パス (C:\Program Files\Blender Foundation) の探索
for /d %%d in ("C:\Program Files\Blender Foundation\Blender 4.*") do (
    if exist "%%d\blender.exe" (
        set "BLENDER_EXE=%%d\blender.exe"
        goto :FOUND
    )
)
for /d %%d in ("C:\Program Files\Blender Foundation\Blender*") do (
    if exist "%%d\blender.exe" (
        set "BLENDER_EXE=%%d\blender.exe"
        goto :FOUND
    )
)

REM 4. Steam版の探索
if exist "%ProgramFiles(x86)%\Steam\steamapps\common\Blender\blender.exe" (
    set "BLENDER_EXE=%ProgramFiles(x86)%\Steam\steamapps\common\Blender\blender.exe"
    goto :FOUND
)
if exist "D:\Steam\steamapps\common\Blender\blender.exe" (
    set "BLENDER_EXE=D:\Steam\steamapps\common\Blender\blender.exe"
    goto :FOUND
)

REM 見つからなかった場合の入力受付
echo [警告] Blenderのインストール先を自動検出できませんでした。
set /p BLENDER_EXE="blender.exe のフルパスを入力してください: "
if not exist "%BLENDER_EXE%" (
    echo [エラー] 指定されたパスに blender.exe が存在しません: %BLENDER_EXE%
    pause
    exit /b 1
)

:FOUND
echo [OK] Blender を検出しました:
echo      "%BLENDER_EXE%"
echo.
echo Blender Level Editor を起動しています...
start "" "%BLENDER_EXE%"
exit /b 0
