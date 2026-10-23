# FileUtils.ps1 - File and Directory utilities for Portfolio Pipeline

function Recreate-Directory {
    param(
        [string]$Path
    )
    if ([string]::IsNullOrWhiteSpace($Path)) { return }

    if (Test-Path $Path) {
        # Clear readonly flags first to avoid UnauthorizedAccessException on Windows
        Get-ChildItem -Path $Path -Recurse -Force -ErrorAction SilentlyContinue | ForEach-Object {
            if ($_.IsReadOnly) { $_.IsReadOnly = $false }
        }

        for ($i = 0; $i -lt 5; $i++) {
            try {
                Remove-Item -Path $Path -Recurse -Force -ErrorAction Stop
                break
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
            # - OS metadata (desktop.ini, Thumbs.db)
            # - macOS metadata (__MACOSX)
            # - Debug information (*.pdb)
            if ($part -match '^\..*' -or $part -eq 'desktop.ini' -or $part -eq 'Thumbs.db' -or $part -eq '__MACOSX' -or $part -like '*.pdb') {
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
