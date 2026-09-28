param (
    [ValidateSet("Source", "Build", "All", "Menu")]
    [string]$Mode = "Menu"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# ルートディレクトリの特定
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
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
function Create-SourcePackage {
    Write-Header "ソースコード提出用パッケージの作成を開始します"

    $ZipFileName = "GravityShooter_SourceCode_$Timestamp.zip"
    $ZipPath = Join-Path $OutputDir $ZipFileName
    $TempStaging = Join-Path $env:TEMP "Submission_Source_$Timestamp"

    if (Test-Path $TempStaging) {
        Remove-Item -Path $TempStaging -Recurse -Force
    }
    New-Item -ItemType Directory -Path $TempStaging | Out-Null

    Write-Info "ファイルを抽出・一時ステージング中..."

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

        $destPath = Join-Path $TempStaging $item

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

    Write-Info "ZIPアーカイブに圧縮中..."
    if (Test-Path $ZipPath) {
        Remove-Item -Path $ZipPath -Force
    }
    Compress-Archive -Path "$TempStaging\*" -DestinationPath $ZipPath -CompressionLevel Optimal

    Remove-Item -Path $TempStaging -Recurse -Force

    $fileSizeMB = [math]::Round((Get-Item $ZipPath).Length / 1MB, 2)
    Write-Success "ソースコードパッケージ作成完了!"
    Write-Host "   ファイル: $ZipPath" -ForegroundColor White
    Write-Host "   サイズ  : $fileSizeMB MB`n" -ForegroundColor Green
}

# -----------------------------------------------------------------------------
# 2. 実行ファイル（プレイ用）パッケージ作成
# -----------------------------------------------------------------------------
function Create-PlayablePackage {
    Write-Header "実行ファイル（プレイ用）パッケージの作成を開始します"

    $ExeReleasePath = Join-Path $RootDir "generated\outputs\Release\Application_solo.exe"
    $BuildOutputDir = Join-Path $RootDir "generated\outputs\Release"

    if (-not (Test-Path $ExeReleasePath)) {
        Write-Warn "Release構成のビルド成果物が見つかりません:"
        Write-Warn $ExeReleasePath
        Write-Host "先に Visual Studio で [Release] 構成でビルドを行ってください。`n" -ForegroundColor Yellow
        return
    }

    $ZipFileName = "GravityShooter_PlayableBuild_$Timestamp.zip"
    $ZipPath = Join-Path $OutputDir $ZipFileName
    $TempStaging = Join-Path $env:TEMP "Submission_Build_$Timestamp"

    if (Test-Path $TempStaging) {
        Remove-Item -Path $TempStaging -Recurse -Force
    }
    New-Item -ItemType Directory -Path $TempStaging | Out-Null

    Write-Info "実行ファイルおよびリソースをステージング中..."

    # 1. Exe本体
    Copy-Item -Path $ExeReleasePath -Destination (Join-Path $TempStaging "GravityShooter.exe") -Force

    # 2. DLL
    Get-ChildItem -Path $BuildOutputDir -Filter "*.dll" | ForEach-Object {
        Copy-Item -Path $_.FullName -Destination $TempStaging -Force
    }

    # 3. resources/
    $resSrc = Join-Path $RootDir "project\Application_solo\resources"
    if (Test-Path $resSrc) {
        $resDest = Join-Path $TempStaging "resources"
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

    # 4. EngineResources/
    $engineResSrc = Join-Path $RootDir "project\IrufemiEngine\EngineResources"
    if (Test-Path $engineResSrc) {
        $engineResDest = Join-Path $TempStaging "EngineResources"
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

    # 5. 説明書PDF
    $pdfSrc = Join-Path $RootDir "TL1\LE3B_15_スエヒロ_コウイチ_プログラム説明書.pdf"
    if (Test-Path $pdfSrc) {
        Copy-Item -Path $pdfSrc -Destination (Join-Path $TempStaging "プログラム説明書.pdf") -Force
        Write-Info "説明書PDFを同梱しました。"
    }

    # 6. README.txt
    $readmeLines = @(
        "================================================================================",
        "  Gravity Shooter (仮) - プレイ用パッケージ",
        "================================================================================",
        "",
        "■ 起動方法",
        "  同梱の「GravityShooter.exe」をダブルクリックして起動してください。",
        "",
        "■ 基本操作方法",
        "  [キーボード / マウス]",
        "    - 移動          : W / A / S / D",
        "    - 視点・照準    : マウス移動",
        "    - 攻撃 / 投擲   : マウス左クリック",
        "    - 重力引き寄せ  : マウス右クリック",
        "    - ポーズ        : ESC",
        "",
        "  [ゲームパッド (推奨)]",
        "    - 移動          : 左スティック",
        "    - 照準          : 右スティック",
        "    - 攻撃 / 投擲   : RT (R2)",
        "    - 重力引き寄せ  : LT (L2)",
        "    - ポーズ        : START / OPTIONS",
        "",
        "■ 動作環境",
        "  - OS: Windows 10 / 11 (64bit)",
        "  - DirectX: DirectX 12 対応グラフィックスカード",
        "  - 画面解像度: 1920x1080 推奨",
        "",
        "■ 補足資料",
        "  技術的な詳細や設計思想につきましては、同梱の「プログラム説明書.pdf」をご参照ください。",
        "================================================================================"
    )
    $readmeText = $readmeLines -join "`r`n"
    [System.IO.File]::WriteAllText((Join-Path $TempStaging "README.txt"), $readmeText, [System.Text.Encoding]::UTF8)

    Write-Info "ZIPアーカイブに圧縮中..."
    if (Test-Path $ZipPath) {
        Remove-Item -Path $ZipPath -Force
    }
    Compress-Archive -Path "$TempStaging\*" -DestinationPath $ZipPath -CompressionLevel Optimal

    Remove-Item -Path $TempStaging -Recurse -Force

    $fileSizeMB = [math]::Round((Get-Item $ZipPath).Length / 1MB, 2)
    Write-Success "実行ファイルパッケージ作成完了!"
    Write-Host "   ファイル: $ZipPath" -ForegroundColor White
    Write-Host "   サイズ  : $fileSizeMB MB`n" -ForegroundColor Green
}

# -----------------------------------------------------------------------------
# メインメニュー
# -----------------------------------------------------------------------------
if ($Mode -eq "Menu") {
    Write-Header "就職活動・技術審査用 パッケージ生成ツール"
    Write-Host "作成したいパッケージを選択してください:" -ForegroundColor Yellow
    Write-Host "  [1] ソースコード提出用パッケージ (VSプロジェクト + .github + ドキュメント)" -ForegroundColor Cyan
    Write-Host "  [2] 実行ファイル提出用パッケージ (遊べるExe + リソース + 説明書PDF)" -ForegroundColor Cyan
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
