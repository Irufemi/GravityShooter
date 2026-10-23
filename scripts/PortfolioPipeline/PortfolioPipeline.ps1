param (
    [string]$ScriptDir = "",
    [string]$TargetWork = $null,
    [switch]$SkipDownload,
    [switch]$SkipBuild,
    [switch]$SkipSmokeTest,
    [switch]$KeepIntermediate
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

# Step 4. Config Loading & Data-Driven Workspace Processing (Chapters 03 - 07)
Write-Host "Step 4: Loading Configuration & Processing Works..." -ForegroundColor Cyan

$configFileName = 'config.psd1'
$libDir = Join-Path $ScriptDir 'lib'
$assertScript = Join-Path $libDir 'ConfigAssert.ps1'
$fileUtilsScript = Join-Path $libDir 'FileUtils.ps1'
$buildUtilsScript = Join-Path $libDir 'BuildUtils.ps1'
$tasksScript = Join-Path $libDir 'PipelineTasks.ps1'

$pipelineSucceeded = $false
$intermediateDir = Join-Path $ScriptDir "Intermediate"

try {
    # 1. Load validators and modules (Chapters 03, 05, 07)
    if (-not (Test-Path $assertScript)) { throw "Validation script not found: $assertScript" }
    . $assertScript

    if (-not (Test-Path $fileUtilsScript)) { throw "FileUtils script not found: $fileUtilsScript" }
    . $fileUtilsScript

    if (-not (Test-Path $buildUtilsScript)) { throw "BuildUtils script not found: $buildUtilsScript" }
    . $buildUtilsScript

    if (-not (Test-Path $tasksScript)) { throw "PipelineTasks script not found: $tasksScript" }
    . $tasksScript

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

    # 3. Clean and prepare workspaces
    Write-Host "  -> Resetting Intermediate workspace..."
    if (Test-Path $intermediateDir) {
        Remove-DirectorySafe -Path $intermediateDir
    }
    if (-not (Test-Path $workDir)) {
        New-Item -Path $workDir -ItemType Directory -Force | Out-Null
    }

    # Root portfolio document download (Placed directly in submission root folder)
    if (-not $SkipDownload) {
        $portfolioId = $config.output.portfolio_google_drive_file_id
        if (-not [string]::IsNullOrEmpty($portfolioId)) {
            $pyDownloadScript = Join-Path $libDir "download_file.py"
            Invoke-PipelineTask -Message "Downloading root portfolio document from Google Drive" `
                -Action { Download-ProgramSpec -SpecId $portfolioId -WorkPath $workDir -PyScriptPath $pyDownloadScript } `
                -ErrorMessage "Failed to download root portfolio document."
        }
    }

    # 4. Iterate over works
    $matchedTarget = $false
    $workIndex = 1

    foreach ($work in $config.works) {
        if ([string]::IsNullOrEmpty($work.work_folder_name)) { continue }
        if (-not [string]::IsNullOrEmpty($TargetWork) -and $work.work_folder_name -ne $TargetWork) { continue }
        $matchedTarget = $true

        Write-Host "  -> Processing work: $($work.work_folder_name)..." -ForegroundColor Yellow
        $workPath = Join-Path $workDir $work.work_folder_name
        New-Item -Path $workPath -ItemType Directory -Force | Out-Null

        $projFolder = if (-not [string]::IsNullOrEmpty($config.output.project_folder_name)) {
            $config.output.project_folder_name
        } else {
            'SourceCode'
        }
        $destProjPath = Join-Path $workPath $projFolder

        $execFolder = if (-not [string]::IsNullOrEmpty($config.output.executable_folder_name)) {
            $config.output.executable_folder_name
        } else {
            '実行ファイル'
        }

        # Intermediate workspace for work item
        $interWorkPath = Join-Path $intermediateDir ("work_{0:00}" -f $workIndex)
        $stagingSourcePath = Join-Path $interWorkPath "Source"
        Recreate-Directory -Path $stagingSourcePath

        # Source acquisition (Git clone or Local copy) into intermediate workspace
        if (-not [string]::IsNullOrEmpty($work.git_repository_url)) {
            Invoke-PipelineTask -Message "Cloning git repository" `
                -Action { Get-SourceFromGit -RepoUrl $work.git_repository_url -CloneDestPath $stagingSourcePath } `
                -ErrorMessage "Failed to clone git repository for '$($work.work_folder_name)'."
        } else {
            $resolvedLocal = if ([System.IO.Path]::IsPathRooted($work.local_source_path)) {
                $work.local_source_path
            } else {
                Join-Path $ScriptDir $work.local_source_path
            }
            Invoke-PipelineTask -Message "Deploying local source" `
                -Action { Copy-ItemWithExclude -Source $resolvedLocal -Destination $stagingSourcePath } `
                -ErrorMessage "Failed to deploy local source for '$($work.work_folder_name)'."
        }

        # Deploy clean source to destination submission folder
        Recreate-Directory -Path $destProjPath
        Copy-ItemWithExclude -Source $stagingSourcePath -Destination $destProjPath

        # Generate repository shortcut (.url) if URL is configured
        $repoWebUrl = if (-not [string]::IsNullOrEmpty($work.repository_url)) {
            $work.repository_url
        } elseif (-not [string]::IsNullOrEmpty($work.git_repository_url)) {
            $work.git_repository_url
        } else {
            $null
        }

        if (-not [string]::IsNullOrEmpty($repoWebUrl)) {
            $shortcutPath = Join-Path $workPath "GitHub_Repository.url"
            New-InternetShortcut -Path $shortcutPath -Url $repoWebUrl
        }

        # Document download from Google Drive
        $pyDownloadScript = Join-Path $libDir "download_file.py"
        if (-not $SkipDownload -and -not [string]::IsNullOrEmpty($work.google_drive_file_id)) {
            Invoke-PipelineTask -Message "Downloading program document from Google Drive" `
                -Action { Download-ProgramSpec -SpecId $work.google_drive_file_id -WorkPath $workPath -PyScriptPath $pyDownloadScript } `
                -ErrorMessage "Failed to download document for '$($work.work_folder_name)'."
        }

        # Executable acquisition from Google Drive ZIP
        if (-not $SkipDownload -and -not [string]::IsNullOrEmpty($work.google_drive_zip_id)) {
            Invoke-PipelineTask -Message "Deploying executable from Google Drive ZIP" `
                -Action { Deploy-ExecutableFromZip -ZipId $work.google_drive_zip_id -WorkPath $workPath -ExecFolder $execFolder -PyScriptPath $pyDownloadScript } `
                -ErrorMessage "Failed to deploy executable ZIP for '$($work.work_folder_name)'."
        }

        # Executable acquisition from local folder/ZIP
        if (-not [string]::IsNullOrEmpty($work.local_executable_path)) {
            Invoke-PipelineTask -Message "Deploying executable from local path" `
                -Action { Deploy-ExecutableFromLocal -LocalPath $work.local_executable_path -WorkPath $workPath -ExecFolder $execFolder -RootPath $ScriptDir } `
                -ErrorMessage "Failed to deploy local executable for '$($work.work_folder_name)'."
        }

        # Automated compilation via MSBuild in intermediate workspace
        if ($work.project_msbuild_target_path -and -not $SkipBuild) {
            Invoke-PipelineTask -Message "Building project via MSBuild" `
                -Action {
                    Invoke-MSBuildAndDeploy -Work $work -WorkIndex $workIndex `
                        -WorkPath $workPath -ProjFolder $projFolder -ExecFolder $execFolder `
                        -IntermediateBaseDir $intermediateDir
                } `
                -ErrorMessage "MSBuild build failed for '$($work.work_folder_name)'."
        }

        # Automated smoke test for executable
        if (-not $SkipSmokeTest) {
            Invoke-PipelineTask -Message "Running smoke test on executable" `
                -Action {
                    Test-ExecutableSmoke -WorkPath $workPath -ExecFolder $execFolder `
                        -TestExecutableName $work.test_executable_name
                } `
                -ErrorMessage "Smoke test failed for '$($work.work_folder_name)'."
        }

        $workIndex++
    }

    if (-not [string]::IsNullOrEmpty($TargetWork) -and -not $matchedTarget) {
        throw "Target work '$TargetWork' was not found in config.psd1."
    }

    $pipelineSucceeded = $true
}
catch {
    Write-Host "Error: $($_.Exception.Message)" -ForegroundColor Red
    Read-Host "Press Enter to exit..."
    exit 1
}
finally {
    if ($pipelineSucceeded -and -not $KeepIntermediate) {
        if (Test-Path $intermediateDir) {
            Remove-Item -Path $intermediateDir -Recurse -Force -ErrorAction SilentlyContinue | Out-Null
        }
    }
}

Write-Host "--- Pipeline processing completed successfully ---" -ForegroundColor Cyan
Read-Host "Press Enter to exit..."
