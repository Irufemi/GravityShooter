param (
    [string]$ScriptDir = "",
    [string]$TargetWork = $null
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

# Safe directory deletion with ReadOnly flag removal and retry logic
function Remove-DirectorySafe {
    param([string]$Path)
    if (-not (Test-Path $Path)) { return }
    Get-ChildItem -Path $Path -Recurse -Force -ErrorAction SilentlyContinue | ForEach-Object {
        if ($_.IsReadOnly) { $_.IsReadOnly = $false }
    }
    for ($i = 0; $i -lt 5; $i++) {
        try {
            Remove-Item -Path $Path -Recurse -Force -ErrorAction Stop
            return
        }
        catch {
            Start-Sleep -Milliseconds 500
        }
    }
    if (Test-Path $Path) {
        cmd /c "rmdir /s /q `"$Path`"" 2>$null
    }
}

# Safe directory move with retry and copy fallback
function Move-DirectorySafe {
    param([string]$Source, [string]$Destination)
    for ($i = 0; $i -lt 5; $i++) {
        try {
            Move-Item -Path $Source -Destination $Destination -Force -ErrorAction Stop
            return
        }
        catch {
            Start-Sleep -Milliseconds 500
        }
    }
    Copy-Item -Path $Source -Destination $Destination -Recurse -Force
    Remove-DirectorySafe -Path $Source
}

# Step 4. Config Loading & Data-Driven Workspace Processing (Chapter 03 & 04)
Write-Host "Step 4: Loading Configuration & Processing Works..." -ForegroundColor Cyan

$configFileName = 'config.psd1'
$libDir = Join-Path $ScriptDir 'lib'
$assertScript = Join-Path $libDir 'ConfigAssert.ps1'

try {
    # 1. Load Assert-Config validator
    if (-not (Test-Path $assertScript)) {
        throw "Validation script not found: $assertScript"
    }
    . $assertScript

    # 2. Check and import config.psd1
    $configPath = Join-Path $ScriptDir $configFileName
    if (-not (Test-Path $configPath)) {
        throw "Configuration file not found: $configFileName"
    }

    try {
        $config = Import-PowerShellDataFile -Path $configPath -ErrorAction Stop
    }
    catch {
        $content = [System.IO.File]::ReadAllText($configPath, [System.Text.Encoding]::UTF8)
        $sb = [scriptblock]::Create($content)
        $config = & $sb
    }
    Assert-Config -Config $config -BaseDir $ScriptDir

    $zipName = $config.output.zip_name
    $generatedDir = Join-Path $ScriptDir $config.output.generated_dir
    $workDir = Join-Path $generatedDir $zipName
    $intermediateDir = Join-Path $ScriptDir "Intermediate"

    # 3. Clean and prepare workspaces
    Write-Host "  -> Resetting Intermediate workspace..."
    if (Test-Path $intermediateDir) {
        Remove-DirectorySafe -Path $intermediateDir
    }
    if (-not (Test-Path $workDir)) {
        New-Item -Path $workDir -ItemType Directory -Force | Out-Null
    }

    # 4. Iterate over works
    $matchedTarget = $false
    foreach ($work in $config.works) {
        if ([string]::IsNullOrEmpty($work.work_folder_name)) { continue }
        if (-not [string]::IsNullOrEmpty($TargetWork) -and $work.work_folder_name -ne $TargetWork) { continue }
        $matchedTarget = $true

        Write-Host "  -> Processing work: $($work.work_folder_name)..." -ForegroundColor Yellow
        $workPath = Join-Path $workDir $work.work_folder_name
        New-Item -Path $workPath -ItemType Directory -Force | Out-Null

        # Determine destination folder name from config
        $projFolder = if (-not [string]::IsNullOrEmpty($config.output.project_folder_name)) {
            $config.output.project_folder_name
        } else {
            'SourceCode'
        }
        $destProjPath = Join-Path $workPath $projFolder

        # Initialize staging directory
        $stagingPath = Join-Path $workPath '_staging_source'
        if (Test-Path $stagingPath) {
            Remove-DirectorySafe -Path $stagingPath
        }

        # Branch acquisition: Git clone or Local copy
        $repoUrl = $work.git_repository_url
        $localPath = $work.local_source_path

        if (-not [string]::IsNullOrEmpty($repoUrl)) {
            Write-Host "     Cloning from $repoUrl (--depth 1)..."
            git clone --depth 1 $repoUrl "$stagingPath"
            if ($LASTEXITCODE -ne 0) {
                throw "git clone failed for '$($work.work_folder_name)' with exit code $LASTEXITCODE."
            }

            # Remove .git metadata directory to save space and avoid file locks
            $gitMetaDir = Join-Path $stagingPath '.git'
            if (Test-Path $gitMetaDir) {
                Remove-DirectorySafe -Path $gitMetaDir
            }
        } else {
            $resolvedLocal = if ([System.IO.Path]::IsPathRooted($localPath)) {
                $localPath
            } else {
                Join-Path $ScriptDir $localPath
            }
            Write-Host "     Copying local source from $resolvedLocal..."
            Copy-Item -Path $resolvedLocal -Destination $stagingPath -Recurse -Force | Out-Null
        }

        # Deploy from staging to destination (atomic replace)
        if (Test-Path $destProjPath) {
            Remove-DirectorySafe -Path $destProjPath
        }
        Move-DirectorySafe -Source $stagingPath -Destination $destProjPath
        Write-Host "     OK: Deployed to $destProjPath" -ForegroundColor Green
    }

    if (-not [string]::IsNullOrEmpty($TargetWork) -and -not $matchedTarget) {
        throw "Target work '$TargetWork' was not found in config.psd1."
    }
}
catch {
    Write-Host "Error: $($_.Exception.Message)" -ForegroundColor Red
    Read-Host "Press Enter to exit..."
    exit 1
}

Write-Host "--- Pipeline processing completed successfully ---" -ForegroundColor Cyan
Read-Host "Press Enter to exit..."
