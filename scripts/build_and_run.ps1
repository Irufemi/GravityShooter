param(
    [ValidateSet("Debug", "Development", "Release")]
    [string]$Configuration = "Debug",
    [switch]$Rebuild,
    [switch]$BuildOnly
)

$ErrorActionPreference = "Stop"
$projectRoot = $PSScriptRoot | Split-Path -Parent
$slnxPath = Join-Path $projectRoot "project\Irufemi.slnx"
$solutionPath = if (Test-Path $slnxPath) { $slnxPath } else { Join-Path $projectRoot "project\Irufemi.sln" }
$outputDir = Join-Path $projectRoot "generated\outputs\$Configuration"
$exePath = Join-Path $outputDir "Application.exe"
$logDir = Join-Path $projectRoot "logs\build"

if (-not (Test-Path $logDir)) { New-Item -ItemType Directory -Path $logDir -Force | Out-Null }

Write-Host "--- IrufemiEngine Build System ---" -ForegroundColor Cyan
Write-Host "Configuration: $Configuration"
Write-Host "Root: $projectRoot"

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    Write-Error "vswhere.exe not found. Visual Studio installation is required."
    exit 1
}

$msbuildPath = & $vswhere -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe | Select-Object -First 1
if (-not $msbuildPath) {
    Write-Error "MSBuild.exe not found."
    exit 1
}

Write-Host "Using MSBuild: $msbuildPath"

$buildTarget = if ($Rebuild) { "Rebuild" } else { "Build" }
$buildLog = Join-Path $logDir "build_$($Configuration)_$(Get-Date -Format 'yyyyMMdd_HHmmss').log"
$buildArgs = @(
    $solutionPath,
    "/t:$buildTarget",
    "/p:Configuration=$Configuration",
    "/p:Platform=x64",
    "/m",
    "/v:minimal",
    "/fl",
    "/flp:logfile=$buildLog;Encoding=UTF-8;verbosity=normal"
)

Write-Host "Building... (Log: $buildLog)" -ForegroundColor Yellow
$startTime = Get-Date

& $msbuildPath $buildArgs

if ($LASTEXITCODE -ne 0) {
    Write-Host "Build FAILED! Check the log for details." -ForegroundColor Red
    exit $LASTEXITCODE
}

$duration = (Get-Date) - $startTime
Write-Host "Build Successful! (Time: $($duration.TotalSeconds.ToString('F2'))s)" -ForegroundColor Green

if ($BuildOnly) {
    exit 0
}

if (Test-Path $exePath) {
    Write-Host "Launching Application..." -ForegroundColor Cyan
    Set-Location (Join-Path $projectRoot "project\Application")
    & $exePath
} else {
    Write-Error "Executable not found at $exePath"
    exit 1
}
