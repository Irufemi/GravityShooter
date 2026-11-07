# IrufemiEngine & Application 総合開発マニュアル

## 目次 (Table of Contents)
- [1. 開発環境とワークスペース運用 (Environment & Workspace)](#1-開発環境とワークスペース運用-environment--workspace)
  - [1.1 プロジェクト構造と配置ルール](#11-プロジェクト構造と配置ルール-project-structure)
  - [1.2 ビルド環境と外部ライブラリのセットアップ (VS 2026)](#12-ビルド環境と外部ライブラリのセットアップ-vs-2026)
  - [1.3 スクリプト体系とワークスペース健全化運用 (scripts/)](#13-スクリプト体系とワークスペース健全化運用-scripts)
  - [1.4 外部ツール群の利用と配置 (tools/)](#14-外部ツール群の利用と配置-tools)
- [2. チーム開発ルールとコーディング規約 (Coding Standards & Best Practices)](#2-チーム開発ルールとコーディング規約-coding-standards--best-practices)
  - [2.1 アーキテクチャと関心の分離](#21-アーキテクチャと関心の分離)
  - [2.2 C++ コーディング規約（引数渡し・コンテナ操作のベストプラクティス）](#22-c-コーディング規約引数渡しコンテナ操作のベストプラクティス)
  - [2.3 マルチスレッド安全性の指針（Synchronized<T> の活用と排他制御ルール）](#23-マルチスレッド安全性の指針synchronizedt-の活用と排他制御ルール)
  - [2.4 エラーハンドリング・DirectXアサーション規約 (IRUFEMI_CHECK_HR / IRUFEMI_ASSERT_HR)](#24-エラーハンドリングdirectxアサーション規約-irufemi_check_hr--irufemi_assert_hr)
  - [2.5 メモリ管理と安全性 (C++ / DirectX)](#25-メモリ管理と安全性-c--directx)
  - [2.6 オブジェクトのライフサイクル管理とプールの安全な運用](#26-オブジェクトのライフサイクル管理とプールの安全な運用)
  - [2.7 命名規則・コードスタイル](#27-命名規則コードスタイル)
  - [2.8 外部ライブラリ利用規約と共通ユーティリティ (JsonUtility / FileSystem / StringUtility)](#28-外部ライブラリ利用規約と共通ユーティリティ-jsonutility--filesystem--stringutility)
- [3. エディタの使い方とレベル制作 (IrufemiEditor & Level Design)](#3-エディタの使い方とレベル制作-irufemieditor--level-design)
  - [3.1 エディタ画面の基本レイアウト](#31-エディタ画面の基本レイアウト)
  - [3.2 Project Browser の操作（2ペイン表示とアセットD&D配置）](#32-project-browser-の操作2ペイン表示とアセットdd配置)
  - [3.3 プレハブ (Prefab) システム](#33-プレハブ-prefab-システム)
  - [3.4 インスペクターと Hierarchy の便利操作](#34-インスペクターと-hierarchy-の便利操作)
  - [3.5 Play / Pause コントロールとゲーム時間の管理](#35-play--pause-コントロールとゲーム時間の管理)
  - [3.6 AI Magic Brush (AI自動マテリアル生成)](#36-ai-magic-brush-ai自動マテリアル生成)
- [4. コアシステムとシーン管理 (Engine Architecture, Scene & GameObject)](#4-コアシステムとシーン管理-engine-architecture-scene--gameobject)
  - [4.1 エンジンアーキテクチャ概要 (IrufemiEngine)](#41-エンジンアーキテクチャ概要-irufemiengine)
  - [4.2 SceneManager と IScene (シーン管理)](#42-scenemanager-と-iscene-シーン管理)
  - [4.3 GameObject と親子階層構造](#43-gameobject-と親子階層構造)
  - [4.4 TransformComponent と階層構造](#44-transformcomponent-と階層構造)
  - [4.5 設定管理 (CVar & Settings Management)](#45-設定管理-cvar--settings-management)
- [5. コンポーネント完全リファレンス (Component System & All Components)](#5-コンポーネント完全リファレンス-component-system--all-components)
  - [5.1 コンポーネントシステムの基礎と新規作成ルール](#51-コンポーネントシステムの基礎と新規作成ルール)
  - [5.2 描画系コンポーネント](#52-描画系コンポーネント)
  - [5.3 カメラ系コンポーネント (Camera System)](#53-カメラ系コンポーネント-camera-system)
  - [5.4 エフェクト・演出系コンポーネント (Effects & Atmosphere)](#54-エフェクト演出系コンポーネント-effects--atmosphere)
  - [5.5 物理・コライダー系コンポーネント (Colliders)](#55-物理コライダー系コンポーネント-colliders)
  - [5.6 ロジック・ユーティリティ系コンポーネント (Logic & Utility)](#56-ロジックユーティリティ系コンポーネント-logic--utility)
  - [5.7 UI系コンポーネント (UI System)](#57-ui系コンポーネント-ui-system)
  - [5.8 大量オブジェクト最適化 (VirtualEntityManagerComponent)](#58-大量オブジェクト最適化-virtualentitymanagercomponent)
- [6. 描画パイプラインとグラフィックス (Graphics, Shaders & PostProcess)](#6-描画パイプラインとグラフィックス-graphics-shaders--postprocess)
  - [6.1 RenderGraph と DrawManager](#61-rendergraph-と-drawmanager)
  - [6.2 3Dアニメーション・GPU Skinning と Root Motion](#62-3dアニメーションgpu-skinning-と-root-motion)
  - [6.3 GPU パーティクルシステム (GPUParticleSystem)](#63-gpu-パーティクルシステム-gpuparticlesystem)
  - [6.4 ポストプロセスシステム (PostProcessManager)](#64-ポストプロセスシステム-postprocessmanager)
  - [6.5 G-Buffer拡張とMRT対応・高品質アウトライン](#65-g-buffer拡張とmrt対応高品質アウトライン)
  - [6.6 GPU カリング (GPU Culling & ExecuteIndirect)](#66-gpu-カリング-gpu-culling--executeindirect)
  - [6.7 Bindless Resources (Descriptor Indexing) 完全移行](#67-bindless-resources-descriptor-indexing-完全移行)
  - [6.8 シェーダーの追加・変更・コンパイル構成](#68-シェーダーの追加変更コンパイル構成)
  - [6.9 動的シャドウマップシステム (ShadowPass & ShadowMap)](#69-動的シャドウマップシステム-shadowpass--shadowmap)
  - [6.10 ライティングシステム (Directional, Point, Spot, Area)](#610-ライティングシステム-directional-point-spot-area)
- [7. 物理・衝突判定と空間分割 (Physics, Colliders & BVH)](#7-物理衝突判定と空間分割-physics-colliders--bvh)
  - [7.1 衝突判定マネージャー (CollisionManager)](#71-衝突判定マネージャー-collisionmanager)
  - [7.2 動的BVH(TLAS)による空間クエリ最適化](#72-動的bvhtlasによる空間クエリ最適化)
  - [7.3 数学・ベクトル演算のベストプラクティス](#73-数学ベクトル演算のベストプラクティス)
- [8. 入力・音響・UIシステム (Input, Audio & UI Framework)](#8-入力音響uiシステム-input-audio--ui-framework)
  - [8.1 入力システム (Input Action System & InputManager)](#81-入力システム-input-action-system--inputmanager)
  - [8.2 音響システム (AudioManager / AudioPlayer)](#82-音響システム-audiomanager--audioplayer)
  - [8.3 UIフレームワーク (Canvas, UISelectionGroup, UINavigationController, UIAnimator, Slider)](#83-uiフレームワーク-canvas-uiselectiongroup-uinavigationcontroller-uianimator-slider)
- [9. リソース管理・最適化・運用 (Resource, Profiling & Troubleshooting)](#9-リソース管理最適化運用-resource-profiling--troubleshooting)
  - [9.1 リソース管理システム (ResourceHandle & 各種Manager)](#91-リソース管理システム-resourcehandle--各種manager)
  - [9.2 外部パフォーマンス監視 (TelemetryMonitor)](#92-外部パフォーマンス監視-telemetrymonitor)
  - [9.3 フレームレート制御・タイマー精度 (AAA Frame Pacing)](#93-フレームレート制御タイマー精度-aaa-frame-pacing)
  - [9.4 エンジン拡張プラグイン機構 (IEngineExtension) とアトラクトデモ](#94-エンジン拡張プラグイン機構-iengineextension-とアトラクトデモ)
  - [9.5 ビルド・提出パッケージングパイプライン](#95-ビルド提出パッケージングパイプライン)
  - [9.6 統合テスト環境 (DebugScene) の利用ルール](#96-統合テスト環境-debugscene-の利用ルール)
  - [9.7 トラブルシューティングと既知の不具合対応履歴](#97-トラブルシューティングと既知の不具合対応履歴)

---


---

## 1. 開発環境とワークスペース運用 (Environment & Workspace)

### 1.1 プロジェクト構造と配置ルール (Project Structure)
新しくコードを書いたり、リソースを追加する際は以下の配置ルールに必ず従ってください。

- **`project/IrufemiEngine/` (エンジンコア)**
  - 描画パイプラインや汎用的なマネージャーが置かれます。**ゲーム特有のロジックやアクターは絶対にここに書かないでください。**
- **`project/IrufemiEditor/` (エディタUI・ツール)**
  - ImGuiを用いたエディタ画面の構築や、シーンビュー、インスペクター等の実装ロジックが置かれます。
- **`project/Application_solo/` / `project/Application_team/` (ゲームロジック)**
  - プレイヤーの動き、敵のAI、各種シーン（Title, InGame等）の処理はすべてプロジェクトに応じたこれらのディレクトリ内に作成します。
- **`resources/` (ランタイム・リソースデータ)**
  - 3Dモデル（`.obj`, `.gltf`）やテクスチャ（`.png`）、音声（`.wav`）など、**ゲームの実行に直接必要な最適化済みアセットのみ**を各Applicationディレクトリ直下の `resources/` 内（`model/`, `ui/`, `audio/` 等）に配置してください。
- **`asset_src/` (DCC制作原本データ)**
  - `.blend` ファイル、フォント原本、未統合テクスチャなどの制作元データはリポジトリルートの `asset_src/` に隔離し、ランタイムパッケージを肥大化させないようにします。
- **`generated/cache/` (派生データ・キャッシュ領域)**
  - 高速ロード用モデルバイナリ（`.model.ibin`）やDirectX12 PSOキャッシュ（`*.pso`）はすべて `generated/cache/` へ出力されます。`resources/` の中にキャッシュフォルダを作成してはいけません。

---

### 1.2 ビルド環境と外部ライブラリのセットアップ (VS 2026)
本プロジェクトは **Visual Studio 2026 (MSVC)** を標準開発環境としています。
ソリューションファイルには、最新のXMLベース高速ソリューション形式 **`project/Irufemi.slnx`**（VS 2026推奨）が用意されています。

#### 1.2.1 外部ライブラリの手動セットアップ（クローン直後の手順）

現在、GitHubのファイル容量制限により、`curl` や `assimp` などの実体バイナリ（`.lib`）はリポジトリから除外（手動管理）されています。
そのため、新しく `git clone` してきた際は、**初めに一度だけ以下の手順でライブラリをビルド（または配置）する必要があります。**

**【curl のビルド・配置手順】**
1. [curl 公式ダウンロードページ](https://curl.se/download.html) から最新のソースコード（`.zip`）をダウンロードして解凍します。
2. Windowsのスタートメニューから **「x64 Native Tools Command Prompt for VS 2026」**（開発者用コマンドプロンプト）を開きます。
3. 解凍した curl のディレクトリ内にある `winbuild` フォルダへ `cd` コマンドで移動します。
4. 以下のコマンドを実行し、スタティックライブラリ（静的リンク用）としてビルドします。
   ```cmd
   nmake /f Makefile.vc mode=static VC=18 MACHINE=x64 DEBUG=yes
   nmake /f Makefile.vc mode=static VC=18 MACHINE=x64 DEBUG=no
   ```
5. `builds` フォルダ内に生成される `libcurl_a_debug.lib` と `libcurl_a.lib` を、プロジェクトの `project/externals/curl/lib/Debug/` および `Release/` フォルダにそれぞれ配置します。（※必要に応じて .vcxproj の設定に合わせてファイル名を変更してください）
6. `include/curl/` フォルダの中身も `project/externals/curl/include/` にコピーします。

※ `assimp` などの他のライブラリについても同様に、CMake でビルドするか、メンバー間でビルド済みの `.lib` を直接共有（Google Drive等）して各々の `externals/` に配置してください。

---

---

### 1.3 スクリプト体系とワークスペース健全化運用 (scripts/)
プロジェクトルート直下および `scripts/` 配下には、用途に応じた3大ドメイン（`build/`, `package/`, `maintenance/`）の自動化スクリプトが整備されています。

- **`BuildAndRun.bat` (ルート)**:
  - `scripts/build/build_and_run.ps1` を呼び出し、VS 2026 環境での即時ビルド・実行を行います（引数: `-Configuration Release|Development|Debug|Editor`, `-Rebuild`, `-BuildOnly`）。
- **`Run_Benchmark.bat` (ルート)**:
  - 10,000オブジェクトの極限ストレステスト用スクリプト。Releaseビルドを自動同期し、`TelemetryMonitor.exe` とゲーム本体を連携起動してリアルタイム負荷を検証します。
- **`Launch_Telemetry.bat` (ルート)**:
  - パフォーマンス監視ツール（WPF）`TelemetryMonitor.exe` を単体起動します。
- **`PackageForSubmission.bat` (ルート)**:
  - `scripts/package/PackageForSubmission.ps1` を呼び出し、就活・コンテスト提出用の配布アーカイブを一発生成します。
- **`scripts/maintenance/CleanAssets.bat`**:
  - `scripts/maintenance/AssetSanitizer.ps1` を呼び出し、アセットの監査とクリーンアップを実行します。

#### 1.3.1 ワークスペース健全化ツール (CleanAssets.bat)
リポジトリを常にクリーンで健全な状態に保つため、`scripts/maintenance/CleanAssets.bat` を配備しています。
- **`[1] Audit Mode`**: ソースコード・シーン定義から逆引き走査し、未参照アセットのレポートを表示（ファイル変更なし）。
- **`[2] Dry-Run Mode`**: クリーンアップ実行時の移動・削除プレビューを表示。
- **`[3] Sanitize Mode`**: 未参照ファイルを `backup/` に安全に退避しつつ、ランタイムアセットを浄化。
- **`[4] Restore Backup`**: 直前のバックアップからワンクリックで完全復元。
- **`[5] Clean Cache & Logs`**: `.gitignore` に準拠し、`generated/cache/`、`logs/`、`__pycache__` などの一時生成物を安全に一括消去。

---

### 1.4 外部ツール群の利用と配置 (tools/)
外部ツールはすべてリポジトリの `tools/` ディレクトリに一元管理されています。
- **`tools/BlenderLevelEditor/`**: Blenderアドオンによるステージ作成・コライダー配置・敵スポーン配置ツール。
- **`tools/ShaderGenerator/`**: シェーダー自動生成支援ツール。
- **`tools/TelemetryMonitor/`**: C#/WPFによるリアルタイム外部パフォーマンス監視ツール（ルートの `Launch_Telemetry.bat` よりワンクリック起動可能）。
- **`tools/GeneratePostProcessHlsl.py`**: C++ SSOT (`PostProcessDefinitions.h`) から HLSL用定数ヘッダー (`PostProcessModes.generated.hlsli`) を自動同期生成する CodeGen スクリプト（Visual Studio の Pre-Build Event に統合）。

---

## 2. チーム開発ルールとコーディング規約 (Coding Standards & Best Practices)

### 2.1 アーキテクチャと関心の分離
- **エンジンの独立性**: `IrufemiEngine/` フォルダ配下のコア機能には、特定のゲームやシーンに依存する処理・固有のデータ・アクターを**絶対に含めない**でください。
- **ゲームロジックの配置**: ゲーム固有のロジックやキャラクター制御は、必ず `Application_team/` や `Application_solo/` (または各ゲームの Application フォルダ) 内に記述し、エンジンとアプリケーションの境界を厳格に保ちます。

---

### 2.2 C++ コーディング規約（引数渡し・コンテナ操作のベストプラクティス）

IrufemiEngineのコアやコンポーネントを拡張する際、パフォーマンス（特にコピーコストやエイリアシング回避）を意識した以下の「モダンC++ / AAAエンジン基準」の引数渡しルールを厳守してください。

1. **小さな数学型（16バイト以下）は「値渡し」**
   - 対象: `Vector2`, `Vector3`, `Vector4`, `Quaternion` など
   - 理由: SIMDレジスタ渡しによる高速化と、ポインタのエイリアシング回避のため。
   - 例: `Vector3 Add(Vector3 a, Vector3 b);`

2. **大きな数学型・構造体（16バイト超過）は「`const` 参照渡し」**
   - 対象: `Matrix4x4`, `Transform`, `Segment`, `Ray`, `AABB`, `CollisionResult` など
   - 理由: レジスタに乗り切らずスタックにコピーされるオーバーヘッド（コピーコスト）を防ぐため。
   - 例: `Vector3 Transform(Vector3 v, const Matrix4x4& m);`

3. **文字列 (`std::string`)**
   - **単なる読み取り (検索等) の場合**: `std::string_view` (C++17) を使用する。
   - **メンバ変数に保存 (Sinkパターン) の場合**: 「値渡し ＋ `std::move`」に統一する。
   - 例: `void SetName(std::string name) { name_ = std::move(name); }`

4. **配列 (`std::vector`) からの順序非依存削除は `Irufemi::Container::EraseSwap`**
   - **対象**: オブジェクトリスト、弾薬、シールド、マネージャー内の管理配列など
   - **理由**: `std::vector::erase` は後続要素のメモリシフトが発生するため $O(N)$ のコストがかかります。順序の維持が不要なコレクションでは、末尾と入れ替えて pop する `EraseSwap` を使用することで $O(1)$（定数時間）で高速削除でき、大量オブジェクト破棄時のラグスパイクを防止できます。
   - **ヘッダー**: `#include "Core/Utility/ContainerUtility.h"`
   - **例**:
     ```cpp
     #include "Core/Utility/ContainerUtility.h"

     // 特定のオブジェクトを O(1) で削除（順序は保持されません）
     Irufemi::Container::EraseSwap(activeObjects_, targetObj);
     ```

5. **コンテナ操作ユーティリティ (`Irufemi::Container`) の活用**
   - **ヘッダー**: `#include "Core/Utility/ContainerUtility.h"`
   - **`PushBackUnique(vec, value)`**: 重複を避けて末尾に追加（AddUnique）。既に存在すれば `false` を返し二重登録を防ぎます。
   - **`Contains(vec, value)` / `ContainsIf(vec, pred)`**: `std::find(begin, end, val) != end` の冗長な記述を直感的な真偽値判定に置き換えます。
   - **`IndexOf(vec, value)`**: 配列内における要素のインデックス（未検出時は `-1`）を取得します。
   - **例**:
     ```cpp
     #include "Core/Utility/ContainerUtility.h"

     // 重複なしで安全に追加
     Irufemi::Container::PushBackUnique(listeners_, this);

     // 含まれているかを簡潔に判定
     if (Irufemi::Container::Contains(activeColliders_, collider)) {
         // ...
     }
     ```

---

### 2.3 マルチスレッド安全性の指針（Synchronized<T> の活用と排他制御ルール）

IrufemiEngine はレンダースレッド、オーディオスレッド、ThreadPool によるバックグラウンドタスクなど、高度な並列処理を行っています。データ競合やデッドロックを防止し、保守性を最大化するため、以下のスレッドセーフティ規約を厳守してください。

#### 1. 共有データ保護の第一選択: `Irufemi::Synchronized<T>`
データとそれを保護するミューテックスが生のままバラバラに定義されていると、ロックの掛け忘れや意図しない競合の原因になります。複数のスレッドからアクセスされるデータホルダー、キャッシュ、レジストリ、キュー等には、RAIIモニタ型ラッパー `Synchronized<T>` を使用してください。

- **ヘッダー**: `#include "Core/Utility/Synchronized.h"`
- **基本機能**:
  - `WithRead([&](const T& data) { ... })`: 読み取りアクセス（戻り値の透過的返却対応）
  - `WithWrite([&](T& data) { ... })`: 排他書き込みアクセス
  - `Write()` / `Read()`: RAII ガードオブジェクトを取得（ポインタ構文 `lock->field` で直接アクセス可能）

```cpp
#include "Core/Utility/Synchronized.h"

// 1. 基本的なデータ構造の保護（デフォルトは std::shared_mutex によるリーダー/ライター最適化）
struct CacheData {
    std::unordered_map<std::string, ModelData> models;
};
Irufemi::Synchronized<CacheData> modelCache_;

// 読み取り（複数スレッドで並行アクセス可能）
auto model = modelCache_.WithRead([&](const auto& data) -> std::shared_ptr<ModelData> {
    auto it = data.models.find(name);
    return (it != data.models.end()) ? it->second : nullptr;
});

// 排他書き込み
modelCache_.WithWrite([&](auto& data) {
    data.models[name] = newModel;
});

// 2. 単純な std::mutex を使用したい場合
Irufemi::Synchronized<std::vector<Task>, std::mutex> taskQueue_;
```

#### 2. 遅延キューのスワップ退避パターン（ロック保持時間の極小化）
キューに追加されたコマンドやリクエストを処理する際、ループ全体でロックを保持し続けるとワーカースレッドが待機（競合）してしまいます。`WithWrite` 内でローカル変数に `std::move` してキューを空にし、ロック解放後に重いループ処理を実行するパターンを徹底してください。

```cpp
// 推奨: ロック内でキューをスワップ・ムーブして即座にロックを解放
std::vector<Collider*> toAdd;
pendingCommands_.WithWrite([&](auto& data) {
    toAdd = std::move(data.adds);
    data.adds.clear();
});

// ロック外で安全に重い初期化や登録ループを実行
for (auto* col : toAdd) {
    RegisterColliderInternal(col);
}
```

#### 3. 生ミューテックス・再帰ミューテックスの例外使用基準
以下の明確な技術的理由がある場合に限り、生の `std::mutex` や `std::recursive_mutex` の使用が許可されます。その際は、**必ずメンバ変数宣言部に理由コメントを明記**してください。

1. **`std::condition_variable` との連携**:
   - 言語仕様上 `std::unique_lock<std::mutex>` が直接必要なため（例: `ThreadPool`, `TelemetrySender`）。
2. **階層トラバース・同一スレッドからの再入**:
   - オブジェクトツリーの往復呼び出しやシーン更新中の再帰削除を保護するため `std::recursive_mutex` が必要な場合（例: `GameObject`, `BaseScene`, `SceneObjectRegistry`）。
3. **GPU API / 低レイヤーの処理フロー直列化**:
   - データコンテナの保護ではなく、DirectX 12 API 呼出シーケンスや GPU フェンス待機を直列化する場合（例: `DXCommandManager`, `DescriptorPool`）。
4. **スレッドセーフでない外部 C-API の直列化**:
   - ライブラリ全体をグローバルに排他制御する場合（例: `AssimpMutex`）。

#### 4. 他の GameObject への参照とダングリングポインタ対策 (`std::weak_ptr`)
マルチスレッド環境下では、参照していた他の `GameObject`（例：敵がプレイヤーを追いかける際のプレイヤー情報）が、別のスレッドで同時に破壊（GCによってメモリ解放）される可能性があります。
**生ポインタ (`GameObject*`) をメンバ変数として長期間保持することは厳禁です（Use-After-Free の原因になります）。**

必ず `std::weak_ptr<GameObject>` を使用し、アクセスする瞬間だけ `lock()` を取得して生存確認を行ってください。

```cpp
// ❌ 悪い例（生ポインタ保持はクラッシュの原因）
// GameObject* targetObject_ = nullptr; 

// ⭕ 良い例（weak_ptr で保持）
std::weak_ptr<GameObject> targetObject_;

// --- 使い方 ---
// 1. 他のオブジェクトを設定する時 (shared_from_this() を渡す)
debrisComp->SetTarget(playerObj->shared_from_this());

// 2. 毎フレーム Update でアクセスする時
if (auto target = targetObject_.lock()) { // lock()で生存確認
    if (target->GetIsActive()) {
        auto transform = target->GetComponent<TransformComponent>();
        // targetの座標へ移動する処理など...
    }
} else {
    // ターゲットは既に破壊された場合の処理
}
```

#### 5. コンポーネント実行中の構造変更 (AddChild / AddComponent) について
`GameObject::Update` は並列実行されていますが、「自分自身」の子リスト（`children_`）やコンポーネントリストを変更するような操作（例えば、Update 中に自身へ `AddComponent` したり `AddChild` すること）は、内部配列の再確保（Reallocation）を引き起こす可能性があるため、十分に注意してください。

- **新規オブジェクトの生成**: 新しいオブジェクトをシーンにスポーンさせる場合、`scene->AddGameObject(obj)` は内部でスレッドセーフなキュー(`pendingAdds_`)に積まれるため、Update 中に呼んでも安全です。
- **オブジェクトの破棄**: `gameObject_->Destroy()` も破棄フラグ (`isDestroyed_`) を立てるだけなので、Update 中に呼んでも安全です（次フレームの開始前に一括削除されます）。

#### 6. ThreadPool と TaskGroup による並列タスク実行と待機
重いアセットのデコードや大量の計算をマルチスレッドで並列実行し、すべてのタスクが完了するまで安全にブロック待機したい場合は、`ThreadPool` と `TaskGroup` を組み合わせて使用します。

```cpp
#include "Core/System/ThreadPool.h"
#include "Core/System/TaskGroup.h"

auto threadPool = engine_->GetThreadPool();
auto taskGroup = std::make_shared<TaskGroup>();

// 1. タスクグループを指定してワーカースレッドへタスクを並列投入
for (int i = 0; i < 100; ++i) {
    threadPool->Enqueue(taskGroup, [i]() {
        // 重い計算やファイルデコードなど...
    });
}

// 2. 全タスクが完了するまで低レイテンシで高効率にブロック待機
// （std::condition_variable により、CPU 100%スピンせず全完了の瞬間に起床します）
taskGroup->Wait();
```

---

---

### 2.4 エラーハンドリング・DirectXアサーション規約 (IRUFEMI_CHECK_HR / IRUFEMI_ASSERT_HR)

DirectX 12 APIの呼び出し失敗（`HRESULT` 失敗）は、ゲーム開発において最もクラッシュやフリーズを引き起こしやすい要因です。
Irufemi Engineでは、**Unreal Engine の `check()` / `ensure()` パターン** および **商用AAAゲームエンジン標準のフェイルセーフ設計** に基づき、エラーの「重大度」と「ビルド構成ごとの役割」を明確に分離したエラーハンドリングAPIを提供しています。

#### 1. エラーハンドリングマクロの選定基準

| マクロ | 用途・重大度 | 戻り値 | 設計指針・書き方 |
| :--- | :--- | :--- | :--- |
| **`IRUFEMI_CHECK_HR(hr, msg)`** | **非致命的 (Recoverable)**<br>例: スクショ保存失敗、オプショナルなテクスチャ読込失敗、フレームバッファキャプチャ失敗 | `bool`<br>（成功時 `true`, 失敗時 `false`）<br>`[[nodiscard]]` 強制 | **戻り値を必ず判定して早期リターンまたは代替処理を記述する**<br>```cpp\nif (!IRUFEMI_CHECK_HR(hr, "スクショ保存に失敗しました")) {\n    return false;\n}\n``` |
| **`IRUFEMI_ASSERT_HR(hr, msg)`** | **致命的 (Fatal)**<br>例: SwapChain/CommandQueue生成失敗、必須パイプラインステート(PSO)作成失敗 | `void` | **続行不可能な初期化失敗。即座に例外送出・診断ブレークを発動する**<br>```cpp\nIRUFEMI_ASSERT_HR(hr, "SwapChainの生成に失敗しました");\n``` |

> [!IMPORTANT]
> **`[[nodiscard]]` による静的チェック**:
> `IRUFEMI_CHECK_HR` は `[[nodiscard]]` 属性が付与されています。戻り値を `if` 文等でチェックせずに単独呼び出しした場合、コンパイラ警告（エンジン標準設定ではエラー扱い）が発生します。これにより、エラー検知後のリカバリ漏れをコンパイル時に100%防止します。

#### 2. 4大ビルド構成別の挙動マトリクス

Irufemi Engineでは、実行対象ユーザー（開発者・アーティスト・テスター・プレイヤー）に合わせて、エラー発生時の挙動をビルドごとに最適化しています。

| ビルド構成 | 対象ユーザー | `IRUFEMI_CHECK_HR` 失敗時 | `IRUFEMI_ASSERT_HR` 失敗時 | 目的と安全保証 |
| :--- | :--- | :--- | :--- | :--- |
| **Debug (`_DEBUG`)** | コアプログラマ | `std::cerr` に赤字ログ出力 ➔ **即座にブレーク (`__debugbreak`)** | 詳細エラーダイアログ提示 ➔ **即座にブレーク** | バグの発生箇所をその場で100%炙り出す |
| **Editor (`EditorMode`)** | プランナー / アーティスト | エディタの `ConsolePanel` に赤字ログ ➔ **falseを返しエディタ継続** | `ConsolePanel` にFatalログ ➔ **例外送出 (PlayModeのみ安全停止)** | **エディタ本体と未保存データを絶対にクラッシュさせない** |
| **Development (`DEVELOPMENT`)** | QA / イベント展示会 | 画面・ログダンプに記録 ➔ **早期リターンでゲーム更新継続** | 画面にFatal警告ダンプ ➔ **例外送出または安全終了** | テストプレイや展示を中断させず、ログを収集する |
| **Release (製品版)** | 一般プレイヤー | サイレントリカバリ（ログのみ） ➔ **ゲーム更新を維持** | `PostQuitMessage(0)` 等で **OSごとクリーン終了** | プレイヤーに無用なダイアログを見せず、フリーズを防ぐ |

#### 3. 実装スニペット例

```cpp
#include "Core/Utility/ErrorUtility.h"

// 【非致命的処理】スクリーンショット保存
bool ScreenCaptureManager::SaveCapture(ID3D12Resource* buffer, const std::wstring& path) {
    HRESULT hr = DirectX::SaveWICTextureToFile(...);
    // [[nodiscard]] のため、戻り値のチェックとリカバリ処理が必須
    if (!IRUFEMI_CHECK_HR(hr, "スクリーンショットのディスク保存に失敗しました")) {
        // 失敗してもゲーム本編は中断せず、falseを返して安全に復帰
        return false;
    }
    return true;
}

// 【致命的処理】バックバッファの取得
void DXCommandManager::CreateRenderTargetViews() {
    for (UINT i = 0; i < kFrameBufferCount; ++i) {
        HRESULT hr = swapChain_->GetBuffer(i, IID_PPV_ARGS(&renderTargets_[i]));
        // バックバッファが取得できない場合は描画パイプライン全体が崩壊するため即座にASSERT
        IRUFEMI_ASSERT_HR(hr, "SwapChainからのバックバッファ取得に失敗しました");
    }
}
```

---

### 2.5 メモリ管理と安全性 (C++ / DirectX)
- **スマートポインタの利用**: メモリリークを防ぐため、生ポインタ(`Raw Pointer`)の新規使用は極力避け、用途に合わせて `std::unique_ptr` や `std::shared_ptr` を優先してください。
- **COMオブジェクト管理**: DirectXのオブジェクトを扱う際は、必ず `Microsoft::WRL::ComPtr` を使用して安全にライフサイクルを管理してください。
- **エラーチェックとロギング**: DirectXのAPI呼び出し時やJSON等のパース時は必ずエラーチェックを行い、エラーメッセージの出力には `printf` や `std::cout` ではなく、エンジン標準の `Log::OutPutLog` を使用してください。
  ```cpp
  #include "Core/Utility/Log.h"
  // エディタのコンソールウィンドウ等にも出力されるように、エンジン指定のロガーを使用する
  Log::OutPutLog(std::cerr, "Error: Failed to load file.\n");
  ```

#### GPUリソースの事前確保（Pre-warming）によるラグ防止
リアルタイム性が命のゲームにおいて、実行中のテクスチャ読み込みやシェーダー・VRAMバッファの構築（遅延評価）は**画面のカクつき（Hitch / Stutter）を生む最大の原因**となります。一線級のエンジンと同様に、ゲーム開始前の初期化フェーズ（ロード画面や `Start()` のタイミング）で重い処理を強制的に終わらせる**プレウォーム（事前確保）**を徹底してください。

- **安全なプレウォームの実装方法**:
  初期化時にフルセットの更新処理を呼ぶと、プールの中で休眠中のエフェクトが勝手に発生・消費されてしまう（いざ使うときに出なくなる）バグの原因になります。これを防ぐため、**「発生パラメータは一切送らず、GPUマネージャーに対してハンドルの取得（VRAM領域の予約）だけを行う」専用のメソッド**を用意・使用してください。
  ```cpp
  void ParticleObject::Initialize() {
      // 実行中のラグを防ぐため、安全なプレウォーム（枠だけの事前確保）を行う
      PrewarmSystem();
  }

  void ParticleObject::PrewarmSystem() {
      // パラメータは送信せず、マネージャーに「このテクスチャとブレンドモードを使う」と登録だけ行う
      if (!emitterHandle_.IsValid() && gpuParticleManager_) {
          // 第4引数: enableLighting (ライティング計算フラグ)
          emitterHandle_ = gpuParticleManager_->RegisterEmitter(texturePath_, blendMode_, isUnscaledTime_, false);
      }
  }
  ```

#### 非同期レイキャスト (Async Raycast) と物理クエリの最適化
毎フレーム大量のオブジェクトに対して同期的にレイキャスト（視線判定など）を行うと、メインスレッドの処理落ち（フレームドロップ）の大きな原因となります。
これを防ぐため、`CollisionManager` にはスレッドプールを利用した非同期レイキャストAPI `RaycastAsync` が用意されています。`std::future` を用いてバックグラウンドで処理させ、結果が出たタイミングでキャッシュを更新する **Time-Slicing** の設計を強く推奨します。

---

### 2.6 オブジェクトのライフサイクル管理とプールの安全な運用
シューティングゲームの敵やヒットエフェクトなど、頻繁に生成と消滅を繰り返すオブジェクト（プーリング対象）を実装する際は、メモリ破壊やプールの崩壊を防ぐために以下の**厳密なルール**に従う必要があります。

本エンジンでは、AAA基準の完全なゼロ・アロケーションを実現するため、オブジェクトプール (`ObjectPool<T>`) はポインタではなく **`Handle` (整理券)** による管理へ移行しました。

- **プールへの返却は必ず `Release(handle)` を使用する**
  プーリング対象のオブジェクトが死ぬとき（非アクティブ化されるとき）は、自身が生成時に受け取った `Handle` を用いてプールへ返却してください。`Destroy()` を呼ぶとメモリから消去されプールが崩壊します。
  ```cpp
  // 敵の生成時に Handle をマップなどに記憶しておく（InstanceID をキーにしてABA問題やダングリング参照を防止）
  activeEnemyHandles_[enemy->GetInstanceID()] = handle;

  enemyComp->SetOnDeathCallback([this](GameObject* deadObj) {
      deadObj->SetIsActive(false);
      if (enemyPool_) {
          auto it = activeEnemyHandles_.find(deadObj->GetInstanceID());
          if (it != activeEnemyHandles_.end()) {
              enemyPool_->Release(it->second); // Handle を使って最速で返却
              activeEnemyHandles_.erase(it);
          }
      }
  });
  ```

- **【重要】Handleの安全性 (Generation) とエディタ連携 (EditorMode)**
  新しい Handle には **`generation` (世代)** という概念が組み込まれています。古い Handle（すでに返却済みのもの）を使って `Resolve(handle)` を呼び出しても、世代が不一致となるため自動的に弾かれて `nullptr` が返ります（ダングリングポインタの完全な防御）。
  また、デバッグビルド（`EditorMode` 等）の時は、Handle 構造体の中に `debugPtr` という実体へのポインタが自動で含まれます。これにより、エディタ（ImGui）のインスペクタ上で「ただの数字の羅列」ではなく、実際のオブジェクトの名前（命名）を確認しながらデバッグを行うことができます。

- **LifetimeComponent の TimeoutAction を活用する**
  一定時間で消滅するエフェクトなどに `LifetimeComponent` をアタッチする場合、インスペクタ上で **Timeout Action** を変更できます。プール運用なら必ず `Disable` を設定してください。コードから生成する際は以下のように上書きすると安全です。
  ```cpp
  auto obj = gameObject_->Instantiate(effectPrefabPath);
  if (auto lifetime = obj->GetComponent<LifetimeComponent>()) {
      lifetime->SetTimeoutAction(TimeoutAction::Disable);
  }
  ```

- **【重要】Updateループ中の遅延削除 (Deferred Deletion / Pending Kill)**
  コンポーネントの `Update()` 処理中に、自分自身や子オブジェクトを即座にツリーから引き剥がす (`RemoveChild` や `ReleaseGameObject` など) 操作を行うと、ループ処理中の親の `children_` 配列のイテレータが無効化され、**アクセス違反（クラッシュ）**の原因となります。
  これを防ぐため、マネージャーによる **遅延削除キュー (Pending Kill)** を必ず実装してください。
  1. マネージャークラスに `std::vector<std::shared_ptr<GameObject>> pendingReleases_;` を定義する。
  2. 削除を要請する側は `manager_->MarkForRelease(this_obj)` を呼ぶだけにする（この時点ではまだ消えない）。
  3. マネージャーの `Update()` の最後（すべてのオブジェクトのUpdateが完了した安全なタイミング）で、キューに溜まったオブジェクトを一気にプールへ返却し、キューをクリアする。

- **パフォーマンスと Data-Oriented な設計 (自己登録型コンポーネント)**:
  シーン内の特定のオブジェクト（敵やボスなど）を毎フレーム再帰的に検索する処理は、オブジェクト数が増えるにつれて急激なCPU負荷（$O(N)$ 問題）を引き起こします。
  これを防ぐため、ロックオン対象などの特定の性質を持つオブジェクトの管理には、**自己登録型コンポーネント（例: `TargetableComponent`）** を使用してください。生成時（`OnEnable`）に対象がグローバルな静的リストへ自動登録され、検索側はリストを直接イテレートするだけで定数時間かつキャッシュ効率の良いアクセスが可能になります。

- **プール対応コンポーネントの作り方 (`IsPooledComponent`)**:
  特定のコンポーネントを `ComponentPool` の管理下に置くには、対象のコンポーネントクラスの宣言の下で `IsPooledComponent` のテンプレート特化を行います。
  ```cpp
  #pragma once
  #include "Framework/Component/Component.h"
  #include "Core/System/ComponentPool.h"

  class TransformComponent : public Component {
      // ... 通常のコンポーネント実装 ...
  };

  // ComponentPool 対応を宣言（ファイルの末尾に記述）
  template<> struct IsPooledComponent<TransformComponent> : std::true_type {};
  ```
  この一行を追加するだけで、`GameObject::AddComponent<TransformComponent>()` や、エディタ・JSONからの自動ロード（`ComponentFactory`）が**すべて自動的にプール経由での生成**に切り替わります。

- **DODの恩恵を最大限に受ける一括更新処理 (`UpdateAll`)**:
  プール化されたコンポーネントは、`ComponentPool::ForEach` を使って全インスタンスを一気にバッチ処理することができます。
  現在、`TransformComponent` の座標行列計算はこの機能を用いて `BaseScene::Update` 内で毎フレーム最初に一括計算 (`TransformComponent::UpdateAll()`) されています。これにより、何万ものオブジェクトの Transform 行列計算がキャッシュミスなしで高速に行われます。
  ```cpp
  void TransformComponent::UpdateAll() {
      currentFrame_++;
      ComponentPool<TransformComponent>::GetInstance().ForEach([](TransformComponent& transform) {
          transform.ComputeMatrix();
      });
  }
  ```
  ※プール対応コンポーネントはゲーム終了時にプールから安全にメモリ解放されるため、手動の解放処理は不要です。

---

### 2.7 命名規則・コードスタイル
- **メンバ変数の命名**: `m_` などの接頭辞は使用せず、**キャメルケースの末尾にアンダーバー**をつけるスタイル (`variableName_`) に統一してください。
- **ヘッダーの注釈**: 関数やクラスのコメントは「Doxygen形式」で記述してください。
- **インクルードガード**: `#pragma once` を使用してください。
- **既存への適応**: 新しくクラスや関数を追加する際は、必ず周囲の「既存のコードベースの命名規則」に合わせ、自己流のスタイルを混入させないでください。
- **文字コードとフォーマット**: ファイルはすべて `UTF-8 (署名なし)` で保存し、`.clang-format` による自動整形を活用してください。

---

### 2.8 外部ライブラリ利用規約と共通ユーティリティ (JsonUtility / FileSystem / StringUtility)

外部ライブラリの肥大化やコンパイル速度低下、二重インクルードを防ぐため、プロジェクト全体で以下のルールを徹底しています。

#### 1. 外部ヘッダーの直接インクルード禁止
- `<nlohmann/json.hpp>` や `<assimp/*>` などの生ヘッダーを各 `.h` / `.cpp` で直接インクルードすることは禁止されています。
- JSON を扱う場合は必ず `#include "Core/Utility/JsonUtility.h"` を使用し、`Irufemi::Json` 型エイリアスを使用してください。

#### 2. ファイル読み書き・パス解決の公式パターン
- `std::ifstream` や `std::ofstream` を直にオープンしてパースするボイラープレートは記述せず、`Irufemi::JsonUtility` のユーティリティ関数を使用してください。
- 実行環境（実行時カレントディレクトリの違い）を吸収するため、パス解決には必ず `Irufemi::FileSystem::GetResourcePath(...)` を使用してください。

```cpp
#include "Core/Utility/JsonUtility.h"
#include "Core/Utility/FileSystem.h"

// 1. パスの安全な解決
std::string fullPath = Irufemi::FileSystem::GetResourcePath("resources/config/example.json");

// 2. 読み込み (ファイルのオープンと例外処理を内包・成否を bool で返却)
Irufemi::Json rootJson;
if (!Irufemi::JsonUtility::LoadFromFile(fullPath, rootJson)) {
    // 読み込み失敗ログは内部で出力されるため、安全に早期リターン可能
    return;
}

// 3. 書き出し (インデント付き整形保存)
Irufemi::JsonUtility::SaveToFile(fullPath, rootJson, 4);
```

#### 3. 画面解像度・アスペクト比のマジックナンバー禁止
- コード内で `1280.0f` や `720.0f` 等の解像度数値をハードコードすることは禁止されています。
- ウィンドウや描画解像度を参照する際は、必ずエンジンの正規アクセサ（`engine->GetClientWidth()` / `engine->GetClientHeight()`、または内部解像度 `engine->GetGameResolutionWidth()` / `engine->GetGameResolutionHeight()`）を利用して一元管理してください。アスペクト比が必要な場合も、これらのアクセサから動的に算出し、`16.0f / 9.0f` 等の決め打ちを避けてください。

#### 4. 文字列相互変換ユーティリティ (ConvertString)
- Windows API (UTF-16: `std::wstring`) やフォント描画（`TextRendererComponent`）と、エンジン内部・JSON (UTF-8: `std::string`) 間で文字列を変換する際は、生 Win32 API (`MultiByteToWideChar` 等) を直接記述せず、共通ユーティリティ `ConvertString` を使用してください。
- **ヘッダー**: `#include "Core/Utility/StringUtility.h"`
```cpp
#include "Core/Utility/StringUtility.h"

// string (UTF-8) -> wstring (UTF-16)
std::wstring wstr = ConvertString("Hello World");

// wstring (UTF-16) -> string (UTF-8)
std::string str = ConvertString(L"日本語テキスト");
```

---

## 3. エディタの使い方とレベル制作 (IrufemiEditor & Level Design)

### 3.1 エディタ画面の基本レイアウト

エディタの画面構成（ドッキングウィンドウの配置など）が崩れてしまった場合や、チーム内で定められた最新の共通レイアウトに更新したい場合は、以下の手順で復元できます。

1. エディター画面上部のメニューバーから **`Window`** をクリック
2. **`Layout` -> `Load Default Layout`** をクリック

現在の自分の使いやすい配置をチームの新しいデフォルト設定にしたい場合は、並び替えたあとに **`Save Current as Default`** を押し、変更された `default_imgui.ini` をGitでコミットしてください。
（※初回クローン時は自動的に共通レイアウトが適用されるようになっています）

#### コンソールパネル (Console)
エンジンからのログ出力（エラー、警告、情報）は、エディタ下部の **Console** パネルに表示されます。
コード内で `Log::OutPutLog(std::cout, "メッセージ\n")`（通常情報ログ）または `Log::OutPutLog(std::cerr, "エラー\n")`（赤字エラーログ）を呼び出すとリアルタイムに反映され、パネル上の「Clear」ボタンでログを消去できます。

---

### 3.2 Project Browser の操作（2ペイン表示とアセットD&D配置）
IrufemiEngine のエディタは、モダンなゲームエンジン（Unity等）ライクなアセット管理をサポートしています。

1. **2ペイン・タイル表示**
   - **左ペイン (ツリー表示)**: フォルダの階層構造のみを表示します。クリックするとその中身が右ペインに表示されます。
   - **右ペイン (タイル表示)**: FontAwesome アイコン付きで、ファイルとフォルダがグリッド状に並びます。画面幅に合わせて自動で折り返されます。

2. **コンテキストメニュー (右クリック操作)**
   - ファイル/フォルダ上で右クリックすると、「**Rename (名前変更)**」や「**Delete (削除)**」が可能です。
   - 余白を右クリックすると、「**Create Folder (新規フォルダ作成)**」ができます。
   - ※これらの操作はOSの実ファイルシステム（Windowsのフォルダ等）に即座に反映されます。

3. **ドラッグ＆ドロップによるアセットの即時配置**
   - **画像の配置**: Project Browser にある `.png` や `.jpg` などの画像を **Hierarchy** にドラッグ＆ドロップすると、`SpriteRendererComponent` がアタッチされた GameObject が自動生成されます。
   - **3Dモデルの配置**: `.obj` や `.fbx` などをドラッグ＆ドロップすると、`MeshRendererComponent` がアタッチされた GameObject が自動生成されます。
   - D&D 時にアセットの相対パスが自動的にコンポーネントへセットされるため、すぐに画面上でプレビュー可能です。

---

### 3.3 プレハブ (Prefab) システム
オブジェクトの構成やパラメータをテンプレート化して再利用する仕組みです。

1. **Prefabの作成・保存**:
   - Hierarchy パネルでオブジェクトを右クリックし、「**Save as Prefab**」を選択します。
   - `resources/prefabs/` 以下に `.prefab.json` として、コンポーネントやパラメータがすべて保存されます。
2. **エディタからの配置**:
   - Project Browser に青いキューブのアイコンで表示されます。これを **Scene View** にドラッグ＆ドロップすると、即座にインスタンス化（実体化）して配置されます。
3. **C++コードからの動的生成 (ランタイム)**:
   - ゲーム中に「敵」や「弾」を安全にスポーンさせるには、シーンの遅延キューヘルパー `InstantiatePrefab` または `PrefabManager` を使用します。
   ```cpp
   // 推奨: シーンの遅延キューを経由して生成 (座標指定 & 自動Add & イテレータ破壊防止)
   auto bullet = scene->InstantiatePrefab("resources/prefabs/Bullet.prefab.json", spawnPos);

   // または エンジンから PrefabManager を取得してクローン生成
   auto bullet = scene->GetEngine()->GetPrefabManager()->Instantiate("resources/prefabs/Bullet.prefab.json");
   if (bullet) {
       bullet->GetTransform()->SetPosition(spawnPos);
       scene->AddGameObject(bullet); // 遅延キューに追加され、次フレームUpdate前に安全にStart()が実行される
   }
   ```
   > [!NOTE]
   > `SceneSerializer::LoadPrefab` も後方互換としてそのまま動作しますが、内部的には `PrefabManager` へ委譲されています。シーン遷移時にはキャッシュが自動解放されます。

4. **Prefab分離編集モード (Isolation Stage)**:
   - シーン内の他オブジェクトに干渉されることなく、プレハブ単体を隔離されたクリーンな空間で編集できます。
   - **進入方法**:
     - Project Browser 上で `.prefab.json` を **ダブルクリック**
     - Hierarchy 上のPrefabインスタンスを **ダブルクリック**
     - Inspector 上部のPrefabバナーの **`[Open Prefab]`** ボタンをクリック
     - コンポーネント内の `.prefab.json` パス項目横の **`[Open]`** ボタンをクリック
   - **編集画面の機能**:
     - 原点 (0, 0, 0) XZ平面グリッドが自動表示され、スケールや接地感の確認が容易です。
     - Hierarchy 最上部に階層インジケータ（`◀ Exit Prefab (SceneName)`）が表示され、クリックまたは **`Esc`** キーで安全にシーンへ復帰できます。
     - **`Ctrl + S`** を押すと、Prefab編集モード中はシーンではなく編集中の `.prefab.json` が自動保存されます。
   - **誤操作防止ガード**:
     - プレハブ編集中は、誤動作防止のため **Playモードへの移行が自動でロック** されます（ツールバーのPlayボタンも無効化）。
     - シーンとプレハブ間でのUndoデータの混ざりを防ぐため、突入時および離脱時にUndo履歴が自動でクリアされます。
     - Prefabのルートオブジェクト（Root）に対する誤った削除や複製操作は保護されます。

5. **カスタムInspector（エディタ拡張）開発向け共通UIウィジェット (`ComponentUIHelpers`)**:
   - `IrufemiEditor` 内で独自のコンポーネントインスペクター（`IComponentEditor`）を実装する際、[`ComponentUIHelpers.h`](project/IrufemiEditor/UI/ComponentUIHelpers.h) の共通関数を利用できます。
   ```cpp
   // IrufemiEditor 内でのインクルード
   #include "IrufemiEditor/UI/ComponentUIHelpers.h"

   // プレハブパスの横にワンクリックで編集モードを開くボタンを描画
   IrufemiEditor::ComponentUIHelpers::DrawOpenPrefabButton(dronePrefabPath_);

   // Inspector最上部にPrefabメタデータバナーを描画
   IrufemiEditor::ComponentUIHelpers::DrawPrefabHeaderBanner(prefabAssetPath_);
   ```
   ※ゲーム固有の複雑なパラメータ編集（ウェーブ出現タイムラインやボスの多層シールド制御）では、`project/Application_solo/Editor/` 内の `BossComponentEditor` や `WaveManagerComponentEditor` がこの仕組みを活用して実装されています。

---

### 3.4 インスペクターと Hierarchy の便利操作

#### インスペクター (Inspector) の便利な操作機能
インスペクター上で作業を効率化するための便利な機能が備わっています。これらはすべて **Undo/Redo (Ctrl+Z / Ctrl+Y)** に対応しています。

1. **3カラムレイアウトと全プロパティの初期化リセット機能**
   - コンポーネントのパラメータは、「プロパティ名」「値」「リセットボタン」の3カラムレイアウトで表示されます。
   - すべての値の右端に「矢印アイコン（リセットボタン）」が付いており、クリックするだけでその値を安全に初期状態へ戻すことができます。この操作も含め、全てUndo/Redoが可能です。
   - また、ウィンドウの幅を変更しても、プロパティ名が見切れないように自動で動的にサイズ調整（55%幅確保）が行われます。
2. **コンポーネントの削除 (Remove Component)**
   - インスペクターに追加されている各コンポーネントの「ヘッダー（名前が書かれた帯の部分）」を **右クリック** すると、コンテキストメニューが表示されます。
   - そこから「**Remove Component**」を選択することで、不要なコンポーネントを安全に削除できます。
3. **Transform の一括リセット**
   - `TransformComponent` のヘッダーの右端にある「**Reset**」ボタンをクリックすると、Position (0, 0, 0)、Rotation (0, 0, 0)、Scale (1, 1, 1) へ一括で初期化されます。
4. **テクスチャのドラッグ＆ドロップ割り当て**
   - `SpriteRendererComponent` などのテクスチャ項目（コンボボックス等のUI）に対して、Project Browser から画像ファイル（`.png` や `.jpg` など）を直接 **ドラッグ＆ドロップ** することで、すぐにテクスチャを適用できます。

#### Hierarchy と Scene View の便利な操作機能
シーンが複雑になりオブジェクトが増えてきた場合、以下の機能を使って整理・保護や効率的な操作を行うことができます。

1. **インライン・リネーム (Inline Rename)**
   - Hierarchy 上でオブジェクトの名前部分を **ダブルクリック** すると、その場で名前を直接編集モード（テキスト入力）に入ることができます。Inspector を開かなくても素早い名前変更が可能です。
2. **可視アイコンとロックアイコン (Eye & Lock)**
   - **目のアイコン (可視性)**: Hierarchy の右端にある目のアイコン（👁/🚫）をクリックすると、オブジェクトの Active/Inactive を即座に切り替えられます。
   - **南京錠アイコン (保護)**: 鍵アイコン（🔒/🔓）をクリックすると編集がロックされます。ロックされたオブジェクトは以下の操作が制限されます：
     - Scene View 上でのギズモ操作の無効化（動かせなくなる）
     - Inspector 上での全パラメータ編集、コンポーネントの追加・削除の無効化（Read-Onlyになる）
     - ドラッグ＆ドロップによる階層移動の禁止（間違って他のオブジェクトの子にしてしまう事故を防ぐ）
3. **フォルダ機能 (Folder)**
   - Hierarchy 上の空白部分を **右クリック** し、「**Create Folder**」を選択すると、空のフォルダ（`isFolder_ = true` の GameObject）が作成されます。
   - 専用のアイコン（📂）が付き、他のオブジェクトを見やすくグループ化できます。フォルダは Scene View 上でギズモが非表示になります。
4. **Scene View のオーバーレイUI (Overlay UI)**
   - Scene View の右上に、半透明のオーバーレイパネルが常駐しています。
   - ここから、ギズモの操作モード（Local/World）、操作ツール（Translate, Rotate, Scale, Bounds）の切り替えや、コライダーのデバッグ表示のON/OFFが素早く行えます。
   - **Bounds ツール**: AABBやOBBコライダーなどのサイズを、Scene View上のハンドルをドラッグして視覚的に調整できる便利なツールです。
   - **Game Camera スナップ機能 ([Cam] ボタン / [P] キー)**:
     - ツールバー右端の **`[Cam]`** ボタンをクリックするか、Scene View上で **`P`** キー（または **`Ctrl + Shift + F`**）を押すと、シーン内の `MainCamera`（およびプレイヤー追従 `TargetFollowComponent`）からPlay開始時と全く同一のカメラ座標・回転角度・画角（FOV）を即座に算出し、Scene View の視点をゲーム視点へピタッとスナップ（同期）させます。
     - Playモードに入ることなく、実際のプレイヤー視点からフォグや背景天球、エネミーの配置がどう見えるかを瞬時に確認できます。スナップ後も通常通りマウス操作でパン・オービット・ズームが可能です。

#### メニューバーの便利機能
画面最上部のメニューバーからも、様々なアクションにアクセスできます。

- **GameObject メニュー**: 
  - ここから即座にプリミティブオブジェクト（Cube, Sphere, 2D Spriteなど）や空のオブジェクトをシーンに追加できます。Hierarchyの右クリックメニューと同じ機能です。
- **Window メニュー**:
  - **Performance**: オンにすると、現在のフレームレート（FPS）やデルタタイム、フレームごとの処理時間（ms）を確認できる「Performance」ウィンドウが表示されます。ゲームの最適化時の確認に便利です。
  - **Layout**: UIレイアウトを初期状態に戻したり、現在の配置をデフォルトとして保存できます。
    - **Reset Layout**: パネルを誤って閉じてしまったり、配置がおかしくなった場合に、ドッキング状態を強制的に初期配置へ復元します。

---

### 3.5 Play / Pause コントロールとゲーム時間の管理
メニューバーの中央にあるコントロールから、エディタ上でのゲームの進行を制御できます。

1. **Play / Stop (▶ / ■)**
   - **Play**: Editモードからゲームを実行状態（Playモード）に移行します。移行した瞬間に現在のシーン状態が自動でバックアップされ、**Stop** を押すと変更が破棄されて Editモードの初期状態に完全にリセットされます。
2. **Pause (⏸)**
   - ゲーム実行中に時間を一時停止します。
   - ポーズ中はゲーム内の `GetDeltaTime()` が常に `0.0f` を返すようになるため、キャラクターの移動や物理演算などの更新処理がすべて止まります。
   - 一方、エディタのUIやカメラ操作などは実時間である `GetRealDeltaTime()` を使用しているため、ゲームが止まっている状態でもシーンを自由に観察したり、Inspectorからパラメータを調整することが可能です。

#### スクリーンショット機能 (Screen Capture)
エディタの「Engine」ウィンドウにある「Screen Capture」タブから、ゲーム画面のスクリーンショットを様々な形式で保存できます。
保存先は `resources/screenshots/` フォルダ内で、ファイル名には自動で現在時刻が付与されます。

- **Capture (Scene Only)**: ImGuiなどのエディタUIが含まれない、純粋なゲームシーンのみの描画結果を保存します。AIによる画面の評価等に最適です。
- **Capture (With UI)**: エディタUI込みの最終的な画面状態を保存します。
- **Capture (Alpha)**: 背景（Skybox等）を透過したアルファチャンネル付きの画像を保存します。
- **Capture (Depth)**: シーンの深度情報を可視化した画像を保存します。

※保存処理はスレッドプールを利用して非同期で行われるため、キャプチャ時にゲームの進行が大きく止まる（カクつく）ことはありません。
※`resources/screenshots/` フォルダは Git の追跡やホットリロードの対象外となるように設定されています。

---

### 3.6 AI Magic Brush (AI自動マテリアル生成)
エディタのUI上から、プロンプト（テキスト）を入力するだけで指定した3Dモデルに合わせたマテリアル・テクスチャ・シェーダーをAIが自動生成・適用する機能です。

1. **使い方**:
   - Hierarchy上で対象となる3Dモデル（GameObject）を選択します。
   - エディタの `ShaderGenerator` ツールウィンドウを開きます。
   - 「Prompt」入力欄に作成したい質感や色味の指示（例: `A metallic red armor with glowing blue lines`）を入力し、**「Generate AI Shader」** ボタンを押します。
   - ※AIが生成したシェーダーでエラーが起きた場合や、期待した結果と異なる場合は、**「Fix (Evaluate & Fix)」** ボタンを押すことで、AIが現在の画面（スクリーンショット）とコンパイルエラーを自己分析し、自動で修正して再適用します。
2. **バックグラウンド・サーバーの仕様**:
   - この機能は、C++エンジン本体とは独立した **Pythonサーバー (`tools/ShaderGenerator/main.py`)** とローカルAPI通信（`127.0.0.1:8000`）を行うことで動作しています。
   - 「Start Server」ボタンを押すと、C++側からバックグラウンドプロセスとしてPythonサーバーが自動起動します。
3. **トラブルシューティング**:
   - **サーバーが起動しない / エラーが出る**: すでに別のプロセスがポート8000を使用しているか、クラッシュ等でゾンビプロセスが残っている可能性があります。エディタUIから「**Stop Server**」を押してプロセスを安全にクリーンアップした後に、再度「**Start Server**」を押してください。

---

---

## 4. コアシステムとシーン管理 (Engine Architecture, Scene & GameObject)

### 4.1 エンジンアーキテクチャ概要 (IrufemiEngine)
- **コアヘッダー**: `#include "Core/System/IrufemiEngine.h"`
- **拡張インターフェース**: `#include "Core/System/IEngineExtension.h"`

エンジン全体を統括するコアクラスです。`WinApp`（ウィンドウ管理）や `DirectXCommon`（DirectX12初期化）を保持し、メインループ（`Update` と `Draw`）を回します。ゲームアプリケーション全体で1つのインスタンスのみが存在します。
`IEngineExtension` によるプラグイン拡張機構を備えており、エディタツールや展示用システムなどの独立機能をエンジン本体を改変することなく安全に追加・管理できます。

---

### 4.2 SceneManager と IScene (シーン管理)
`SceneManager` は画面遷移、シーンスタック（Push/Pop）、およびフェーズ別非同期ロード（Closing, Initializing, LoadingWait, Opening）を統括します。
- **ヘッダー**: `#include "Framework/Scene/SceneManager.h"`
- **基底クラス**: `#include "Framework/Scene/BaseScene.h"`

#### シーンの作り方
ゲームの画面（タイトル、インゲーム、リザルトなど）を切り替えるための仕組みです。
新しいシーンを追加する場合は `IScene` または `BaseScene` を継承したクラスを作成します。
```cpp
#pragma once
#include "Framework/Scene/BaseScene.h"
#include <memory>

class ExampleScene : public BaseScene {
public:
    ~ExampleScene() override = default;
    
    // シーン遷移時に一度だけ呼ばれる（リソース読み込みなど）
    void Initialize(IrufemiEngine* engine) override;
    
    // 毎フレーム呼ばれる（ロジックの更新）
    void Update() override;
    
    // 毎フレーム呼ばれる（描画リクエストの送信）
    void Draw() override;

    // --- ライフサイクル関数（必要に応じてオーバーライド） ---
    // void OnEnter() override;   // アクティブになった時
    // void OnExit() override;    // 非アクティブ・破棄される直前
    // void OnSuspend() override; // 上に別のシーンがPushされた時
    // void OnResume() override;  // 上のシーンがPopされ最前面に戻った時

private:
    // コンポーネント指向を利用するため、特定のオブジェクトは SceneManager (GameObject) 経由で管理します
};
```

#### シーン遷移（自動データ駆動遷移と手動遷移）
シーンを切り替えるには主に2つの方法があります。

**1. データ駆動の自動遷移 (推奨)**
あらかじめエディタで作った `Title.json` などを指定して画面を遷移します。C++で専用のシーンクラスを作る必要はありません。
```cpp
// "Title" (Title.json) を読み込んで遷移。トランジション効果は Fade で 1.0秒かける
engine_->GetSceneManager()->LoadScene("Title", SceneTransition::Type::Fade, 1.0f);

// ホワイトアウト遷移（白フェード）を行う場合: customColor に Vector4(1,1,1,1) を指定
engine_->GetSceneManager()->LoadScene("Result", SceneTransition::Type::Fade, 1.5f, EaseType::EaseOutQuad, Irufemi::Vector4(1.0f, 1.0f, 1.0f, 1.0f));

// 重力ワープ遷移を行う場合
engine_->GetSceneManager()->LoadScene("InGame", SceneTransition::Type::GravitySingularity, 1.2f, EaseType::EaseInOutQuad);
```

##### 【選べるトランジション効果 (`SceneTransition::Type`)】
エンジンには以下の6種類の高品位トランジションが標準搭載されています。

| トランジション種別 (`SceneTransition::Type`) | 視覚効果と用途 |
| :--- | :--- |
| **`Fade`** | 画面全体を徐々に指定色（黒・白など）で塗りつぶす最も標準的なフェード遷移。第5引数の `customColor` で白や任意色に変更可能。 |
| **`Dissolve`** | ノイズテクスチャを利用し、画面がバラバラに崩れ去る、または再構築されるようなディゾルブ演出。 |
| **`Slide`** | 画面の端からワイプ（掃き取り）のように色がスライドしてくる演出。 |
| **`RadialBlur`** | 画面中心に向かって引き込まれるような放射状ブラーを伴いながら暗転遷移する演出。 |
| **`RadialBlurWhite`** | 放射状ブラーがかかりながら真夜中から真昼へ抜けるようなホワイトアウト遷移演出。 |
| **`GravitySingularity`** | 空間歪み・重力色収差・流体渦巻きが連動する重力特異点崩壊・ワープ遷移演出。インゲーム突入時等に最適。 |

**2. C++クラスによる手動遷移 (std::string_view によるゼロコピー呼び出し)**
`BaseScene` を継承した専用クラスへ遷移します。引数はすべて `std::string_view` に統一されており、一時オブジェクトを生成せず高速に処理されます。
また、シーン名文字列をハードコードせず、アプリケーション層で定義された定数（SSOT）を渡すことが推奨されます。
```cpp
if (isClear) {
    // 登録済みの "ClearScene" クラスに遷移する (string_view 渡し)
    engine_->GetSceneManager()->TransitionTo("ClearScene", SceneTransition::Type::Fade, 1.0f);
}

// 現在のシーンの上に一時的なシーン（ポーズ画面など）を重ねる場合
engine_->GetSceneManager()->PushScene("PauseScene");

// 重ねたシーンを終了し、元のシーンに戻る場合（PauseScene側で呼ぶ）
engine_->GetSceneManager()->PopScene();
```

#### 非同期遷移フェーズとロード画面の制御
`SceneManager` は重いシーンの生成・破棄時にメインスレッドのフレームレートを落とさないよう、以下のフェーズ遷移を非同期で制御します。
- **`TransitionPhase::Closing`**: 前のシーンのフェードアウト中（現在のシーンの `Update()` は継続）
- **`TransitionPhase::Initializing`**: ワーカースレッドで新シーンのメモリ割り当て・初期化を実行中
- **`TransitionPhase::LoadingWait`**: 初期化完了後、最小暗転時間待機中（`LoadingScreen` を描画）
- **`TransitionPhase::Opening`**: 新しいシーンのフェードイン中

```cpp
// 1. チラつき防止機能付きで、現在ローディング画面を描画すべきか判定
if (engine_->GetSceneManager()->ShouldDrawLoadingScreen()) {
    // ローディング画面の描画処理
}

// 2. インゲームプレイ中の誤ロード画面割り込み防止
// SceneManager::IsLoading() は非同期遷移待機フェーズ中のみ true を返すよう安全化されています。
// インゲーム中のアセット非同期読み込み等で誤って LoadingScreen が割り込むことはありません。

// 3. PauseScene などのスタックシーンから呼び出し元のシーン名を取得
std::string prevScene = engine_->GetSceneManager()->GetPreviousSceneName();
if (prevScene == "GameScene") {
    // GameScene からポーズされた場合の処理
}
```

##### 【カスタムローディング画面の実装 (`ILoadingScreen`)】
独自のローディング画面（ゲージアニメーションやTips表示など）を描画したい場合は、エンジン提供の `ILoadingScreen`（`#include "Core/System/ILoadingScreen.h"`）を継承したクラスを作成し、エンジンに注入します。

```cpp
#include "Core/System/ILoadingScreen.h"
#include "Core/System/IrufemiEngine.h"

class CustomLoadingScreen : public ILoadingScreen {
public:
    void OnDrawLoadingScreen(IrufemiEngine* engine, float deltaTime) override {
        // ローディング中の2Dスプライトやテキスト、アニメーションを描画
    }
    void SetFadeAlpha(float alpha) override {
        // 暗転フェード進行度に応じたアルファ制御
    }
};

// ゲーム初期化時（GameApplication等）にエンジンへ登録
engine->SetLoadingScreen(std::make_shared<CustomLoadingScreen>());
```

#### GameObject の検索（名前、タグ、ID）
シーン内に存在する GameObject を取得したい場合、用途に合わせて以下の3つの検索機能を利用できます。

**1. 名前で検索 (`FindGameObject`)**
特定の名前を持つオブジェクトを1つ探す場合に使用します。
```cpp
auto playerObj = scene->FindGameObject("Player");
```
※同名のオブジェクトが複数存在する場合は、最初に見つかったものが返されます。

**2. タグで一括検索 (`FindGameObjectsWithTag`)**
「Enemy」や「Obstacle」など、特定の役割を持つオブジェクトをまとめて取得したい場合に使用します。タグは `GameObject::SetTag("Enemy")` やエディタから設定できます。
```cpp
// "Enemy" タグを持つすべてのオブジェクトを取得
std::vector<std::shared_ptr<GameObject>> enemies = scene->FindGameObjectsWithTag("Enemy");
for (auto& enemy : enemies) {
    // 敵全体に対する処理
}
```

**3. インスタンスIDで検索 (`FindGameObjectByID`)**
オブジェクトの生成時に自動で割り当てられる一意のID（`uint64_t`）を利用して、確実に特定のインスタンスを取得します。
```cpp
uint64_t targetId = targetObj->GetInstanceID();
// ... 後でIDを使って再取得する
auto obj = scene->FindGameObjectByID(targetId);
```

#### 動的生成 (Clone) 時の自動命名ルール
`GameObject::Clone()` を使用してオブジェクトを複製すると、エディタ上の識別や名前検索の競合を防ぐため、名前に自動でサフィックス（`(1)` など）が付きます。
* 例: `"Enemy"` を Clone → `"Enemy(1)"`
* さらに Clone → `"Enemy(2)"`
これにより、プレハブから動的生成した敵などもインスペクター上で個別に識別しやすくなっています。

---

### 4.3 GameObject と親子階層構造

本エンジンは、Unityなどのモダンなエンジンと同様のコンポーネント指向で設計されています。シーン内のオブジェクト（`GameObject`）に対して、様々な機能（`Component`）をアタッチ・取得することでゲームロジックを構築します。

#### 基本的なコンポーネントの取得
アタッチされているコンポーネントを取得するには、`GetComponent<T>()` を使用します。
```cpp
// 自身にアタッチされている TransformComponent を取得する
if (auto transform = gameObject_->GetComponent<TransformComponent>()) {
    transform->SetPosition(Vector3(0, 10, 0));
}
```

#### GetComponentsInChildren を使った子孫の探索
`GameObject` 自身およびすべての子孫階層から特定のコンポーネントを一括検索する強力な機能が備わっています。特定のオブジェクト（プレハブのルートなど）の下に連なっているパーティクルやコライダーを一斉に操作したい場合に非常に便利です。

```cpp
auto obj = gameObject_->GetScene()->FindGameObject("Player");

// 1. ツリー全体から、指定した型のコンポーネントを「すべて（配列で）」取得する
auto emitters = obj->GetComponentsInChildren<ParticleEmitterComponent>();
for (auto pe : emitters) {
    pe->Restart(false); // 全てのエミッターを一斉に再発火
}

// 2. ツリー全体から、指定した型のコンポーネントを「1つだけ」取得する
if (auto collider = obj->GetComponentInChildren<ColliderComponent>()) {
    collider->SetIsActive(false);
}
```

**【重要】**
この探索機能は「自身（ルート）にアタッチされているコンポーネント」も検索対象に含まれます。そのため、ルートに目的のコンポーネントがあるかどうかを事前に気にする必要はなく、非常にシンプルで堅牢なコードを記述できます。

#### コンポーネントのライフサイクル (OnEnable / OnDisable)
コンポーネントには初期化(`Initialize`)や毎フレームの更新(`Update`)に加えて、オブジェクトが有効化・無効化されたタイミングで呼ばれるフックが用意されています。

- **`OnEnable()`**: `GameObject` の `SetIsActive(true)` が呼ばれた際や、アクティブな状態でコンポーネントがアタッチされた直後に呼ばれます。オブジェクトプールから復帰した際の状態リセットや、イベントの登録に最適です。
- **`OnDisable()`**: `GameObject` の `SetIsActive(false)` が呼ばれた際や、破棄される直前に呼ばれます。イベントの解除などに使用します。

---

### 4.4 TransformComponent と階層構造

本エンジンの `TransformComponent` は、AAAエンジン（Unreal Engine や Unity）に匹敵する高度な最適化と堅牢性を備えています。シーン内のすべてのオブジェクトの位置・回転・スケールを管理する最も重要なコンポーネントです。

#### 1. DOD (Data-Oriented Design) による一括更新
Transformの更新処理は、従来の `GameObject::Update()` から完全に分離され、コンポーネントプールを用いた **DODベースの一括更新（UpdateAll）** へと移行しました。これにより、キャッシュミスが最小限に抑えられ、数万単位のオブジェクトが存在するシーンでも高速に計算が完了します。

#### 2. 遅延評価 (Lazy Evaluation) と論理的Const性
Transformの `GetWorldMatrix()` や `GetWorldPosition()` を呼び出した際、**「自身または親のTransformに変更があった場合のみ」** 再計算（ComputeMatrix）が走ります。
`SetPosition()` などを何度呼んでも、その都度行列の乗算が走ることはなく、最終的に値が必要になった瞬間に一度だけ計算されるため、非常に軽量です。
また、値を取得するGetterメソッド（`const` 修飾）の内部でキャッシュの更新が行われる「論理的Const性」を採用しているため、プログラマは更新タイミングを一切気にせず安全に値を取得できます。

#### 3. Quaternion への完全移行と直接計算
回転の管理は、ジンバルロックや補間（Slerp）の破綻を防ぐため、内部的にすべて **Quaternion（クォータニオン）** に完全移行しています。
従来の `SetRotation` (オイラー角入力) も内部でクォータニオンへ変換されますが、この際の計算には行列を経由せず、最適化された **半角公式ベースの直接計算 (Direct Computation)** が用いられているため、変換による処理負荷や誤差は極小化されています。

#### 4. マイナススケール（フリップ）とゼロスケールへの安全対策 (Safe Guard)
親オブジェクトを反転（スケールをマイナス）させた状態での回転や、スケールを `0.0f` にした特異な状態など、Transform階層における計算の破綻（NaNやせん断の発生）を完全に防止する堅牢な対策が施されています。
- **マイナススケール**: 親がマイナススケール（フリップ）していても、子への回転抽出処理が破綻（NaN化）しないよう、安全に符号を相殺・復元するロジックが組み込まれています。
- **ゼロスケール**: 親のスケールが `0`（または `1e-6f` 以下の極小値）のときに子オブジェクトに対して `SetWorldPosition` 等を呼ぶと、数学的に逆行列が存在せず計算が崩壊します。本エンジンでは、これを検知して自動的に「ワールド空間からの逆算をスキップ（フォールバック）」する安全装置が働いているため、オブジェクトが画面から消し飛ぶなどの致命的バグが発生しません。

#### 5. カプセル化と主なAPI
従来のように `transform->position_` のようなパブリックメンバへの直接代入・参照は禁止されており、代わりに **Getter / Setter** を使用する必要があります。

```cpp
auto transform = gameObject_->GetComponent<TransformComponent>();

// --- 取得 (Getter) ---
// ローカル座標系
Vector3 localPos = transform->GetPosition();
Vector3 localRot = transform->GetRotation(); // オイラー角(ラジアン)
Vector3 localScl = transform->GetScale();

// ワールド座標系（親のTransformを加味した最終結果）
Vector3 worldPos = transform->GetWorldPosition();
Vector3 worldRot = transform->GetWorldRotation();
Vector3 worldScl = transform->GetWorldScale();

// ワールド空間の方向ベクトル（正規化済み）
Vector3 right   = transform->GetWorldRight();
Vector3 up      = transform->GetWorldUp();
Vector3 forward = transform->GetWorldForward();

// --- 更新 (Setter) ---
// 値を更新すると、内部で Dirty フラグ (isLocalDirty_) が立ちます
transform->SetPosition(Vector3(10, 5, 0));
transform->SetRotation(Vector3(0, ToRadian(90.0f), 0)); // Core/Math/MathFunction.h
transform->SetScale(Vector3(2, 2, 2));
```

---

### 4.5 設定管理 (CVar & Settings Management)

ゲームのコア設定（画面解像度、ウィンドウモード、VSync、各種デバッグ表示ON/OFFなど）は、**CVarSystem** 等を通じてデータ駆動で一元管理されています。
これにより、ソースコードの再コンパイルなしで設定の変更や、ユーザー環境に合わせた設定ファイルの分離・保存（永続化）が可能になります。

#### 4.5.1 設定ファイルの分離アーキテクチャ
CVarSystem は以下の2つのファイルを段階的にロードします。
1. **`resources/settings.json` (プリセット設定)**
   - プロジェクトとしてのデフォルト（初期）状態を定義します。Gitリポジトリで管理され、ビルド時に自動コピーされます。
2. **`resources/settings_local.json` (ユーザーのローカル設定)**
   - ユーザーがImGui等で設定を変更した際に保存されるファイルです。ロード時は `settings.json` の後に上書きで適用されます。
   - このファイルは `.gitignore` 対象であり、ビルドプロセスで初期化（上書きコピー）されないように保護されています。

#### 4.5.2 CVar の定義と使い方
新しいコンソール変数を追加するには、`Core/System/EngineCVars.cpp` 等で以下のマクロを使用します。

```cpp
#include "Framework/Utility/CVar.h"

// 整数型のCVarを定義（変数名, 初期値, 説明文）
DEFINE_CVAR_INT("r.DisplayMode", 0, "Display mode: 0=Windowed, 1=Borderless");

// bool型のCVarを定義
DEFINE_CVAR_BOOL("r.VSync", true, "Enable Vertical Sync");
```

定義した変数は、どこからでも `CVarSystem::GetInt` などで取得・変更が可能です。
```cpp
// 取得
bool vsync = Irufemi::CVarSystem::GetBool("r.VSync");

// 変更（ImGui等から）
Irufemi::CVarSystem::SetBool("r.VSync", false);
```

#### 4.5.3 コールバックの登録 (値変更時の即時反映)
変数の値が変更された際（あるいはロード時に値が設定された直後）に、自動的に特定の関数を走らせるためのコールバック機構（`SetOnChangeCallback`）が備わっています。

```cpp
// エンジン初期化時などに登録
Irufemi::CVarSystem::SetOnChangeCallback("r.VSync", [this]() {
    bool vsync = Irufemi::CVarSystem::GetBool("r.VSync");
    this->SetVSync(vsync); // 値が変わったら即座に描画システムへ反映
});
```
※コールバックを登録した瞬間に、現在の値をもって一度即時実行されるため、ロード直後の初期化もこれ一つでカバーできます。

#### 4.5.4 コンソールコマンドシステム (ConsoleCommandSystem)
ゲーム内CLI、外部監視ツール（`TelemetryMonitor`）、コマンドライン引数から文字列ベースのデバッグコマンドを安全に実行するための仕組みです。スレッドセーフにコマンドの登録と実行を管理できます。
- **ヘッダー**: `#include "Framework/Utility/ConsoleCommandSystem.h"`

```cpp
#include "Framework/Utility/ConsoleCommandSystem.h"

// 1. コマンドの登録（コマンド名, 説明文, 引数を受け取るコールバック）
Irufemi::ConsoleCommandSystem::RegisterCommand("spawn_debris", "指定数のデブリを生成します (例: spawn_debris 100)", [](const std::vector<std::string>& args) {
    if (args.size() > 1) {
        int count = std::stoi(args[1]);
        // 生成処理...
    }
});

// 2. コマンドライン文字列の実行
std::string outputResult;
bool success = Irufemi::ConsoleCommandSystem::ExecuteCommand("spawn_debris 500", &outputResult);
```

---

## 5. コンポーネント完全リファレンス (Component System & All Components)

本章では、汎用ゲームエンジン `IrufemiEngine` が提供する標準コンポーネント群を網羅します。

> [!NOTE]
> **ゲーム固有ロジック（Application層）との分離**:
> レール移動（`SplineFollowerComponent`, `RailRelativeFollowerComponent`）、重力アクション（`GravityPlayerComponent`）、マルチロックオン（`PlayerTargetingComponent`, `TargetableComponent`）、デブリ制御（`DebrisManagerComponent`）、ボス制御（`BossComponent`）など、**ゲーム固有のアクター・ステートマシンに関するコンポーネント仕様** は [`project/Application_solo/docs/GameArchitecture.md`](file:///f:/school/3_5/WP1/WP1/project/Application_solo/docs/GameArchitecture.md) を参照してください。

### 5.1 コンポーネントシステムの基礎と新規作成ルール
- **基底ヘッダー**: `#include "Framework/Component/Component.h"`
#### 新規コンポーネント作成時のルール（自動UI化とメニュー登録）
エンジンの「簡易リフレクション」と「動的メニュー生成」により、コンポーネントを追加する際の手間が大幅に削減されています。
変数を宣言して `OnRegisterProperties` に書くだけで、**自動でインスペクターにUIが作成され、JSONに保存・復元される**ようになります。手動で ImGui を書く必要はありません。
さらに、作成したクラスを `ComponentFactory` に登録するだけで、エディタの「Add Component」メニューにも自動で追加されます（エディタのソースコードを変更する必要はありません）。

```cpp
#pragma once
#include "Framework/Component/Component.h"

class PlayerStatusComponent : public Component {
public:
    int hp_ = 100;
    float speed_ = 5.0f;

    // 1. コンポーネント名を返す
    // ※この名前に特定のキーワードを含めることで、Add Component メニューのカテゴリが自動で決まります。
    // （例："Renderer" や "Emitter" -> Renderer枠, "Collider" -> Collider枠, "UI", "Button" -> UI枠）
    std::string GetComponentName() const override { return "PlayerStatusComponent"; }

    // 2. 変数をリフレクションシステムに登録する
    void OnRegisterProperties() override {
        RegisterProperty("Max HP", &hp_);
        RegisterProperty("Move Speed", &speed_);
    }
};
```
*※作成したコンポーネントは、必ず `ComponentFactory.cpp` の `RegisterAllCoreComponents` 関数内（またはゲーム側の初期化処理）でファクトリに登録してください。*
```cpp
// 登録例 (通常)
ComponentFactory::Register("PlayerStatusComponent", "Utility", []() { return std::make_shared<PlayerStatusComponent>(); });

// 登録例 (DOD対応 / IsPooledComponent の場合)
ComponentFactory::Register("TransformComponent", "Core", []() { 
    return std::static_pointer_cast<Component>(ComponentPool<TransformComponent>::GetInstance().Create());
});
```

#### 【ベストプラクティス】マジックナンバーの排除とManagerへのパラメータ集約
ゲームのバランス調整に関わる数値（移動速度、ダメージ量、発光の色など）をC++のコード内に直接ハードコード（マジックナンバー化）することは避けてください。
必ず上記のように `OnRegisterProperties` で変数を露出させ、**Inspectorからリアルタイムに調整・保存できるようにする**のが基本方針です。これにより、ゲーム実行中（ポーズ中も含む）に値を変更し、再コンパイルなしで最適なバランスを探ることができます。

さらに、大量にスポーンするオブジェクト（敵の弾やガレキなど）が共通のパラメータを持つ場合、個々のオブジェクトが別々にパラメータを持つのは非効率です。
このような場合は、**Managerコンポーネント（例: `DebrisManagerComponent`）を新設して全体のパラメータを集約管理**し、各オブジェクトは生成時にManagerから値を動的に取得する設計パターン（データ指向アーキテクチャ）を採用してください。

#### 衝突判定とコールバック (OnCollisionEnter / Destroy)
ゲームロジックとして「何かにぶつかったら壊れる」「ダメージを受ける」といった処理は、Component 内の仮想関数をオーバーライドして実装します。

```cpp
#include "Framework/Component/Component.h"
#include "Framework/GameObject/GameObject.h"

class BulletComponent : public Component {
public:
    std::string GetComponentName() const override { return "BulletComponent"; }

    // 当たり判定が行われ、他のコライダーと接触した瞬間に自動で呼ばれます
    void OnCollisionEnter(GameObject* hitObject) override {
        // 相手の名前やComponentを見て処理を分ける
        if (hitObject->GetName() == "Enemy") {
            // Destroy() を呼ぶと、現在のフレームの終わりに安全にオブジェクトが破棄(GC)されます
            GetGameObject()->Destroy();
        }
    }
};
```

#### ポーズ（一時停止）時の動作制御
エディタのデバッグ機能としてのPauseボタン押下時（`TimeScale == 0.0f`時）は、すべてのコンポーネントの `Update()` 呼び出しが**フレームワーク側で自動的にスキップ**されます。そのため、各コンポーネント内で `deltaTime <= 0.0f` をチェックして手動で停止させる処理を書く必要はありません。

もし「UIのアニメーション」や「エフェクト」など、**ポーズ中であっても更新し続けたい**特殊なコンポーネントを作成する場合は、`CanUpdateWhenPaused()` 仮想関数をオーバーライドして `true` を返すようにしてください。

```cpp
class AlwaysMovingComponent : public Component {
public:
    std::string GetComponentName() const override { return "AlwaysMovingComponent"; }

    // ポーズ中（TimeScale == 0.0f）でもUpdateを実行するかどうか
    bool CanUpdateWhenPaused() const override { 
        return true; 
    }

    void Update() override {
        // ポーズ中も呼ばれる。
        // ※この中で GetGameDeltaTime() を使うと 0 になるため、
        //   ポーズ中も動かしたい場合は GetRealDeltaTime() など実時間を使う必要があります。
    }
};
```

#### 動的生成 (Instantiate)
弾を撃つ、敵を出現させるといった「ゲームプレイ中にオブジェクトを生み出す」処理は、あらかじめ作成したプレハブ（JSON）を指定して呼び出します。

```cpp
#include "Framework/Component/Component.h"
#include "Framework/GameObject/GameObject.h"

class PlayerShooterComponent : public Component {
public:
    std::string GetComponentName() const override { return "PlayerShooterComponent"; }

    void Update() override {
        // 例: 左クリックされたら弾を生成する
        if (GetEngine()->GetInputManager()->IsMouseButtonPressed(Mouse::Button::Left)) {
            // 現在のワールド座標を取得（TransformComponent のカプセル化 API）
            Vector3 pos = GetTransform()->GetWorldPosition();
            
            // プレハブパスをもとに、指定した座標へオブジェクトを動的生成
            auto bullet = GetGameObject()->Instantiate("resources/prefabs/Bullet.prefab.json", pos);
        }
    }
};
```

#### 演出・エフェクトコンポーネント (AudioSource / ParticleEmitter / VoxelParticle)
BGMやSEを鳴らしたり、エフェクトを発生させるには、インスペクターからプロパティを設定するだけで動く以下のコンポーネントが便利です。

- **`AudioSourceComponent`**: アタッチしたオブジェクトから音を鳴らします。
  - `Audio Path`: 再生したい音声ファイルのパス（例: `audio/se/boom.wav`）。
  - `Play On Awake`: シーン開始時に自動で再生するか。
  - `Loop`: ループ再生するか。
- **`ParticleEmitterComponent`**: アタッチしたオブジェクトの位置でエフェクトを発生させます。
  - `Texture Path`: パーティクルに使う画像パス。
  - `Emit Type`: 0(球体), 1(ビーム), 2(ボックス), 3(円柱) などの放出形状。
  - `Color`, `Velocity`, `Emit Count` などで自由にエフェクトを構築できます。
- **`VoxelParticleComponent`**: 指定した3Dモデルをボクセル化し、大量のパーティクルとして爆発（四散）させる高度なエフェクトコンポーネントです。
  - `Override Model Name`: ベースとなるモデルのファイル名を指定します。空欄の場合はアタッチされているRendererのモデルを使用します。
  - `Pre Allocate Count`: シーンロード時に事前計算・プールしておく数を指定し、再生時の処理落ちを防ぎます。1モデルごとに最大32インスタンスまでの自動オブジェクトプール機能を備え、上限を超えた爆発要求は安全に破棄されます。
  - `Resolution`: ボクセルの分割数（例：32x32x32）。
  - **エフェクトの制御**: 以前のハードコード（固定寿命など）はすべて撤廃され、インスペクターから初速（Velocity）、回転（Angular Velocity）、重力（Gravity）、寿命（LifeTime）、基本サイズ（Size）を完全に動的に制御できるようになりました。また、放出終了後も寿命が尽きるまではシーン内に自然に滞留し、シーン遷移時には自動的に安全なリセットが行われます。
  - **再生方法**: C++コードから `GetComponent<VoxelParticleComponent>()->Explode();` を呼ぶことで、設定したパラメータに基づく破砕エフェクトが起動します。
- **`SkyDomeComponent`**: 3Dメッシュ不要で無限遠の背景天球（プロシージャル天球）を描画する背景描画コンポーネントです。
  - 頂点バッファや3Dモデルデータ（obj/mtl）を使わず、全画面最奥三角形（SV_VertexID）と逆射影レイキャストによってカメラの回転に応じた天球テクスチャを極低コストかつ境界破綻（クリッピング）ゼロでレンダリングします。
  - `Texture Path`: 天球パノラマ画像のパス（例: `resources/texture/sky/skydome.png`）。
  - `Color` / `Intensity`: 空のカラー乗数および輝度乗数。
  - `UvOffset` / `UvTiling`: テクスチャの回転・オフセットおよびタイリング設定。
- **`FogComponent`**: シーンの大気・距離フォグをリアルタイムに制御するコンポーネントです。
  - 独立したレンダーパス（`FogPass`）で深度バッファを参照し、不透明オブジェクトから無限遠の天球の境界に至る空気遠近（大気効果）を自然にブレンドします。
  - `Enabled`: フォグの有効/無効。
  - `Color`: フォグのカラー（RGB）。天球（SkyDome）の地平線カラーと合わせることで、遠景のメッシュが空へ溶け込む表現が可能です。
  - `Start Distance` / `End Distance`: フォグの開始距離および完全濃縮距離（m）。
  - `Density`: フォグの密度。
  - `Type (0:Lin, 1:Exp)`: 線形フォグ（Linear: 0）または指数関数フォグ（Exponential: 1）。

#### UIコンポーネント (Canvas / Button / Text)
ゲーム内の2D UIを構築するための専用コンポーネントです。

- **`TextRendererComponent`**: 高品質なMSDF（マルチチャンネル符号付き距離場）フォントを利用し、任意の文字列を描画するコンポーネントです。動的に必要な文字だけを生成・パッキングするため、メモリ効率が非常に高く、どれだけ拡大しても文字がジャギりません（ぼやけません）。
  - `Text`: 表示する文字列。複数行の入力（改行）にも対応しています。
  - `Font ID`: 使用するフォント名（`resources/fonts/` 以下に配置したファイル名、例：`toro_glitch`）。
  - `Base Scale`: 文字の大きさ。
  - `Color`: 文字の色と透明度。
  - `Top Most`: ブルームなどのポストプロセスの影響を受けない、最前面レイヤーに描画するかどうか。
  - `Alignment`: テキストのアライメント（`0=Left`, `1=Center`, `2=Right`）。複数行の場合は行ごとに適用されます。

- **`ButtonComponent`**: マウスカーソルのホバーやクリックを検知し、色を自動的に変えたり、明滅（Pulse）アニメーションを行うUI機能を提供します。
  - `Normal/Hover/Click Color`: マウスの操作状態に合わせて、アタッチされているSpriteの色を自動的に変化させます。
  - `Enable Hover Pulse / Idle Pulse`: ホバー時や待機時の明滅アニメーションを有効化します。
  - *(※ボタン押下によるシーン遷移機能はエンジン側の責務ではなく、`Application/` 側の `SceneTransitionButtonComponent` などで個別に実装してアタッチしてください)*
- **`CanvasComponent`**: UI要素をグループ化し、アルファ値（透明度）などを一括管理します。
  - `Group Alpha`: このコンポーネントを持つGameObject自身と、そのすべての子要素にある `SpriteRendererComponent` のアルファ値を一括で制御します。フェードイン・フェードアウトの演出に便利です。

#### ユーティリティ・コンポーネント (Lifetime / Spline)
ゲームプレイの実装を補助する便利なコンポーネント群です。

- **`LifetimeComponent`**: 指定した時間（秒）が経過すると、自動的にオブジェクトを破棄 (`Destroy`) または非アクティブ化 (`Disable`) します。弾やエフェクトの寿命管理に最適です。
  - `Life Time`: 寿命（秒）
  - `Timeout Action`: 0(Destroy), 1(Disable)
- **`SplineComponent`**: 複数のウェイポイント（座標リスト）をセットし、Catmull-Rom スプライン補間による滑らかな曲線パスを作成します。
  - C++コードから進行度ベース（`GetPointAt(t)`, `GetTangentAt(t)`）だけでなく、等速移動に不可欠な距離ベース（`GetPointAtDistance(distance)`, `GetTangentAtDistance(distance)`）や総延長距離（`GetTotalLength()`）を取得できます。エディタ上で子ノード（`SplineNodeComponent`）を配置するだけで視覚的にレールが生成されます。
- **`VirtualEntityManagerComponent`**: 大量のオブジェクト（弾幕、草、群集など）の座標やスケールを「仮想データ」として超軽量な密配列で管理し、必要なときだけ実体の GameObject として生成（Promote）したり、プールに戻す（Demote）ことができる最適化コンポーネントです。
  - `Setup()` で最大数と実体化用のファクトリ関数（GameObjectの生成処理）を渡して初期化します。
  - `AddVirtualInstance()` で座標などを登録するとIDが返され、そのIDを使って `Promote(id)` を呼ぶことでプールから GameObject が貸し出されます。
  - `ModelBatchRendererComponent` と組み合わせることで、何万ものオブジェクトをGPUインスタンシングで描画しつつ、プレイヤーの近くにあるものだけを実体化して当たり判定を行う、といった高度な最適化が可能です。

---

### 5.2 描画系コンポーネント
- **MeshRendererComponent**: `#include "Framework/Component/Renderer/MeshRendererComponent.h"`（静的モデル描画・Unlit切替・スカイドームモード対応）
- **SkinnedMeshRendererComponent**: `#include "Framework/Component/Renderer/SkinnedMeshRendererComponent.h"`（ボーンアニメーション描画）
- **PrimitiveRendererComponent**: `#include "Framework/Component/Renderer/PrimitiveRendererComponent.h"`（Cube, Sphere等の3D基本形状描画）
- **Primitive2DRendererComponent**: `#include "Framework/Component/Renderer/Primitive2DRendererComponent.h"`（2D矩形・円・ライン等のプリミティブ描画・UI背景やゲージ枠等）
- **SkyDomeComponent**: `#include "Framework/Component/Renderer/SkyDomeComponent.h"`（全天スカイドーム・大気フォグ統合）
- **SpriteRendererComponent**: `#include "Framework/Component/Renderer/SpriteRendererComponent.h"`（2Dスプライト・ビルボード描画）
- **TextRendererComponent**: `#include "Framework/Component/Renderer/TextRendererComponent.h"`（SDF・ビットマップフォントによるテキスト描画）
- **SkeletonDebugRendererComponent**: `#include "Framework/Component/Renderer/SkeletonDebugRendererComponent.h"`（ボーン骨格のX-Ray可視化）
- **ModelBatchRendererComponent**: `#include "Framework/Component/Renderer/ModelBatchRendererComponent.h"`（GPUカリング対応大量モデルバッチ描画）

---

### 5.3 カメラ系コンポーネント (Camera System)

#### 1. CameraComponent (基本カメラ)
シーンの視点・画角（FOV）・クリッピングプレーン（Near/Far）を制御する基底カメラコンポーネントです。
- **ヘッダー**: `#include "Framework/Component/Camera/CameraComponent.h"`
- **主な機能**:
  - `SetFovAngleY(float fov)`: 垂直視野角の設定（ラジアン）
  - `SetNearZ(float n)` / `SetFarZ(float f)`: クリップ面の設定
  - `SetPositionOffset(offset)` / `SetRotationOffset(offset)`: Shake演出等による一時オフセット
  - `GetCamera()->GetViewMatrix()` / `GetCamera()->GetProjectionMatrix()`: 行列の取得
  - `GetCamera()`: 内包する低レイヤー `Camera` インスタンスを取得

#### 2. CameraManager (マルチカメラ統括管理)
エンジン全体で複数のカメラ（メインカメラ、演出用サブカメラ、デバッグカメラ等）を一元管理し、描画に使用するアクティブカメラの切り替えを統括します。
- **ヘッダー**: `#include "Renderer/Camera/CameraManager.h"`
- **主なAPI**:
  ```cpp
  auto* cameraManager = engine_->GetCameraManager();

  // 1. カメラの登録・切り替え
  // cameraManager->AddCamera("Cinematic", cinematicCam);
  cameraManager->SetActiveCamera("Cinematic");

  // 2. 現在アクティブなカメラの取得
  Camera* activeCam = cameraManager->GetActiveCamera();
  const std::string& activeCamName = cameraManager->GetActiveCameraName();
  ```

#### 3. CameraShakeComponent (高品位カメラシェイク)
被弾時や大爆発、加速時などに、パーリンノイズとイージングを用いたAAA品質のカメラ揺れ演出を付与します。複数イベントの揺れが線形合成され、自然な減衰衰弱を行います。
- **ヘッダー**: `#include "Framework/Component/Camera/CameraShakeComponent.h"`
- **主なAPI**:
  ```cpp
  #include "Framework/Component/Camera/CameraShakeComponent.h"

  // 1. フレーム数指定でシェイク再生 (強度, 持続フレーム数, 振動周波数)
  if (auto* shake = cameraObj->GetComponent<CameraShakeComponent>()) {
      shake->PlayShake(1.5f, 30, 15.0f);
  }

  // 2. 秒数指定でシェイク再生 (強度, 持続秒数, 振動周波数)
  if (auto* shake = cameraObj->GetComponent<CameraShakeComponent>()) {
      shake->PlayShakeSeconds(2.0f, 0.5f, 20.0f);
  }

  // 3. 再生状態の確認
  bool isShaking = shake->IsPlaying();
  ```

#### 4. TargetFollowComponent (スムーズダンピング追従)
注視点および位置をスムーズダンピング（追従遅延・Lerp補間）しながら滑らかに追尾するカメラコンポーネントです。
- **ヘッダー**: `#include "Framework/Component/Camera/TargetFollowComponent.h"`
- **主な機能・API**:
  - `SnapToTarget()`: シーン遷移直後やプレイヤーリスポーン時、補間遅延を行わずにターゲットの現在位置・向きへカメラを即座にスナップ配置（Camera Cut）します。
  - インスペクターから `Target Object ID`、`Offset` (右, 上, 前)、`Follow Delay` (追従遅延係数: 0.0で即時追従、1.0に近いほど滑らかに遅延) をリアルタイムに調整・シリアライズ可能。
  - 追従対象への参照は `std::weak_ptr<GameObject>` による安全な弱参照で保護されており、対象破棄時のクラッシュを防止します。

---

### 5.4 エフェクト・演出系コンポーネント (Effects & Atmosphere)

#### 1. GlobalPostProcessComponent (シーン全域ポストプロセス設定)
Unreal Engine の PostProcessVolume に相当するコンポーネントです。GameObjectにアタッチすることで、シーン全体のポストプロセス（Bloom, Fog, Vignette, ToneMapping 等）のOverride設定をデータ駆動で管理・シリアライズできます。
- **ヘッダー**: `#include "Framework/Component/Effect/GlobalPostProcessComponent.h"`
- **主な機能**:
  - エディタの Inspector から各エフェクトのパラメータをオーバーライド設定可能
  - シーン定義ファイル（`.json`）へ自動シリアライズ・復元

#### 2. ScreenEffectComponent (ワンショット画面エフェクト)
被弾時のノイズグリッチや危険状態の赤色ヴィネットなど、一時的に画面全体へポストプロセス効果を適用し、時間経過でイージング減衰・自己終了する戦略ドライバコンポーネントです。
- **ヘッダー**: `#include "Framework/Component/Effect/ScreenEffectComponent.h"`
- **主なAPI**:
  ```cpp
  #include "Framework/Component/Effect/ScreenEffectComponent.h"
  #include "Renderer/PostProcess/PostProcessManager.h"

  // 被弾時グリッチ効果の再生
  if (auto* fx = playerObj->GetComponent<ScreenEffectComponent>()) {
      fx->SetMode(PostProcessMode::Glitch);
      fx->SetDuration(0.3f); // 0.3秒かけて減衰
      fx->Play();
  }
  ```

#### 3. FogComponent (大気フォグ演出)
距離フォグの開始・終了深度、フォグカラー、密度パラメータを設定し、シーン全体の空気感・奥行き感を表現します。
- **ヘッダー**: `#include "Framework/Component/Effect/FogComponent.h"`

#### 4. ParticleEmitterComponent / ParticleFieldComponent / VoxelParticleComponent
- **ParticleEmitterComponent**: 特定の発生源からパーティクルを噴射（球、ビーム、ボックス、円柱などの形状に対応）。
- **ParticleFieldComponent**: GPUパーティクルに対して重力・風・吸引・斥力などの**フォースフィールド（力場）**を与えるコンポーネント。竜巻、ブラックホール吸引、強風域などを表現します。
  - **ヘッダー**: `#include "Framework/Component/Effect/ParticleFieldComponent.h"`
  - エディタの Inspector からフィールドの種類（Gravity, Wind, Attract, Repel）、強さ（Strength）、影響範囲（Radius / BoxSize）をリアルタイムに調整・シリアライズ可能。
- **VoxelParticleComponent**: 敵やオブジェクト破壊時にブロック状のキューブ破片を四方へ飛散（事前プール32体、寿命・初速動的制御対応）。

#### 5. EffectMaskComponent (個別オブジェクトマスク)
特定オブジェクトに対してのみアウトラインやシルエット発光を適用するためのマスクバッファ登録コンポーネントです。

---

### 5.5 物理・コライダー系コンポーネント (Colliders)

IrufemiEngine は用途と計算負荷に合わせて3種類の3Dコライダーコンポーネントを提供しています。

| コンポーネント | 形状 | 計算負荷 | 用途・選定指針 |
| :--- | :--- | :--- | :--- |
| **`SphereColliderComponent`** | 球体 | **極小 (最速)** | **自機弾、敵弾、アイテム回収判定、大半の当たり判定の第一選択** |
| **`AABBColliderComponent`** | 軸平行直方体 | **小** | 回転しないブロック、ステージ地形、静的障害物 |
| **`OBBColliderComponent`** | 有向直方体 (回転追従) | **中** | 自機本体、ボス機体、細長いレーザー照射体、傾斜した地形 |

- **ヘッダー**:
  - `#include "Framework/Component/Collider/SphereColliderComponent.h"`
  - `#include "Framework/Component/Collider/AABBColliderComponent.h"`
  - `#include "Framework/Component/Collider/OBBColliderComponent.h"`
- **衝突コールバックの実装**:
  ```cpp
  #include "Framework/Component/Component.h"
  #include "Framework/GameObject/GameObject.h"
  #include "Framework/Component/Collider/ColliderComponent.h"

  class BulletBehavior : public Component {
  public:
      void OnCollisionEnter(GameObject* hitObject) override {
          // 相手のタグや属性をチェック
          if (hitObject->GetTag() == "Enemy") {
              // ダメージ処理...
              GetGameObject()->Destroy();
          }
      }
  };
  ```

---

### 5.6 ロジック・ユーティリティ系コンポーネント (Logic & Utility)

#### 1. SpawnPointComponent (レベルデザイン・出現地点マーカー)
ステージエディタ（BlenderLevelEditor）やJSONシーン上で、敵機やアイテム、リスポーン位置を指定するための汎用マーカーコンポーネントです。
- **ヘッダー**: `#include "Framework/Component/Logic/SpawnPointComponent.h"`
- **主なAPI**:
  ```cpp
  #include "Framework/Component/Logic/SpawnPointComponent.h"

  auto* spawn = obj->GetComponent<SpawnPointComponent>();
  std::string waveId = spawn->GetWaveId();       // 例: "Wave1"
  std::string enemyType = spawn->GetEnemyType(); // 例: "CruiserEnemy"
  ```

#### 2. LifetimeComponent (自動自己消滅)
一定時間（秒数）経過後に自動的に `GetGameObject()->Destroy()` を呼び出すタイマーコンポーネントです。弾薬や一時的なパーティクル発生オブジェクトにアタッチします。
- **ヘッダー**: `#include "Framework/Component/Utility/LifetimeComponent.h"`
- **主な機能**: `SetLifeTime(float seconds)`, `SetTimeoutAction(TimeoutAction action)` (0: Destroy, 1: Disable) によるプール運用対応。

#### 3. SplineComponent / SplineNodeComponent (レール移動・カメラパス)
Catmull-Rom スプライン曲線を定義し、レールシューターの自機・敵編隊の飛行ルートやカメラレールを構築します。
- **ヘッダー**: 
  - `#include "Framework/Component/Utility/SplineComponent.h"`
  - `#include "Framework/Component/Utility/SplineNodeComponent.h"`
- **エディタでの制作手順**:
  1. 親となる `GameObject`（例: "RailPath"）に `SplineComponent` をアタッチします。
  2. その子要素として複数の空の `GameObject` を作成し、それぞれに `SplineNodeComponent`（または単なる `TransformComponent`）をアタッチします。
  3. エディタの Scene View 上で各子ノードの座標をギズモで移動させると、自動的に滑らかなスプライン曲線が生成され、エディタ上でデバッグレールが視覚的にリアルタイム描画されます。
- **C++ コードからのサンプリングAPI**:
  ```cpp
  #include "Framework/Component/Utility/SplineComponent.h"

  auto* spline = railObj->GetComponent<SplineComponent>();

  // 1. 進行度 t (0.0f = 開始点 〜 1.0f = 終点) による座標・接線取得
  Vector3 pos = spline->GetPointAt(t);
  Vector3 forward = spline->GetTangentAt(t); // 正規化された進行方向

  // 2. 走行距離（メートル）ベースでの高精度サンプリング（等速移動に最適）
  float totalLen = spline->GetTotalLength(); // スプライン全体の総延長距離(m)
  Vector3 posAtDist = spline->GetPointAtDistance(currentDistance);
  Vector3 forwardAtDist = spline->GetTangentAtDistance(currentDistance);
  ```

#### 4. BoneAttachmentComponent (特定ボーン追従)
モデルの特定のボーン（関節）に追従するコンポーネントです。武器やアクセサリーをキャラクターの手に持たせる際や、特定の関節からエフェクトを発生させる際に、対象ボーンのワールド行列に同期して移動・回転させます。
- **ヘッダー**: `#include "Framework/Component/Logic/BoneAttachmentComponent.h"`
- **主なAPI**:
  ```cpp
  #include "Framework/Component/Logic/BoneAttachmentComponent.h"

  auto* attach = weaponObj->AddComponent<BoneAttachmentComponent>();
  attach->SetTargetName("PlayerCharacter");    // 追従対象のGameObject名
  attach->SetTargetBoneName("RightHandBone");  // 追従対象のボーン名
  ```

#### 5. AnimatorComponent (アニメーション制御)
3Dモデルのスケルタルアニメーション再生、遷移、ブレンドを制御するコンポーネントです。
- **ヘッダー**: `#include "Framework/Component/Logic/AnimatorComponent.h"`

---

### 5.7 UI系コンポーネント (UI System)
- **CanvasComponent**: `#include "Framework/Component/UI/CanvasComponent.h"`（UIヒエラルキーのルート）
- **ButtonComponent**: `#include "Framework/Component/UI/ButtonComponent.h"`（ボタンインタラクション・ホバー・クリック）

#### 3. SliderComponent (数値スライダーUI)
マウスドラッグによる数値変更が可能なUIスライダーコンポーネントです。BGM/SE音量調整やマウス感度設定画面などで使用されます。
- **ヘッダー**: `#include "Framework/Component/UI/SliderComponent.h"`
- **主なAPI**:
  - `float GetValue() const`: 現在のスライダー値（0.0 〜 1.0）を取得
  - `void SetValue(float value)`: 値を設定しツマミの位置を更新
  - `bool CheckBounds(const Irufemi::Vector2& mousePos)`: マウス座標がスライダーの操作矩形内にあるか判定
  - `float CalculateValueFromPosition(float posX) const`: 指定したUI X座標からスライダーの正規化値(0.0〜1.0)を算出
  - `void SetOnValueChangedCallback(std::function<void(float)> cb)`: 値変更時のイベントハンドラを登録
  - `void SetHandleObjectID(uint64_t id)`: ツマミとなるGameObjectのIDを紐付け
- **実装例**:
  ```cpp
  #include "Framework/Component/UI/SliderComponent.h"

  auto* slider = sliderObj->GetComponent<SliderComponent>();
  slider->SetValue(0.75f);
  slider->SetOnValueChangedCallback([](float newValue) {
      // コールバックによる即時反映
  });
  ```

---

### 5.8 大量オブジェクト最適化 (VirtualEntityManagerComponent)

本エンジンでは、数万個レベルの大量のオブジェクト（がれき、草、弾幕など）を最適化して描画・管理するためのシステムとして、**`VirtualEntityManagerComponent`** (Instance Replacement / Promotion パターン) をサポートしました。

このコンポーネントを使用すると、普段は `GameObject` を実体化せずに軽量な「行列データ（Virtual Transform）」としてのみ管理・GPU描画し、プレイヤーが干渉した瞬間など **本当に必要なときだけ本物のアクター（GameObject）に昇格（Promote）させる** ことができます。

#### 導入手順
1. **マネージャーの準備**:
   ゲーム側の管理クラス（例: `DebrisManagerComponent`）で、`VirtualEntityManagerComponent` をアタッチして初期化します。

```cpp
#include "Framework/Component/VirtualEntity/VirtualEntityManagerComponent.h"

// 1. コンポーネントの追加
auto virtualManager = gameObject_->AddComponent<VirtualEntityManagerComponent>();

// 2. プールサイズと最大予約数、生成ファクトリの登録
auto factory = [this]() -> std::shared_ptr<GameObject> {
    auto obj = std::make_shared<GameObject>("MyEntity");
    obj->AddComponent<DebrisComponent>(); // 物理挙動などをアタッチ
    obj->SetIsActive(false);
    gameObject_->AddChild(obj);
    return obj;
};
// 第1引数: 実体化プールの最大数(500), 第2引数: 仮想インスタンスの最大予約数(2000), 第3引数: 生成関数
virtualManager->Setup(500, 2000, factory);
```

2. **データの追加（実体化しない）**:
```cpp
// 座標や回転だけを登録し、仮想インスタンスIDを受け取る
int id = virtualManager->AddVirtualInstance(Vector3(10, 0, 0));
```

3. **データの更新 (Data-Oriented Update)**:
   アニメーションなどを適用したい場合は、`GetDenseInstances()` で配列を参照し、`GetInstance(id)` を介して直接書き換えます。実体がないため超高速に処理されます。
```cpp
const auto& instances = virtualManager->GetDenseInstances();
for (const auto& vi : instances) {
    if (!vi.isPromoted && !vi.isDestroyed) {
        if (auto* mutableInst = virtualManager->GetInstance(vi.id)) {
            // 例：Y座標をフワフワさせる
            mutableInst->position.y += std::sin(time) * 0.1f;
        }
    }
}
```

4. **昇格 (Promote) と 降格 (Demote)**:
   プレイヤーが近づいた、攻撃を当てた等のタイミングで、IDを指定して本物の `GameObject` に昇格させます。
```cpp
// id を指定してGameObjectをプールから取得し、仮想インスタンスの座標を同期する
auto realObj = virtualManager->Promote(id);
if (realObj) {
    // 物理演算を有効化したり、ターゲットを追従させたりする
}

// 用済みになったらデータに戻す
virtualManager->Demote(id);
```

このシステムと前述の「GPU Culling」を組み合わせることで、数万個のオブジェクトがあっても 60FPS を余裕で維持できるパフォーマンスを実現できます。

---

## 6. 描画パイプラインとグラフィックス (Graphics, Shaders & PostProcess)

### 6.1 RenderGraph と DrawManager
- **ヘッダー**: `#include "Renderer/DrawManager.h"`, `#include "Renderer/Pipeline/RenderGraph/RenderGraph.h"`

#### RenderGraph によるポストプロセスのサブパス化と自動バリア

IrufemiEngine では、DirectX 12 におけるリソース状態（PIXEL_SHADER_RESOURCE と RENDER_TARGET 等）の遷移不整合を防ぎ、一時リソースのメモリ再利用（エイリアシング）を安全に行うため、ポストプロセスの各ステップを **RenderGraph のサブパス** として展開・実行する設計を採用しています。

#### 1. アーキテクチャ概要
従来の「単一の PostProcessPass の中で全エフェクトを直列実行する方式」から移行し、各ポストエフェクト（Bloom, DualKawaseBlur, SeparableBlur, LightShafts, CombinedPass）の内部ステップを独立した `RenderPass` として RenderGraph に直接登録します。

- **Bloom**: 高輝度抽出（Extract） ➔ 水平ブラー（BlurH） ➔ 垂直ブラー（BlurV） ➔ 加算合成（Combine）
- **DualKawaseBlur**: 複数段階のダウンサンプルパス ➔ 複数段階のアップサンプルパス
- **SeparableBlur**: 水平ブラー ➔ 垂直ブラー
- **LightShafts**: オクルージョン抽出 ➔ 放射状ブラー ➔ 加算合成

#### 2. リソースハンドル確定と自動バリアの仕組み
1. **即時仮想ハンドル発行 (`GetOrCreateTransientResource`)**:
   `BuildRenderGraph` のパス構築フェーズにおいて、`RenderGraph` から一時リソース（作業バッファやダウンサンプルバッファ）の仮想ハンドル `TransientResourceHandle` を確定・発行します。
2. **RenderGraph による自動バリア管理**:
   各サブパスの `Setup` 内で `RequireTransientState` を宣言することで、RenderGraph がリソースの寿命を計算し、パスの実行直前に `D3D12_RESOURCE_BARRIER_TYPE_TRANSITION` および `D3D12_RESOURCE_BARRIER_TYPE_ALIASING` を自動で発行します。
3. **テクスチャの安全な取得**:
   各サブパスの `Execute` 内では、`rc.drawManager->GetRenderGraph()->GetTransientRenderTexture(handle)` を用いて直接テクスチャを取得するため、初期化順序やキャッシュ不整合によるヌルポインタ参照を防止します。

---

#### 静的モデルの描画 (MeshRendererComponent)

アニメーションを持たない背景モデルやプロップを描画する場合は `MeshRendererComponent` を使用します。

#### 基本的な使い方
`GameObject` に `MeshRendererComponent` をアタッチし、描画したいモデル（OBJ / GLTF / FBXなど）をセットします。

```cpp
auto renderer = gameObject_->AddComponent<MeshRendererComponent>();
renderer->LoadModel("sample/cube.gltf");
```

※ アニメーションを行わないため、後述の `SkinnedMeshRendererComponent` よりも軽量に動作します。動かない物体にはこちらを優先して使用してください。

#### ライティングの有効/無効切替 (Unlit描画)
スカイドーム（天球）や自己発光するオブジェクト、UI用3Dモデルなど、**「シーンの平行光源（DirectionalLight）による陰影計算を無視し、テクスチャ本来の色を100%忠実に出力させたい」** 場合は、ライティングを無効化（Unlit化）できます。

- **C++ コードからの制御:**
```cpp
auto renderer = gameObject_->GetComponent<MeshRendererComponent>();
renderer->SetEnableLighting(false); // Unlit（陰影なし・自己発光）モードに切り替え
```

- **Scene / Prefab JSON からの設定:**
```json
{
    "type": "MeshRendererComponent",
    "modelName": "resources/model/skydome/skydome.obj",
    "enableLighting": false
}
```
※ デフォルトは `true`（通常ライティング有効）です。

#### スカイドームモード (最奥深度固定・Infinite Sky)
天球モデル（`.obj` 等）を背景空として描画する場合、通常の3Dメッシュとして描画すると球殻の距離（例: 300m）が深度バッファに書き込まれ、**遠くの環境オブジェクト（ビル、浮遊島、山など）が空に隠れて近づいた時にひょっこり出現してしまう（クリッピング現象）** が発生します。
`isSkydome: true`（または `SetIsSkydome(true)`）を設定することで、**業界標準の「最奥深度固定トリック（Infinite Sky）」** が有効になります。

- **仕様と効果:**
  - 頂点シェーダー（`Skydome.VS.hlsl`）でクリップ座標を `.xyww` 出力し、GPU深度値を常に最奥（$Z=1.0$）に固定。
  - **深度書き込み無効（`DepthWrite: Disable`）** により、どれだけ遠くにある環境物・敵・雲も **100% 空の手前に描画** されます（境界の突き抜け・ひょっこり出現が完全消滅）。
  - **前面カリング（`CullMode: Front`）** により天球の内側を正常描画。
  - カメラの平行移動が無効化され、天球の中心が常にカメラに自動追従します。

- **C++ コードからの制御:**
```cpp
auto renderer = gameObject_->GetComponent<MeshRendererComponent>();
renderer->SetIsSkydome(true); // 最奥深度Skydomeパイプラインを適用
```

- **Scene / Prefab JSON からの設定:**
```json
{
    "type": "MeshRendererComponent",
    "modelName": "resources/model/skydome/skydome.obj",
    "isSkydome": true,
    "enableLighting": false
}
```

#### 3Dプリミティブの描画 (PrimitiveRendererComponent)
テスト用の床や障害物など、モデルファイルを用意せずに簡易的な立体を描画したい場合は `PrimitiveRendererComponent` を使用します。

```cpp
auto primitive = gameObject_->AddComponent<PrimitiveRendererComponent>();
primitive->SetShape(PrimitiveType::Cube); // Cube, Sphere, Cylinder, Cone, Torus など
primitive->SetColor(Vector4(1.0f, 0.0f, 0.0f, 1.0f)); // 赤色
```

インスペクタ上からも形状の変更や、材質パラメータ（Roughness / Metallic）の調整が可能です。

---

#### アニメーションモデルとデバッグ機能の利用方法

本エンジンでは、アニメーションする3Dモデルを描画するための `SkinnedMeshRendererComponent` と、アニメーションの再生ロジックを管理する `AnimatorComponent` の**二段構え（分業）アーキテクチャ**を採用しています。

#### 基本的な使い方
1. GameObjectに `SkinnedMeshRendererComponent` をアタッチし、描画したいモデル（GLTF等）をセットします。
2. 同一のGameObjectに `AnimatorComponent` をアタッチし、再生したいアニメーションファイルをセットします。
3. `AnimatorComponent` 側で再生（Play）指示を出すと、自動的に `SkinnedMeshRendererComponent` へ姿勢データが転送され、GPU Skinningによって高速に描画されます。

#### ボーンのデバッグ可視化 (X-Ray描画)
アニメーションのモーション確認や、武器の取り付け位置の確認などに、キャラクターの骨格（ボーン）を画面上に直接描画してデバッグすることができます。

1. **全体表示の切り替え**
   エディタの `Camera & Lights` タブ（DebugScene等）内にある **`Show All Debug Bones`** チェックボックスをONにします。
   これにより、シーン内のすべてのアニメーションモデルのボーンが一斉に可視化されます。
   
2. **描画の仕様 (X-Ray表示)**
   - **関節 (Joints)**: スカイブルーの「球（Sphere）」で描画されます。
   - **骨 (Bones)**: ピンク色の「八面体（Octahedron）」で描画されます。
   - 深度テストが無効（X-Ray表示）になっているため、キャラクターのメッシュに隠れることなく、常に最前面に現在のボーンの姿勢が描画されます。

#### 個別オブジェクトの骨格デバッグ (SkeletonDebugRendererComponent)
シーン全体ではなく、特定のキャラクターのみの骨格を確認したい場合や、ボーンの「ローカル軸の向き（X, Y, Z軸）」を個別に可視化したい場合は、対象の GameObject に `SkeletonDebugRendererComponent` をアタッチしてください。
（※ 同一の GameObject に `SkinnedMeshRendererComponent` がアタッチされている必要があります）

#### デバッグカメラのシームレスな移行
エディタの `Camera & Lights` タブにある `Debug Camera Mode` にチェックを入れると、ゲームカメラからデバッグカメラ（自由操作）に切り替わります。
この際、カメラの位置が初期化されて画面が飛ぶことはなく、**「現在見ているメインカメラの座標と回転」を自動的に引き継いで（スナップして）スタートする** 仕様になっているため、気になった箇所から即座にデバッグ作業へ移行できます。

---

### 6.2 3Dアニメーション・GPU Skinning と Root Motion

本エンジンでは、3Dキャラクターなどのスケルタルアニメーション（ボーン変形）を描画・制御するためのコンポーネントシステムとして、`SkinnedMeshRendererComponent` と `AnimatorComponent` の分離アーキテクチャを採用しています。
これにより、アニメーションのロジック（再生やブレンド）と描画の責務が完全に分離され、高速な処理が可能になっています。

#### 基本的なアニメーション再生

アタッチしたモデル（GLTF / FBX）をシーンに配置し再生する場合は、GameObjectに2つのコンポーネントをアタッチします。

```cpp
// 1. 描画コンポーネントの追加とモデルのロード
auto renderer = gameObject_->AddComponent<SkinnedMeshRendererComponent>();
renderer->LoadModel("sample/walk.gltf");

// 2. アニメーション制御コンポーネントの追加
auto animator = gameObject_->AddComponent<AnimatorComponent>();

// 3. アニメーションの再生 (ファイル名, ループフラグ)
animator->Play("sample/walk.gltf", true);
```

#### アニメーションのクロスフェード (Blend Tree)

走っている状態から歩く状態へ切り替わる際など、モーションがパキッと切り替わるのを防ぐため、本エンジンは**球面線形補間（Slerp）を用いた自動クロスフェード機能**を備えています。

```cpp
auto animator = gameObject_->GetComponent<AnimatorComponent>();

// 第3引数にフェード時間（秒）を指定することで、現在のポーズから次のポーズへ滑らかに遷移します。
// 例：1.0秒かけて "walk" から "sneakWalk" に滑らかに移行する
animator->Play("sample/sneakWalk.gltf", true, 1.0f);
```

#### 足滑りを防ぐ Root Motion (ルートモーション)

従来のアニメーション再生では「その場で足踏みするアニメーション」を再生しながら、プログラム側でキャラクターの座標を移動（Translate）させる必要がありましたが、これでは足の動きと実際の移動速度が合わず「足が滑っている（ムーンウォーク）」ように見えてしまう問題がありました。

本エンジンは、アニメーションデータ自体が持っている「移動量（ルートボーンの差分）」を自動抽出し、それを直接 GameObject の `TransformComponent` の座標に還元する **Root Motion** 機能をサポートしています。

**Root Motion の有効化方法**:
エディタの Inspector（インスペクタ）上にある `Apply Root Motion` のチェックボックスをONにする ことで有効になります。
有効化後は、"walk.gltf" などを再生すると、アニメーションの移動量に連動して自動的に GameObject そのもの（Transform）が移動するようになります。

---

#### GPU Skinning によるアニメーション最適化

本エンジンでは、スケルタルアニメーション（ボーン変形）の処理を、従来のCPU計算や頂点シェーダ(VS)で行うのではなく、事前に **Compute Shader (`Skinning.CS.hlsl`) で並列計算（プレコンピュート）** する最新のアーキテクチャ（GPU Skinning）を採用しています。

#### アーキテクチャのメリット
- **CPU負荷の完全開放**: CPU側ではアニメーションの再生時間と「行列パレット（Matrix Palette）」をGPUへ転送するだけで済み、数千〜数万の頂点に対する行列乗算からCPUが完全に解放されます。
- **描画パイプラインとの親和性**: Compute Shaderで変形した後の頂点データがバッファに書き出されるため、その後のGPUフラスタムカリングなどにそのまま使い回すことができる先進的な設計です。

開発チームの皆様は、通常通りモデルを読み込んで再生するだけで、裏側で自動的にこの恩恵を受けることができます。

---

#### スカイボックス (Skybox) の描画

3Dシーン全体の背景（空や遠景）を描画するための専用クラス `Skybox` が `Renderer/Object/Skybox` に用意されています。
このクラスは `IRenderable` を継承しており、初期化時に全天球画像（Equirectangular 形式のDDS/HDR等）を指定することで、自動的にキューブマップに変換・描画されます。

#### 基本的な使い方

シーン（例：`DebugScene`）内で `Skybox` のインスタンスを保持し、初期化と描画を行います。

```cpp
#include "Renderer/Object/Skybox/Skybox.h"

// 1. ヘッダなどでインスタンスを保持
std::unique_ptr<Skybox> skybox_;

// 2. 初期化時にテクスチャパスを指定
skybox_ = std::make_unique<Skybox>();
skybox_->Initialize("resources/skybox_texture.dds"); 
// ※テクスチャを指定しない場合は、デフォルトの空画像が使用されます。

// 3. 毎フレームの更新と描画
skybox_->Update();
skybox_->SyncBeforeDraw();
skybox_->Draw();
```

※ スカイボックスは常にカメラに追従し、無限遠にあるように描画されるため、特別な座標（Transform）の指定は不要です。

---

#### 汎用2Dプリミティブ描画 (Primitive2DObject / Primitive2DRendererComponent) の利用方法

2D空間での汎用的な図形（四角形、円、線、リング、三角形など）を簡単に描画するための `Primitive2DObject` および、それをエディタで直感的に扱える `Primitive2DRendererComponent` が提供されています。
デバッグ表示、UIの枠線、シンプルなゲージや2Dエフェクトなどに最適です。

- **ヘッダー**: `#include "Framework/Component/Renderer/Primitive2DRendererComponent.h"`
- **主な形状 (`Primitive2DType`)**: `Rect`, `Circle`, `Triangle`, `Line`, `Ring`, `Capsule`
- **C++ コードでの使用例**:
  ```cpp
  #include "Framework/Component/Renderer/Primitive2DRendererComponent.h"
  #include "Core/Type/Primitive2DType.h"

  auto* p2d = uiObj->AddComponent<Primitive2DRendererComponent>();
  p2d->SetShape(Irufemi::Primitive2DType::Rect);
  p2d->SetSize(Irufemi::Vector2(200.0f, 40.0f));     // 幅・高さ
  p2d->SetColor(Irufemi::Vector4(0.1f, 0.6f, 1.0f, 0.8f)); // 水色半透明
  p2d->SetPivot(Irufemi::Vector2(0.5f, 0.5f));        // 中央原点
  p2d->SetTopMost(true);                              // ポストプロセス影響外の最前面UIレイヤー描画
  ```

---

### 6.3 GPU パーティクルシステム (GPUParticleSystem)

本エンジンのパーティクルシステムは、コンピュートシェーダー(CS)によってGPU上で高速に動作します。
`GPUParticleManager` の内部状態は `Irufemi::Synchronized` で保護されており、メインスレッド・ワーカースレッドを問わず、スレッドセーフにエミッターの登録・破棄・パラメータ更新を行えます。
スクリプトやコンポーネントから以下の手順でエミッターを追加・操作することができます。

#### コンポーネントからの利用
GameObject に `ParticleEmitterComponent` をアタッチするだけで、自動的にエディタ上で操作・プレビューが可能です。
また、より高度な表現として、ベクトル場を用いた `ParticleFieldComponent` や、ボクセルベースの `VoxelParticleComponent` などの拡張コンポーネントも提供されています。
エディタ（ImGui）上で設定したパラメータは、JSONファイルとして自動的にシリアライズされ、再実行時にも完全に復元されます。

#### プログラムからの直接利用 (ParticleObject)
ゲーム内でコードから動的にパーティクルを生成・制御したい場合は `ParticleObject` クラスを使用します。
JSONファイルから設定をロードすることで、エディタで作成した複雑なエフェクトをそのまま呼び出すことができます。

```cpp
#include "Renderer/Object/Particle/ParticleObject.h"

// 1. ParticleObject の生成とJSONの読み込み
ParticleObject myParticle;
myParticle.LoadFromJson("resources/particles/explosion.json");

// 2. 座標や必要に応じたパラメータの上書き
myParticle.position_ = Vector3(10.0f, 5.0f, 0.0f);
myParticle.emissionRate_ = 100.0f; // 1秒間に100個発生

// （パラメータをコードから変更した場合は MarkDirty() を呼ぶか、Update内で自動反映されます）
myParticle.MarkDirty();

// 3. 毎フレーム Update を呼ぶ
myParticle.Update();

// 4. 一度に大量に発生（バースト）させたい場合
myParticle.EmitBurst(50);
```

#### 【上級者向け】GPUParticleManager の直接利用
直接マネージャーに通信して描画リクエストを送ることも可能です（独自の最適化を行いたい場合など）。
マネージャーはエンジンインスタンスから取得し、エミッターは「テクスチャ + ブレンドモード + タイムスケール + ライティング」の複合キーで一元管理されます。

```cpp
#include "Renderer/System/ParticleGPU/GPUParticleManager.h"
#include "Renderer/System/ParticleGPU/GPUParticleSystem.h"

// 1. エンジンからマネージャーを取得
auto* gpm = engine_->GetGPUParticleManager();

// 2. マネージャーにエミッターを登録（テクスチャ、ブレンドモード、ポーズ中動作フラグ、ライティング計算フラグ）
auto handle = gpm->RegisterEmitter(
    "effect/particle_tex.png", 
    Irufemi::BlendMode::kBlendModeAdd, 
    false, // trueにするとポーズ中(UnscaledTime)でも動作する
    false  // enableLighting: ライティング影響フラグ
);

// 3. パラメータを設定してマネージャーに更新を通知
GPUParticleEmitter data;
data.emit = 1;
data.type = 0; // 0: Sphere, 1: Beam, 2: Ring, 3: Cylinder, 4: Box
data.translateX = 10.0f;
data.emissionRate = 50.0f; // 1秒あたりの連続放出数

// 4. データの適用
gpm->UpdateEmitterData(handle, data);

// 5. 不要になったら安全に登録解除
// gpm->UnregisterEmitter(handle);
```

#### GPU Bitonic Sort による半透明ソート (Zソート)
本エンジンのGPUParticleSystemは、単に更新処理をGPUで行うだけでなく、**カメラからの距離に応じた半透明描画の並び替え（Zソート）も、完全にGPU上のCompute Shader（ビトニックソート）で完結**しています。
これにより、CPUへデータを差し戻すオーバーヘッドをゼロにしつつ、数万のパーティクルが奥から手前へ正しくアルファブレンドされるAAA品質のレンダリングパイプラインが構築されています。開発者はソートの負荷や描画順序の破綻を一切気にする必要がありません。

#### ゲーム中での一時的なエフェクト再生 (爆発など)
シーン内の特定座標に単発（ワンショット）の爆発エフェクトなどを出したい場合は、`Effect` クラス（低レイヤー）またはプレハブ・コンポーネント（`VoxelParticleComponent` や `ParticleEmitterComponent`）を使用します。

> [!NOTE]
> `Effect` クラス（低レイヤー）を直接利用する場合、エンジン共通ポインタ `Effect::SetEngine(engine)` が設定されている必要があります。通常のゲーム開発では、シーンのプレハブ動的生成（`Instantiate`）や `VoxelParticleComponent::Explode()`、`ScreenEffectComponent` 等のコンポーネント指向での実装が強く推奨されます。

```cpp
#include "Renderer/Object/Effect/Effect.h"

// 1. エフェクトインスタンスの作成と初期化（例：爆発）
Effect myEffect;
myEffect.Initialize(EffectType::kExplosion);

// 2. 指定した座標でエフェクトを発生させる
myEffect.Play(Vector3(10.0f, 0.0f, 5.0f));

// 3. 毎フレーム Update と Draw を呼ぶ
myEffect.Update();
myEffect.SyncBeforeDraw();
myEffect.Draw();
```

#### インスペクターからの ParticleType などの設定
`ParticleEmitterComponent` を GameObject にアタッチした場合、エディターの **Inspector パネル** から以下の新機能を直感的に操作できます。

- **Particle Mesh & Shape (形状と発生範囲)**
  - `Sphere`, `Beam`, `Ring`, `Cylinder`, `Box` などの発生形状を選択可能です。
  - `Box` を選択した場合のみ、専用の `Area Size (X,Y,Z)` を指定して箱状の範囲内に発生させることができます。
  - **Billboard Mode**: パーティクルのカメラに対する向きを `None` (固定), `Billboard` (常にカメラを向く), `Y-Axis` (Y軸固定でカメラを向く・魔法陣などに最適) から選べます。

- **Animation & Visuals (アニメーションと見た目)**
  - **Atlas Rows / Cols**: 連番テクスチャ（スプライトシート）の分割数を指定するだけで、自動的にアニメーション再生されます。
  - **Start / Mid / End Color & Scale**: これまでの開始/終了だけでなく、「中間色・中間スケール」と「それがどのタイミング(Mid Point)で切り替わるか」を設定でき、爆発（白→オレンジ→黒煙）などの複雑な表現が可能になりました。

- **Physics (物理挙動)**
  - 重力やバウンドに加えて、**Jitter (ジッター)** によって不規則なブレ（ノイズ）を与え、魔法の粉や舞い散る火の粉のようなランダムな動きを表現できます。

【重要】これらのパラメータはすべて Inspector のGUIからリアルタイムに変更・確認できます。
本エンジンでは `ParticleEmitterComponent` が `CanUpdateInEditMode()` をサポートしているため、**ゲームを再生していなくても（エディタ編集モードでも）、パラメータを変更した瞬間にリアルタイムでパーティクルの見た目が更新・プレビューされます。**

#### 【開発者向け】パラメータ追加・UI連携の仕組み (RegisterProperties)
`ParticleObject` が持つパラメータ群をインスペクターに表示するためのUI登録処理は、すべて `ParticleObject::RegisterProperties(Component* comp)` というメソッドに集約されています。
これにより、エンジンコアの描画処理とエディターUIの責務が完全に分離されました。もし新しい機能やパラメータを追加したい場合は、この `RegisterProperties` 内に `comp->RegisterProperty(...)` などを1行追記するだけで、自動的にエディタUIに項目が追加され、保存（シリアライズ）も連動して行われるようになっています。

---

---

### 6.4 ポストプロセスシステム (PostProcessManager)

画面全体にかけるポストプロセスエフェクト（PostProcessManager）を使用する際は、**「エフェクトをスタックに追加する順番（描画順序）」** を意識することで、プロの現場でも通用する意図した映像表現が可能になります。

#### 推奨される描画順序（スタックに追加する順）
1. **色調補正系**: ToneMapping, Grayscale, Sepia, HSV など
2. **空間・ぼかし系**: Smoothing, GaussianFilter, RadialBlur, DualKawaseBlur など
3. **画面演出系**: Vignette, Noise, Glitch, Dissolve, GravityDistortion, BlackHoleSupernova など
4. **画面遷移系**: Fade, Slide など

**なぜこの順番なのか？**
例えば、`Vignette`（画面の端を暗くする/色をつける演出）のあとに `Grayscale`（白黒化）をかけてしまうと、ビネットで赤色などを指定してもモノクロになってしまいます。「色調補正」を先に行い、その上から「画面演出」を乗せるのがセオリーです。

#### レイヤー機能 (PreUI / PostUI)
各エフェクトはスタックに追加する際、適用レイヤーを指定することができます。
- **`EffectLayer::PreUI`**: 3Dシーンや背景にのみ適用され、UI（ImGuiやCanvas等）には影響を与えません（デフォルト）。
- **`EffectLayer::PostUI`**: 最終的なUI描画もすべて完了した後に、画面全体に対して適用されます。

#### RAIIによるエフェクト寿命管理 (ScopedEffectHandle)
被弾時のノイズグリッチや危険状態の赤色ヴィネットなど、一時的に適用する画面エフェクトを手動で `RemoveActiveMode` すると、オブジェクト破棄時や例外発生時に解除が漏れ、画面にエフェクトが残り続ける不具合が発生します。
これを根本から防止するため、スコープ脱出時やデストラクタで自動的にエフェクトを安全解除する **`Irufemi::ScopedEffectHandle`**（`#include "Renderer/PostProcess/PostProcessHandle.h"`）が提供されています。

```cpp
#include "Renderer/PostProcess/PostProcessHandle.h"

class PlayerDamageVisualizerComponent : public Component {
private:
    // メンバ変数としてハンドルを保持
    Irufemi::ScopedEffectHandle glitchHandle_;

public:
    void PlayDamageEffect() {
        auto* ppm = GetEngine()->GetPostProcessManager();
        // 推奨: ActivateScoped により、エフェクト有効化とハンドル生成（寿命管理）を1行で安全に実行
        glitchHandle_ = ppm->ActivateScoped(PostProcessMode::Glitch, PostProcessManager::Layer::PreUI);
    }
    // コンポーネント破棄時やオブジェクト削除時、glitchHandle_ のデストラクタにより
    // 自動的に RemoveActiveMode が呼ばれ、エフェクト残留バグを100%防止
};
```

#### マルチパスエフェクトの型安全な操作 (GetEffect<T>)
`Bloom` や `LightShafts`、`Smoothing` などの中間バッファを消費するマルチパスエフェクトは、`PostProcessManager::GetEffect<T>()` を用いてインスタンスを直接取得し、自己完結したパラメータ制御を行えます。

```cpp
#include "Renderer/PostProcess/PostProcessManager.h"
#include "Renderer/PostProcess/Effects/BloomEffect.h"

// Bloomエフェクトの型安全な取得と設定変更
if (auto* bloom = engine->GetPostProcessManager()->GetEffect<BloomEffect>()) {
    bloom->SetThreshold(0.85f);
    bloom->SetIntensity(1.4f);
}
```

#### エフェクトパラメータの自動リセット機構 (SSOT自動展開)
`CombinedPostEffect` 内部では、`IRUFEMI_POSTPROCESS_EFFECT_LIST` マクロによって各エフェクトのパラメータ構造体（`TonemapParameters`, `VignetteParameters` 等）が一元定義されています。
シーン切り替えやリセット時には、このマクロから全構造体のデフォルト初期化代入コードが自動展開されるため、**手動の分岐コードやボイラープレートを追加することなく、パラメータが完全自動で初期値へ安全にリセット** されます。

#### 提供されているエフェクト群と分類
IrufemiEngine は、色調補正から光学ブラー、重力演出に至るまで、豊富なポストプロセスエフェクトを標準搭載しています。

- **色調・階調**: `ToneMapping`(ACES), `Grayscale`, `Sepia`, `HSV`, `Posterization` など
- **空間・光学ブラー**: `DualKawaseBlur`, `DepthOfField`(DoF), `LightShafts`(ゴッドレイ), `RadialBlur`, `DirectionalBlur` など
- **画面演出・ディストーション**: `Vignette`, `Noise`, `Glitch`, `Dissolve`, `ChromaticAberration`, `DisplacementMap` など
- **輪郭・境界検出**: `DepthBasedOutline`(深度+法線複合), `LuminanceBasedOutline`(2Dトゥーン調・アウターシルエット) など
- **画面遷移**: `Fade`(単色フェード), `Slide`(ワイプ演出)
- **重力アクション専用**: `GravityDistortion`(時空歪曲波紋), `BlackHoleSupernova`(重力特異点崩壊・超新星爆発) など

> [!TIP]
> **最新のエフェクト一覧と定義 (Single Source of Truth)**:
> 本エンジンでは、エフェクトの増減に伴うドキュメントの陳腐化を防ぎ、型安全性を100%保証するため、全エフェクトの識別子・パラメータ構造体・レンダーパス種別を [`PostProcessDefinitions.h`](file:///f:/school/3_5/WP1/WP1/project/IrufemiEngine/Renderer/PostProcess/PostProcessDefinitions.h) の `IRUFEMI_POSTPROCESS_EFFECT_LIST` にて一元管理（SSOT）しています。最新の対応状況やパラメータ詳細は該当ヘッダーを参照してください。

#### 【開発者向け】新しいポストプロセスの追加手順 (How to Add New Effect)
新しいエフェクトを追加する際は、以下の自動化パイプラインによって手動ボイラープレートを最小限に抑えて追加できます：
1. **SSOT定義の追加**: [`PostProcessDefinitions.h`](file:///f:/school/3_5/WP1/WP1/project/IrufemiEngine/Renderer/PostProcess/PostProcessDefinitions.h) の `IRUFEMI_POSTPROCESS_EFFECT_LIST` に1行追加する（形式: `V(ModeName, StructType, PassType, DefaultLayer, DisplayName)`）。
2. **HLSL定数の自動同期 (CodeGen)**: Visual Studio の Pre-Build Event により `tools/GeneratePostProcessHlsl.py` が自動実行され、`PostProcessModes.generated.hlsli` に HLSL 定数が自動出力されます（手動編集不要）。
3. **パラメータ構造体の定義と PropertyVisitor**: `Renderer/PostProcess/PostProcessParams.h` に専用構造体を作成し、`template <typename TVisitor> void Visit(TVisitor& visitor)` メソッドを記述して各メンバを `visitor.Visit(...)` で公開します。これにより、DebugUI（ImGui）描画コードを手書きすることなく、エディタ上でパラメータ調整スライダーが完全自動展開されます。
4. **シェーダー・レンダーパスの実装**:
   - **Inlineパスの場合**: `CombinedPostEffect.PS.hlsl` にシェーディング計算を追記する。
   - **Separate / MultiPassの場合**: `Renderer/PostProcess/Effects/` に `IPostProcessEffect` を継承した専用エフェクトクラスを作成し、`PostProcessManager` に登録する。

#### 重力演出専用ポストプロセス
##### 1. GravityDistortion (重力波・空間歪み)
自機の重力ダイブ、ガレキ引き寄せ、ボスの重力攻撃、水界面膜突破時に発生する放射状の同心円時空歪み＋色収差エフェクトです。
- **ワンショット発火 API (推奨・自動減衰)**:
  ```cpp
  auto* ppm = engine->GetPostProcessManager();
  // 中心スクリーンUV, 最大歪み強度, 持続時間(秒), 最大半径, 色収差比率
  ppm->TriggerGravityDistortion(Irufemi::Vector2(0.5f, 0.5f), 0.10f, 0.5f, 1.3f, 0.035f);
  ```
- **パラメータ構造体による手動制御**:
  ```cpp
  PostProcessManager::GravityDistortionParams params{};
  params.center = Irufemi::Vector2(0.5f, 0.5f);
  params.radius = 0.35f;
  params.intensity = 0.06f;
  params.waveWidth = 0.08f;
  params.chromaticRatio = 0.04f;
  ppm->GetGravityDistortionParams() = params;
  ```

##### 2. BlackHoleSupernova (ブラックホール・重力特異点＆超新星爆発)
自機撃墜時や大ボス撃破時のクライマックス演出に用いられる専用フルスクリーン・ポストプロセスです。事象の地平面（漆黒吸光）、求心スワール重力レンズ、過熱プラズマ火球、およびGRAVITY DAZE風の重力色収差衝撃波を一体描画します。
```cpp
#include "Renderer/PostProcess/PostProcessManager.h"

auto* ppm = engine->GetPostProcessManager();
ppm->AddActiveMode(PostProcessMode::BlackHoleSupernova, PostProcessManager::Layer::PreUI);

PostProcessManager::BlackHoleSupernovaParams params{};
params.center = Irufemi::Vector2(0.5f, 0.5f); // 撃墜瞬間のスクリーンUV
params.blackHoleRadius = 0.15f;              // 事象の地平面の半径
params.blackHolePullIntensity = 0.08f;       // 求心スワール吸引強度
params.supernovaRadius = 0.45f;              // 超新星プラズマ火球半径
params.supernovaIntensity = 0.12f;           // 衝撃波変位強度
params.supernovaChromaticRatio = 0.05f;      // 重力色収差比率
params.supernovaFlashIntensity = 0.30f;      // ガンマバースト白熱光強度
ppm->GetBlackHoleSupernovaParams() = params;
```

#### オブジェクト個別のカスタムエフェクト適用 (CustomEffectParams)
画面全体ではなく、特定のキャラクターがダメージを受けたときの点滅や、敵が倒れたときのディゾルブ（消失）など、**特定のオブジェクトに対してのみエフェクトを適用**するための機能です。

##### 1. カスタムパラメータの登録方法
```cpp
auto* ppm = engine->GetPostProcessManager();
PostProcessManager::CustomEffectParams params;
params.param1 = 0.5f; // 例: ディゾルブのしきい値など
uint32_t id = ppm->RegisterCustomEffectParams(params);
float effectParam = static_cast<float>(id) / 255.0f;
```

##### 2. 動的オブジェクトへの適用 (EffectMaskComponent)
```cpp
auto* mask = gameObject->AddComponent<EffectMaskComponent>();
mask->SetEnableEffectMask(true);
mask->SetCustomEffectType(static_cast<int32_t>(PostProcessMode::Dissolve));
mask->SetCustomParams(params);
```

##### 【仕様】アウター・シルエット方式アウトライン (Outer Silhouette)
本エンジンの個別輪郭線（`LuminanceBasedOutline`）は、**モデル表面のテクスチャやライティングを100%保護し、外側の背景ピクセルのみに輪郭線を描画する「アウター・シルエット方式」**（Overwatch等の商用AAAタイトル標準）を採用しています。
- モデル表面のデザインが塗り潰されず、0.5px単位のシャープな輪郭線を付与可能。
- ロックオン時の敵ハイライト表示等に最適です。

##### 3. バッチ描画（環境物）への適用 (ModelBatchRendererComponent)
```cpp
// 1個目は通常描画（エフェクトなし）
batchComponent->AddInstance(transform1);

// 2個目には個別のエフェクト（例: Type 8 = Dissolve）を適用
batchComponent->AddInstance(transform2, 8, effectParam, true);
```

---

### 6.5 G-Buffer拡張とMRT対応・高品質アウトライン

本エンジンでは高品質なポストプロセス（法線ベースのアウトライン等）を実現するため、**G-Buffer（複数レンダーターゲット: MRT）** による描画パイプラインを採用しています。
3Dオブジェクト（メッシュやパーティクル、ラインなど）を描画するピクセルシェーダーを作成する際は、必ず エンジン共通ヘッダー `#include "BasePassPixelOutput.hlsli"` を記述し、そこに含まれる以下の出力フォーマット（`PixelShaderOutput`）に従ってください。

```hlsl
// EngineResources/shaders/BasePassPixelOutput.hlsli に定義されている出力構造体
struct PixelShaderOutput {
    float32_t4 color : SV_TARGET0;    // メインカラー
    float32_t4 mask : SV_TARGET1;     // 個別エフェクト用のマスク
    float32_t4 normal : SV_TARGET2;   // 法線（xyzに格納、[-1,1] の範囲など）
    float32_t4 material : SV_TARGET3; // マテリアル情報（Roughness, Metallic等）
    float32_t4 velocity : SV_TARGET4; // ベロシティ（モーションブラー用など）
};
```
※法線やベロシティを持たない描画物（単純なライン等）であっても、パイプラインエラーを防ぐためダミー値（`float32_t4(0,0,0,0)` 等）を全ターゲットに出力する必要があります。

#### 法線ベースの高品質アウトライン (DepthBasedOutline)
ポストプロセスのモードとして `DepthBasedOutline` を適用すると、**深度（Depth）の差分**に加えて、G-Bufferに出力された**法線（Normal）の差分**も組み合わせてエッジ検出が行われます。
これにより、深度差がほとんどない平面上の折り目（ハードエッジ）にも美しい輪郭線が描画される AAA 品質のアウトライン表現が可能となっています。
※個別オブジェクトへのマスク適用（`EffectMaskComponent`）やアウターシルエットの仕様は、前述の [6.4節 オブジェクト個別のカスタムエフェクト適用](#オブジェクト個別のカスタムエフェクト適用-customeffectparams) を参照してください。

---

#### 半透明・エフェクトオブジェクトの描画とZソート

本エンジンでは、地形やキャラクターなどの不透明オブジェクトの後に、オーラやレーザーなどの半透明エフェクトを正しい順番（奥から手前）で描画するための **独立した半透明描画パス (MainTransparentPass)** をサポートしました。

これまで半透明オブジェクトを描画する際、Zバッファへの書き込み（DepthWrite）をDisableにすると、後から描画される不透明オブジェクトに上書きされて見えなくなる問題がありましたが、この機能を利用することで正しく描画されます。

#### 利用方法

半透明や加算合成で描画したい Primitive3DObject （またはそれを保持するRendererComponent）に対して、初期化時に SetIsTransparent(true) を設定し、同時にPSO設定で DepthWrite::Disable を指定します。

```cpp
#include "Renderer/Object/3D/Primitive/Primitive3DObject.h"

// 1. オブジェクトの初期化
auto aura = std::make_shared<Primitive3DObject>();
aura->Initialize(PrimitiveType::Sphere);

// 2. カスタムPSOの適用 (DepthWrite を Disable にする)
auto pso = engine->GetPSOManager()->GetPSO("EnergyCore", BlendMode::kBlendModePremultiplied, PSOManager::DepthWrite::Disable, PSOManager::CullMode::Back);
aura->SetCustomPSO(pso);

// 3. 半透明フラグを有効にする (重要！)
// これにより、不透明オブジェクトをすべて描き終わった後に、カメラからの距離でソートされて描画されます。
aura->SetIsTransparent(true);
```

#### 注意点
- SetIsTransparent(true) を設定したオブジェクトは、自動的にカメラからの距離（distanceToCamera）を計算し、**Z値の降順（Back-to-Front）**でソートされて描画されます。
- 不透明な通常のモデルに SetIsTransparent(true) を設定しないでください（Early-Zカリングなどの恩恵が受けられず、パフォーマンスが低下します）。

---

### 6.6 GPU カリング (GPU Culling & ExecuteIndirect)

本エンジンでは、大量の同一モデル（がれき、草、パーティクルなど）を描画する際のCPU負荷（フラスタムカリングやメモリ転送）を劇的に削減するため、**Compute Shader による GPU フラスタムカリング** をサポートしました。

この機能を有効にすると、CPU側でのカリング判定がスキップされ、GPUが自身で「カメラに映っているオブジェクト」だけを判定し、`ExecuteIndirect` を通して一括描画するようになります。

#### 利用方法 (コンポーネントからの利用)
最も簡単な方法は、`ModelBatchRendererComponent` （またはそれを内部で生成するマネージャークラス）に対して、初期化時にフラグを有効化することです。

```cpp
#include "Framework/Component/Renderer/ModelBatchRendererComponent.h"

// 1. バッチレンダラーコンポーネントの取得または追加
auto batchRenderer = gameObject->AddComponent<ModelBatchRendererComponent>();

// 2. GPUカリングを有効にする
batchRenderer->SetUseGPUCulling(true);
```

#### 利用方法 (プログラム/バッチからの直接利用)
`ModelBatch` などのバッチクラスを直接生成して描画している場合も、同様に `SetUseGPUCulling(true)` を呼び出すだけです。

```cpp
#include "Renderer/Object/Batch/ModelBatch.h"

// 1. バッチの生成
ModelBatch myBatch;
myBatch.Initialize("DebrisModel");

// 2. GPUカリングを有効化
myBatch.SetUseGPUCulling(true);

// 3. インスタンスの追加 (CPU側でTransformを設定)
for (int i = 0; i < 10000; ++i) {
    myBatch.AddInstanceWorld(Matrix4x4::MakeTranslation(Vector3(i * 1.0f, 0, 0)));
}

// 4. 描画
// 内部で自動的にGPUカリング用Compute Shaderが実行され、その後ExecuteIndirectで描画されます
myBatch.Draw();
```

#### 注意点・制限事項
- GPUカリングは、**1000個以上の大量のインスタンス**を描画する場合に効果を発揮します。少数のオブジェクトに対しては、逆にCompute Shaderのディスパッチオーバーヘッドが上回る可能性があります。
- 描画対象のオブジェクトには、ローカル空間での正確な **バウンディングスフィア (BoundingSphere)** が設定されている必要があります（`GetBoundingSphereRadius` の値がカリングに使用されます）。

---

### 6.7 Bindless Resources (Descriptor Indexing) 完全移行

現在、IrufemiEngine はパフォーマンス向上を目的とした **Bindless Resources** (Descriptor Indexing) への移行を完了しました。

#### テクスチャバインドのルール（完全移行後）
C++側の基盤構築およびHLSL側の対応がすべて完了しており、全テクスチャが巨大な配列 (gTextures および gTextureCubes) に格納されています。
**HLSL（シェーダー）側はレガシーな register(t0) への個別バインドを廃止し、定数バッファ（MaterialやParams）経由で渡された textureIndex を用いて配列からテクスチャを参照します。**

エンジンのルートシグネチャからは互換性維持のためのレガシースロット (RootSlot::Texture, RootSlot::EnvMap) が完全に削除されました。
これにより、各バッチ処理やレンダラーの実装から手動でのテクスチャバインド処理（SetGraphicsRootDescriptorTable）は不要になっています。

- **C++側の変更**: 各種描画コマンド（PrimitiveBatch, ModelBatch, GPUParticleSystem等）や RenderPackets から textureHandle が削除されました。テクスチャの切り替えは自動的に Material バッファ内の textureIndex の更新によって行われます。
- **PostProcess側の変更**: PostProcessManager は各エフェクトの描画前に PostProcessBindlessParams 定数バッファ (b1) を更新し、mainTextureIndex および extraTextureIndex をシェーダーへ渡します。シェーダー内では gTexture マクロが自動的にインデックス解決を行うため、既存のエフェクト計算コードを書き直すことなくBindlessの恩恵を受けられます。

---

### 6.8 シェーダーの追加・変更・コンパイル構成

本エンジンでは、パフォーマンスと開発効率の両立のため、ビルド構成（Configuration）によってシェーダーのコンパイル方式が完全に切り替わる「ハイブリッド構成」を採用しています。

#### 6.8.1 開発時のホットリロード (Editor / Debug ビルド)
Editor または Debug モードで起動している場合、ゲームを実行したまま（エディタを立ち上げたまま）、シェーダーファイル（`.hlsl` または `.hlsli`）を上書き保存するだけで、自動的に **ホットリロード** が行われます。
- バックグラウンドでフォルダを監視しており、保存を検知すると安全に古いシェーダーとPSOキャッシュを破棄し、再コンパイルして即座に描画に反映します。
- 一々ビルドし直す必要がないため、ライティングの微調整やエフェクト作成のイテレーションが非常に高速です。

#### 6.8.2 シェーダーの追加手順と命名規則
新しいシェーダー（`.hlsl`）を追加する際は、必ずファイル名の中に「プロファイル名」を含めるようにしてください。ビルド時の自動コンパイルバッチ（`CompileShaders.bat`）がこのファイル名を見て適切なコンパイルを行います。

- **頂点シェーダー**: `xxx.VS.hlsl`
- **ピクセルシェーダー**: `xxx.PS.hlsl`
- **コンピュートシェーダー**: `xxx.CS.hlsl`
- **ジオメトリシェーダー**: `xxx.GS.hlsl`

#### 6.8.3 配布環境 (Release ビルド) と CSOファイル
Releaseビルドでは、`dxcompiler.dll` などのコンパイラを一切ロードせず、最速で起動させる仕組みになっています。
- Visual Studio で F5（またはビルド）を押した直後に、裏で自動的に `CompileShaders.bat` が走り、すべての `.hlsl` をコンパイルして `.cso`（コンパイル済みバイナリ）を生成します。
- 実行時には `.hlsl` ではなく生成された `.cso` を直接メモリに読み込みます。
- そのため、最終的にプレイヤーに配布（リリース）する際は、**「すべての `.hlsl` / `.hlsli` ファイルは削除して `.cso` だけを含める」** ことで、ソースコードの秘匿化と容量削減が可能です（Compute Shaderも含む）。

#### 6.8.4 Bindless Resources とカスタムシェーダーでのテクスチャアクセス
本エンジンは最新の **Bindless アーキテクチャ (Descriptor Indexing)** に移行しています。
C++ 側でシェーダーごとにテクスチャを個別にバインドするのではなく、すべてのテクスチャが1つの巨大な配列に登録されており、シェーダー側からは **「テクスチャのインデックス（番号）」** さえ分かれば自由にアクセスできます。

独自のシェーダー (`xxx.PS.hlsl` など) を書く場合は、以下のようにお作法に従って記述してください。

```hlsl
// 1. エンジン標準の Bindless 定義をインクルードする
#include "Bindless.hlsli"
#include "Material.hlsli"
#include "PerFrame.hlsli"

// マテリアル定数バッファ（ここに textureIndex が入っている）
ConstantBuffer<Material> gMaterial : register(b0);
// 共通サンプラ
SamplerState gSampler : register(s0);

float4 main(VertexShaderOutput input) : SV_TARGET {
    // 2. gTextures (Bindless配列) から、テクスチャインデックスを使って色を取得する
    float4 color = gTextures[gMaterial.textureIndex].Sample(gSampler, input.texcoord);
    return color;
}
```
※このように書くことで、C++ 側でマテリアルにテクスチャパスを指定するだけで、自動的に適切なインデックスがシェーダーに渡るようになります。

---

### 6.9 動的シャドウマップシステム (ShadowPass & ShadowMap)

本エンジンでは、DirectX 12 と RenderGraph に統合された **動的リアルタイムシャドウマップ（Depth Shadow Mapping）** を標準搭載しています。

#### 1. アーキテクチャ概要
- **解像度**: 高精細な `2048 x 2048` ピクセルの単一浮動小数点深度バッファ (`DXGI_FORMAT_R32_TYPELESS` / `D32_FLOAT` / `R32_FLOAT`) をダブルバッファリング管理 (`kMaxFramesInFlight`)。
- **RenderPass 構成**: 不透明描画（`MainOpaquePass`）に先立ち、レンダーグラフの `ShadowPass` が起動して深度値のみを高速レンダリングします。
  - 専用の軽量シェーダー（`ShadowMap.VS.hlsl`, `ShadowMapSkinning.VS.hlsl`, `ShadowMapBatch.VS.hlsl`）を用い、ピクセルシェーダーの評価やカラー出力を完全にバイパスすることで、GPU負荷を極小化しています。
- **カスケード・直交射影の自動更新**:
  `DrawManager::UpdateDirectionalLight` 内で、平行光源（DirectionalLight）の照射方向ベクトルとプレイヤーの注視点（Target Position）から、シーンを過不足なくカバーする直交射影行列（`orthoSize`）を毎フレーム自動算出してライト空間のビュー・プロジェクション行列（`viewProjection`）を構築します。

#### 2. HLSL シェーダー側での影計算 (PCF フィルタリング)
影の境界のジャギ（エイリアシング）や、セルフシャドウのモアレ（シャドウアクネ）を防ぐため、エンジン共通シェーダー [`Lighting.hlsli`](EngineResources/shaders/Lighting.hlsli) には高品質な **PCF (Percentage-Closer Filtering)** が組み込まれています。

```hlsl
// Lighting.hlsli に実装されている影係数計算
float CalculateShadow(float4 shadowPos, Texture2D<float> shadowMap, SamplerComparisonState shadowSampler, float3 normal, float3 lightDir) {
    // 1. 同次座標系からライト空間 UV / 深度へ変換
    float3 projCoords = shadowPos.xyz / shadowPos.w;
    float2 uv = projCoords.xy * float2(0.5f, -0.5f) + 0.5f;
    float currentDepth = projCoords.z;

    // 範囲外なら影を作らない (日照エリア扱い)
    if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f || currentDepth > 1.0f) {
        return 1.0f;
    }

    // 2. 法線傾斜に応じたスロープスケールデプスバイアス
    float bias = max(0.0005f * (1.0f - dot(normal, lightDir)), 0.0001f);

    // 3. 4タップ PCF 比較サンプリング (SampleCmpLevelZero)
    // テクセル近傍をサンプリングし、半影（ペナンブラ）を滑らかに表現
    float shadowFactor = 0.0f;
    // ... 4タップ平均処理 ...
    return shadowFactor;
}
```

#### 3. Bindless / レガシースロットの連携
- **PerFrame 定数バッファ (Bindless)**: `fr.perFrameData->shadowMapIndex` にシャドウマップのSRVインデックスが渡され、空間空間テクスチャ配列から参照可能です。
- **レガシー互換テーブル**: ピクセルシェーダーの `register(t5)`（`RootSlot::ShadowMap`）およびサンプラ `register(s1)`（`SamplerComparisonState`）にも自動バインドされているため、カスタムシェーダーからも即座に影計算を利用できます。

---

### 6.10 ライティングシステム (Directional, Point, Spot, Area)

IrufemiEngine は 3D空間の陰影を豊かに表現するため、4大主要光源（平行光源・点光源・スポットライト・面光源）をサポートしています。

#### 1. 光源の種類とデータ構造
- **平行光源 (`DirectionalLight`)**:
  - 太陽光や月光などの無限遠からの平行光線。影の生成源（キャスター）となります。
  - パラメータ: `color` (RGB), `intensity` (輝度), `direction` (照射方向ベクトル・正規化必須)。
- **点光源 (`PointLight`)**:
  - 爆発、松明、弾薬のマズルフラッシュなど、特定座標から全方向へ放射状に広がる光源。
  - パラメータ: `position` (ワールド座標), `color`, `intensity`, `radius` (減衰限界距離), `decay` (減衰指数・通常2.0)。
- **スポットライト (`SpotLight`)**:
  - サーチライト、懐中電灯、レーザー照射部など、円錐状に特定方向を照らす光源。
  - パラメータ: `position`, `direction`, `color`, `intensity`, `distance`, `decay`, `cosAngle` (外角コサイン), `cosFalloffStart` (内角コサイン)。
- **面光源 (`AreaLight`)**:
  - ネオン看板、モニター画面、発光パネルなど、矩形平面から特定方向へ広がる矩形面光源。
  - パラメータ: `color` (RGBA), `position` (中心座標), `intensity`, `direction` (照射法線), `range` (到達最大距離), `size` (矩形幅・高さ Vector2), `isActive` (有効フラグ)。

#### 2. C++ コードからのライト設定
シーン（`BaseScene`）または管理コンポーネントから、フレーム描画時に `DrawManager` へライト情報を渡します。
```cpp
#include "Renderer/DrawManager.h"
#include "Renderer/Data/DirectionalLight.h"
#include "Renderer/Data/PointLight.h"
#include "Renderer/Data/SpotLight.h"
#include "Renderer/Data/AreaLight.h"

// 1. 平行光源の設定（太陽光）
DirectionalLight sunLight{};
sunLight.color = { 1.0f, 0.95f, 0.9f };
sunLight.intensity = 1.2f;
sunLight.direction = Vector3(0.3f, -1.0f, 0.5f).GetNormalized();

// 2. 点光源の動的追加（爆発の瞬間発光など）
PointLight explosionLight{};
explosionLight.position = hitPos;
explosionLight.color = { 1.0f, 0.5f, 0.1f };
explosionLight.intensity = 5.0f;
explosionLight.radius = 25.0f;
explosionLight.decay = 2.0f;

// 3. 面光源の追加（SFパネル等の発光）
AreaLight panelLight{};
panelLight.position = panelPos;
panelLight.color = { 0.2f, 0.8f, 1.0f, 1.0f };
panelLight.intensity = 3.0f;
panelLight.direction = Vector3(0.0f, 0.0f, 1.0f);
panelLight.size = Vector2(4.0f, 2.0f);
panelLight.range = 15.0f;
panelLight.isActive = 1;

std::vector<PointLight*> pointLights = { &explosionLight };
std::vector<SpotLight*> spotLights = {};
std::vector<AreaLight*> areaLights = { &panelLight };

// DrawManager にフレームデータとして転送 (解像度はエンジンアクセサから動的取得)
drawManager->SetFrameData(camera, time, deltaTime, sunLight, pointLights, spotLights, areaLights, engine->GetGameResolution());
```

#### 3. PBR / Blinn-Phong シェーダーへの統合
GPU上の定数バッファおよび構造化バッファ（StructuredBuffer）に転送され、すべての不透明・半透明シェーダーにおいて、Lambert拡散反射、Blinn-Phong鏡面反射、および減衰逆二乗則が自動適用されます。
※陰影計算を完全にバイパスしたいモデルは、前述の `renderer->SetEnableLighting(false)` により Unlit 化が可能です。

---

## 7. 物理・衝突判定と空間分割 (Physics, Colliders & BVH)

### 7.1 衝突判定マネージャー (CollisionManager)
- **ヘッダー**: `#include "Physics/CollisionManager.h"`

#### 7.1.1 物理・当たり判定 (Colliders & Raycast)
3D空間での衝突判定には、以下のコライダーコンポーネントを使用します。
- **`SphereColliderComponent`**: 球体形状での当たり判定（最速・弾やアイテム回収判定に最適）。
- **`AABBColliderComponent`**: 軸平行境界箱による当たり判定（回転しない静的障害物向け）。
- **`OBBColliderComponent`**: 有向境界箱による当たり判定（自機・ボス機体・回転する障害物向け）。

```cpp
auto obj = std::make_shared<GameObject>("PlayerCollider");
auto* collider = obj->AddComponent<OBBColliderComponent>();
collider->SetSize({ 1.0f, 2.0f, 1.0f });
```
*(※衝突時に処理を行いたい場合は、各コンポーネントの `OnCollisionEnter(GameObject* hitObject)` コールバックをオーバーライドします)*

#### 7.1.2 幾何形状の直接交差判定 (Collision)
`OBB` (有向境界箱) や `Sphere` などの定義と交差判定を行います。

```cpp
#include "Physics/Collision/Collision.h"
#include "Core/Math/Geometry/OBB.h"
#include "Core/Shape/Sphere.h"

// 衝突判定の実装例
if (IsCollision(playerOBB, enemySphere)) {
    // プレイヤーと敵の球が当たった時の処理
}
```

#### 7.1.3 デバッグ描画と当たり判定 (CollisionManager & DebugPrimitiveRenderer)

コライダー（AABB, Sphere, OBB）のデバッグ描画（ワイヤーフレーム表示）は、各コンポーネント内で個別に実装・描画する必要はありません。
すべてのコライダーのデバッグ描画は、`CollisionManager::DrawDebug()` にて一元管理・一括描画されるアーキテクチャに変更されています。

- **デバッグ表示の自動化**:
  `ColliderComponent` を継承してコンポーネントを作成し、`GetWorldAABB()` などの形状取得メソッドを正しくオーバーライドすれば、エディタ上で自動的にワイヤーフレームが表示されます。
- **自分で `DrawDebug` を呼ばない**:
  各コンポーネント内に独自の `DrawDebug` 等の描画命令（PrimitiveManager等の呼び出し）を記述すると、描画が重複したり描画ステートが壊れる原因となるため、当たり判定の描画は完全にマネージャに委譲してください。

##### 【開発者向け】自前のデバッグ図形描画 (`DebugPrimitiveRenderer`)
ゲーム固有の攻撃範囲、索敵球、スポーン境界、レールのノードなどを一時的に画面へデバッグ描画したい場合は、GPUインスタンシング対応の `DebugPrimitiveRenderer`（`#include "Renderer/Object/Batch/DebugPrimitiveRenderer.h"`）を利用できます。
`DebugCategory` によるカテゴリ別フィルタリング（表示/非表示）に対応しており、毎フレームの `Update` 内で登録するだけで自動的にバッチ描画されます。

```cpp
#include "Renderer/Object/Batch/DebugPrimitiveRenderer.h"

// Engine コアから直接取得（DrawManager 経由ではありません）
auto* debugRenderer = engine_->GetDebugPrimitiveRenderer();
if (debugRenderer && debugRenderer->IsEnabled()) {
    // 1. デバッグ球の描画 (中心座標, 半径, カラーRGBA, カテゴリ)
    debugRenderer->AddSphere(targetPos, 2.5f, {1.0f, 0.2f, 0.2f, 0.6f}, DebugCategory::Combat);

    // 2. デバッグ直方体の描画 (ワールド変換行列, カラーRGBA, カテゴリ)
    Matrix4x4 boxWorld = Matrix4x4::MakeScale({4.0f, 2.0f, 4.0f}) * Matrix4x4::MakeTranslation(areaCenter);
    debugRenderer->AddCube(boxWorld, {0.2f, 1.0f, 0.4f, 0.5f}, DebugCategory::Level);
}
```

#### 7.1.4 レイキャストによる空間クエリ (`Raycast` / `RaycastAsync`)
シーン内のオブジェクトやコライダーに対する視線判定・着弾判定を行うには、`CollisionManager` のレイキャスト機能を利用します。同期版とThreadPoolを活用した非同期版が提供されています。

```cpp
#include "Physics/CollisionManager.h"
#include "Core/Shape/LinePrimitive.h"

// 1. 同期レイキャスト
Irufemi::Ray ray;
ray.origin = GetTransform()->GetWorldPosition(); // 始点
ray.diff = GetTransform()->GetWorldForward();   // 終点への方向・差分ベクトル

RaycastHit hitInfo;
if (collisionManager->Raycast(ray, hitInfo, 1000.0f, 0xFFFFFFFF, GetGameObject())) {
    // hitInfo.hitObject : 当たったGameObject
    // hitInfo.hitPoint  : 衝突したワールド座標
    // hitInfo.distance  : 衝突点までの距離
}

// 2. 非同期レイキャスト (ThreadPool によるバックグラウンド並行クエリ)
auto future = collisionManager->RaycastAsync(engine_->GetThreadPool(), ray, 1000.0f);
// ... 次フレームや必要になったタイミングで future.get() により結果を取得
```

#### 7.1.5 衝突レイヤーとフィルタリング (CollisionLayers.json / layerMask)
オブジェクトの種類（自機、敵、デブリ、地形など）ごとに衝突可否を柔軟に制御するため、ビットマスクによるレイヤーシステムを搭載しています。

- **設定ファイル**: `resources/config/CollisionLayers.json` (または `layers.json`)
  - レイヤーID（0: Default, 1: Player, 2: Enemy, 3: Debris_Neutral, 4: Debris_Player, 5: Debris_Enemy, 6: Environment 等）を定義。
- **レイヤーマスクの指定とフィルタリング**:
  `CollisionManager::GetLayerMask("LayerName")` でビットマスクを取得し、`Raycast` の `layerMask` 引数に渡すことで、特定のレイヤーのみを対象としたクエリが可能です。
  ```cpp
  // "Enemy" と "Environment" レイヤーのみを対象にレイキャスト
  uint32_t mask = collisionManager->GetLayerMask("Enemy") | collisionManager->GetLayerMask("Environment");
  if (collisionManager->Raycast(ray, hitInfo, 1000.0f, mask, GetGameObject())) {
      // 敵または地形にヒットした際の処理
  }
  ```

---

---

### 7.2 動的BVH(TLAS)による空間クエリ最適化
#### 動的BVH(TLAS)による当たり判定の自動最適化
本エンジンでは、数万のオブジェクトが同時に存在しても当たり判定が破綻しないよう、内部で **動的BVH (Dynamic Bounding Volume Hierarchy)** と呼ばれる空間分割木が自動的に構築されています。
開発者は、各オブジェクトに `SphereColliderComponent` や `OBBColliderComponent` などをアタッチするだけで、`CollisionManager` が裏側でO(N^2)の判定を **O(N log N)** に高速化し、不要な計算を自動で枝刈りします。
特別な空間分割のコーディングをゲーム側で行う必要はありません。

#### C++コード上での手動コライダー追加（プレハブを使わない場合）
JSONプレハブを使用せず、C++から直接オブジェクトを生成してBVHによる当たり判定を適用する場合は、以下のように記述します。マネージャーへの手動登録は不要で、`AddComponent` するだけで完結します。

```cpp
#include "Framework/GameObject/GameObject.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/Component/Collider/SphereColliderComponent.h"
#include "Framework/Component/Component.h"

// 衝突時の処理を記述したカスタムコンポーネント
class MyEnemyComponent : public Component {
public:
    std::string GetComponentName() const override { return "MyEnemyComponent"; }

    // 他のコライダーと接触した瞬間に自動で呼ばれる
    void OnCollisionEnter(GameObject* hitObject) override {
        if (hitObject->GetName() == "PlayerBullet") {
            GetGameObject()->Destroy(); 
        }
    }
};

// シーンの Initialize 等でオブジェクトをスポーンさせる関数
void SpawnEnemyFromCode(BaseScene* scene) {
    // 1. オブジェクト生成
    auto enemyObj = std::make_shared<GameObject>("Enemy");

    // 2. Transformの追加と座標設定
    auto* transform = enemyObj->AddComponent<TransformComponent>();
    transform->SetPosition({0.0f, 0.0f, 5.0f});

    // 3. コライダーの追加（これだけで自動的にBVHツリーに組み込まれます）
    auto* collider = enemyObj->AddComponent<SphereColliderComponent>();
    collider->SetLocalRadius(2.0f);

    // 4. カスタムロジックコンポーネントの追加
    enemyObj->AddComponent<MyEnemyComponent>();

    // 5. シーンへの登録（以降、毎フレームBVHによる当たり判定が走ります）
    scene->AddGameObject(enemyObj);
}
```

*(※開発コラム：BVHとデータ指向設計における「最適なオブジェクトプール」)*
> ゲーム開発において、大量のガレキや敵を管理する際に `ObjectPool<T>` (ポインタの使い回し) を使うのは常識ですが、BVHのような極めて高速なツリー走査が求められるシステムにおいては、ポインタベースのプールはフラグメンテーションによるキャッシュミスを引き起こし逆効果となります。
> そのため本エンジンの `DynamicBVH` 等では、ポインタの代わりに「インデックスベースの配列プール (`std::vector` とフリーリストの組み合わせ)」を採用し、完全なゼロアロケーションと極大のL1/L2キャッシュヒット率を実現しています。

#### 非同期レイキャスト (RaycastAsync) によるタイムスライシング
敵のロックオンや地形判定などで毎フレーム大量の `Raycast` を発行すると、BVHで高速化されているとはいえCPUスパイク（処理落ち）の原因になります。
そのため、即座に結果が必要ない判定（例：定期的な索敵など）には、エンジンの `ThreadPool` を用いて別スレッドで判定を行う **`RaycastAsync`** を使用してください。

```cpp
// 毎フレーム判定するのではなく、一定時間ごとに非同期でRaycastを発行する例
if (timeSinceLastCheck > 0.1f) {
    auto* collisionManager = engine_->GetCollisionManager();
    // 非同期でレイキャストを発行し、結果を future として受け取る
    raycastFuture_ = collisionManager->RaycastAsync(engine_->GetThreadPool(), ray, maxDistance, layerMask);
    timeSinceLastCheck = 0.0f;
}

// 別のフレームで、判定が完了しているかチェックして結果を受け取る
if (raycastFuture_.valid() && raycastFuture_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
    auto [isHit, hitInfo] = raycastFuture_.get();
    if (isHit) {
        // ヒットした場合の処理
    }
}
```

---

### 7.3 数学・ベクトル演算のベストプラクティス

本エンジンの数学クラス（`Vector2`, `Vector3`, `Vector4`）は、AAAエンジンの標準仕様に合わせて大幅に拡張されており、IDEの入力補完（インテリセンス）に最適化されています。
グローバル関数（例: `Normalize(v)`）を探す必要はなく、すべて直感的なメンバ関数や定数としてアクセス可能です。

```cpp
// 1. 便利な標準定数の利用
Vector3 up = Vector3::up;       // (0, 1, 0)
Vector3 zero = Vector3::zero;   // (0, 0, 0)
Vector3 right = Vector3::right; // (1, 0, 0)

// 2. 直感的なメンバ関数
float len = direction.Length();
direction.Normalize();              // 破壊的正規化
Vector3 norm = direction.GetNormalized(); // 非破壊的正規化

// 3. 内積 (Dot) と 外積 (Cross)
// メンバ関数呼び出し、または Core/Math/MathFunction.h のグローバル関数 Dot / Cross を使用可能
float d = forward.Dot(targetDir);   // または Dot(forward, targetDir)
Vector3 c = up.Cross(right);        // または Cross(up, right)

// 4. 浮動小数点の安全な比較
// == 演算子は厳密な一致(ビット完全一致)を判定しますが、計算誤差が含まれる場合は Equals を使います。
if (currentPos.Equals(targetPos, 0.001f)) {
    // 目標地点に到達した
}
```

#### 5. 補間・イージング基盤 (Lerp / Slerp / EvaluateEase)
キャラクターの移動補間、カメラワーク、UIフェードイン・浮遊など、ゲームプレイの滑らかな演出には `#include "Core/Utility/Ease.h"` のイージング・補間ユーティリティを使用してください。

- **ヘッダー**: `#include "Core/Utility/Ease.h"`
- **主な関数と型**:
  - `Lerp(a, b, t)` / `LerpClamped(a, b, t)`: スカラー (`float`) およびベクトル (`Vector2`, `Vector3`, `Vector4`) に対応した汎用線形補間。
  - `Lerp(q1, q2, t)` / `Slerp(v1, v2, t)`: クォータニオンおよび方向ベクトルの球面線形補間。
  - `EvaluateEase(EaseType type, float t)`: 指定したイージング関数で進行度 `t (0.0f〜1.0f)` をカーブ変換。
  - **`EaseType` 一覧**: `Linear`, `EaseInSine`, `EaseOutSine`, `EaseInOutSine`, `EaseInQuad`, `EaseOutQuad`, `EaseInOutQuad`, `EaseInCubic`, `EaseOutCubic`, `EaseInOutCubic`, `EaseInQuart`, `EaseOutQuart`, `EaseInOutQuart`, `EaseInQuint`, `EaseOutQuint`, `EaseInOutQuint`

```cpp
#include "Core/Utility/Ease.h"

// 1. イージングカーブを通した位置補間 (EaseOutCubic: 急速に動き、最後滑らかに減速)
float easeT = EvaluateEase(EaseType::EaseOutCubic, progress); // progress は 0.0f 〜 1.0f
Vector3 currentPos = Lerp(startPos, endPos, easeT);

// 2. 0.0f〜1.0f に安全にクランプする線形補間
float currentAlpha = LerpClamped(0.0f, 1.0f, timer / duration);

// 3. クォータニオン回転の補間 (最短経路)
Quaternion currentRot = Lerp(startRot, targetRot, easeT);
```

---

## 8. 入力・音響・UIシステム (Input, Audio & UI Framework)

### 8.1 入力システム (Input Action System & InputManager)
- **ヘッダー**: `#include "Platform/Input/InputManager.h"`

現在の入力システムは、キーボードやゲームパッドのボタンを直接監視するだけでなく、物理入力と「論理アクション（例：Jump、Move）」を紐付けて管理する「アクションベース」入力システムをサポートしています。

#### 8.1.1 アクションベース入力システム (推奨)
ゲームの初期化処理（Sceneの `Initialize` など）で、`InputManager` に対してアクションと物理デバイス（キーやボタン）の対応付け（バインディング）を行います。

```cpp
InputManager* input = engine_->GetInputManager();

// "Jump" アクションに Spaceキー と ゲームパッドのAボタン を割り当て
input->BindAction("Jump", InputId::Keyboard_Space);
input->BindAction("Jump", InputId::GamePad_A);

// "MoveX" アクション（アナログ/1D軸入力）の割り当て
// スケール値（第3引数）を使って、物理入力を最終的なアクション値に変換します。
// 例: Dキー(1.0)は右方向(+1.0)に、Aキー(1.0)はマイナスを掛けて左方向(-1.0)にする
input->BindAction("MoveX", InputId::Keyboard_D, 1.0f);
input->BindAction("MoveX", InputId::Keyboard_A, -1.0f);
// パッドのスティックはそのままの値(-1.0～1.0)を使う、あるいは 0.5f 等を掛けて感度調整も可能
input->BindAction("MoveX", InputId::GamePad_LeftStickX, 1.0f);
```

毎フレームの更新処理（Sceneの `Update` など）では、バインドしたアクション名を指定して状態を取得します。

```cpp
InputManager* input = engine_->GetInputManager();

// デジタル入力（ボタンが押された瞬間）の判定
if (input->IsActionTriggered("Jump")) {
    // ジャンプ処理を実行
}

// アナログ入力（移動量など）の取得
float moveInputX = input->GetActionValue("MoveX").x;
// プレイヤーの移動処理へ
```

##### 【ベストプラクティス】JSON設定によるデータ駆動入力バインディング (`input_actions.json`)
C++コード内に手動で `BindAction` を並べるのではなく、設定ファイル `resources/config/input_actions.json` にキー・ボタン割り当てを一括定義し、起動時またはシーン初期化時に `LoadBindingsFromJson` でロードする運用が推奨されます。

```cpp
// 1. JSONファイルから全アクションバインディングを一括読み込み
engine_->GetInputManager()->LoadBindingsFromJson("resources/config/input_actions.json");

// 2. コード側はアクション名（SSOT）のみで透過的に入力判定
if (input->IsActionTriggered("Fire")) {
    // 攻撃処理
}
```
※これにより、キーコンフィグの変更やゲームパッド対応の追加をC++再コンパイルなしで即座に反映できます。

#### 8.1.2 直接キー・ボタン・マウス入力の取得
キーボード、マウス、ゲームパッド（XInput互換）の操作を直接取得することも可能です。
`BaseScene` を継承したクラスでは、直接 `PressedVK()` や `IsButtonPressed()` などのヘルパー関数を呼び出すのが最も簡単です。

```cpp
// 【キーボード (BaseScene内での使用例)】
// Spaceキーが「押された瞬間」か？ (VK: 仮想キーコード)
if (PressedVK(VK_SPACE)) { ... }
// Wキーが「押され続けている」か？ (DIK: DirectInput互換キーコード)
if (DownDIK(DIK_W)) { ... }

// 【ゲームパッド (BaseScene内での使用例)】
// Aボタンが押された瞬間か？
if (IsButtonPressed(XINPUT_GAMEPAD_A)) { ... }

// 左スティックの入力値 (-1.0f 〜 1.0f) ※入力値取得は InputManager 経由で行います
auto input = engine_->GetInputManager();
float lx = input->GetLeftStickX();
float ly = input->GetLeftStickY();

// 【マウス (InputManager経由)】
// 左クリックされたか？
if (input->IsMouseButtonPressed(Mouse::Button::Left)) { ... }
// マウスの現在座標 (スクリーン座標)
Vector2 mousePos = input->GetMousePosition();
```

---

### 8.2 音響システム (AudioManager / AudioPlayer)
- **AudioManager**: `#include "Audio/AudioManager.h"`
- **AudioPlayer**: `#include "Audio/AudioPlayer.h"`
- **AudioSourceComponent**: `#include "Framework/Component/Audio/AudioSourceComponent.h"`

本エンジンのAudioシステムは、コンポーネントからの利用とプログラムからの直接利用の2通りの方法をサポートしています。
これまでの `Bgm` や `Se` クラスは廃止され、統合された `AudioPlayer` クラスによって一元管理されます。リソースリークを防ぐ安全な設計（ComPtrやRAIIの活用）が内部で行われているため、プログラマは生成・破棄のタイミングを気にせず利用できます。

#### コンポーネントからの利用
GameObject に `AudioSourceComponent` をアタッチすることで、インスペクタ（エディタ）上からサウンドの設定が可能です。
- **Audio Type (0:BGM, 1:SE)**: `BGM` か `SE` かを選択します。BGMはデフォルトでループ再生されます。
- **Audio Path**: 再生するオーディオファイルのパス（例: `resources/audio/bgm/field.wav`）を指定します。
- **Volume**: 音量を 0.0 ～ 1.0 の間で調整します。
- **Loop**: ループ再生のON/OFFを任意に切り替えます。
- **Play On Awake**: シーン読み込み完了時に自動再生するかを指定します。

スクリプトから再生・停止を行う場合は、コンポーネントを取得して以下のように呼び出します。
```cpp
auto audioSource = GetComponent<AudioSourceComponent>();
if (audioSource) {
    audioSource->Play(); // 再生（インスペクタの設定が反映されます）
    // audioSource->Stop(); // 停止したい場合
}
```

#### プログラムからの直接利用 (AudioPlayer)
UIの操作音や、特定のコンポーネント（GameObject）に紐付かない効果音を再生する場合は、直接 `AudioPlayer` クラスのインスタンスを生成して使用するのが便利です。

```cpp
#include "Audio/AudioPlayer.h"
#include "Audio/AudioType.h"

// 1. エンジンから AudioManager を取得して、AudioPlayer を生成
auto audioManager = engine->GetAudioManager();
AudioPlayer clickSe(audioManager, AudioType::SE);

// 2. 音声ファイルのパスを指定して初期化
clickSe.Initialize("resources/audio/se/click.wav");

// 3. 再生（オプションで音量設定などが可能）
clickSe.SetVolume(0.8f);
clickSe.Play(); // 1回再生

// 再生中のサウンドを明示的に止めたい場合は Stop() を呼びます
// clickSe.Stop();
```

#### 【推奨ベストプラクティス】データ駆動サウンド基盤 (`sound_actions.json` & `SoundHelper`)
C++コード内に音声ファイルのパス（`"resources/audio/..."`）や音量数値を直接ハードコードすることは避け、JSON設定ファイル（Single Source of Truth）で一元管理する設計パターンが推奨されます。

- **設定ファイル**: `resources/config/sound_actions.json`
  - SE/BGMのファイルパス、ボリューム、同時再生制限（ポリフォニー）、ピッチランダマイズ幅などを一括定義。
- **利用例 (Application層)**:
  ```cpp
  #include "Audio/SoundHelper.h"
  #include "Audio/GameSoundAction.h"

  // 1. シーン初期化時に sound_actions.json をロード＆一括プリロード
  SoundHelper::Initialize(engine->GetAudioManager(), "resources/config/sound_actions.json");
  SoundHelper::PreloadAll();

  // 2. 列挙型（GameSE）を指定して型安全にワンショット再生（音量・ピッチ乗数の指定も可能）
  SoundHelper::Play(GameSE::PlayerBoost);
  SoundHelper::Play(GameSE::DebrisShatter);
  // 文字列キーによる再生（JSONイベント・スクリプト連携用）
  SoundHelper::PlayByName("MenuDecide");

  // 3. BGMの再生（第2引数で音量乗数 volumeMultiplier を指定可能）
  SoundHelper::PlayBGM(GameBGM::Stage1);
  // BGMの停止
  SoundHelper::StopBGM();
  ```
  ※これにより、プランナーやサウンドデザイナーがC++の再ビルドなしで音量バランスの調整や音源差し替えを即座に行えます。

---

### 8.3 UIフレームワーク (Canvas, UISelectionGroup, UINavigationController, UIAnimator, Slider)
- **UISelectionGroup**: `#include "Framework/UI/UISelectionGroup.h"`
- **UINavigationController**: `#include "Framework/UI/UINavigationController.h"`
- **UIAnimator**: `#include "Framework/UI/UIAnimator.h"`
- **SliderComponent**: `#include "Framework/Component/UI/SliderComponent.h"`

#### 8.3.1 UIメニュー構築の定石 (UISelectionGroup)
タイトル画面やポーズメニューなど、「上下で選んで決定する」UIを作るための便利なクラスです。手動でカーソル管理や長押し防止を書く必要がなくなります。

```cpp
#include "Framework/UI/UISelectionGroup.h"

// 1. メンバ変数に定義
UISelectionGroup uiGroup_;

// 2. 選択対象のスプライト等を登録（上から順にインデックス 0, 1, 2... となる）
uiGroup_.AddItem(btnStartSprite_.get());
uiGroup_.AddItem(btnOptionSprite_.get());
uiGroup_.AddItem(btnExitSprite_.get());

// オプション設定
uiGroup_.SetActiveBaseColor({1.0f, 1.0f, 1.0f, 1.0f}); // 選択中のカラー（パルスアニメーション適用）
uiGroup_.SetInactiveColor({0.4f, 0.4f, 0.4f, 0.8f});    // 非選択項目のカラー
// 横並び（左右キー）メニューにする場合: uiGroup_.SetHorizontalMode(true);

// 3. Update() で入力処理（方向キー操作、明滅演出、決定入力）
uiGroup_.Update(engine_->GetInputManager(), engine_->GetGameDeltaTime());

// 決定演出（点滅等）が完了して次シーンへ遷移すべきタイミングを判定
if (uiGroup_.ShouldTransition()) {
    int selected = uiGroup_.GetSelectedIndex();
    if (selected == 0) {
        // 「ゲーム開始」
    } else if (selected == 1) {
        // 「設定」
    }
}

// 4. 標準のカラー反映付き描画
uiGroup_.Draw();
```

#### 8.3.2 ハイブリッドUI調停・キーリピートコントローラー (`UINavigationController`)
ゲームパッド・キーボード・マウスが同時に接続された環境において、**「十字キーでボタンを移動した直後に、画面上に静止していたマウスカーソルのホバー判定によって元のボタンへ勝手に引き戻されてしまう事故」** を完全に防止するAAA品質のUIナビゲーションコントローラーです（`PauseScene` や `TitleMenuControllerComponent` 等で採用）。

- **ヘッダー**: `#include "Framework/UI/UINavigationController.h"`
- **主な機能**:
  - **ハイブリッド入力調停**: マウスが物理的に動いた時のみマウスモードに遷移し、パッド・キー入力時はマウスホバーのフォーカス奪取を自動ブロック。
  - **オートリピート**: 十字キーやスティックの長押し時に、初回ディレイ（0.35秒）を経て一定間隔（0.10秒）で滑らかに連続送り。
  - **選択変更イベント**: `SetOnSelectionChanged` でカーソル移動音の再生などを1行で紐付け可能。
  - **決定入力との連携**: 選択項目の遷移と調停に特化しているため、決定判定は `InputManager` の論理アクション（`UI_Submit`）やカーソルクリック判定と組み合わせて行います。

```cpp
#include "Framework/UI/UINavigationController.h"
#include "Platform/Input/InputManager.h"

// 1. 項目数 3、縦並びメニューとして初期化（カーソル音コールバックも設定可能）
UINavigationController navController_(3, false);
navController_.SetOnSelectionChanged([](int newIndex) {
    // カーソル移動音の再生など
});

// 2. 毎フレームのUpdate（キーボード/ゲームパッドの長押しリピート・即時移動）
bool isSelectionMoved = navController_.Update(input, deltaTime);

// 3. マウスホバーとの調停（静止マウスによる誤フォーカス奪取を完全ブロック）
if (mouseHoveredIndex >= 0) {
    if (navController_.TrySetHoveredIndex(mouseHoveredIndex, input)) {
        // マウスが実際に移動した場合のみフォーカスを更新
    }
}

// 4. 決定判定（論理アクションまたはカーソルクリック判定）
int currentIndex = navController_.GetSelectedIndex();
if (input->IsActionTriggered("UI_Submit") || input->IsCursorActionPressed()) {
    // 決定時の処理を実行
}
```

#### 8.3.3 UIのアニメーション演出 (`UIAnimator`)
UIの「明滅」や「浮遊」といった数学的なアニメーション（サイン波ベース）を簡単に計算してくれる便利なユーティリティクラスです。自分で時間を管理して計算式を書く必要がなくなります。

```cpp
#include "Framework/UI/UIAnimator.h"

// 1. 宣言
UIAnimator uiAnimator_;

// 2. 毎フレームの更新 (Update内)
uiAnimator_.Update(engine_->GetGameDeltaTime());

// 3. 値の取得と適用
// ダークソウル風のゆっくりとした明滅アルファ値 (基本値0.6, 振幅0.4, 速度3.0)
float alpha = uiAnimator_.GetPulseAlpha(0.6f, 0.4f, 3.0f);
sprite_->SetColor({1.0f, 1.0f, 1.0f, alpha});

// 浮遊する上下のオフセット値
float offsetY = uiAnimator_.GetFloatOffset(10.0f, 2.0f);
sprite_->SetPosition({ 640.0f, 360.0f + offsetY });

// 警告やダメージ時の高速点滅 (true/false)
if (uiAnimator_.GetFlashVisibility(40.0f)) {
    // 描画する
}
```

---

#### 8.3.4 UIシステム (Canvas & 2D描画)

ゲームのHUDやメニュー画面を構築するために、階層的なUIシステムが用意されています。

1. **CanvasComponent (UIルート)**: すべてのUI要素の親となるコンポーネントです。画面解像度の変更に伴う自動スケーリングや、アスペクト比の維持を担当します。UIを作成する際は、必ずルートの `GameObject` にこのコンポーネントをアタッチしてください。
2. **SpriteRendererComponent (2D画像描画)**: UIとして2Dテクスチャ（スプライト）を描画します。色や透明度、アンカーポイント（Pivot）の変更が可能です。
3. **TextRendererComponent (テキスト描画)**: TrueTypeフォント（`.ttf`）を用いて、画面上に文字列を描画します。サイズや色、配置揃え（左寄せ・中央揃えなど）を調整できます。
4. **ButtonComponent (インタラクション)**: ボタンとしてのクリック判定と、ホバー時・クリック時のコールバック処理を管理します。
```cpp
auto button = uiObject->AddComponent<ButtonComponent>();
button->SetOnClickCallback([]() {
    // ボタンがクリックされたときの処理
    Log::OutPutLog(std::cout, "Button Clicked!\n");
});
```
5. **SliderComponent (数値スライダーUI)**: BGM/SE音量調整やマウス感度設定など、マウスドラッグ操作による値変更を提供するスライダーUIです。
```cpp
#include "Framework/Component/UI/SliderComponent.h"

auto slider = sliderObj->AddComponent<SliderComponent>();
slider->SetValue(0.8f);
slider->SetOnValueChangedCallback([](float val) {
    // スライダー値変更時の処理 (0.0f 〜 1.0f)
});

// シーン側でのマウス操作判定の実装例（ハードコード矩形を撤廃したモダン設計）
auto* input = engine_->GetInputManager();
Irufemi::Vector2 mousePos = input->GetMousePosition();

// マウス左クリック・ドラッグ中にスライダー範囲内であれば値を自動更新
if (input->IsMouseButtonDown(Mouse::Button::Left) && slider->CheckBounds(mousePos)) {
    float newValue = slider->CalculateValueFromPosition(mousePos.x);
    slider->SetValue(newValue);
}
```
*(※詳細なAPI仕様は [5.7 UI系コンポーネント](#57-ui系コンポーネント-ui-system) を参照してください)*

---

#### 8.3.5 ScreenCaptureManager (スクリーンショット・メタデータ)

画面のキャプチャを安全に行うためのシステムです。UIを含めない純粋なシーンのみ (`ScreenCaptureType::SceneOnly`) や、UIを含めた最終画面 (`ScreenCaptureType::WithUI`) の出力、さらにはアルファチャンネルや深度バッファのみの出力に対応しています。
- **メタデータ連携**: キャプチャ時にエンジン内の状態をJSONメタデータとして同時に出力する機能（`RequestCaptureWithMetadata`）も備わっており、機械学習用データセットの作成などに応用可能です。
- **スレッドセーフ非同期保存**: `ThreadPool` を用いてバックグラウンドで画像エンコードとディスク書き出しを行うため、保存時のフレームドロップが発生しません。

```cpp
#include "Renderer/ScreenCaptureManager.h"

auto* captureManager = engine_->GetScreenCaptureManager();

// 1. 純粋な3Dシーン（UI抜き）のキャプチャ
captureManager->RequestCapture(L"screenshots/scene.png", ScreenCaptureType::SceneOnly, []() {
    Log::OutPutLog(std::cout, "シーンキャプチャ保存完了\n");
});

// 2. UIを含めた全画面＋メタデータJSON付き保存
captureManager->RequestCaptureWithMetadata(L"screenshots/gameplay.png", ScreenCaptureType::WithUI);

// 3. 深度マップ（Depth）の出力
captureManager->RequestCaptureDepth(L"screenshots/depth.png");
```

---

---

## 9. リソース管理・最適化・運用 (Resource, Profiling & Troubleshooting)

### 9.1 リソース管理システム (ResourceHandle & 各種Manager)

#### 9.1.1 TextureManager (テクスチャ管理)
- **ヘッダー**: `#include "Resource/Texture/TextureManager.h"`

`Sprite` などの描画に必要な画像をロードし、GPU用のハンドルを取得します。同じ画像を何度ロードしても、1度だけメモリに乗るようになっています。
C++のコードから直接テクスチャを管理する場合は、戻り値として `ResourceHandle` を受け取ります。

```cpp
// 1. 画像のロード (resources/ フォルダからの相対パス)
ResourceHandle handle = engine_->GetTextureManager()->LoadTexture("ui/title_logo.png");

// 2. フォルダ内の画像一括ロード (ローディング画面などで使用)
engine_->GetTextureManager()->LoadAllFromFolder("resources/ui");

// ※ Sprite等の Initialize に文字列でファイル名を渡せば、内部で自動的にロードされます。
```

#### 9.1.2 ModelManager (3Dモデル管理)
- **ヘッダー**: `#include "Resource/Model/ModelManager.h"`

##### ModelManager のメモリバジェット管理と自動キャッシュパージ
3Dモデルデータ（頂点バッファ、インデックスバッファ、マテリアル等）が肥大化しVRAM/RAMを圧迫することを防ぐため、`ModelManager` はLRU（Least Recently Used）に基づくメモリバジェット管理を備えています。
- 一定容量（バジェット）を超過した場合、参照カウントが0の未使用モデルを自動的に安全アンロードします。
- **キャッシュ逆引きマップの整合性確保**: パージ時にアセットパスと内部ハンドルの逆引きマップを適正に整合・クリーンアップするため、パージ後に同一モデルが再要求された場合でも、古いハンドルのダングリングポインタを発生させず安全に再ロード・キャッシュ構築が行われます。
`.obj` や `.gltf` 形式の3Dモデルを読み込み、最適化してキャッシュします。

```cpp
// モデルの事前ロード（インゲーム開始前などに呼ぶとカクつきを防げます）
// ※歴史的経緯によりAPI名は GetObjModelManager() ですが、実体はOBJ/GLTF等を一元管理する ModelManager* を返します。
ResourceHandle handle = engine_->GetObjModelManager()->LoadModel("enemy/enemy.obj");

// ※ コンポーネント等に文字列で "enemy/enemy.obj" を渡せば、内部で自動ロードされます。
```

#### 9.1.3 AudioManager (サウンド管理)
BGMやSEの再生・停止・音量調節を行います。
内部マップ（レジストリ）はミューテックスによりスレッドセーフに保護されており、`ThreadPool` を渡すことで全コアによる並列PCMデコードが可能です。

```cpp
// 1. サウンドの事前ロード
engine_->GetAudioManager()->GetOrLoadSoundByFile("resources/audio/bgm_title.wav");

// 2. フォルダごとの一括ロード (同期ロード)
engine_->GetAudioManager()->LoadSoundsFromFolder("resources/audio/se", "SE");

// 3. 【高速化】ThreadPool を渡してマルチスレッド並列ロード（起動時やロード画面推奨）
// ※ 全CPUコアのワーカースレッドへPCMデコードを分散し、ロード完了まで高速に並列待機します。
engine_->GetAudioManager()->LoadAllSoundsFromFolder("resources/audio", engine_->GetThreadPool());

// 4. 再生
auto soundData = engine_->GetAudioManager()->GetOrLoadSoundByFile("resources/audio/bgm_title.wav");
// (サウンドデータ, ループフラグ, 音量 0.0f~1.0f)
std::weak_ptr<VoiceInstance> voice = engine_->GetAudioManager()->Play(soundData, true, 0.8f);

// 5. 停止
engine_->GetAudioManager()->Stop(voice);

// 6. 【カテゴリ別音量制御・フェード】(設定画面・ポーズメニュー用)
// 全体マスター音量、BGM音量、SE音量を独立制御（0.0f〜1.0f）
engine_->GetAudioManager()->SetCategoryVolume(AudioCategory::BGM, 0.7f);
engine_->GetAudioManager()->SetCategoryVolume(AudioCategory::SE, 0.9f);

// 2秒かけてBGMを目標音量へ滑らかにフェード
engine_->GetAudioManager()->FadeCategoryVolume(AudioCategory::BGM, 0.0f, 2.0f);

// ポーズ時のカテゴリ別一時停止と復帰
engine_->GetAudioManager()->PauseCategory(AudioCategory::SE);
engine_->GetAudioManager()->ResumeCategory(AudioCategory::SE);
```

#### 9.1.4 FontManager (フォントとテキストの管理)
TTF等のフォントファイルを読み込み、テキスト描画用の高品質なMSDF画像を動的に生成・キャッシュします。

```cpp
// 1. フォルダ内の全フォント一括ロード
engine_->GetFontManager()->LoadAllFromFolder("resources/fonts");

// 2. 【超重要】スパイク（処理落ち）防止のための事前生成
// プレイスルーを滑らかにするため、シーン開始時のロード中などに「使う予定の全文字」を事前生成してください。
engine_->GetFontManager()->PrecacheText("my_font", L"このシーンで使う予定の会話テキストや、よく使う漢字一覧など...");
```

#### 9.1.5 キャッシュ管理 (generated/cache/)
ゲームを高速にロード・実行するために、各種リソースのパース結果や重いコンパイル結果は自動的にキャッシュ（バイナリ化）されます。
本エンジンでは、人間が直接編集・管理するリソースデータ（`.obj`, `.png`, `.hlsl` 等）と、システムが自動生成する中間バイナリを明確に分離するため、**キャッシュファイルはすべて `generated/cache/` 配下に集約して出力されます（`resources/` 内にキャッシュフォルダを作成してはいけません）。**

- **`generated/cache/model/`**: `ModelManager` や `AnimationManager` がロードした `.obj` や `.gltf` 等を解析し、次回以降のロードを爆速にするための独自バイナリ (`.ibin`) が保存されます。
- **`generated/cache/pso/`**: `PSOManager` が生成した DirectX12 のパイプラインステートオブジェクト（PSO）の塊 (`.pso`) が保存されます。これによりゲーム起動時やシーン遷移時のカクつき（スタッター）を防ぎます。さらに、エンジン起動時には全登録パイプラインを `ThreadPool` によるマルチスレッド並列ワーカージョブで一括ウォームアップ（PreWarm）するため、ゲームプレイ中の初回描画スタッターを完全にゼロに抑制します。
- **`generated/cache/shaders/`**: Releaseビルドなどで事前にコンパイルされたシェーダーバイナリ (`.cso`) が格納されます。

※ **【重要】** `generated/cache/` フォルダは `.gitignore` に登録されており、Git のバージョン管理から除外されています。また、キャッシュのバージョンが古かったり見つからない場合は、エンジンが**自動で元のソースファイルから再生成する（フォールバック機構）** ため、不具合が起きた際は `generated/cache/` フォルダごと手動で削除しても全く問題ありません。

---

#### 9.1.6 ResourceHandle の利用ルール

本エンジンでは、AAA規模の商用エンジン（メモリ予算の厳格な管理やLRUパージ機構）を見据え、テクスチャやモデルデータなどの巨大なリソースの管理を `std::shared_ptr` から独自の **`ResourceHandle` ベースのアーキテクチャ** へと完全移行しました。
これにより、不用意な `shared_ptr` の循環参照によるメモリリークや、解放タイミングの制御不能といった問題を解決し、高速かつ安全なリソース参照が可能になっています。

##### コンポーネント開発時におけるリソースの持ち方
自作のコンポーネント（RendererやEffectなど）でテクスチャやモデルを保持する場合、これまでのように `std::shared_ptr<ManagedModel>` をメンバ変数に持つことは禁止されています。代わりに `ResourceHandle` を保持してください。

```cpp
#include "Core/System/ResourceCachePool.h" // ResourceHandle用

class MyCustomRenderer : public Component {
private:
    ResourceHandle myModelHandle_;
    ResourceHandle myTextureHandle_;
};
```

##### リソースのロード（取得）と解放
リソースのロードは、各Manager（`TextureManager`, `ModelManager` など）の `Load***` メソッドを使用します。**取得したハンドルは、コンポーネント破棄時に必ず手動で `Release***` を呼んで解放（参照カウントを下げる）してください。**

```cpp
void MyCustomRenderer::Initialize() {
    // リソースをロードしてハンドルを保持
    myModelHandle_ = engine_->GetObjModelManager()->LoadModel("EnemyModel");
    myTextureHandle_ = engine_->GetTextureManager()->LoadTexture("EnemyTex");
}

void MyCustomRenderer::Finalize() {
    // 破棄時にハンドルの参照カウントを減らす
    if (myModelHandle_.IsValid()) {
        engine_->GetObjModelManager()->ReleaseModel(myModelHandle_);
    }
    if (myTextureHandle_.IsValid()) {
        engine_->GetTextureManager()->ReleaseTexture(myTextureHandle_);
    }
}
```

##### 毎フレームの描画（ハンドルの解決）
描画や更新（Update / Draw）のタイミングで初めて、ハンドルから実際の生ポインタ（実データ）を **解決 (Resolve)** します。
※解決した生ポインタはメンバ変数に保持せず、その関数（スコープ）内だけで使い捨ててください。万が一リソースが裏でパージ（破棄）されても、安全にフォールバック（ダミー白テクスチャなど）が返る仕組みになっています。

```cpp
void MyCustomRenderer::Draw() {
    // ハンドルから実体を解決（ポインタ取得）
    auto model = engine_->GetObjModelManager()->Resolve(myModelHandle_);
    
    // データがまだロードされていない、または無効な場合は処理をスキップ（安全装置）
    if (!model || !model->cpuModel) {
        return; 
    }
    
    // テクスチャのSRVハンドル（GPU用）を解決
    auto srvHandle = engine_->GetTextureManager()->Resolve(myTextureHandle_);
    
    // 実際の描画処理へ...
    model->Draw(srvHandle);
}
```

---

### 9.2 外部パフォーマンス監視 (TelemetryMonitor)
ゲームの描画ループに負荷をかけずに、パフォーマンスをリアルタイムで監視するC#製のWPFツールが用意されています。

**【起動手順】**
チーム開発で共有しやすいよう、このツールは独立した単一の実行ファイル（`.exe`）としてビルドされています。
1. 以下の場所にある `TelemetryMonitor.exe` を直接ダブルクリックして起動します。
   📂 `project/Binaries/TelemetryMonitor/TelemetryMonitor.exe`
   （※ルートの `Launch_Telemetry.bat` からもワンクリックで起動可能です）
2. ツールが立ち上がったら、続けてゲーム本体 (`IrufemiEngine`) を起動します。
3. UDP通信によって自動で接続され、FPSやCPU/GPUの処理時間がネオングラフで表示されます。

> [!TIP]
> **10,000オブジェクト負荷検証（ワンクリック連動起動）**:
> リポジトリルートの **`Run_Benchmark.bat`** を実行すると、未ビルドであれば `TelemetryMonitor.exe` を自動ビルドした上で、Release版のゲーム本体と TelemetryMonitor を自動で連動起動します。10,000個のオブジェクトが存在する高負荷環境下でのフレームペーシングやGPUカリングの挙動を手軽に検証できます。

**【表示される主な指標】**
- **System/FPS**: 指数移動平均 (EMA) で平滑化された滑らかなFPS値です。
- **System/FrameTime_ms**: 1フレームにかかったトータル時間（ハイブリッド・スリープによる待機時間を含む）です。60FPS時は約16.67msになります。
- **System/CPU_Time_ms**: エンジン側のロジック計算と描画コマンド生成にかかった「純粋なCPU稼働時間」です（GPUのフェンス同期待ち時間を除外して正確に計測されます）。
- **System/GPU_Time_ms**: 前フレームで実際にGPUが描画処理に費やした時間です。

*(※プログラマー向け)*
もしC#側のツールのソースコード（`Tools/TelemetryMonitor/`）を拡張・改修した場合は、対象ディレクトリで以下のコマンドを実行することで、チーム配布用の新しい単一exeが `Binaries` フォルダに上書き生成されます。
```powershell
dotnet publish TelemetryMonitor.csproj -c Release -r win-x64 --self-contained false -p:PublishSingleFile=true -o ../../Binaries/TelemetryMonitor
```

**【カスタムデータの送り方】**
ゲーム固有の変数（例：プレイヤーのHPやボスのフェーズ）を監視したい場合は、エンジン内の任意の場所から以下の1行を呼ぶだけで、ツール側に新しい折れ線グラフが追加されます。
```cpp
// 毎フレームのUpdate内などで呼ぶ (engineポインタ経由)
if (auto* sender = engine->GetTelemetrySender()) {
    sender->SetMetric("Game/PlayerHP", player->GetHP());
}
```

---

### 9.3 フレームレート制御・タイマー精度 (AAA Frame Pacing)
本エンジンは、Windows環境下における最高精度のフレームペーシングを実現するため、「ハイブリッド・スリープ」を採用しています。

#### 9.3.1 フレームレート制御とタイマー精度
- `IrufemiEngine::Initialize` 時に `timeBeginPeriod(1)` が呼ばれ、OS全体のタイマー解像度が 1ms に引き上げられます。
- `FrameRateController::Update` において、次のフレームまでの待機時間が 2ms を切るまでは `std::this_thread::sleep_for(1ms)` でOSに処理を譲り（省電力化）、残り時間が 2ms 未満になった瞬間に `YieldProcessor()` を用いた超高精度のスピンロック（ビジーウェイト）へ移行します。
- これにより、「OSの寝過ごし」による意図しないFPS低下（60FPS目標なのに57FPSになってしまう問題）を完全に防ぎ、ミリ秒未満の正確なペーシングを実現しています。

#### 9.3.2 ディスプレイモードと VSync (Tearing) 制御
エンジンは最新のAAAゲーム水準のディスプレイ制御をサポートしており、コードおよびエディタのUIから動的に変更可能です。

- `DisplayMode::Windowed` : 通常のウィンドウモード（タイトルバーあり）。
- `DisplayMode::Borderless` : ボーダーレスウィンドウモード。画面全体を覆い、タイトルバーによる操作フリーズを防ぎます。
- **VSync と Tearing 対応 (低遅延描画)**: `DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING` に対応しており、ボーダーレスモードでもVSyncをオフにして入力遅延を極限まで削減可能です。VSyncオフ時でも `FrameRateController` が正確に目標FPSを維持します。

#### 9.3.3 展示会・審査向けキオスクモードと緊急脱出 (Kiosk Mode & Fail-Safe)
展示会や試遊環境において、Windowsキー誤押下によるOSスタートメニュー表示やフォーカスアウトを防止する低レベルキーボードフック（`WH_KEYBOARD_LL`）を標準搭載しています。自ウィンドウがアクティブな場合のみWindowsキーを無効化し、`Alt + F4` は常に安全に通過させます。

**多重の安全脱出導線 (Fail-Safe Exit)**:
1. **ポーズ画面 [QUIT] メニュー**: `ESC` キーを押してポーズ画面を開き、[QUIT] を選択。
2. **ESCキー 2秒長押し (ハードコード・フェイルセーフ)**: シーンやUIのスタック時でも、`ESC` キーを2秒間長押しすることでエンジンメインループから強制安全シャットダウン (`PostQuitMessage(0)`)。
3. **`Alt + F4`**: OS標準の即時クローズ。

#### 9.3.4 包括的入力検知とゲームパッド仮想カーソル制御
- **包括的活動検知 (`InputManager::HasAnyInput()`)**: キーボード、マウス、ゲームパッド（全ボタン・スティック不感帯0.2f考慮）のいずれかが操作されたかを1行で判定し、無操作放置（Idle）タイマーの計測に利用できます。
- **ゲームパッド仮想カーソル制御 (`SetIsUsingGamepadCursor`)**: 十字キーフォーカスと仮想カーソルの同期をサポート。

---

### 9.4 エンジン拡張プラグイン機構 (IEngineExtension) とアトラクトデモ

IrufemiEngine は、エンジンのコアループ（`Update`, `DrawUI`, `Draw` 等）を改変することなく、独立したシステムを追加するための拡張プラグインインターフェース **`IEngineExtension`**（`#include "Core/System/IEngineExtension.h"`）を提供しています。

#### 1. IEngineExtension のインターフェース仕様
エンジン初期化時や各フレームの描画パイプラインから安全にフックされます。
```cpp
#pragma once
#include "Core/System/IEngineExtension.h"

class CustomExtension : public IEngineExtension {
public:
    void OnInitialize(IrufemiEngine* engine) override {
        // エンジン初期化時に呼び出されます
    }
    void OnUpdate(float deltaTime) override {
        // 毎フレームのゲームロジック更新後に呼び出されます
    }
    void OnDrawUI() override {
        // ImGui やデバッグUI描画フェーズで呼び出されます
    }
    void OnDraw() override {
        // シーン描画後・ポストプロセス前の描画フェーズで呼び出されます
    }
    void OnFinalize() override {
        // エンジン終了時に安全にクリーンアップされます
    }
};
```

登録はゲーム初期化時に `engine->AddExtension(std::make_shared<CustomExtension>());` を呼び出すだけで完了します。

#### 2. 実装事例: 展示用キオスクモード＆アトラクトデモ
この拡張機構の実装例として、作品展示やコンテスト審査における無人運用・自動ループ機能を提供する **`AttractDemoManager`**（`project/Application_solo/Core/AttractDemoManager.h`）が実装されています。

- **プラグイン登録と設定構造体 (`AttractDemoConfig`)**:
  ```cpp
  #include "Core/System/IrufemiEngine.h"
  #include "Core/AttractDemoManager.h" // Application層の拡張クラス

  AttractDemoConfig config{};
  config.enabledOnStart = false;     // 通常プレイ優先（コマンドライン -demo 指定時のみ true）
  config.enableDevHotkeys = true;   // F7/F8 ホットキー有効化
  auto attractDemo = std::make_shared<AttractDemoManager>(config);

  // エンジンへプラグインとして安全に登録
  engine->AddExtension(attractDemo);
  ```
- **コマンドライン自動判定**:
  `-demo` や `-kiosk` が渡された場合は自動的に展示ループを起動し、無操作8秒でアトラクトデモを開始、ボタン操作検知で即座にプレイヤー入力へシームレス復帰する運用が実現されています。

---

### 9.5 ビルド・提出パッケージングパイプライン

#### 9.5.1 ゲームの即時ビルド＆実行 (`BuildAndRun.bat`)
リポジトリのルート直下にある `BuildAndRun.bat` をダブルクリックするだけで、Visual Studio を開かずに最新コードを Release ビルドしてゲームが起動します。

- **自動化内容**:
  1. `vswhere.exe` による Visual Studio 2026 (MSBuild) の自動検出。
  2. `-restore` による NuGet パッケージ（`freetype2`）の自動ダウンロード・復元（新規クローン環境でも即実行可能）。
  3. `Irufemi.slnx` から `Application_solo` と依存ライブラリ（`IrufemiEngine.lib` 等）の先行自動解決ビルド。
  4. HLSLシェーダーの事前オフラインコンパイル（`CompileShaders.bat`）。
  5. 作業ディレクトリを自動解決して `Application_solo.exe` を直接起動。

#### 9.5.2 就活・技術審査用パッケージの一発生成 (`PackageForSubmission.bat`)
リポジトリのルートにある `PackageForSubmission.bat` をダブルクリックすると対話メニューが立ち上がり、用途に応じたパッケージが `_Submission/` フォルダに出力されます。

- **`[1] プレイ用パッケージ (GravityShooter_PlayableBuild)`**
  - **スマートアセットクッカー（独自設計）** により、Releaseバイナリと C++コード・シーンJSON・マテリアル（`.mtl`）の依存関係のみを逆引き走査。不要なテスト用アセット（`bunny.obj` 等）や未参照テクスチャ、Blender制作原本を完全除外（**約 228MB ➔ 約 62MB に約73%軽量化**）。
  - ワンクリックで解凍・即座にプレイ可能な完成品を出力。
- **`[2] プレイ用パッケージ (既存 Exe 使用 + スマートクック)`**
  - 直近でビルド済みの実行ファイルを用いて高速にパッケージング。
- **`[3] ソースコード提出用パッケージ (GravityShooter_SourceCode)`**
  - Visual Studio 2026 / 2022 で開いて 100% ビルド・デバッグ可能な完全ソースアーカイブ。`.vs` や派生キャッシュなどの一時ファイルを除外。
- **`[4] 一括生成`**
  - `[1]` と `[3]` を同時にワンステップでビルド・生成。
- **`[5] SEGAチャレンジ提出用 一式パッケージ (Playable + Source + 概要書ZIP)`**
  - SEGAコンテスト等の応募規定に完全準拠し、プレイ可能ビルド、ソースコード一式、および提出ドキュメントを規定フォルダ構成で一括生成・ZIP化。

#### 9.5.3 アセット監査 ＆ 健全化クリーンアップ (`CleanAssets.bat`)
大手ゲームスタジオ準拠の「3層アセット分離アーキテクチャ（DCC層 / ランタイム層 / 製品層）」に基づき、開発環境内のアセット健全性を自動監査・クリーンアップするツールです。

- **`[1] Audit Mode`**: C++・JSON・マテリアルの逆引き走査を行い、未参照ファイルやDCC生データを一覧レポート。
- **`[2] Dry-Run Mode`**: ファイルを操作せず、移動・削除のシミュレーション結果をプレビュー。
- **`[3] Sanitize Mode`**: 実行直前に `backup/` へ完全自動バックアップを作成した上で、安全にクリーンアップを実行。
  - **DCC元データの分離**: Blender元データ（`無題.blend`）や未接続テクスチャ、原本フォントを新設の `asset_src/` へ自動退避。
  - **ゼロ容認（Zero Tolerance）**: 開発中スクリーンショットや過去講義課題などのゴミファイルをランタイム（`resources/`）から完全排除。
- **`[4] Restore Backup`**: 直前のバックアップから1秒で元の状態に完全ロールバック。

> [!NOTE]
> アセットクッカーおよびアセットサニタイザーの内部仕様、新しいモデル・音源を追加した際の命名・配置ルールについては、[scripts/README.md](file:///f:/school/3_5/WP1/WP1/scripts/README.md) を参照してください。

---

### 9.6 統合テスト環境 (DebugScene) の利用ルール

本エンジンには、エンジンコアの機能テストや描画テストを行うための独立したサンドボックス環境として **`DebugScene`** が `IrufemiEngine/Framework/` 配下に統合されています。（以前は各ゲームアプリケーション側に重複して存在していましたが、リファクタリングによりエンジン側に一元化されました。）

#### DebugScene の目的と立ち位置
エンジンの機能追加（新しいコンポーネント、シェーダー、アニメーション機構など）を行う際は、**いきなりゲーム本編のシーン（`InGameScene` など）に組み込むのではなく、まずはこの `DebugScene` にテスト用のオブジェクトを配置して単体テスト・動作確認を行う** ことが強く推奨されます。

これにより、ゲーム特有の複雑なロジック（ステートマシンやカメラ制御など）の干渉を受けずに純粋なエンジンのバグ切り分けが可能になります。

#### テスト用オブジェクトの追加と管理 (Activationパネル)
`DebugScene` に新しいテスト要素を追加する際は、常に描画し続けるのではなく、ImGuiのトグルスイッチを使って「必要なときだけ表示・更新」できるように実装してください。

1. **フラグとオブジェクト変数の定義 (`DebugScene.h`)**:
   ```cpp
   std::unique_ptr<GameObject> myTestObj_ = nullptr;
   bool isActiveMyTest_ = false;
   ```

2. **Activation ウィンドウへの登録 (`DebugScene.cpp` の Update 内)**:
   ```cpp
   if (ImGui::Begin("Activation")) {
       ImGui::Checkbox("My Test Feature", &isActiveMyTest_);
   }
   ImGui::End();
   ```

3. **遅延生成と更新・描画ロジック**:
   チェックボックスがONにされた瞬間（または初回フレーム）に初めてオブジェクトを生成（Instantiate/make_unique）し、チェックが入っている間だけ Update / Draw を呼ぶようにします。
   ```cpp
   if (isActiveMyTest_) {
       if (!myTestObj_) {
           myTestObj_ = std::make_unique<GameObject>("TestObj");
           myTestObj_->AddComponent<MyNewComponent>();
       }
       myTestObj_->Update();
   }
   ```

※なお、`DebugScene` はあくまでエンジンのテスト機能であるため、特定のゲーム（シューティングやアクション等）のプレイヤーキャラや敵キャラの仕様をそのまま持ち込むことは禁止されています。（チーム開発ルール 1. アーキテクチャと関心の分離 に従うこと）

---

### 9.7 トラブルシューティングと既知の不具合対応履歴

#### 9.7.1 アプリケーション終了時に `LIVE_DEVICE` エラーでクラッシュする
**現象**: Visual Studio の出力ウィンドウに `D3D12 WARNING: Live ID3D12Device` と表示され、`D3DResourceLeakChecker` でブレークポイントが止まる。

**原因**: 
- `PrimitiveManager` などのシングルトンクラスが GPU リソース（頂点バッファ等）を保持したまま、DirectX のデバイスが破棄されようとした際に発生します。
- または、`RenderTexture` や `DescriptorPool` から確保したリソースが解放されていない場合や、`TransientResourceManager` 等でフレーム終了時の遅延破棄(`ReleaseAfterFence`) キューに積まれたリソースがアプリケーション終了時にクリアされていない場合に発生します。
- 注意点として、エラー追跡用に追加した `ID3D12InfoQueue` などの COMポインタ自体のスコープが長すぎて、`ReportLiveObjects` の時点でデバイスの参照を保持してしまう（見かけ上のリーク）こともあります。

**解決策**:
- `IrufemiEngine::Finalize()` 内で、`dxCommon_` が破棄される前に、すべてのマネージャーの `Finalize()` または `reset()` を呼ぶようにしてください。シングルトンの場合は `PrimitiveManager::Finalize()` のように明示的に呼び出します。
- **フレーム遅延破棄の注意**: `dxCommon_->ReleaseAfterFence(resource)` で破棄を予約したリソースは、`DirectXCommon::pendingResources_` に保持されます。エンジン終了時には、必ずGPU同期待ち（`WaitForGPU()`）の直後に `pendingResources_.clear()` を呼び出して完全に破棄してください。
- COMポインタ（`ComPtr`）を使用する場合は、不要になったら `Reset()` を呼ぶか、寿命を強制的に限定するためローカルスコープ `{}` 内で宣言するようにしてください。

#### 9.7.2 独自キャッシュからの復元（SSOバッファ破壊）と非同期ロードのすり抜けによるアクセス違反

- **現象**: GLTF などのアニメーションモデルを非同期ロードする際、`std::_Tree::empty()` 等の STL 内部（`m->cpuModel->skinClusterData.empty()` など）で `0xC0000005` (アクセス違反) や `0xB8` 等の不正なポインタ参照が発生し、高い頻度でクラッシュする。
- **原因**: 以下の2つの致命的なバグが連鎖して起きていた。
  1. **SSO（Small String Optimization）バッファの破壊**: `ModelSerializer` でバイナリキャッシュ (`.ibin`) から `std::string` を復元する際、`ifs.read(str.data(), size)` で直接書き込んでいた。古いキャッシュや中途半端なデータが読み込まれた際にSSOの管理領域やヒープが破壊され、直後の `skinClusterData`（std::map）のメモリ構造が壊れていた。
  2. **非同期ロードのすり抜け**: 非同期でモデルをロードしている最中に、`AnimatedMeshObject::Update` や `Draw` 側で `m->cpuModel` が完全に構築される前（`nullptr` の状態）にアクセスする防御抜けが存在した。
- **当初の誤認**: エラーが `nlohmann::json` のパース処理周辺で発生しているように見えたため、Assimp 内部の `nlohmann::json` とエンジン側の `nlohmann::json` の ODR（One Definition Rule）違反が疑われ、ABIタグを変更する対策が取られたが、検証の結果これは濡れ衣（無関係）であることが判明した。
- **解決策**:
  - `ModelSerializer` における文字列の復元を `std::vector<char>` 経由で行うように修正し、すべての読み込み関数に `ifs.fail()` の厳格なエラーチェックを追加した。
  - キャッシュフォーマットのバージョン (`kVersion`) をインクリメントし、古いキャッシュを無効化した。
  - `AnimatedMeshObject` の `Update` および `Draw` メソッドの冒頭に、`m->status.load() != Loaded` および `!m->cpuModel` の場合は即座にリターンする「強力なフェイルセーフ（早期リターン）」を追加した。さらにモデル切り替え時に `meshResources_.clear()` を行うようにした。
- **教訓**: バイナリシリアライゼーションを行う際は、STL コンテナの内部実装（SSOなど）に依存する直接的なメモリ書き込みを避け、安全なバッファを経由すること。また、非同期ロード時のステート管理（排他制御や状態チェック）は、描画ループ側でも徹底すること。

---

> **ドキュメント更新履歴**
> - 2026/05: RenderGraph / パケット分離対応を反映、Phase 1~3 および 便利機能（カメラ、ディレクトリ構造、UISelectionGroup）を追記
> - 2026/05: エディタ機能（Project Browserの2ペイン化、FontAwesome対応、ドラッグ＆ドロップ機能）の解説を追記
> - 2026/05: コンポーネントシステム（GameObject/Component）の導入と、UIロジックの分離（Registry方式）を追記
> - 2026/05: 高品質テキスト描画システム (`TextRendererComponent` / `FontManager`) と、Play時の自動保存機能を追記
> - 2026/05: 終了時のリソースリーク（LIVE_DEVICE）対策とトラブルシューティングを追記
> - 2026/05: 3D爆発エフェクト (Effect::kExplosion) 仕様とスニペットの追加
> - 2026/06: カスタムエディタUI構築時の Undo/Redo (Ctrl+Z) 対応用ヘルパー関数の解説を追記
> - 2026/06: プリミティブ描画の `Primitive3DObject` への統合、および `MeshDesc` / `MaterialDesc` へのデータ構造リファクタリングを反映
> - 2026/06: 描画リソースのマルチバッファ同期処理を `MultiBufferSyncState` 基底クラスへ集約し、`Manual.md` に使い方を追加
> - 2026/06: シェーダーのハイブリッドコンパイル（Releaseの完全CSO化）およびEditor/Debug向けのホットリロード機能の解説を追加
> - 2026/06: マルチスレッド化による GameObject::Update の並列処理と、ダングリングポインタ対策としての `std::weak_ptr` の利用ルールを追記
> - 2026/06: エディタ操作性向上（インスペクターのRemove/Reset機能、D&D割り当て）の解説を追記
> - 2026/06: Hierarchyでのオブジェクト管理機能（フォルダ化・ロック・保護機能）の解説を追加
> - 2026/06: メニューバーの刷新（モダンダークテーマ化）、Play/Pause機能による時間管理（DeltaTime/RealDeltaTime）、およびレイアウト初期化（Reset Layout）の解説を追加
> - 2026/07: エディタ機能にスクリーンショット（Screen Capture）機能を追加。マルチスレッドでの非同期保存および各描画パスごとの保存機能の解説を追記
> - 2026/11: ポストプロセス記述をSSOT（`PostProcessDefinitions.h`）誘導および新規追加ワークフローへ刷新。音響システム（`SoundHelper` / `GameSE` / `GameBGM`）のAPIスニペット整合、`Run_Benchmark.bat` 負荷検証手順、`GameArchitecture.md` への相互参照誘導を追記
> - 2026/11: 総合精査を実施。VS2026ソリューション構成（Irufemi.slnx）一本化、面光源（`AreaLight`）および `DrawManager::SetFrameData` 最新シグネチャ追記、`PostProcessManager` / `EffectMaskComponent` / `AttractDemoManager` のAPIスニペット修正、`UINavigationController` / `input_actions.json` / `CollisionLayers.json` / `ConsoleCommandSystem` の仕様を追記
> - 2026/11: Manual.mdの総合精査および実コード乖離修正。解像度正規アクセサ（`GetClientWidth()` 等）への整合、`ScopedEffectHandle`（RAII寿命管理）および `GetEffect<T>()` 追記、CodeGen（`GeneratePostProcessHlsl.py`）および `PropertyVisitor` によるDebugUI自動展開ワークフローの明文化、6.5節の重複記述（マジックナンバー残存部）の整理と更新履歴配置の適正化
> - 2026/11: Manual.mdの精密ファクトチェックおよび不足基盤システムの完全拡充。`UINavigationController` の決定入力（存在しない `IsSubmitted()` 撤廃）および `PostProcessManager`（存在しない `SetModeEnabled` 撤廃・`ActivateScoped` 推奨化）の実コード整合、`CameraComponent` の正規アクセサ整合、補間・イージング基盤（`Ease.h` / `Lerp` / `Slerp` / `EvaluateEase`）、GPUインスタンシングデバッグ描画（`DebugPrimitiveRenderer`）、マルチカメラ管理（`CameraManager`）、文字列変換（`StringUtility.h` / `ConvertString`）の仕様・スニペットを追記
> - 2026/11: Manual.mdの精密再精査と全コードベース整合。`EaseType::EaseOutQuad` / `ToRadian(90.0f)` / `MeshRendererComponent` の JSON キー名（`modelName`）の誤記修正、`Collision.h` インクルード漏れおよび `DebugPrimitiveRenderer` 取得元（`engine_->GetDebugPrimitiveRenderer()`）のコンパイルエラー解消、`Dot` / `Cross` のグローバルスコープ整合、`TargetFollowComponent`（`SnapToTarget()`）および `LifetimeComponent` / `TextureManager` / `ModelManager` のヘッダーパス・API仕様を完全拡充
> - 2026/11: Manual.mdの精密ファクトチェック修正および未記載基盤の完全網羅。`Irufemi::Ray`（`origin`）および `Log`（`OutPutLog`）のコンパイルエラー解消、`GPUParticleManager`（`GetGPUParticleManager()` / `enableLighting` 引数 / `UnregisterEmitter`）と `SoundHelper`（`PlayByName` / 音量乗数 / 引数なし `StopBGM`）の実コード整合、`Effect` クラスの初期化注記、シーン遷移トランジション全6種（`SceneTransition::Type`）とカスタムローディング画面（`ILoadingScreen`）、`AudioManager` のカテゴリ音量・フェード制御（`SetCategoryVolume` / `FadeCategoryVolume`）、および `ScreenCaptureManager` のC++リクエストコードを完全拡充

