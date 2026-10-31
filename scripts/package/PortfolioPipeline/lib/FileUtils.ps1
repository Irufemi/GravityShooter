# FileUtils.ps1 - File and Directory utilities for Portfolio Pipeline

function Remove-DirectorySafe {
    param(
        [string]$Path
    )
    if ([string]::IsNullOrWhiteSpace($Path) -or -not (Test-Path $Path)) { return }

    # Clear readonly flags first to avoid UnauthorizedAccessException on Windows
    Get-ChildItem -Path $Path -File -Recurse -Force -ErrorAction SilentlyContinue | ForEach-Object {
        if ($_.IsReadOnly) { $_.IsReadOnly = $false }
    }

    for ($i = 0; $i -lt 5; $i++) {
        try {
            Remove-Item -Path $Path -Recurse -Force -ErrorAction Stop
            return
        }
        catch {
            Start-Sleep -Milliseconds 300
        }
    }

    # Fallback to cmd rmdir if locked
    if (Test-Path $Path) {
        cmd /c "rmdir /s /q `"$Path`"" 2>$null
    }
}

function Recreate-Directory {
    param(
        [string]$Path
    )
    if ([string]::IsNullOrWhiteSpace($Path)) { return }
    Remove-DirectorySafe -Path $Path
    New-Item -Path $Path -ItemType Directory -Force | Out-Null
}

function Copy-ItemWithExclude {
    param(
        [string]$Source,
        [string]$Destination
    )

    if (-not (Test-Path $Source)) {
        throw "Source path does not exist: $Source"
    }

    # If source is a single file, copy directly
    if (Test-Path $Source -PathType Leaf) {
        $destDir = Split-Path -Parent $Destination
        if (-not (Test-Path $destDir)) {
            New-Item -Path $destDir -ItemType Directory -Force | Out-Null
        }
        Copy-Item -Path $Source -Destination $Destination -Force
        return
    }

    if (-not (Test-Path $Destination)) {
        New-Item -Path $Destination -ItemType Directory -Force | Out-Null
    }

    $sourceNormalized = (Get-Item $Source).FullName
    $sourceLen = $sourceNormalized.Length

    $items = Get-ChildItem -Path $sourceNormalized -Recurse -Force
    foreach ($item in $items) {
        $relPath = $item.FullName.Substring($sourceLen).TrimStart('\')
        if ([string]::IsNullOrEmpty($relPath)) { continue }

        $parts = $relPath -split '\\'
        $exclude = $false
        foreach ($part in $parts) {
            # Exclusion rules:
            # - Dot-prefixed folders/files (.git, .vscode, .vs, etc.)
            # - Build outputs and temporary folders (Generated, Intermediate, _Submission, bin, obj, Binaries)
            # - OS metadata (desktop.ini, Thumbs.db)
            # - macOS metadata (__MACOSX)
            # - Debug binaries (*.pdb, *.ilk)
            if ($part -match '^\..*' -or 
                $part -in @('Generated', 'Intermediate', '_Submission', 'bin', 'obj', 'Binaries', 'desktop.ini', 'Thumbs.db', '__MACOSX') -or 
                $part -like '*.pdb' -or $part -like '*.ilk') {
                $exclude = $true
                break
            }
        }

        if ($exclude) { continue }

        $targetPath = Join-Path $Destination $relPath
        if ($item.PSIsContainer) {
            if (-not (Test-Path $targetPath)) {
                New-Item -Path $targetPath -ItemType Directory -Force | Out-Null
            }
        } else {
            $parentDir = Split-Path $targetPath -Parent
            if (-not (Test-Path $parentDir)) {
                New-Item -Path $parentDir -ItemType Directory -Force | Out-Null
            }
            Copy-Item -Path $item.FullName -Destination $targetPath -Force
        }
    }
}

function New-InternetShortcut {
    param (
        [string]$Path,
        [string]$Url
    )
    $content = "[InternetShortcut]`r`nURL=$Url`r`n"
    [System.IO.File]::WriteAllText($Path, $content, [System.Text.Encoding]::ASCII)
}

function Expand-AndFlattenArchive {
    param (
        [string]$ZipPath,
        [string]$TempExtractDir,
        [string]$DestinationDir
    )
    if (-not (Test-Path $ZipPath)) {
        throw "ZIP archive not found: $ZipPath"
    }

    Recreate-Directory -Path $TempExtractDir
    Expand-Archive -Path $ZipPath -DestinationPath $TempExtractDir -Force

    Recreate-Directory -Path $DestinationDir

    $topItems = Get-ChildItem -Path $TempExtractDir
    $effectiveSource = $TempExtractDir
    if ($topItems.Count -eq 1 -and $topItems[0].PSIsContainer) {
        $effectiveSource = $topItems[0].FullName
    }

    Copy-ItemWithExclude -Source $effectiveSource -Destination $DestinationDir
    Remove-DirectorySafe -Path $TempExtractDir
}
