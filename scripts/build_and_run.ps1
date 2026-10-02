param(
    [ValidateSet("Release", "Development", "Debug")]
    [string]$Configuration = "Release",
    [switch]$Rebuild,
    [switch]$BuildOnly
)

$ErrorActionPreference = "Stop"

$ScriptDir = if ((Test-Path variable:PSScriptRoot) -and $PSScriptRoot) {
    $PSScriptRoot
} elseif ($MyInvocation.MyCommand -and ($MyInvocation.MyCommand | Get-Member -Name Path)) {
    Split-Path -Parent $MyInvocation.MyCommand.Path
} else {
    Join-Path $PWD.Path "scripts"
}
$projectRoot = (Resolve-Path "$ScriptDir\..").Path
$slnxPath = Join-Path $projectRoot "project\Irufemi.slnx"
$solutionPath = if (Test-Path $slnxPath) { $slnxPath } else { Join-Path $projectRoot "project\Irufemi.sln" }
$outputDir = Join-Path $projectRoot "generated\outputs\$Configuration"
$exePath = Join-Path $outputDir "Application_solo.exe"
$logDir = Join-Path $projectRoot "logs\build"

if (-not (Test-Path $logDir)) { New-Item -ItemType Directory -Path $logDir -Force | Out-Null }

Write-Host "--- IrufemiEngine Build & Run System ---" -ForegroundColor Cyan
Write-Host "Configuration : $Configuration"
Write-Host "Target Project: Application_solo"
Write-Host "Root Path     : $projectRoot"

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    Write-Error "vswhere.exe not found. Visual Studio 2026 installation is required."
    exit 1
}

$msbuildPath = & $vswhere -latest -prerelease -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe | Select-Object -First 1
if (-not $msbuildPath) {
    Write-Error "MSBuild.exe not found."
    exit 1
}

Write-Host "Using MSBuild : $msbuildPath"

$buildTarget = if ($Rebuild) { "Application_solo:Rebuild" } else { "Application_solo" }
$buildLog = Join-Path $logDir "build_$($Configuration)_$(Get-Date -Format 'yyyyMMdd_HHmmss').log"
$buildArgs = @(
    $solutionPath,
    "-restore",
    "/t:$buildTarget",
    "/p:Configuration=$Configuration",
    "/p:Platform=x64",
    "/m",
    "/v:minimal",
    "/fl",
    "/flp:logfile=$buildLog;Encoding=UTF-8;verbosity=normal"
)

Write-Host "Building Application_solo ($Configuration)... (Log: $buildLog)" -ForegroundColor Yellow
$startTime = Get-Date

& $msbuildPath $buildArgs

if ($LASTEXITCODE -ne 0) {
    Write-Host "Build FAILED! Check the log for details: $buildLog" -ForegroundColor Red
    exit $LASTEXITCODE
}

$duration = (Get-Date) - $startTime
Write-Host "Build Successful! (Time: $($duration.TotalSeconds.ToString('F2'))s)" -ForegroundColor Green

if ($BuildOnly) {
    exit 0
}

if (Test-Path $exePath) {
    Write-Host "Launching Application_solo.exe..." -ForegroundColor Cyan
    Set-Location (Join-Path $projectRoot "project\Application_solo")
    & $exePath
} else {
    Write-Error "Executable not found at $exePath"
    exit 1
}

