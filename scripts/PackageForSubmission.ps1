param (
    [ValidateSet("Source", "Build", "All", "Menu")]
    [string]$Mode = "Menu",
    [string]$ScriptDir = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# ルートディレクトリの特定
if (-not $ScriptDir) {
    if (Test-Path variable:PSScriptRoot -and $PSScriptRoot) {
        $ScriptDir = $PSScriptRoot
    } elseif ($MyInvocation.MyCommand -and ($MyInvocation.MyCommand | Get-Member -Name Path)) {
        $ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
    } else {
        $ScriptDir = $PWD.Path
    }
}
$RootDir = (Resolve-Path "$ScriptDir\..").Path
$OutputDir = Join-Path $RootDir "_Submission"
$Timestamp = Get-Date -Format "yyyyMMdd_HHmm"

# コンソール出力ヘルパー
function Write-Header {
    param([string]$Text)
    Write-Host "`n========================================================" -ForegroundColor Cyan
    Write-Host " $Text" -ForegroundColor Cyan
    Write-Host "========================================================`n" -ForegroundColor Cyan
}

function Write-Success {
    param([string]$Text)
    Write-Host "[OK] $Text" -ForegroundColor Green
}

function Write-Warn {
    param([string]$Text)
    Write-Host "[WARN] $Text" -ForegroundColor Yellow
}

function Write-Info {
    param([string]$Text)
    Write-Host "[INFO] $Text" -ForegroundColor Gray
}

# 出力先ディレクトリの確保
if (-not (Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir | Out-Null
}

# -----------------------------------------------------------------------------
# 1. ソースコード提出用パッケージ作成
# -----------------------------------------------------------------------------
function Get-FolderSizeMB {
    param([string]$Path)
    $bytes = (Get-ChildItem -Path $Path -Recurse -File -Force -ErrorAction SilentlyContinue | Measure-Object -Property Length -Sum).Sum
    if (-not $bytes) { return 0 }
    return [math]::Round($bytes / 1MB, 2)
}

# -----------------------------------------------------------------------------
# 1. ソースコード提出用パッケージ作成
# -----------------------------------------------------------------------------
function Create-SourcePackage {
    Write-Header "ソースコード提出用パッケージ（フォルダ）の作成を開始します"

    $TargetDir = Join-Path $OutputDir "GravityShooter_SourceCode"

    if (Test-Path $TargetDir) {
        Write-Info "既存の出力フォルダをクリーンアップ中..."
        Remove-Item -Path $TargetDir -Recurse -Force
    }
    New-Item -ItemType Directory -Path $TargetDir | Out-Null

    Write-Info "ファイルを抽出・出力中..."

    # コピー対象ルートアイテム（不要なgakkousuraidoは除外）
    $IncludeItems = @(
        ".github",
        ".editorconfig",
        ".gitattributes",
        ".gitignore",
        "_typos.toml",
        "Doxyfile",
        "LICENSE.txt",
        "Manual.md",
        "README.md",
        "C_plus_plus_Setup_Workflow.md",
        "TL1",
        "project",
        "scripts"
    )

    # 除外ディレクトリ名
    $ExcludeDirs = @(
        ".git",
        ".agents",
        ".vs",
        ".vscode",
        "generated",
        "Binaries",
        "Debug",
        "Release",
        "x64",
        "x86",
        "obj",
        "bin",
        "outputs",
        "_Submission",
        ".cache",
        "cache",
        "__pycache__",
        ".venv",
        "venv",
        "scratch",
        "logs"
    )

    # 除外ファイル
    $ExcludeExtensions = @(
        "*.obj",
        "*.pdb",
        "*.ilk",
        "*.tlog",
        "*.idb",
        "*.log",
        "*.pso",
        "*.cachefile",
        "*.suo",
        "*.user",
        "*.opendb",
        "*.VC.db",
        "*.exp",
        "*.tmp",
        "*.bak",
        "*.ipch",
        "build_error.log",
        "shader_error.txt",
        "files.txt",
        "planned_moves.txt",
        "md_contents.txt",
        "*.tmp_prefab_backup.json"
    )

    foreach ($item in $IncludeItems) {
        $srcPath = Join-Path $RootDir $item
        if (-not (Test-Path $srcPath)) {
            continue
        }

        $destPath = Join-Path $TargetDir $item

        if ((Get-Item $srcPath).PSIsContainer) {
            New-Item -ItemType Directory -Path $destPath -Force | Out-Null
            $rcParams = @(
                $srcPath,
                $destPath,
                "/E",
                "/NJH", "/NJS", "/NDL", "/NC", "/NS", "/NP",
                "/XF"
            ) + $ExcludeExtensions + @("/XD") + $ExcludeDirs

            & robocopy @rcParams | Out-Null
        } else {
            Copy-Item -Path $srcPath -Destination $destPath -Force
        }
    }

    $fileSizeMB = Get-FolderSizeMB $TargetDir
    Write-Success "ソースコードパッケージ作成完了!"
    Write-Host "   出力先: $TargetDir" -ForegroundColor White
    Write-Host "   サイズ: $fileSizeMB MB`n" -ForegroundColor Green
}

# -----------------------------------------------------------------------------
# 2. 実行ファイル（プレイ用）パッケージ作成
# -----------------------------------------------------------------------------
function Create-PlayablePackage {
    Write-Header "実行ファイル（プレイ用）パッケージ（フォルダ）の作成を開始します"

    $ExeReleasePath = Join-Path $RootDir "generated\outputs\Release\Application_solo.exe"

    if (-not (Test-Path $ExeReleasePath)) {
        Write-Warn "Release構成のビルド成果物が見つかりません:"
        Write-Warn $ExeReleasePath
        Write-Host "先に Visual Studio で [Release] 構成でビルドを行ってください。`n" -ForegroundColor Yellow
        return
    }

    $TargetDir = Join-Path $OutputDir "GravityShooter_PlayableBuild"

    if (Test-Path $TargetDir) {
        Write-Info "既存の出力フォルダをクリーンアップ中..."
        Remove-Item -Path $TargetDir -Recurse -Force
    }
    New-Item -ItemType Directory -Path $TargetDir | Out-Null

    Write-Info "実行ファイルおよびリソースを配置中..."

    # 1. Exe本体（完全静的リンクのため単体で動作）
    Copy-Item -Path $ExeReleasePath -Destination (Join-Path $TargetDir "GravityShooter.exe") -Force

    # 2. resources/
    $resSrc = Join-Path $RootDir "project\Application_solo\resources"
    if (Test-Path $resSrc) {
        $resDest = Join-Path $TargetDir "resources"
        New-Item -ItemType Directory -Path $resDest -Force | Out-Null
        $rcArgs = @(
            $resSrc,
            $resDest,
            "/E",
            "/NJH", "/NJS", "/NDL", "/NC", "/NS", "/NP",
            "/XF", "*.pso", "*.blend1", "*.xcf", "*.aseprite",
            "/XD", ".cache", "cache"
        )
        & robocopy @rcArgs | Out-Null
    }

    # 3. EngineResources/
    $engineResSrc = Join-Path $RootDir "project\IrufemiEngine\EngineResources"
    if (Test-Path $engineResSrc) {
        $engineResDest = Join-Path $TargetDir "EngineResources"
        New-Item -ItemType Directory -Path $engineResDest -Force | Out-Null
        $rcArgs = @(
            $engineResSrc,
            $engineResDest,
            "/E",
            "/NJH", "/NJS", "/NDL", "/NC", "/NS", "/NP",
            "/XF", "*.pso",
            "/XD", ".cache", "cache"
        )
        & robocopy @rcArgs | Out-Null
    }

    # 4. プレイ用ドキュメント（Application_solo から正規の README.md を同梱）
    $appReadmeSrc = Join-Path $RootDir "project\Application_solo\README.md"
    if (Test-Path $appReadmeSrc) {
        Copy-Item -Path $appReadmeSrc -Destination (Join-Path $TargetDir "README.md") -Force
        Write-Info "README.md を同梱しました。"
    } else {
        Write-Warn "README.md が見つかりませんでした (project\Application_solo\README.md)"
    }

    $fileSizeMB = Get-FolderSizeMB $TargetDir
    Write-Success "実行ファイルパッケージ作成完了!"
    Write-Host "   出力先: $TargetDir" -ForegroundColor White
    Write-Host "   サイズ: $fileSizeMB MB`n" -ForegroundColor Green
}

# -----------------------------------------------------------------------------
# メインメニュー
# -----------------------------------------------------------------------------
if ($Mode -eq "Menu") {
    Write-Header "就職活動・技術審査用 パッケージ生成ツール"
    Write-Host "作成したいパッケージを選択してください:" -ForegroundColor Yellow
    Write-Host "  [1] ソースコード提出用パッケージ (VSプロジェクト + .github + ドキュメント)" -ForegroundColor Cyan
    Write-Host "  [2] 実行ファイル提出用パッケージ (遊べるExe + リソース + README.md)" -ForegroundColor Cyan
    Write-Host "  [3] 両方一括生成" -ForegroundColor Cyan
    Write-Host "  [Q] 終了`n" -ForegroundColor Gray

    $choice = Read-Host "選択 (1/2/3/Q)"
    switch ($choice) {
        "1" { $Mode = "Source" }
        "2" { $Mode = "Build" }
        "3" { $Mode = "All" }
        default {
            Write-Host "処理をキャンセルしました。" -ForegroundColor Gray
            exit 0
        }
    }
}

switch ($Mode) {
    "Source" {
        Create-SourcePackage
    }
    "Build" {
        Create-PlayablePackage
    }
    "All" {
        Create-SourcePackage
        Create-PlayablePackage
    }
}

Write-Header "すべての処理が完了しました"
Write-Host "出力先フォルダ: $OutputDir" -ForegroundColor White
