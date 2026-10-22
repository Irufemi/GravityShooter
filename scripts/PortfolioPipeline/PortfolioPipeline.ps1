param (
    [string]$ScriptDir = ""
)

$ErrorActionPreference = 'Stop'
$requirementsFileName = 'requirements.txt'

# Script directory resolution (supports direct execution and BAT invocation)
if (-not $ScriptDir) {
    if ((Test-Path variable:PSScriptRoot) -and $PSScriptRoot) {
        $ScriptDir = $PSScriptRoot
    } elseif ($MyInvocation.MyCommand -and ($MyInvocation.MyCommand | Get-Member -Name Path)) {
        $ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
    } else {
        $ScriptDir = Join-Path $PWD.Path "scripts\PortfolioPipeline"
    }
}

Write-Host "--- Starting Environment Check ---" -ForegroundColor Cyan

# Step 1. Python check
Write-Host "Step 1: Checking Python..."
try {
    $pythonVersion = python --version 2>&1
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace("$pythonVersion") -or "$pythonVersion" -match "was not found") {
        throw "Python is not installed or not found in PATH."
    }
    Write-Host "OK: $pythonVersion" -ForegroundColor Green
}
catch {
    Write-Host "Error: Python is not available." -ForegroundColor Red
    Write-Host "Details: $_" -ForegroundColor DarkGray
    Read-Host "Press Enter to exit..."
    exit 1
}

# Step 2. Git check
Write-Host "Step 2: Checking Git..."
try {
    $gitVersion = git --version 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "Git is not installed or not found in PATH."
    }
    Write-Host "OK: $gitVersion" -ForegroundColor Green
}
catch {
    Write-Host "Error: Git is not available." -ForegroundColor Red
    Write-Host "Details: $_" -ForegroundColor DarkGray
    Read-Host "Press Enter to exit..."
    exit 1
}

# Step 3. Dependencies check & auto installation
Write-Host "Step 3: Checking Dependencies..."
try {
    $reqFile = Join-Path $ScriptDir $requirementsFileName
    if (-not (Test-Path $reqFile)) {
        throw "File not found: $requirementsFileName"
    }

    $oldEAP = $ErrorActionPreference
    $ErrorActionPreference = 'SilentlyContinue'
    python -c "import googleapiclient, google_auth_oauthlib" 2>$null
    $ErrorActionPreference = $oldEAP
    $installed = ($LASTEXITCODE -eq 0)

    if (-not $installed) {
        Write-Host "Not installed: Starting installation..." -ForegroundColor Yellow
        $oldEAP = $ErrorActionPreference
        $ErrorActionPreference = 'SilentlyContinue'
        python -m pip install -r $reqFile --upgrade --no-warn-script-location 2>$null | Out-Null
        $pipExit = $LASTEXITCODE
        $ErrorActionPreference = $oldEAP

        if ($pipExit -ne 0) {
            throw "pip install failed with exit code $pipExit."
        }
        Write-Host "Installation completed." -ForegroundColor Green
    }
    else {
        Write-Host "Dependencies are already installed." -ForegroundColor Green
    }
}
catch {
    Write-Host "Error: $_" -ForegroundColor Red
    Read-Host "Press Enter to exit..."
    exit 1
}

# Step 4. Workspace Preparation & Git Clone (Chapter 02)
Write-Host "Step 4: Preparing Workspace & Cloning Repository..." -ForegroundColor Cyan

$zipName = 'LE3B_15_スエヒロ_コウイチ_GravityShooter'
$generatedDir = Join-Path $ScriptDir "Generated"
$workDir = Join-Path $generatedDir $zipName
$intermediateDir = Join-Path $ScriptDir "Intermediate"

$work1Path = Join-Path $workDir "01_個人作品"
$interWork1Path = Join-Path $intermediateDir "01_個人作品"
$cloneTempPath = Join-Path $interWork1Path "Source"
$repoUrl = 'https://github.com/Irufemi/Irufemi.git'

try {
    # 1. Clean and initialize Intermediate and Generated workspace
    Write-Host "  -> Resetting Intermediate workspace..."
    if (Test-Path $intermediateDir) {
        Remove-Item -Path $intermediateDir -Recurse -Force | Out-Null
    }
    if (-not (Test-Path $workDir)) {
        New-Item -Path $workDir -ItemType Directory -Force | Out-Null
    }
    New-Item -Path $work1Path -ItemType Directory -Force | Out-Null
    New-Item -Path $cloneTempPath -ItemType Directory -Force | Out-Null

    # 2. Shallow clone (--depth 1)
    Write-Host "  -> Cloning repository from $repoUrl (--depth 1)..."
    git clone --depth 1 $repoUrl "$cloneTempPath"
    if ($LASTEXITCODE -ne 0) {
        throw "git clone failed with exit code $LASTEXITCODE."
    }
    Write-Host "  OK: Repository clone completed." -ForegroundColor Green

    # 3. Deploy to Generated folder
    $destProjPath = Join-Path $work1Path "ソースコード"
    Write-Host "  -> Deploying to $destProjPath..."
    if (Test-Path $destProjPath) {
        Remove-Item -Path $destProjPath -Recurse -Force | Out-Null
    }
    Copy-Item -Path $cloneTempPath -Destination $destProjPath -Recurse -Force | Out-Null
    Write-Host "  OK: Source files deployed to Generated successfully." -ForegroundColor Green
}
catch {
    Write-Host "Error: Workspace preparation or git clone failed." -ForegroundColor Red
    Write-Host "Details: $_" -ForegroundColor DarkGray
    Read-Host "Press Enter to exit..."
    exit 1
}

Write-Host "--- Pipeline processing completed successfully ---" -ForegroundColor Cyan
Read-Host "Press Enter to exit..."
