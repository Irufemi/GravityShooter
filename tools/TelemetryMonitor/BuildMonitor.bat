@echo off
echo =======================================================
echo Building TelemetryMonitor (Release Build)...
echo =======================================================

dotnet publish TelemetryMonitor.csproj -c Release -r win-x64 --self-contained false -p:PublishSingleFile=true -o ../../project/Binaries/TelemetryMonitor

echo.
echo =======================================================
echo Build complete! The executable is located at:
echo project\Binaries\TelemetryMonitor\TelemetryMonitor.exe
echo =======================================================
pause
