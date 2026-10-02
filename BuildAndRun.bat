@echo off
chcp 65001 > nul
title IrufemiEngine - Build and Run

powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "$ErrorActionPreference = 'Stop'; [Console]::OutputEncoding = [System.Text.Encoding]::UTF8; [Console]::InputEncoding = [System.Text.Encoding]::UTF8; $scriptPath = '%~dp0scripts\build_and_run.ps1'; $content = [System.IO.File]::ReadAllText($scriptPath, [System.Text.Encoding]::UTF8); & ([scriptblock]::Create($content))"

if %ERRORLEVEL% neq 0 (
    echo.
    echo [ERROR] Build and Run failed with exit code %ERRORLEVEL%.
    pause
)
