function Invoke-PipelineTask {
    param(
        [string]$Message,
        [scriptblock]$Action,
        [string]$ErrorMessage
    )
    Write-Host "  -> $Message..." -NoNewline
    try {
        & $Action
        Write-Host " OK" -ForegroundColor Green
    }
    catch {
        Write-Host " FAILED" -ForegroundColor Red
        throw "$ErrorMessage Cause: $($_.Exception.Message)"
    }
}

function Get-SourceFromGit {
    param(
        [string]$RepoUrl,
        [string]$CloneDestPath
    )
    git clone --depth 1 $RepoUrl "$CloneDestPath"
    if ($LASTEXITCODE -ne 0) {
        throw "git clone failed with exit code $LASTEXITCODE."
    }

    $gitDir = Join-Path $CloneDestPath '.git'
    if (Test-Path $gitDir) {
        Remove-Item -Path $gitDir -Recurse -Force | Out-Null
    }
}

function Download-ProgramSpec {
    param(
        [string]$SpecId,
        [string]$WorkPath,
        [string]$PyScriptPath
    )
    python "$PyScriptPath" "$SpecId" "$WorkPath"
    if ($LASTEXITCODE -ne 0) {
        throw "download_file.py failed with exit code $LASTEXITCODE."
    }
}

function Deploy-ExecutableFromZip {
    param(
        [string]$ZipId,
        [string]$WorkPath,
        [string]$ExecFolder,
        [string]$PyScriptPath
    )
    $tempZipDir = Join-Path $WorkPath 'temp_zip_extract'
    Recreate-Directory -Path $tempZipDir
    python "$PyScriptPath" "$ZipId" "$tempZipDir"
    if ($LASTEXITCODE -ne 0) {
        throw "download_file.py failed with exit code $LASTEXITCODE."
    }

    $zipFiles = Get-ChildItem -Path $tempZipDir -Filter '*.zip'
    if ($zipFiles.Count -ne 1) {
        throw "Expected exactly 1 ZIP file, but found: $($zipFiles.Count)"
    }

    $destExecPath = Join-Path $WorkPath $ExecFolder
    Expand-AndFlattenArchive -ZipPath $zipFiles[0].FullName `
        -TempExtractDir (Join-Path $tempZipDir 'extracted') `
        -DestinationDir $destExecPath
    Remove-Item -Path $tempZipDir -Recurse -Force | Out-Null
}

function Deploy-ExecutableFromLocal {
    param(
        [string]$LocalPath,
        [string]$WorkPath,
        [string]$ExecFolder,
        [string]$RootPath
    )
    $resolvedPath = $LocalPath
    if (-not [System.IO.Path]::IsPathRooted($LocalPath)) {
        $resolvedPath = [System.IO.Path]::GetFullPath((Join-Path $RootPath $LocalPath))
    }
    if (-not (Test-Path $resolvedPath)) {
        throw "Specified local executable path does not exist: $resolvedPath"
    }

    $destExecPath = Join-Path $WorkPath $ExecFolder
    if ((Test-Path $resolvedPath -PathType Leaf) -and $resolvedPath -like '*.zip') {
        Expand-AndFlattenArchive -ZipPath $resolvedPath `
            -TempExtractDir (Join-Path $WorkPath 'temp_local_zip_extract') `
            -DestinationDir $destExecPath
    } else {
        Copy-ItemWithExclude -Source $resolvedPath -Destination $destExecPath
    }
}

function Invoke-MSBuildAndDeploy {
    param(
        [hashtable]$Work,
        [int]$WorkIndex,
        [string]$WorkPath,
        [string]$ProjFolder,
        [string]$ExecFolder,
        [string]$IntermediateBaseDir
    )
    $interWorkPath = Join-Path $IntermediateBaseDir ("work_{0:00}" -f $WorkIndex)
    $interProjPath = Join-Path $interWorkPath 'Source'
    $destProjPath = Join-Path $WorkPath $ProjFolder

    # 1. Clean and deploy source to submission folder
    Recreate-Directory -Path $destProjPath
    Copy-ItemWithExclude -Source $interProjPath -Destination $destProjPath

    # 2. Prepare MSBuild parameters
    $msbuildPath = Find-MSBuildPath
    $msbuildTarget = Join-Path $interProjPath $Work.project_msbuild_target_path
    if (-not (Test-Path $msbuildTarget)) {
        throw "MSBuild target solution/project not found: $msbuildTarget"
    }

    $configName = if ($Work.project_msbuild_configuration) {
        $Work.project_msbuild_configuration
    } else {
        'Release'
    }

    $logFilePath = Join-Path $interWorkPath 'msbuild.log'
    $msbuildArgs = @(
        "`"$msbuildTarget`"",
        '/t:Build',
        "/p:Configuration=$configName",
        '/v:normal',
        '/nr:false',
        '/fl',
        "/flp:LogFile=`"$logFilePath`";Verbosity=normal"
    )
    if ($Work.project_msbuild_platform) {
        $msbuildArgs += "/p:Platform=`"$($Work.project_msbuild_platform)`""
    }

    # 3. Execute MSBuild in intermediate workspace
    & $msbuildPath $msbuildArgs *> $null
    if ($LASTEXITCODE -ne 0) {
        throw "MSBuild failed with exit code $LASTEXITCODE. Build log: $logFilePath"
    }

    # 4. Deploy build artifacts to destination execution folder
    $destExecPath = Join-Path $WorkPath $ExecFolder
    Recreate-Directory -Path $destExecPath

    $copyList = $Work.project_build_copy_list
    if ($copyList -and $copyList.Count -gt 0) {
        foreach ($relItem in $copyList) {
            if ([string]::IsNullOrEmpty($relItem)) { continue }
            $resolvedSource = Join-Path $interProjPath $relItem
            if (-not (Test-Path $resolvedSource)) {
                throw "Build output item not found: $resolvedSource"
            }
            Copy-ItemWithExclude -Source $resolvedSource `
                -Destination (Join-Path $destExecPath (Split-Path $resolvedSource -Leaf))
        }
    } else {
        $outputPathRel = $Work.project_build_output_relative_path
        if ([string]::IsNullOrEmpty($outputPathRel)) {
            throw "Either project_build_copy_list or project_build_output_relative_path must be configured for MSBuild."
        }
        $resolvedOutput = Join-Path $interProjPath $outputPathRel
        if (-not (Test-Path $resolvedOutput)) {
            throw "Build output directory not found: $resolvedOutput"
        }
        Copy-ItemWithExclude -Source $resolvedOutput -Destination $destExecPath
    }
}

function Test-ExecutableSmoke {
    param(
        [string]$WorkPath,
        [string]$ExecFolder,
        [string]$TestExecutableName,
        [int]$WaitSeconds = 3
    )
    $execDir = Join-Path $WorkPath $ExecFolder
    if (-not (Test-Path $execDir)) { return }

    if ($TestExecutableName) {
        $exePath = Join-Path $execDir $TestExecutableName
    } else {
        $exeFiles = Get-ChildItem -Path $execDir -Filter '*.exe' -File
        $exePath = if ($exeFiles.Count -gt 0) { $exeFiles[0].FullName } else { $null }
    }
    if ($null -eq $exePath -or -not (Test-Path $exePath)) { return }

    $process = $null
    try {
        if (Get-Command Unblock-File -ErrorAction SilentlyContinue) {
            Unblock-File -Path $exePath -ErrorAction SilentlyContinue
        }
        $process = Start-Process -FilePath $exePath `
            -WorkingDirectory $execDir -PassThru -WindowStyle Minimized `
            -ErrorAction Stop
        Start-Sleep -Seconds $WaitSeconds
        if ($process.HasExited -and $process.ExitCode -ne 0) {
            throw "Process crashed immediately with exit code $($process.ExitCode)."
        }
    }
    finally {
        if ($null -ne $process -and -not $process.HasExited) {
            $process.Kill()
        }
    }
}
