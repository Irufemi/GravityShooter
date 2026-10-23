param (
    [ValidateSet("Source", "Build", "All", "Sega", "Menu")]
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
        $ScriptDir = Join-Path $PWD.Path "scripts"
    }
}
$RootDir = if (Test-Path (Join-Path $ScriptDir "..\project")) {
    (Resolve-Path "$ScriptDir\..").Path
} elseif (Test-Path (Join-Path $PWD.Path "project")) {
    $PWD.Path
} else {
    (Resolve-Path "$ScriptDir\..").Path
}
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
# 堅牢なファイル操作・アトミック置換基盤（大手スタジオRelease Engineering準拠）
# -----------------------------------------------------------------------------
function Remove-DirectorySafe {
    param([string]$Path)
    if (-not (Test-Path $Path)) { return }
    Get-ChildItem -Path $Path -File -Recurse -Force -ErrorAction SilentlyContinue | ForEach-Object {
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

# -----------------------------------------------------------------------------
# 構成別スマート・スモークテスト（出荷前自己検証）
# -----------------------------------------------------------------------------
function Test-PackageIntegrity {
    param(
        [string]$PackageDir,
        [string]$Configuration # "Release", "Editor", "Source"
    )
    Write-Info "パッケージの整合性検証（スモークテスト: $Configuration 構成）を実行中..."

    if ($Configuration -in @("Release", "Editor")) {
        $exeName = if ($Configuration -eq "Editor") { "GravityShooter_Editor.exe" } else { "GravityShooter.exe" }
        $exePath = Join-Path $PackageDir $exeName
        if (-not (Test-Path $exePath) -or (Get-Item $exePath).Length -lt 100KB) {
            throw "整合性チェック失敗: 実行可能バイナリが存在しないか不正です: $exeName"
        }

        # 起動必須コアアセットの存在確認
        $coreAssets = @(
            "resources\scenes\InGame.json",
            "resources\fonts\toro_glitch.otf"
        )
        foreach ($asset in $coreAssets) {
            $assetPath = Join-Path $PackageDir $asset
            if (-not (Test-Path $assetPath) -or (Get-Item $assetPath).Length -eq 0) {
                throw "整合性チェック失敗: 起動必須アセットが欠落しています: $asset"
            }
        }
    }

    if ($Configuration -eq "Release") {
        # Release構成: オフラインコンパイル済みシェーダーバイナリ (.cso) の完備チェック
        $csoDir = Join-Path $PackageDir "resources\shaders\compiled"
        $csoFiles = Get-ChildItem -Path $csoDir -Filter "*.cso" -ErrorAction SilentlyContinue
        if (-not $csoFiles -or $csoFiles.Count -eq 0) {
            throw "整合性チェック失敗: オフラインシェーダー (.cso) が生成されていません。"
        }
        Write-Success ("シェーダーバイナリ整合性OK: {0} 個の .cso を確認しました。" -f $csoFiles.Count)
    }

    if ($Configuration -eq "Editor") {
        # Editor構成: 専用設定ファイル (imgui_editor.ini) の存在確認
        $iniPath = Join-Path $PackageDir "imgui_editor.ini"
        if (-not (Test-Path $iniPath)) {
            Write-Warn "エディタ設定ファイル (imgui_editor.ini) が見当たりません（デフォルト配置で起動します）。"
        }
    }

    if ($Configuration -eq "Source") {
        # Source構成: ソリューションファイル等の健全性チェック
        $slnxPath = Join-Path $PackageDir "project\Irufemi.slnx"
        if (-not (Test-Path $slnxPath)) {
            throw "整合性チェック失敗: ソリューションファイルが存在しません: $slnxPath"
        }
    }

    Write-Success "[$Configuration] 整合性検証クリア: 全ての必須要素が正常に揃っています。"
}

# -----------------------------------------------------------------------------
# ビルドマニフェスト（BuildManifest.json）生成
# -----------------------------------------------------------------------------
function New-BuildManifest {
    param(
        [string]$PackageDir,
        [string]$Configuration
    )
    $commitHash = (git rev-parse HEAD 2>$null)
    $branchName = (git rev-parse --abbrev-ref HEAD 2>$null)
    $exeName = if ($Configuration -eq "Editor") { "GravityShooter_Editor.exe" } else { "GravityShooter.exe" }
    $exePath = Join-Path $PackageDir $exeName
    $exeHash = if (Test-Path $exePath) { (Get-FileHash -Path $exePath -Algorithm SHA256).Hash } else { "N/A" }

    $manifest = [ordered]@{
        "ApplicationName"    = "GravityShooter"
        "Engine"             = "IrufemiEngine"
        "Configuration"      = $Configuration
        "BuildTimestamp"     = (Get-Date -Format "yyyy-MM-dd HH:mm:ss")
        "GitCommit"          = if ($commitHash) { "$commitHash" } else { "N/A" }
        "GitBranch"          = if ($branchName) { "$branchName" } else { "N/A" }
        "ExecutableSHA256"   = $exeHash
        "PackagingTool"      = "Irufemi Packaging Pipeline (Release Engineering Edition)"
    }
    $manifestJson = $manifest | ConvertTo-Json -Depth 4
    [System.IO.File]::WriteAllText((Join-Path $PackageDir "BuildManifest.json"), $manifestJson, [System.Text.Encoding]::UTF8)
    Write-Success "ビルドマニフェスト (BuildManifest.json) を生成しました。"
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

function Get-DxcPath {
    $dxcCmd = Get-Command dxc.exe -ErrorAction SilentlyContinue
    if ($dxcCmd) { return $dxcCmd.Source }
    $sdkBins = Get-ChildItem -Path "${env:ProgramFiles(x86)}\Windows Kits\10\bin" -Filter "dxc.exe" -Recurse -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match '\\x64\\dxc\.exe$' } | Sort-Object FullName -Descending
    if ($sdkBins -and $sdkBins.Count -gt 0) {
        return $sdkBins[0].FullName
    }
    return "dxc.exe"
}

function Invoke-ProjectBuild {
    param(
        [string]$Configuration = "Release",
        [switch]$Rebuild
    )
    Write-Header "Application_solo ($Configuration) のビルドを開始します"

    $msbuildPath = Get-MSBuildPath
    Write-Info "使用する MSBuild: $msbuildPath"

    $slnxPath = Join-Path $RootDir "project\Irufemi.slnx"
    if (-not (Test-Path $slnxPath)) {
        throw "ソリューションファイルが見つかりません: $slnxPath"
    }

    $target = if ($Rebuild) { "Application_solo:Rebuild" } else { "Application_solo" }

    $logDir = Join-Path $RootDir "logs\build"
    if (-not (Test-Path $logDir)) { New-Item -ItemType Directory -Path $logDir -Force | Out-Null }
    $buildLog = Join-Path $logDir "package_build_${Configuration}_$Timestamp.log"

    $buildArgs = @(
        $slnxPath,
        "-restore",                         # 新規クローン環境での NuGet パッケージ (freetype2) 自動復元
        "/t:$target",                       # ソリューション依存関係（エンジン層 .lib）を先に解決してビルド
        "/p:Configuration=$Configuration",
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
    param(
        [string]$CustomOutputDir = ""
    )
    Write-Header "ソースコード提出用パッケージの作成を開始します (クリーン開発モード)"

    $TargetDir = if ($CustomOutputDir) { $CustomOutputDir } else { Join-Path $OutputDir "GravityShooter_SourceCode" }
    $stagingDir = "$TargetDir._staging_$Timestamp"

    Remove-DirectorySafe -Path $stagingDir
    New-Item -ItemType Directory -Path $stagingDir | Out-Null

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
        "scratch",
        "backup"
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
        "*.exe",                            # 実行ファイルを確実に除外
        "*.dll",                            # バイナリDLLを除外
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
        $destPath = Join-Path $stagingDir $item.Name

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

    # 整合性検証 & ビルドマニフェスト生成（ステージング内で検証）
    Test-PackageIntegrity -PackageDir $stagingDir -Configuration "Source"
    New-BuildManifest -PackageDir $stagingDir -Configuration "Source"

    # アトミック置換（安全スワップ）
    Remove-DirectorySafe -Path $TargetDir
    Move-DirectorySafe -Source $stagingDir -Destination $TargetDir

    $fileSizeMB = Get-FolderSizeMB $TargetDir
    Write-Success "ソースコードパッケージ作成完了! (アトミック置換適用)"
    Write-Host "   出力先: $TargetDir" -ForegroundColor White
    Write-Host "   サイズ: $fileSizeMB MB`n" -ForegroundColor Green
}

# -----------------------------------------------------------------------------
# 3. 実行ファイル（プレイ用）パッケージ作成（完全スマートクックモード）
# -----------------------------------------------------------------------------
function Create-PlayablePackage {
    param(
        [switch]$SkipBuild,
        [string]$CustomOutputDir = ""
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

    $TargetDir = if ($CustomOutputDir) { $CustomOutputDir } else { Join-Path $OutputDir "GravityShooter_PlayableBuild" }
    $stagingDir = "$TargetDir._staging_$Timestamp"

    Remove-DirectorySafe -Path $stagingDir
    New-Item -ItemType Directory -Path $stagingDir | Out-Null

    Write-Info "実行ファイルおよびリソースを配置中..."

    # 1. Exe本体（完全静的リンクのため単体で動作）
    Copy-Item -Path $ExeReleasePath -Destination (Join-Path $stagingDir "GravityShooter.exe") -Force

    # 2. resources/（スマートクック転送）
    $resSrc = Join-Path $RootDir "project\Application_solo\resources"
    if (Test-Path $resSrc) {
        $resDest = Join-Path $stagingDir "resources"
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
        $engineResDest = Join-Path $stagingDir "EngineResources"
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

    # 4. オフラインシェーダーコンパイル（CompileShaders.bat による .cso 直接配置）
    $compiledShaderDest = Join-Path $stagingDir "resources\shaders\compiled"
    if (-not (Test-Path $compiledShaderDest)) {
        New-Item -ItemType Directory -Path $compiledShaderDest -Force | Out-Null
    }
    $compileBat = Join-Path $RootDir "project\CompileShaders.bat"
    $engineShaderSrc = Join-Path $RootDir "project\IrufemiEngine\EngineResources\shaders"
    $appShaderSrc = Join-Path $RootDir "project\Application_solo\resources\shaders"
    $dxcPath = Get-DxcPath

    Write-Info "HLSLシェーダーを事前コンパイル中 (.cso を直接生成・配置)..."
    & cmd.exe /c "`"$compileBat`" `"$engineShaderSrc`" `"$compiledShaderDest`" `"$dxcPath`"" | Out-Null
    & cmd.exe /c "`"$compileBat`" `"$appShaderSrc`" `"$compiledShaderDest`" `"$dxcPath`"" | Out-Null

    $csoCount = (Get-ChildItem -Path $compiledShaderDest -Filter "*.cso" -ErrorAction SilentlyContinue).Count
    Write-Success ("シェーダーバイナリ直接配置完了: {0} 個の .cso を生成しました。" -f $csoCount)

    # 5. プレイ用ドキュメント（Application_solo から正規の README.md を同梱）
    $appReadmeSrc = Join-Path $RootDir "project\Application_solo\README.md"
    if (Test-Path $appReadmeSrc) {
        Copy-Item -Path $appReadmeSrc -Destination (Join-Path $stagingDir "README.md") -Force
        Write-Info "README.md を同梱しました。"
    }


    # 6. アセット最適化実績の算出
    $rawAssetSize = Get-FolderSizeMB (Join-Path $RootDir "project\Application_solo\resources")
    $cookedAssetSize = Get-FolderSizeMB (Join-Path $stagingDir "resources")
    $savedMB = [math]::Round([math]::Max(0, $rawAssetSize - $cookedAssetSize), 2)
    $savePercent = if ($rawAssetSize -gt 0) { [math]::Round(($savedMB / $rawAssetSize) * 100, 1) } else { 0 }

    # 7. 整合性検証（スモークテスト）& ビルドマニフェスト生成（ステージング内で検証）
    Test-PackageIntegrity -PackageDir $stagingDir -Configuration "Release"
    New-BuildManifest -PackageDir $stagingDir -Configuration "Release"

    # 8. アトミック置換（安全スワップ）
    Remove-DirectorySafe -Path $TargetDir
    Move-DirectorySafe -Source $stagingDir -Destination $TargetDir

    $fileSizeMB = Get-FolderSizeMB $TargetDir
    Write-Success "実行ファイルパッケージ作成完了! (スマートクック＆アトミック置換適用)"
    Write-Host "   出力先: $TargetDir" -ForegroundColor White
    Write-Host "   パッケージサイズ: $fileSizeMB MB" -ForegroundColor Green
    Write-Host "   アセット最適化実績: $rawAssetSize MB -> $cookedAssetSize MB (約 $savedMB MB [$savePercent%] 削減)`n" -ForegroundColor Cyan
}

# -----------------------------------------------------------------------------
# 4. 自作エディタモード（EditorBuild）パッケージ作成
# -----------------------------------------------------------------------------
function Create-EditorPackage {
    param(
        [switch]$SkipBuild,
        [string]$CustomOutputDir = ""
    )
    Write-Header "自作エディタモード（EditorBuild）パッケージの作成を開始します"

    $ExeEditorPath = Join-Path $RootDir "generated\outputs\Editor\Application_solo.exe"

    if (-not $SkipBuild) {
        $buildOk = Invoke-ProjectBuild -Configuration "Editor"
        if (-not $buildOk) {
            Write-Warn "Editor構成のビルドに失敗したため、エディタパッケージの作成を中断しました。"
            return
        }
    } else {
        if (-not (Test-Path $ExeEditorPath)) {
            Write-Warn "Editor構成のビルド成果物が見つかりません: $ExeEditorPath"
            return
        }
        Write-Info "既存のビルド成果物を使用します: $ExeEditorPath"
    }

    $TargetDir = if ($CustomOutputDir) { $CustomOutputDir } else { Join-Path $OutputDir "GravityShooter_EditorBuild" }
    $stagingDir = "$TargetDir._staging_$Timestamp"

    Remove-DirectorySafe -Path $stagingDir
    New-Item -ItemType Directory -Path $stagingDir | Out-Null

    Write-Info "エディタ実行ファイルおよび専用設定ファイルを配置中..."

    # 1. Exe本体
    Copy-Item -Path $ExeEditorPath -Destination (Join-Path $stagingDir "GravityShooter_Editor.exe") -Force

    # 2. imgui_editor.ini の配置（ウィンドウ配置設定ファイル）
    $iniCandidates = @(
        (Join-Path $RootDir "generated\outputs\Editor\imgui_editor.ini"),
        (Join-Path $RootDir "project\Application_solo\imgui_editor.ini"),
        (Join-Path $RootDir "project\IrufemiEngine\EngineResources\default_imgui.ini")
    )
    foreach ($ini in $iniCandidates) {
        if (Test-Path $ini) {
            Copy-Item -Path $ini -Destination (Join-Path $stagingDir "imgui_editor.ini") -Force
            Write-Info "エディタ設定ファイルを配置しました: $(Split-Path -Leaf $ini) -> imgui_editor.ini"
            break
        }
    }

    # 3. resources/（エディタ用リソース転送）
    $resSrc = Join-Path $RootDir "project\Application_solo\resources"
    if (Test-Path $resSrc) {
        $resDest = Join-Path $stagingDir "resources"
        New-Item -ItemType Directory -Path $resDest -Force | Out-Null
        $rcArgs = @(
            $resSrc,
            $resDest,
            "/E",
            "/NJH", "/NJS", "/NDL", "/NC", "/NS", "/NP",
            "/XF", "*.blend", "*.blend1", "*.xcf", "*.aseprite", "*.pso",
            "/XD", ".cache", "cache", "screenshots", "temp"
        )
        & robocopy @rcArgs | Out-Null
    }

    # 4. EngineResources/
    $engineResSrc = Join-Path $RootDir "project\IrufemiEngine\EngineResources"
    if (Test-Path $engineResSrc) {
        $engineResDest = Join-Path $stagingDir "EngineResources"
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

    # 5. オフラインシェーダーコンパイル
    $compiledShaderDest = Join-Path $stagingDir "resources\shaders\compiled"
    if (-not (Test-Path $compiledShaderDest)) {
        New-Item -ItemType Directory -Path $compiledShaderDest -Force | Out-Null
    }
    $compileBat = Join-Path $RootDir "project\CompileShaders.bat"
    $engineShaderSrc = Join-Path $RootDir "project\IrufemiEngine\EngineResources\shaders"
    $appShaderSrc = Join-Path $RootDir "project\Application_solo\resources\shaders"
    $dxcPath = Get-DxcPath

    Write-Info "HLSLシェーダーを事前コンパイル中 (.cso を直接生成・配置)..."
    & cmd.exe /c "`"$compileBat`" `"$engineShaderSrc`" `"$compiledShaderDest`" `"$dxcPath`"" | Out-Null
    & cmd.exe /c "`"$compileBat`" `"$appShaderSrc`" `"$compiledShaderDest`" `"$dxcPath`"" | Out-Null

    # 6. エディタ操作ガイド
    $editorReadme = Join-Path $stagingDir "README_Editor.txt"
    $readmeText = @"
================================================================================
 IrufemiEngine 自作エディタモード 操作ガイド
================================================================================

【概要】
本パッケージは、自作ゲームエンジン「IrufemiEngine」に内包されている
ゲーム内エディタ機能（Prefab分離編集モード、インスペクタ、ギズモ操作）を
体験・審査していただくための独立した実行環境です。

【起動方法】
GravityShooter_Editor.exe をダブルクリックして起動してください。

【主なエディタ機能】
1. インスペクタパネル: シーン内のオブジェクトやコンポーネントの数値をリアルタイム変更
2. プレハブ分離編集モード: Prefabファイルを単独で開き、原点グリッド上で安全に編集
3. ギズモ操作: ImGuizmoによるオブジェクトの平行移動・回転・拡縮

※ウィンドウ配置やドッキングレイアウトは imgui_editor.ini により初期化されます。
"@
    [System.IO.File]::WriteAllText($editorReadme, $readmeText, [System.Text.Encoding]::UTF8)

    # 7. 整合性検証 & ビルドマニフェスト生成（ステージング内で検証）
    Test-PackageIntegrity -PackageDir $stagingDir -Configuration "Editor"
    New-BuildManifest -PackageDir $stagingDir -Configuration "Editor"

    # 8. アトミック置換（安全スワップ）
    Remove-DirectorySafe -Path $TargetDir
    Move-DirectorySafe -Source $stagingDir -Destination $TargetDir

    $fileSizeMB = Get-FolderSizeMB $TargetDir
    Write-Success "自作エディタパッケージ作成完了! (アトミック置換適用)"
    Write-Host "   出力先: $TargetDir" -ForegroundColor White
    Write-Host "   サイズ: $fileSizeMB MB`n" -ForegroundColor Green
}

# -----------------------------------------------------------------------------
# 5. SEGAカレッジ提出用 一式パッケージ生成（Playable + Editor + Source + 資料 + 動画むき出しZIP化）
# -----------------------------------------------------------------------------
function Create-SegaSubmissionPackage {
    param(
        [string]$ApplicantId = "S28000478",
        [string]$ApplicantName = "末廣航一"
    )
    Write-Header "SEGAカレッジ提出用 一括パッケージの作成を開始します"

    $pkgName = "${ApplicantId}_${ApplicantName}"
    $targetDir = Join-Path $OutputDir $pkgName
    $stagingDir = "$targetDir._staging_$Timestamp"
    $zipPath = Join-Path $OutputDir "${pkgName}.zip"

    Remove-DirectorySafe -Path $stagingDir
    New-Item -ItemType Directory -Path $stagingDir | Out-Null

    # 1. 01_PlayableBuild (最新Releaseビルド＆スマートクック: 製品版)
    Write-Info "【1/5】01_PlayableBuild（製品版ゲーム本体）を生成中..."
    Create-PlayablePackage -SkipBuild:$false -CustomOutputDir (Join-Path $stagingDir "01_PlayableBuild")

    # 2. 02_EditorBuild (自作エディタ体験パッケージ: エディタ版)
    Write-Info "【2/5】02_EditorBuild（自作エディタ環境・Prefab編集）を生成中..."
    Create-EditorPackage -SkipBuild:$false -CustomOutputDir (Join-Path $stagingDir "02_EditorBuild")

    # 3. 03_SourceCode (バイナリ完全除外クリーンソースコード)
    Write-Info "【3/5】03_SourceCode（クリーンソースコード）を生成中..."
    Create-SourcePackage -CustomOutputDir (Join-Path $stagingDir "03_SourceCode")

    # 4. 提出資料PDFの直下むき出し配置
    Write-Info "【4/5】提出資料PDFを直下配置中..."
    $docPortfolio = Join-Path $RootDir "docs\Portfolio_Koichi_Suehiro.pdf"
    if (-not (Test-Path $docPortfolio)) {
        $foundP = Get-ChildItem -Path (Join-Path $RootDir "docs") -Filter "*Portfolio*.pdf" -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($foundP) { $docPortfolio = $foundP.FullName }
    }
    if ($docPortfolio -and (Test-Path $docPortfolio)) {
        Copy-Item -Path $docPortfolio -Destination (Join-Path $stagingDir (Split-Path -Leaf $docPortfolio)) -Force
        Write-Success "ポートフォリオPDFを配置しました: $(Split-Path -Leaf $docPortfolio)"
    }

    $docManual = Join-Path $RootDir "docs\ProgramSpec_Koichi_Suehiro.pdf"
    if (-not (Test-Path $docManual)) {
        $foundM = Get-ChildItem -Path (Join-Path $RootDir "docs") -Filter "*ProgramSpec*.pdf" -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($foundM) { $docManual = $foundM.FullName }
    }
    if ($docManual -and (Test-Path $docManual)) {
        Copy-Item -Path $docManual -Destination (Join-Path $stagingDir (Split-Path -Leaf $docManual)) -Force
        Write-Success "プログラム説明書PDFを配置しました: $(Split-Path -Leaf $docManual)"
    }

    # 5. 動画ファイル (mp4) の直下むき出し配置
    Write-Info "【5/5】動画ファイル (mp4) の確認と直下配置..."
    $foundMovie = Get-ChildItem -Path $RootDir -Recurse -Filter "*.mp4" -Force -ErrorAction SilentlyContinue | 
        Where-Object { $_.FullName -notmatch '\\_Submission\\' -and $_.FullName -notmatch '\\\.git\\' } | 
        Select-Object -First 1

    if ($foundMovie) {
        $destMovieName = "${pkgName}_プレイ動画.mp4"
        Copy-Item -Path $foundMovie.FullName -Destination (Join-Path $stagingDir $destMovieName) -Force
        Write-Success "動画ファイルを検出・直下配置しました: $destMovieName"
    } else {
        $destMoviePlaceholder = Join-Path $stagingDir "${pkgName}_プレイ動画_ここに配置.txt"
        Set-Content -Path $destMoviePlaceholder -Value "※提出用の mp4 動画ファイルをこの直下に配置してください。`nファイル名例: ${pkgName}_プレイ動画.mp4" -Encoding UTF8
        Write-Warn "動画ファイル (*.mp4) が自動検出されなかったため、案内ファイルを配置しました: $(Split-Path -Leaf $destMoviePlaceholder)"
    }

    # 6. アトミック置換（安全スワップ）
    Remove-DirectorySafe -Path $targetDir
    Move-DirectorySafe -Source $stagingDir -Destination $targetDir

    # 7. 提出用一括 ZIP 圧縮（完全に完成した本番フォルダから圧縮）
    Write-Info "提出用ZIPファイルを作成中: $zipPath..."
    if (Test-Path $zipPath) { Remove-Item -Path $zipPath -Force }
    Compress-Archive -Path "$targetDir\*" -DestinationPath $zipPath -CompressionLevel Optimal

    $zipSizeMB = [math]::Round((Get-Item $zipPath).Length / 1MB, 2)
    Write-Header "SEGAカレッジ提出用パッケージ生成完了! (完全アトミック検証済み)"
    Write-Success "完成ZIP: $zipPath"
    Write-Host "   ZIPサイズ: $zipSizeMB MB" -ForegroundColor Green
    Write-Host "   解凍後フォルダ: $targetDir`n" -ForegroundColor White
}

# -----------------------------------------------------------------------------
# メインメニュー
# -----------------------------------------------------------------------------
if ($Mode -eq "Menu") {
    Write-Header "就職活動・技術審査用 パッケージ生成ツール (Cook & Pipeline Mode)"
    Write-Host "作成したいパッケージを選択してください:" -ForegroundColor Yellow
    Write-Host "  [1] プレイ用パッケージ（最新コードを Release ビルド ＋ スマートクックで極限軽量化）" -ForegroundColor Cyan
    Write-Host "  [2] プレイ用パッケージ（既存 Exe を使用 ＋ スマートクック生成）" -ForegroundColor Cyan
    Write-Host "  [3] ソースコード提出用パッケージ (クリーン開発モード・完全整合版)" -ForegroundColor Cyan
    Write-Host "  [4] 両方一括生成（Release ビルド後に両パッケージを作成）" -ForegroundColor Cyan
    Write-Host "  [5] SEGAカレッジ提出用 一式パッケージ生成（Playable + Source + 資料/動画むき出しZIP化） ★SEGA推奨" -ForegroundColor Green
    Write-Host "  [Q] 終了`n" -ForegroundColor Gray

    $choice = Read-Host "選択 (1/2/3/4/5/Q)"
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
        "5" { 
            Create-SegaSubmissionPackage -ApplicantId "S28000478" -ApplicantName "末廣航一"
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
        "Sega" {
            Create-SegaSubmissionPackage -ApplicantId "S28000478" -ApplicantName "末廣航一"
        }
    }
}

Write-Header "すべての処理が完了しました"
Write-Host "出力先フォルダ: $OutputDir" -ForegroundColor White



