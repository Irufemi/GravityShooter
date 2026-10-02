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
    if ((Test-Path variable:PSScriptRoot) -and $PSScriptRoot) {
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

    $target = if ($Rebuild) { "Application_solo:Rebuild" } else { "Application_solo" }

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
# 1. スマートアセットクッカー（手法B: 依存関係自動抽出）
# -----------------------------------------------------------------------------
function Get-CookedAssetMap {
    param([string]$AppResDir)

    Write-Info "アセット依存関係を解析中 (Release本編C++ ＋ シーンJSON ＋ マテリアル)..."

    # 1. 探索対象テキストの収集（※ DebugScene.*, DebugUI.*, Editor/ はデバッグ専用のため除外）
    $sourceFiles = Get-ChildItem -Path "$RootDir\project\Application_solo" -Recurse -File -Force | Where-Object {
        $_.Extension -in @(".cpp", ".h", ".json", ".hlsl") -and 
        $_.FullName -notmatch '\\\.cache\\' -and 
        $_.FullName -notmatch '\\Editor\\'
    }
    $engineFiles = Get-ChildItem -Path "$RootDir\project\IrufemiEngine" -Recurse -File -Force | Where-Object {
        $_.Extension -in @(".cpp", ".h", ".hlsl") -and 
        $_.Name -notmatch '^Debug(Scene|UI)\.'
    }

    $allSearchFiles = @($sourceFiles) + @($engineFiles)
    $sourceTexts = @()
    foreach ($sf in $allSearchFiles) {
        $sourceTexts += [System.IO.File]::ReadAllText($sf.FullName, [System.Text.Encoding]::UTF8)
    }

    $allAssets = Get-ChildItem -Path $AppResDir -Recurse -File -Force | Where-Object {
        $_.FullName -notmatch '\\\.cache\\' -and 
        $_.FullName -notmatch '\\shaders\\compiled\\'
    }

    # 強制セーフティブラックリスト（いかなる参照があっても配布用パッケージには含めない）
    $hardBlacklistPatterns = @(
        '\\screenshots\\',
        '\\model\\sample\\',
        '\\model\\CG4\\',
        '\\shaders\\generated\\',
        '\.blend$',
        '\.blend1$',
        '\.xcf$',
        '\.aseprite$',
        '\\\.cache\\'
    )

    $validRelPaths = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
    $referencedObjFiles = @()

    foreach ($asset in $allAssets) {
        $relPath = $asset.FullName.Substring($AppResDir.Length + 1).Replace('\', '/')

        $isBlacklisted = $false
        foreach ($pattern in $hardBlacklistPatterns) {
            if ($asset.FullName -match $pattern) {
                $isBlacklisted = $true
                break
            }
        }
        if ($isBlacklisted) { continue }

        # コアシステムアセット（常に含めるもの）
        if ($relPath -match '^(config|GameData)/' -or $relPath -eq 'fonts/toro_glitch.otf' -or $relPath -match '^scenes/.*\.json$' -or $relPath -match '^prefabs/.*\.json$') {
            [void]$validRelPaths.Add($relPath)
            continue
        }

        $filename = $asset.Name
        $basename = [System.IO.Path]::GetFileNameWithoutExtension($asset.Name)

        # 一次参照判定（C++ / JSON 全文走査）
        $isReferenced = $false
        foreach ($text in $sourceTexts) {
            if ($text.IndexOf($relPath, [System.StringComparison]::OrdinalIgnoreCase) -ge 0 -or
                ($filename.Length -gt 4 -and $text.IndexOf($filename, [System.StringComparison]::OrdinalIgnoreCase) -ge 0) -or
                ($basename.Length -ge 6 -and $text.IndexOf("""$basename""", [System.StringComparison]::OrdinalIgnoreCase) -ge 0)) {
                $isReferenced = $true
                break
            }
        }

        if ($isReferenced) {
            [void]$validRelPaths.Add($relPath)
            if ($asset.Extension.ToLower() -eq ".obj") {
                $referencedObjFiles += $asset
            }
        }
    }

    # 二次参照の解決 (.obj -> .mtl -> テクスチャ)
    foreach ($objFile in $referencedObjFiles) {
        $objDir = $objFile.DirectoryName
        $objContent = [System.IO.File]::ReadAllText($objFile.FullName, [System.Text.Encoding]::UTF8)

        $mtlMatches = [regex]::Matches($objContent, '(?im)^\s*mtllib\s+(.+)$')
        foreach ($match in $mtlMatches) {
            $mtlName = $match.Groups[1].Value.Trim()
            $mtlPath = Join-Path $objDir $mtlName
            if (Test-Path $mtlPath) {
                $relMtl = $mtlPath.Substring($AppResDir.Length + 1).Replace('\', '/')
                [void]$validRelPaths.Add($relMtl)

                $mtlContent = [System.IO.File]::ReadAllText($mtlPath, [System.Text.Encoding]::UTF8)
                $texMatches = [regex]::Matches($mtlContent, '(?im)^\s*map_[a-z0-9_]+\s+(?:-[^\s]+\s+)*(?:.*\\)?([^\s\r\n]+\.(?:png|jpg|jpeg|dds|tga|bmp))')
                foreach ($tMatch in $texMatches) {
                    $texName = $tMatch.Groups[1].Value.Trim()
                    $texPath = Join-Path $objDir $texName
                    if (Test-Path $texPath) {
                        $relTex = $texPath.Substring($AppResDir.Length + 1).Replace('\', '/')
                        [void]$validRelPaths.Add($relTex)
                    }
                }
            }
        }
    }

    Write-Info ("クック完了: {0} 個の正当な製品アセットを特定しました。" -f $validRelPaths.Count)
    return $validRelPaths
}

# -----------------------------------------------------------------------------
# 2. ソースコード提出用パッケージ作成（クリーン開発モード: アプローチA）
# -----------------------------------------------------------------------------
function Create-SourcePackage {
    Write-Header "ソースコード提出用パッケージの作成を開始します (クリーン開発モード)"

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

    # project 配下等で除外する中間・作業ディレクトリ名
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
        "asset_src",
        "screenshots"                       # 開発用スクショを除外
    )

    # 除外ファイル拡張子
    # （※ 3Dモデル *.obj は保護、DCC編集元データ *.blend や中間ファイルを除外）
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
        "*.blend",                          # Blender元データを除外
        "*.blend1",
        "*.xcf",
        "*.aseprite",
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
# 3. 実行ファイル（プレイ用）パッケージ作成（完全スマートクックモード）
# -----------------------------------------------------------------------------
function Create-PlayablePackage {
    param(
        [switch]$SkipBuild
    )
    Write-Header "実行ファイル（プレイ用）パッケージの作成を開始します (スマートクック適用)"

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

    # 2. resources/（スマートクック転送）
    $resSrc = Join-Path $RootDir "project\Application_solo\resources"
    if (Test-Path $resSrc) {
        $resDest = Join-Path $TargetDir "resources"
        New-Item -ItemType Directory -Path $resDest -Force | Out-Null

        $cookedMap = Get-CookedAssetMap -AppResDir $resSrc
        Write-Info "クック済みアセットを配置中..."

        foreach ($rel in $cookedMap) {
            $srcFile = Join-Path $resSrc $rel
            $destFile = Join-Path $resDest $rel
            $destSubDir = Split-Path -Parent $destFile
            if (-not (Test-Path $destSubDir)) {
                New-Item -ItemType Directory -Path $destSubDir -Force | Out-Null
            }
            if (Test-Path $srcFile) {
                Copy-Item -Path $srcFile -Destination $destFile -Force
            }
        }
    }

    # 3. EngineResources/（グラフィックス基盤のため完全保護転送）
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
    Write-Success "実行ファイルパッケージ作成完了! (スマートクック適用済み)"
    Write-Host "   出力先: $TargetDir" -ForegroundColor White
    Write-Host "   サイズ: $fileSizeMB MB`n" -ForegroundColor Green
}

# -----------------------------------------------------------------------------
# メインメニュー
# -----------------------------------------------------------------------------
if ($Mode -eq "Menu") {
    Write-Header "就職活動・技術審査用 パッケージ生成ツール (Cook & Pipeline Mode)"
    Write-Host "作成したいパッケージを選択してください:" -ForegroundColor Yellow
    Write-Host "  [1] プレイ用パッケージ（最新コードを Release ビルド ＋ スマートクックで極限軽量化） ★推奨" -ForegroundColor Cyan
    Write-Host "  [2] プレイ用パッケージ（既存 Exe を使用 ＋ スマートクック生成）" -ForegroundColor Cyan
    Write-Host "  [3] ソースコード提出用パッケージ (クリーン開発モード・完全整合版)" -ForegroundColor Cyan
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


