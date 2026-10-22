@echo off
setlocal
cd /d "%~dp0"
chcp 65001 > nul
title Portfolio Pipeline - Environment Setup

powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "$ErrorActionPreference = 'Stop'; [Console]::OutputEncoding = [System.Text.Encoding]::UTF8; [Console]::InputEncoding = [System.Text.Encoding]::UTF8; $scriptPath = '%~dp0PortfolioPipeline.ps1'; $content = [System.IO.File]::ReadAllText($scriptPath, [System.Text.Encoding]::UTF8); & ([scriptblock]::Create($content)) -ScriptDir '%~dp0'"

if %ERRORLEVEL% neq 0 (
    echo.
    echo [ERROR] Portfolio Pipeline failed with exit code %ERRORLEVEL%.
    pause
)
endlocal
