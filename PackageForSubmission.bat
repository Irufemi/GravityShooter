@echo off
chcp 65001 > nul
title Package For Submission Tool

powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "$ErrorActionPreference = 'Stop'; [Console]::OutputEncoding = [System.Text.Encoding]::UTF8; [Console]::InputEncoding = [System.Text.Encoding]::UTF8; $scriptPath = '%~dp0scripts\PackageForSubmission.ps1'; $content = [System.IO.File]::ReadAllText($scriptPath, [System.Text.Encoding]::UTF8); & ([scriptblock]::Create($content)) -ScriptDir '%~dp0scripts'"

if %ERRORLEVEL% neq 0 (
    echo.
    echo [ERROR] Package generation script failed with exit code %ERRORLEVEL%.
)

echo.
pause
