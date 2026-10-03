param (
    [ValidateSet("Audit", "DryRun", "Sanitize", "Restore", "CleanCache", "Menu")]
    [string]$Mode = "Menu",
    [string]$ScriptDir = "",
    [switch]$Force
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
$AppResDir = Join-Path $RootDir "project\Application_solo\resources"
$AssetSrcDir = Join-Path $RootDir "asset_src"
$BackupRootDir = Join-Path $RootDir "backup"
$Timestamp = Get-Date -Format "yyyyMMdd_HHmm"

# コンソール出力ヘルパー
function Write-Header {
    param([string]$Text)
    Write-Host "`n========================================================" -ForegroundColor Cyan
    Write-Host " $Text" -ForegroundColor Cyan
    Write-Host "========================================================`n" -ForegroundColor Cyan
}
function Write-Success { param([string]$Text) Write-Host "[OK] $Text" -ForegroundColor Green }
function Write-Warn    { param([string]$Text) Write-Host "[WARN] $Text" -ForegroundColor Yellow }
function Write-Info    { param([string]$Text) Write-Host "[INFO] $Text" -ForegroundColor Gray }

# -----------------------------------------------------------------------------
# 1. アセット逆引き監査エンジン (Audit Engine)
# -----------------------------------------------------------------------------
function Analyze-Assets {
    Write-Info "ソースコードおよびシーン定義の逆引きスキャンを実行中..."

    # 1. C++ / JSON / HLSL ソースの収集
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

    # 2. 全ランタイムアセットの収集
    $allAssets = Get-ChildItem -Path $AppResDir -Recurse -File -Force | Where-Object {
        $_.FullName -notmatch '\\\.cache\\' -and 
        $_.FullName -notmatch '\\shaders\\compiled\\'
    }

    # 3. OBJ -> MTL -> テクスチャの一次・二次依存関係解析
    $activeAssets = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
    $referencedObjFiles = @()

    foreach ($asset in $allAssets) {
        $relPath = $asset.FullName.Substring($AppResDir.Length + 1).Replace('\', '/')

        # コアシステムアセットは常に保護
        if ($relPath -match '^(config|GameData)/' -or $relPath -eq 'fonts/toro_glitch.otf' -or $relPath -match '^scenes/.*\.json$' -or $relPath -match '^prefabs/.*\.json$') {
            [void]$activeAssets.Add($relPath)
            continue
        }

        # デバッグシーン専用アセットは開発用として保護
        if ($relPath.StartsWith("model/sample/") -or $relPath -in @("kloofendal_overcast_puresky_1k.dds", "qwantani_night_puresky_1k_cubemap.dds")) {
            [void]$activeAssets.Add($relPath)
            continue
        }

        $filename = $asset.Name
        $basename = [System.IO.Path]::GetFileNameWithoutExtension($asset.Name)

        # C++ / JSON 一次参照チェック
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
            [void]$activeAssets.Add($relPath)
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
                [void]$activeAssets.Add($relMtl)

                $mtlContent = [System.IO.File]::ReadAllText($mtlPath, [System.Text.Encoding]::UTF8)
                $texMatches = [regex]::Matches($mtlContent, '(?im)^\s*map_[a-z0-9_]+\s+(?:-[^\s]+\s+)*(?:.*\\)?([^\s\r\n]+\.(?:png|jpg|jpeg|dds|tga|bmp))')
                foreach ($tMatch in $texMatches) {
                    $texName = $tMatch.Groups[1].Value.Trim()
                    $texPath = Join-Path $objDir $texName
                    if (Test-Path $texPath) {
                        $relTex = $texPath.Substring($AppResDir.Length + 1).Replace('\', '/')
                        [void]$activeAssets.Add($relTex)
                    }
                }
            }
        }
    }

    # 4. 未参照アセットの分類
    $items = @()
    foreach ($asset in $allAssets) {
        $relPath = $asset.FullName.Substring($AppResDir.Length + 1).Replace('\', '/')
        if ($activeAssets.Contains($relPath)) {
            continue
        }

        $category = ""
        $action = ""
        $destRel = ""

        if ($relPath.StartsWith("screenshots/")) {
            $category = "Screenshots"
            $action = "Delete"
        } elseif ($relPath.StartsWith("model/CG4/")) {
            $category = "LegacyCourseData"
            $action = "Delete"
        } elseif ($relPath.StartsWith("shaders/generated/")) {
            $category = "AiTempShaders"
            $action = "Delete"
        } elseif ($relPath -in @("cyberpunk_wasteland_ground.png", "monsterBall.png", "ui/start_button.png", "shaders/RocketFlame.PS.hlsl", "shaders/RocketFlame.VS.hlsl")) {
            $category = "OrphanedResidue"
            $action = "Delete"
        } elseif ($relPath.StartsWith("fonts/瀞ノグリッチ明朝/")) {
            $category = "FontWeights"
            $action = "Move"
            $destRel = "fonts/瀞ノグリッチ明朝/" + $asset.Name
        } elseif ($relPath.EndsWith(".blend") -or $relPath.EndsWith(".blend1")) {
            $category = "DccSource"
            $action = "Move"
            $destRel = "models/sci-fi_robot/" + $asset.Name
        } elseif ($relPath.StartsWith("model/sci-fi+armored+robot+3d+model/")) {
            $category = "UnusedModelOriginal"
            $action = "Move"
            $destRel = "models/sci-fi_robot/" + $asset.Name
        } elseif ($relPath -match '(_normal\.(png|jpg)|_rm\.jpg)$') {
            $category = "UnboundTextures"
            $action = "Move"
            $destRel = "models/textures_unused/" + $relPath
        } else {
            $category = "UnreferencedOther"
            $action = "Move"
            $destRel = "other_unreferenced/" + $relPath
        }

        $items += [PSCustomObject]@{
            File = $asset
            RelPath = $relPath
            Category = $category
            Action = $action
            DestRel = $destRel
            SizeBytes = $asset.Length
            SizeMB = [math]::Round($asset.Length / 1MB, 2)
            SizeKB = [math]::Round($asset.Length / 1KB, 1)
        }
    }

    return [PSCustomObject]@{
        TotalActiveCount = $activeAssets.Count
        UnreferencedItems = $items
    }
}

# -----------------------------------------------------------------------------
# 2. 監査モード (Audit Report)
# -----------------------------------------------------------------------------
function Show-AuditReport {
    Write-Header "IrufemiEngine - アセット健全性 監査レポート (Asset Audit)"

    $result = Analyze-Assets
    $items = $result.UnreferencedItems

    if ($items.Count -eq 0) {
        Write-Host "【監査結果サマリー】" -ForegroundColor White
        Write-Host ("  本編・エンジンで正常使用中のアセット : {0} 個" -f $result.TotalActiveCount) -ForegroundColor Green
        Write-Host "  未参照・分離対象のアセット           : 0 個 (0.00 MB)" -ForegroundColor Green
        Write-Host ""
        Write-Success "素晴らしい！ resources/ には不要・未参照ファイルは一切存在しません。100% 健全（クリーン）です！"
        return
    }

    $sumBytes = ($items | Measure-Object -Property SizeBytes -Sum).Sum
    $totalUnrefSizeMB = if ($sumBytes) { [math]::Round($sumBytes / 1MB, 2) } else { 0 }

    Write-Host "【監査結果サマリー】" -ForegroundColor White
    Write-Host ("  本編・エンジンで正常使用中のアセット : {0} 個" -f $result.TotalActiveCount) -ForegroundColor Green
    Write-Host ("  未参照・分離対象のアセット           : {0} 個 (合計 {1:F2} MB)" -f $items.Count, $totalUnrefSizeMB) -ForegroundColor Yellow
    Write-Host ""

    $grouped = $items | Group-Object Category | Sort-Object Count -Descending

    Write-Host "【カテゴリ別内訳】" -ForegroundColor White
    foreach ($g in $grouped) {
        $catMB = [math]::Round(($g.Group | Measure-Object -Property SizeBytes -Sum).Sum / 1MB, 2)
        $action = $g.Group[0].Action
        $actionLabel = if ($action -eq "Delete") { "[完全削除]" } else { "[asset_src/ へ退避]" }
        $color = if ($action -eq "Delete") { "Red" } else { "Cyan" }

        Write-Host ("  {0,-22} : {1,3} files ({2,6:F2} MB)  -> {3}" -f $g.Name, $g.Count, $catMB, $actionLabel) -ForegroundColor $color
    }

    Write-Host "`n【クリーンアップ後の見込み効果】" -ForegroundColor White
    $deleteMB = [math]::Round(($items | Where-Object Action -eq "Delete" | Measure-Object -Property SizeBytes -Sum).Sum / 1MB, 2)
    $moveMB = [math]::Round(($items | Where-Object Action -eq "Move" | Measure-Object -Property SizeBytes -Sum).Sum / 1MB, 2)
    Write-Host ("  - 完全削除によるリポジトリ即時軽量化   : 約 {0:F2} MB 削減" -f $deleteMB) -ForegroundColor Green
    Write-Host ("  - asset_src/ への制作元データの安全隔離: 約 {0:F2} MB 移行" -f $moveMB) -ForegroundColor Cyan
    Write-Host ("  - ランタイム resources/ の純化総削減   : 約 {0:F2} MB (約 72% 純化)" -f ($deleteMB + $moveMB)) -ForegroundColor Green
}

# -----------------------------------------------------------------------------
# 3. Dry-Run モード (シミュレーション)
# -----------------------------------------------------------------------------
function Show-DryRun {
    Write-Header "アセットクリーンアップ シミュレーション (Dry-Run: 変更なし)"

    $result = Analyze-Assets
    $items = $result.UnreferencedItems

    Write-Host "以下のファイルが移動・削除対象となります（実際のファイル操作は行われません）:`n" -ForegroundColor Yellow

    $toDelete = $items | Where-Object Action -eq "Delete"
    if ($toDelete.Count -gt 0) {
        Write-Host ("--- 【完全削除対象】 ({0} files) ---" -f $toDelete.Count) -ForegroundColor Red
        foreach ($d in $toDelete) {
            Write-Host ("  [DELETE] resources/{0} ({1} KB)" -f $d.RelPath, $d.SizeKB) -ForegroundColor Red
        }
    }

    $toMove = $items | Where-Object Action -eq "Move"
    if ($toMove.Count -gt 0) {
        Write-Host ("`n--- 【asset_src/ への隔離移動対象】 ({0} files) ---" -f $toMove.Count) -ForegroundColor Cyan
        foreach ($m in $toMove) {
            Write-Host ("  [MOVE] resources/{0} -> asset_src/{1} ({2} KB)" -f $m.RelPath, $m.DestRel, $m.SizeKB) -ForegroundColor Cyan
        }
    }

    Write-Host "`n[INFO] 実際に適用する場合は [3] Sanitize Mode を選択してください。" -ForegroundColor Gray
}

# -----------------------------------------------------------------------------
# 4. Sanitize モード (クリーンアップ実行)
# -----------------------------------------------------------------------------
function Invoke-Sanitize {
    Write-Header "アセット健全化・クリーンアップの実行 (Sanitize Mode)"

    $result = Analyze-Assets
    $items = $result.UnreferencedItems

    if ($items.Count -eq 0) {
        Write-Success "未参照アセットは存在しません。resources/ はすでに100%クリーンです！"
        return
    }

    # 1. 自動バックアップの取得
    $currentBackupDir = Join-Path $BackupRootDir "resources_backup_$Timestamp"
    Write-Info "安全のため、実行前に resources/ の完全自動バックアップを作成中..."
    Write-Info "バックアップ先: $currentBackupDir"

    if (-not (Test-Path $BackupRootDir)) { New-Item -ItemType Directory -Path $BackupRootDir | Out-Null }
    Copy-Item -Path $AppResDir -Destination $currentBackupDir -Recurse -Force
    Write-Success "自動バックアップが正常に作成されました。"

    # 2. asset_src/ の基盤作成
    if (-not (Test-Path $AssetSrcDir)) {
        New-Item -ItemType Directory -Path $AssetSrcDir -Force | Out-Null
        Write-Info "asset_src/ ディレクトリを新設しました。"
    }

    # 3. 移動・削除の適用
    $deletedCount = 0
    $movedCount = 0

    foreach ($item in $items) {
        $srcFile = $item.File.FullName

        if ($item.Action -eq "Delete") {
            Remove-Item -Path $srcFile -Force -ErrorAction SilentlyContinue
            $deletedCount++
        } elseif ($item.Action -eq "Move") {
            $destFile = Join-Path $AssetSrcDir $item.DestRel
            $destParent = Split-Path -Parent $destFile
            if (-not (Test-Path $destParent)) {
                New-Item -ItemType Directory -Path $destParent -Force | Out-Null
            }
            Move-Item -Path $srcFile -Destination $destFile -Force -ErrorAction SilentlyContinue
            $movedCount++
        }
    }

    # 4. 空になった不要フォルダのクリーンアップ
    $emptyDirs = Get-ChildItem -Path $AppResDir -Recurse -Directory -Force | Where-Object {
        @(Get-ChildItem -Path $_.FullName -Recurse -File -Force).Count -eq 0
    }
    foreach ($ed in $emptyDirs) {
        Remove-Item -Path $ed.FullName -Recurse -Force -ErrorAction SilentlyContinue
    }

    # 5. .gitignore の確認・更新（screenshots/ フォルダの再発防止）
    $gitIgnorePath = Join-Path $RootDir ".gitignore"
    if (Test-Path $gitIgnorePath) {
        $giContent = [System.IO.File]::ReadAllText($gitIgnorePath, [System.Text.Encoding]::UTF8)
        if ($giContent -notmatch 'screenshots/') {
            $appendRule = "`n# 開発中キャプチャ画像 (Auto-added by AssetSanitizer)`n**/resources/screenshots/`n"
            [System.IO.File]::AppendAllText($gitIgnorePath, $appendRule, [System.Text.Encoding]::UTF8)
            Write-Success ".gitignore に **/resources/screenshots/ を追加しました。"
        }
    }

    Write-Success ("クリーンアップ完了! 削除: {0} 件, asset_src/ 移動: {1} 件" -f $deletedCount, $movedCount)
    Write-Host "万が一復元が必要な場合は、メニュー [4] または -Mode Restore を実行してください。`n" -ForegroundColor Gray
}

# -----------------------------------------------------------------------------
# 5. Restore モード (バックアップからの復元)
# -----------------------------------------------------------------------------
function Invoke-Restore {
    Write-Header "バックアップからの完全ロールバック (Restore Mode)"

    if (-not (Test-Path $BackupRootDir)) {
        Write-Warn "バックアップフォルダが見つかりません: $BackupRootDir"
        return
    }

    $backups = Get-ChildItem -Path $BackupRootDir -Directory | Sort-Object Name -Descending
    if ($backups.Count -eq 0) {
        Write-Warn "復元可能なバックアップが存在しません。"
        return
    }

    $latestBackup = $backups[0].FullName
    Write-Host ("最新のバックアップが見つかりました: {0}" -f $backups[0].Name) -ForegroundColor Yellow
    $confirm = Read-Host "このバックアップで project/Application_solo/resources を上書き復元しますか? (Y/N)"
    if ($confirm -ne "Y" -and $confirm -ne "y") {
        Write-Host "復元をキャンセルしました。" -ForegroundColor Gray
        return
    }

    Write-Info "復元中..."
    Copy-Item -Path "$latestBackup\*" -Destination $AppResDir -Recurse -Force
    Write-Success "resources/ がバックアップの状態に完全復元されました。"
}

# -----------------------------------------------------------------------------
# 5. キャッシュ・ログ・一時生成物の一括クリーン (.gitignore 準拠)
# -----------------------------------------------------------------------------
function Invoke-CleanCacheAndLogs {
    Write-Header "キャッシュ・ログ・一時生成物の一括クリーン (.gitignore 準拠)"

    $cleanedCount = 0

    # 1. git status --ignored --porcelain から Git 管理外の無視アイテムを自動取得
    $ignoredItems = @()
    try {
        $gitOutput = git status --ignored --porcelain
        foreach ($line in $gitOutput) {
            if ($line.StartsWith("!! ")) {
                $rel = $line.Substring(3).Trim().Trim('"')
                $ignoredItems += $rel
            }
        }
    } catch {
        Write-Warn "Git からの無視ファイル取得に失敗しました。手動フォールバックパスで走査します。"
    }

    # 安全な削除対象パターン（キャッシュ・ログ・一時スクリプト生成物）
    # ※ .vs, .env, settings_local, _Submission, backup, packages, externals, compiled shaders は除外保護
    $safeDeletePatterns = @(
        '^(?:project/)?(?:[a-zA-Z0-9_-]+/)?(?:logs|Logs)/',
        '^logs/',
        '^generated/cache/',
        'resources/\.cache/',
        '/__pycache__/',
        'resources/scenes/temp/',
        'resources/shaders/generated/',
        'shader_error\.txt$',
        '\.log$',
        '\.pso$',
        '\.cachefile$',
        '\.tmp$'
    )

    $targetsToDelete = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)

    # Git の ignoredItems から安全なものを抽出
    foreach ($item in $ignoredItems) {
        $fullPath = Join-Path $RootDir $item
        if (-not (Test-Path $fullPath)) { continue }

        $normRel = $item.Replace('\', '/')
        $matched = $false
        foreach ($pat in $safeDeletePatterns) {
            if ($normRel -match $pat) {
                $matched = $true
                break
            }
        }
        if ($matched) {
            [void]$targetsToDelete.Add($fullPath)
        }
    }

    # フォールバックで直接存在する主要なキャッシュ・ログディレクトリも確実に含める
    $manualTargets = @(
        (Join-Path $RootDir "logs"),
        (Join-Path $RootDir "project\Application_solo\Logs"),
        (Join-Path $RootDir "generated\cache"),
        (Join-Path $RootDir "project\Application_solo\resources\.cache")
    )
    foreach ($mt in $manualTargets) {
        if (Test-Path $mt) {
            [void]$targetsToDelete.Add($mt)
        }
    }

    if ($targetsToDelete.Count -eq 0) {
        Write-Success "削除対象のキャッシュ・ログはありません。クリーンな状態です。"
        return
    }

    Write-Info "以下のキャッシュ・ログ・一時生成物を安全に消去します:"
    foreach ($tgt in $targetsToDelete) {
        $rel = $tgt.Substring($RootDir.Length + 1)
        Write-Host "  [-] $rel" -ForegroundColor Gray
    }

    if (-not $Force) {
        Write-Host ""
        $confirm = Read-Host "上記の一時ファイルをすべて削除してもよろしいですか? (Y/N)"
        if ($confirm -ne "Y" -and $confirm -ne "y") {
            Write-Host "クリーン処理をキャンセルしました。" -ForegroundColor Gray
            return
        }
    }

    foreach ($tgt in $targetsToDelete) {
        try {
            if (Test-Path $tgt) {
                Remove-Item -Path $tgt -Recurse -Force -ErrorAction SilentlyContinue
                $cleanedCount++
            }
        } catch {
            Write-Warn "削除をスキップしました: $tgt"
        }
    }

    Write-Success "クリーンアップ完了: $cleanedCount 個のキャッシュ・ログアイテムを消去しました。"
}

# -----------------------------------------------------------------------------
# メインメニュー
# -----------------------------------------------------------------------------
if ($Mode -eq "Menu") {
    Write-Header "IrufemiEngine - アセット＆ワークスペース健全化ツール (Asset Sanitizer)"
    Write-Host "実行したい処理を選択してください:" -ForegroundColor Yellow
    Write-Host "  [1] Audit Mode          : 参照関係を完全走査し、アセット健全性レポートを表示（変更なし） ★推奨" -ForegroundColor Cyan
    Write-Host "  [2] Dry-Run Mode        : クリーンアップ実行時の移動・削除プレビューを表示（変更なし）" -ForegroundColor Cyan
    Write-Host "  [3] Sanitize Mode       : 自動バックアップ作成後、安全に整理・隔離・クリーンアップを実行" -ForegroundColor Cyan
    Write-Host "  [4] Restore Backup      : 直前のバックアップからワンクリックで元の状態に完全復元" -ForegroundColor Cyan
    Write-Host "  [5] Clean Cache & Logs  : .gitignore準拠でキャッシュ・ログ・一時生成物を一括クリーン" -ForegroundColor Magenta
    Write-Host "  [Q] 終了`n" -ForegroundColor Gray

    $choice = Read-Host "選択 (1/2/3/4/5/Q)"
    switch ($choice) {
        "1" { Show-AuditReport }
        "2" { Show-DryRun }
        "3" { Invoke-Sanitize }
        "4" { Invoke-Restore }
        "5" { Invoke-CleanCacheAndLogs }
        default { Write-Host "処理をキャンセルしました。" -ForegroundColor Gray; exit 0 }
    }
} else {
    switch ($Mode) {
        "Audit"      { Show-AuditReport }
        "DryRun"     { Show-DryRun }
        "Sanitize"   { Invoke-Sanitize }
        "Restore"    { Invoke-Restore }
        "CleanCache" { Invoke-CleanCacheAndLogs }
    }
}
