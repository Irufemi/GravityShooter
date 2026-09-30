@echo off
chcp 65001 > nul
title 就職活動・技術審査用 パッケージ生成ツール

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0PackageForSubmission.ps1"

echo.
pause
