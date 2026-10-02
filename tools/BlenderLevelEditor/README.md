# 3D Rail Shooter - Blender Level Editor & Production Pipeline

本ディレクトリは、3Dレールシューティングゲーム『GravityShooter』のレベルデザイン、敵編隊（Wave）出現タイミング、および当たり判定配置を直感的に行うために独自開発された **Blender 拡張外部オーサリングパイプライン** です。

DirectX 12 ゲームエンジン本体に内蔵されたデバッグ・調整用エディタ（`IrufemiEditor`）と連携し、DCCツール（Blender）の強力な3Dモデリング・空間配置機能をそのままステージ制作環境として活用できるように設計されています。

---

## 1. 主要機能とアーキテクチャ

```text
[ Blender 4.x (Level Editor) ]
      │
      ├─ 敵・障害物・カメラの直感的な3D配置
      ├─ コライダー（球・ボックス）のビジュアル生成
      ├─ スプラインレール沿いの出現距離（TriggerDistance）設定
      │
      ▼ (Export: JSON)
[ tools/BlenderLevelEditor/sample_scenes/SampleStage.json ]
      │
      ▼ (Runtime Load: C++ / DirectX 12)
[ IrufemiEngine / Application_solo (GravityShooter) ]
```

### ① リアルタイム・コライダーオーサリング
- Blender 上でメッシュに対して「当たり判定（球・カプセル・OBB）」を視覚的に生成・編集。
- ゲーム本編の当たり判定システムへシームレスに適用されるコライダーデータをエクスポート。

### ② レールシューター特化のスポーン・イベント配置
- 自機の移動経路（スプラインレール）に沿った敵の発生地点（`SpawnPoint`）や編隊データ（V字編隊、単機突撃等）の直感的配置。
- カメラアングル変更トリガーやイベントトリガーの距離ベース配置。

### ③ ホットリロード機構
- ゲーム本編を実行したまま、Blender側で編集・出力したステージJSONを即座に再読み込みし、イテレーション時間を極小化。

### ④ AI MagicBrush 統合
- 自然言語プロンプトまたは参考画像から、Python AIサーバーと連携してHLSLシェーダーコードを自動生成・プレビューする次世代ワークフローを実装。

---

## 2. ツール構成とファイル一覧

```text
tools/BlenderLevelEditor/
├── addon/                          <-- Blender 4.x 用アドオンパッケージ（Python）
│   ├── __init__.py                 <-- アドオン登録エントリーポイント
│   ├── my_menu.py                  <-- 専用UIパネル・メニュー
│   ├── export_scene.py             <-- ステージJSONエクスポーター
│   ├── import_scene.py             <-- ステージJSONインポーター
│   ├── spawn.py                    <-- スポーンポイント生成ロジック
│   ├── collider.py                 <-- コライダー生成ロジック
│   └── draw_collider.py            <-- Blenderビューポート上でのコライダー描画
│
├── Launch_BlenderLevelEditor.bat   <-- 【起動】環境非依存のBlender多層自動検出ランチャー
├── Sync_BlenderAddon.bat           <-- 【同期】Blender AppDataへのアドオン自動同期ツール
├── BlenderAddons_Folder.lnk        <-- Blenderアドオンフォルダへのショートカット
├── sample_scenes/                  <-- サンプルステージデータ (.json / .scene)
└── Roadmap_AI_MagicBrush.md        <-- AIシェーダー生成ツールの開発ロードマップ
```

---

## 3. クイックスタートガイド

### Step 1: アドオンの同期（配置）
1. `Sync_BlenderAddon.bat` をダブルクリックして実行します。
2. メニューで `1: 【Deploy】` を選択すると、お使いのPCにインストールされている最新の Blender AppData へアドオンが自動配置されます。

### Step 2: Blender の起動
- `Launch_BlenderLevelEditor.bat` を実行すると、インストール先（独自パス、標準パス、Steam版）を自動検出して Blender が立ち上がります。
- Blender の「編集」→「プリファレンス」→「アドオン」から「Level Editor」を有効化してください。

### Step 3: ステージの編集とエクスポート
1. 3Dビューポートのサイドバー（Nキー）に表示される「Level Editor」タブを開きます。
2. 敵や障害物を配置し、「Export Scene」を実行すると、ゲーム本編でロード可能な JSON データが出力されます。

---

## 4. 技術仕様と開発環境

- **開発言語**: Python 3.10+ / Blender Python API (bpy)
- **対応環境**: Blender 4.0 / 4.1 / 4.2 / 4.3 / 4.4 / 4.5
- **連携先エンジン**: IrufemiEngine (DirectX 12 / C++20)
