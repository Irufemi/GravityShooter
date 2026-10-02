param (
    [ValidateSet("Source", "Build", "All", "Menu")]
    [string]$Mode = "Menu",
    [string]$ScriptDir = "",
    [switch]$SkipBuild
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

function Get-FolderSizeMB {
    param([string]$Path)
    $bytes = (Get-ChildItem -Path $Path -Recurse -File -Force -ErrorAction SilentlyContinue | Measure-Object -Property Length -Sum).Sum
    if (-not $bytes) { return 0 }
    return [math]::Round($bytes / 1MB, 2)
}

# -----------------------------------------------------------------------------
# 0. ビルドパイプライン（Visual Studio 2026 MSBuild）
# -----------------------------------------------------------------------------
function Get-MSBuildPath {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        throw "vswhere.exe が見つかりません。Visual Studio 2026 がインストールされているか確認してください。"
    }
    $msbuild = & $vswhere -latest -prerelease -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe | Select-Object -First 1
    if (-not $msbuild -or -not (Test-Path $msbuild)) {
        throw "MSBuild.exe が見つかりませんでした。"
    }
    return $msbuild
}

function Invoke-ProjectBuild {
    param(
        [switch]$Rebuild
    )
    Write-Header "Application_solo (Release) のビルドを開始します"

    $msbuildPath = Get-MSBuildPath
    Write-Info "使用する MSBuild: $msbuildPath"

    $slnxPath = Join-Path $RootDir "project\Irufemi.slnx"
    if (-not (Test-Path $slnxPath)) {
        throw "ソリューションファイルが見つかりません: $slnxPath"
    }

    $target = if ($Rebuild) { "Application_solo:Rebuild" } else { "Application_solo:Build" }

    $logDir = Join-Path $RootDir "logs\build"
    if (-not (Test-Path $logDir)) { New-Item -ItemType Directory -Path $logDir -Force | Out-Null }
    $buildLog = Join-Path $logDir "package_build_Release_$Timestamp.log"

    $buildArgs = @(
        $slnxPath,
        "-restore",                         # 新規クローン環境での NuGet パッケージ (freetype2) 自動復元
        "/t:$target",                       # ソリューション依存関係（エンジン層 .lib）を先に解決してビルド
        "/p:Configuration=Release",
        "/p:Platform=x64",
        "/m",
        "/v:minimal",
        "/fl",
        "/flp:logfile=$buildLog;Encoding=UTF-8;verbosity=normal"
    )

    Write-Info "ソリューション依存関係（エンジン層 + アプリ層）を解決してビルド中..."
    Write-Info "ログ出力先: $buildLog"
    $startTime = Get-Date

    & $msbuildPath $buildArgs

    if ($LASTEXITCODE -ne 0) {
        Write-Host "`n[ERROR] ビルドに失敗しました。ログを確認してください: $buildLog" -ForegroundColor Red
        return $false
    }

    $elapsed = (Get-Date) - $startTime
    Write-Success ("ビルド成功! 所要時間: {0:F1} 秒" -f $elapsed.TotalSeconds)
    return $true
}

# -----------------------------------------------------------------------------
# 1. ソースコード提出用パッケージ作成（動的スキャン方式: アプローチA）
# -----------------------------------------------------------------------------
function Create-SourcePackage {
    Write-Header "ソースコード提出用パッケージの作成を開始します (動的スキャン方式)"

    $TargetDir = Join-Path $OutputDir "GravityShooter_SourceCode"

    if (Test-Path $TargetDir) {
        Write-Info "既存の出力フォルダをクリーンアップ中..."
        Remove-Item -Path $TargetDir -Recurse -Force
    }
    New-Item -ItemType Directory -Path $TargetDir | Out-Null

    Write-Info "ルートアイテムを動的に走査中..."

    # ルート直下で絶対に提出物に含めないシステムフォルダ・作業フォルダ
    $ExcludeRootNames = @(
        ".git",
        ".agents",
        ".vs",
        ".vscode",
        "generated",
        "Binaries",
        "gakkousuraido",
        "_Submission",
        "logs",
        "scratch"
    )

    # project 配下等で除外する中間ディレクトリ名
    # （※ assimp/lib/Release や resources/ を破壊しないよう、generated/obj 等のみを確実に除外）
    $ExcludeDirs = @(
        ".vs",
        ".vscode",
        "generated",
        "Binaries",
        "obj",
        "bin",
        ".cache",
        "cache",
        "__pycache__",
        ".venv",
        "venv",
        "logs",
        "Logs",
        "Dumps",
        "asset_src"
    )

    # 除外ファイル拡張子
    # （※ *.obj は 3Dモデルデータを保護するため絶対に除外しない。中間 obj は generated フォルダごと除外済み）
    $ExcludeExtensions = @(
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

    $rootItems = Get-ChildItem -Path $RootDir -Force
    foreach ($item in $rootItems) {
        if ($item.Name -in $ExcludeRootNames) {
            continue
        }

        $srcPath = $item.FullName
        $destPath = Join-Path $TargetDir $item.Name

        if ($item.PSIsContainer) {
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
    param(
        [switch]$SkipBuild
    )
    Write-Header "実行ファイル（プレイ用）パッケージの作成を開始します"

    $ExeReleasePath = Join-Path $RootDir "generated\outputs\Release\Application_solo.exe"

    if (-not $SkipBuild) {
        $buildOk = Invoke-ProjectBuild
        if (-not $buildOk) {
            Write-Warn "ビルドに失敗したため、プレイ用パッケージの作成を中断しました。"
            return
        }
    } else {
        if (-not (Test-Path $ExeReleasePath)) {
            Write-Warn "Release構成のビルド成果物が見つかりません:"
            Write-Warn $ExeReleasePath
            Write-Host "ビルドを実行してからパッケージ化するか、Visual Studio でビルドを行ってください。`n" -ForegroundColor Yellow
            return
        }
        Write-Info "既存のビルド成果物を使用します: $ExeReleasePath"
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
    }

    # 5. 審査員向け資料（ポートフォリオPDF & プログラム説明書PDFの同梱）
    $docPortfolio = Join-Path $RootDir "docs\LE3B_15_スエヒロ_コウイチ_ポートフォリオ.pdf"
    if (Test-Path $docPortfolio) {
        Copy-Item -Path $docPortfolio -Destination (Join-Path $TargetDir "LE3B_15_スエヒロ_コウイチ_ポートフォリオ.pdf") -Force
        Write-Info "ポートフォリオ.pdf を同梱しました。"
    }
    $docManual = Join-Path $RootDir "TL1\LE3B_15_スエヒロ_コウイチ_プログラム説明書.pdf"
    if (Test-Path $docManual) {
        Copy-Item -Path $docManual -Destination (Join-Path $TargetDir "LE3B_15_スエヒロ_コウイチ_プログラム説明書.pdf") -Force
        Write-Info "プログラム説明書.pdf を同梱しました。"
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
    Write-Header "就職活動・技術審査用 パッケージ生成ツール (Pipeline Mode)"
    Write-Host "作成したいパッケージを選択してください:" -ForegroundColor Yellow
    Write-Host "  [1] プレイ用パッケージ（最新コードを Release ビルド して完全生成） ★推奨" -ForegroundColor Cyan
    Write-Host "  [2] プレイ用パッケージ（既存の Exe を使用して即座に生成）" -ForegroundColor Cyan
    Write-Host "  [3] ソースコード提出用パッケージ (動的スキャン方式・完全整合版)" -ForegroundColor Cyan
    Write-Host "  [4] 両方一括生成（Release ビルド後に両パッケージを作成） ★審査提出時推奨" -ForegroundColor Cyan
    Write-Host "  [Q] 終了`n" -ForegroundColor Gray

    $choice = Read-Host "選択 (1/2/3/4/Q)"
    switch ($choice) {
        "1" { 
            Create-PlayablePackage -SkipBuild:$false
        }
        "2" { 
            Create-PlayablePackage -SkipBuild:$true
        }
        "3" { 
            Create-SourcePackage 
        }
        "4" { 
            Create-PlayablePackage -SkipBuild:$false
            Create-SourcePackage
        }
        default {
            Write-Host "処理をキャンセルしました。" -ForegroundColor Gray
            exit 0
        }
    }
} else {
    switch ($Mode) {
        "Source" {
            Create-SourcePackage
        }
        "Build" {
            Create-PlayablePackage -SkipBuild:$SkipBuild
        }
        "All" {
            Create-PlayablePackage -SkipBuild:$SkipBuild
            Create-SourcePackage
        }
    }
}

Write-Header "すべての処理が完了しました"
Write-Host "出力先フォルダ: $OutputDir" -ForegroundColor White

