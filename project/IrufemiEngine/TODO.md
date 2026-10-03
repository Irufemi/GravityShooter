# 🎮 IrufemiEngine コア開発＆拡張計画

以下は、`IrufemiEngine`（エンジン側）の次期アップデート計画や、将来的なエディタ拡張機能などのTODOリストです。
※ゲーム固有のロジックやタスクは `Application/TODO.md` で管理します。

## 🚀 次期アップデート計画 (Ongoing & Future Tasks)
より「ツール」として使いやすく、商用水準のパフォーマンスを発揮するための拡張機能群です。

### 🏗️ GameObject＆プレハブ・ライフサイクル抜本的改革 (Lifecycle & Prefab Architecture Revamp)
- [x] **Phase 1: JsonUtility への安全なファイルI/O集約**
    - [x] `JsonUtility::LoadFromFile` および `SaveToFile`（自動ディレクトリ作成、例外保護、パースエラーログ完備）の実装。
    - [x] `SceneSerializer` の `Save`, `Load`, `SavePrefab`, `GetPrefabJson` を `JsonUtility` 呼び出しに置き換え、ボイラープレートを排除。
- [x] **Phase 2: PrefabManager の新設とアセット保持責務の完全分離**
    - [x] `Framework/Prefab/PrefabManager` を新設し、JSON/テンプレートキャッシュおよびディープコピー生成を一元管理。
    - [x] `SceneSerializer` から静的キャッシュを撤廃し、純粋なシリアライザへと特化。既存の `LoadPrefab` 呼び出しは `PrefabManager` へ委譲し100%後方互換を維持。
    - [x] `SceneManager` のシーン遷移（同期・非同期）と連動したプレハブキャッシュの自動解放。
- [x] **Phase 3: GameObjectLifeState の導入と初期化・破棄の多重実行防止**
    - [x] `GameObjectLifeState` (`Constructed`, `Awake`, `Spawned`, `Started`, `Destroyed`) の確立。
    - [x] `Component` 基底クラスに `OnAwake()`, `OnSpawned()`, `OnDestroy()` 仮想メソッドと `isInitialized_` フラグを追加。
    - [x] `GameObject::Initialize()`, `Start()` に二重実行防止ガードを実装。
    - [x] `GameObject::Deserialize()` での初期化フェーズ整流化により、シーンロード時のコンポーネント二重初期化バグを根絶。
    - [x] デストラクタおよび `Destroy()` での `OnDestroy()` 確実呼び出しによるメモリ/マネージャー解放リーク防止。
- [x] **Phase 4: 遅延スポーンキュー (Deferred Spawn) での OnSpawned() 自動通知**
    - [x] `BaseScene::Update` 冒頭での `pendingAdds_` フラッシュ時に、シーン・座標確定通知 `NotifySpawned()` を自動発行。
- [ ] **Phase 5: 各コンポーネントの OnAwake / Start 分離移行**
    - [x] **コライダー系コンポーネント (`ColliderComponent`, `Sphere`, `AABB`, `OBB`)**:
        - 派生クラスの重複 `Initialize()` を全廃し、基底 `ColliderComponent` の `Start()` / `OnDestroy()` / `OnEnable()` / `OnDisable()` に登録・解除を一元化。
        - `CollisionManager::FlushPendingCommands` での追加・削除相殺バグを修正し、オブジェクトプール等での多重有効化/無効化時のコライダー消失を根絶。
        - `CheckAllCollisions` におけるTriggerコライダーの片方向レイヤーマスク検知のサポート。
        - `DebrisManagerComponent` でのワールド座標設定の整合性修正。
    - [x] **動的スポーン・親子階層および他コンポーネントのライフサイクル整流化**:
        - `GameObject::AddChild` / `InsertChild`: 親オブジェクトがすでに `Spawned` / `Started` の場合、後から動的に追加された子階層へ `NotifySpawned()` / `Start()` を自動伝播。
        - `BaseScene`: `AddGameObject` / `RemoveGameObject` において、`pendingAdds_` や `gameObjects_` への二重登録ガードを実装。
        - `AudioSourceComponent`: 遅延初期化 (`InitializeAudio`) と `Start()` / `OnDestroy()` を実装し、シーンバインド前に動的生成された場合の不鳴バグと解放漏れを解消。
        - `CameraComponent`: `OnDestroy()` による `CameraManager` からの登録解除を実装し、動的破棄時のダングリング参照を防止。
        - `EffectManagerComponent` / `DestructibleEnvironmentComponent`: エフェクト発生・瓦礫スポーン時の座標設定を `SetWorldPosition` に統一し、階層構造下での位置ズレを防止。
    - [x] **主要コンポーネントの順次ライフサイクル移行 (完了)**:
        - [x] `MeshRendererComponent` / `SkinnedMeshRendererComponent`:
            - `OnAwake()`: `StaticModelObject` / `AnimatedMeshObject` の内部生成とモデルロード。
            - `OnSpawned()`: `SyncRenderState()` を呼び出し、スポーン直後の原点チラつきバグを根本防止。
            - `Initialize()`: 後方互換レイヤーとして `OnAwake()` / `OnSpawned()` を呼ぶよう整流化。
        - [x] 2D・テキスト・パーティクル系コンポーネント (`SpriteRendererComponent`, `TextRendererComponent`, `ParticleEmitterComponent`):
            - `OnAwake()`: `Sprite` / `Text` / `ParticleObject` の内部生成および初期プロパティ適用。
            - `OnSpawned()`: 初期Transformおよび描画・放出ステートの即時同期（`SyncRenderState()`）。
            - `Initialize()`: 後方互換レイヤーとして `OnAwake()` / `OnSpawned()` を呼ぶよう整流化。
        - [x] ロジック・エフェクト・UI系コンポーネント (`VoxelParticleComponent`, `AnimatorComponent`, `LifetimeComponent`, `CameraShakeComponent`, `ButtonComponent`, `SliderComponent`):
            - `VoxelParticleComponent`: `OnAwake()` でモデル名キャッシュ、`Start()` で `ReservePool` を実行し、動的生成時のメモリ予約漏れを防止。
            - `AnimatorComponent`: `OnSpawned()` でエンジン取得・初期化、`Start()` でデフォルトアニメーション再生開始に整流化。
            - `LifetimeComponent`: `OnSpawned()` および `OnEnable()` で寿命タイマーを自動リセットし、オブジェクトプール再利用時の手動初期化を不要化。
            - `CameraShakeComponent` / `ButtonComponent` / `SliderComponent`: 同一オブジェクト内のコンポーネント参照解決を `OnAwake()` に移譲。
        - [x] 残存エンジンコンポーネントの完全移行 (エンジンコア内コンポーネント100%達成):
            - `ParticleFieldComponent`: `Start()` で `GPUParticleManager::RegisterField`、`OnDestroy()` で登録解除を行い、動的スポーン時の登録失敗と解放漏れを根絶。
            - `PrimitiveRendererComponent` / `Primitive2DRendererComponent`: `OnAwake()` でプリミティブオブジェクト生成、`OnSpawned()` で初期Transform同期（チラつき防止）。
            - `ModelBatchRendererComponent`: `OnAwake()` で `ModelBatch` 生成とGPUカリング設定。
            - `SkeletonDebugRendererComponent`: `OnAwake()` でボーン八面体メッシュ・軸描画バッチを生成・初期化。
            - `SplineComponent`: `OnAwake()` でデバッグ描画バッチ初期化と距離テーブル構築。
            - `VirtualEntityManagerComponent`: `OnAwake()` で同一オブジェクトの `ModelBatchRendererComponent` を取得・設定。
            - `EffectMaskComponent`: `OnAwake()` で自メッシュレンダラー取得、`OnSpawned()` で `PostProcessManager` キャッシュに責務分離。
            - `TargetFollowComponent`: `Start()` でターゲットTransformを事前解決し、動的検索コストを削減。
            - `ScreenEffectComponent`: `OnDestroy()` を実装し、オブジェクト破棄時のアクティブポストプロセス解除を保証。
            - `AudioSourceComponent`: `OnSpawned()` での初期化保証と、`OnDisable()` / `OnDestroy()` での停止（`Stop()`）処理を統合。
            - `GlobalPostProcessComponent`: `OnDisable()` / `OnDestroy()` でマネージャー側のエフェクトスタックを確実にリセット（`Reset()`）。
            - `GameObject`:
                - `SetIsActive()`: 子オブジェクト（`children_`）へアクティブ状態を再帰伝播し、パーツや付属コライダーのゴースト化を防止。
                - `Destroy()`: アクティブ状態のまま破棄される場合の「`OnDisable()` ➔ `OnDestroy()`」順序を厳格化。
                - `RemoveComponent()`: 削除前に `OnDisable()` および `OnDestroy()` を明示的に発行し、マネージャーからの登録解除漏れ（ダングリング参照・当たり判定幽霊化）を根絶。
                - `Awake()`: 各コンポーネントの `OnAwake()` 直後に未実行の `Initialize()` を呼び出し、一括Awake時の後方互換を完全保証。
                - `AddChild()` / `InsertChild()`: 親の `isActive_` が false の場合に子を非アクティブ化し、親が Awake 状態の場合に子へ `Awake()` を確実に伝播。
            - `BaseScene`:
                - `ClearGameObjects()`: シーン破棄・クリア時に全オブジェクトの `Destroy()` を明示的に実行し、コンポーネントの確実なライフサイクル終了を保証。
            - `SceneTransitionButtonComponent`: 同一オブジェクトのコンポーネント取得を `OnAwake()` に移行。
            - `PlayerTargetingComponent`: `Start()` での `LockonMarkerUIComponent` 未取得時フォールバック解決を追加。

### 🏃 次世代アニメーション＆モデルアーキテクチャ (Ultimate Animation System)
- [x] **Phase 1: 二段構えアーキテクチャの構築（低レイヤー＆コンポーネント分離）**
    - [x] `AnimationModel` の解体と `AnimatedMeshObject` (描画特化)・`Animator` (ロジック特化) への分離。
    - [x] `SkinnedMeshRendererComponent` および `AnimatorComponent` の新規作成とインスペクタ公開。
    - [x] `DebugScene` 等の既存コードを新アーキテクチャへ移行。
- [x] **Phase 2: アニメーションロジックの高度化 (スクリプト主導 & アニメーション主導)**
    - [x] アニメーションの切り替えを滑らかにする自動クロスフェード機能の実装。
    - [x] 再生速度（Playback Speed）の動的スケーリング制御。
    - **Root Motion** の抽出アルゴリズム実装と、`TransformComponent` 等への還元APIの実装。
- [ ] **Phase 3: リソース分離とイベントシステム (※一時保留)**
    - [x] `Skeleton` を `SkeletonData` (共有リソース) と `SkeletonPose` (インスタンス状態) に分離しメモリ最適化。（※Phase8のリファクタリングで既に完了済み）
    - [ ] `AnimationEvent` システムの実装（※現在テストできるゲーム側のサンプルがないため、必要になったタイミングへ実装を先送り）

- [ ] **Phase 4: 高度なアニメーション制御 (Humanoid / 動きの掛け算)**
    - **パラメトリック Blend Tree (1D/2D)**: 入力値(X,Y)に基づく3つ以上のアニメーションの同時継続ブレンド（前後左右の傾き等のプロシージャルな動き）。
    - **ボーンマスク (Bone Masking) / 部分ブレンド**: 上半身と下半身で別々のアニメーションを再生・合成する機構。
    - **加算ブレンド (Additive Blending)**: 基本姿勢に対して、ダメージののけぞりや回避アクションの差分のみを加算する機能。
    - **IK (Inverse Kinematics)**: Look-At IK (顔をターゲットに向ける) および Two-Bone IK (手足を特定のワールド座標に追従させる)。

- [ ] **Phase 5: プロ仕様アニメーション・デバッグ基盤の構築 (Advanced Animation Debugging)**
    - **ボーンの八面体メッシュ化とカラーコーディング**: 現在のLine描画を廃止し、ボーンの向き(Roll等)を視覚化するため八面体(Octahedron)等の3Dメッシュ描画に差し替え。階層深度に応じた色相グラデーション(Heatmap)や、IK適用等の状態に応じた動的カラー変更を実装。常に最前面(X-Ray)半透明描画パスで描画する。
    - **座標軸・テキストオーバーレイ**: 選択中、または主要なジョイントのローカルXYZ軸をデバッグ描画。ImGuiのDrawList等を利用し、ジョイントの3D座標をスクリーンに投影してボーン名を画面にオーバーレイ表示。
    - **オニオンスキニングと軌跡(Trail)描画**: 過去数フレームのボーン姿勢を保存するリングバッファを設け、姿勢を半透明(ゴースト)で同時描画。手首など指定エンドエフェクタの過去の位置をラインで結んで描画し、モーションの軌道(アーク)を可視化する。
    - **デバッグUI統合 (Skeleton Tree View)**: ImGuiインスペクタ上にスケルトンの階層ツリービューを構築。メッシュ/ライン切替、色分けON/OFF、ラベル表示トグルなどを制御する専用のデバッグパネルを実装する。

### 🖥️ ディスプレイモード＆ウィンドウ制御の高度化 (Display & Window Management)
- [x] **ディスプレイモード切替（F11 / Alt+Enter）のエンジンコア集約と OptionsScene UI ガイド統合**
    - **実装完了**: 一線級エンジン（UE/Unity等）のアーキテクチャ標準に則り、ディスプレイモード切替（`F11` / `Alt + Enter`）の責務をエンジンコア（`IrufemiEngine::Update`）に集約・一元管理。CVar `r.DisplayMode` を経由してウィンドウとボーダーレスフルスクリーンを全シーン共通で即座に相互トグル可能に整備。`OptionsScene` には二重トグルを排除した上で親切な操作ガイド（`[F11] FULLSCREEN`）を統合。レターボックスおよび仮想カーソルの境界も動的に追従。
- [ ] **展示・審査向け「キオスク（没入）モード（Windowsキー無効化）」オプションの追加検討**
    - **背景**: 展示会や審査プレイ中、プレイヤーの激しい操作（Ctrl / Shift / Alt等）によるWindowsキー誤爆（スタートメニューが飛び出してゲームが中断する事故）を防止する。
    - **実装仕様**:
        - `WinApp` に低レベルキーボードフック（`WH_KEYBOARD_LL`）を用いたWindowsキー（`VK_LWIN` / `VK_RWIN`）ブロック機能をオプションとして実装。
        - CVar（例: `r.LockWindowsKey`）または展示用ビルドフラグで有効/無効を切り替え可能にする。
    - **⚠️ 必須要件（安全な脱出導線の保証 / 閉じ込め防止）**:
        - Windowsキーを無効化する場合、プレイヤーが安全にゲームを終了できるよう以下の導線を**必ずセットで実装**する。
            1. ESCキー / コントローラーのMenuボタンによる「ポーズメニュー」および「ゲーム終了」ボタンの実装。
            2. タイトル画面における「ゲーム終了」メニューの実装。
            3. 開発者用の緊急終了ショートカット（例: F12キーやESC長押しでの強制終了）のフェイルセーフ用意。

### ⌨️ 入力システム (Input System) の AAA アーキテクチャ化
- [ ] **同時押し (Chord) キーバインドのサポート**
    - 現在1対1の物理キーバインドしかサポートしていない `InputManager` と `InputBinding` を拡張し、「Ctrl + Z」のようなモディファイアキーを含む複数キーの同時押し判定をサポートする。
- [ ] **InputMappingContext によるコンテキストルーティングの本格化**
    - エディタ用の `InputMappingContext` を定義し、`ImGui::IsKeyPressed` で一時的に処理されているエディタのショートカット（Undo/Redo 等）をエンジンの入力パイプラインに完全統合する。
    - ゲームプレイ中とエディタ操作中で入力フォーカスを安全に切り替えられる仕組み（Input Context Priority）を構築する。

### ⚡ 次世代パフォーマンス・アーキテクチャ最適化 (Next-Gen AAA Performance)
- [x] **CollisionManager のスレッドセーフ化と Async Raycast の実装**
    - `shared_mutex` を用いたRead-Write Lockの導入と、ThreadPoolと連携した非同期物理クエリAPIの提供。（完了）
- [ ] **Bindless Resources (Descriptor Indexing) の完全移行完了**
    - 全テクスチャ/リソースを巨大な Descriptor Heap に格納し、Shader にインデックス(uint)だけを渡す方式へ移行。
    - **進行状況**: C++基盤、および `Application_solo` / `Application_team` 双方のほぼすべてのHLSLファイルの移行が完了。
    - **残タスク**: エディタ用シェーダー `OutlineComposite.PS.hlsl` と `SelectionOutlinePass.cpp` を Bindless 経由でテクスチャを読むように修正する。
    - **残タスク**: 上記の完了後、互換維持のために復刻した `RootSlot::LegacyPSTexture` を削除し、真の Bindless Root Signature を完成させる。
    - **残タスク**: 既存の `RenderPackets` 系（PrimitiveBatchPacket, ModelBatchPacket 等）に残存している明示的なテクスチャバインドのコードを完全に撤去し、コードをシンプルにする。
- [ ] **マルチスレッドコマンド録画 (Multi-threaded Command Recording)**
    - Job System と連携し、D3D12 の CommandList 構築を複数スレッドで並列に行い、CPUの描画ボトルネックを解消する。
- [ ] **DirectStorage API / GDeflate による超高速リソースロードの対応**
    - NVMe SSD から VRAM へのダイレクト転送（CPUバイパス）と、GPU上での並列解凍(GDeflate)の実装。
    - 画面に映っているミップレベルだけをオンデマンドロードする Sampler Feedback 機構の実装。
    - 大量のアセットを一括ロードする際の大幅なパフォーマンス向上（ロード時間の短縮）と、高度な最新グラフィックスAPIへのキャッチアップによる技術アピールを目的とする。
- [ ] **自動アセット圧縮ツール群 (ビルドパイプライン) の構築**
    - **テクスチャの自動DDS化**: ビルド前イベントを利用し、画像(PNGやHDR)を自動的に DDSフォーマット (BC7等) に圧縮するツール（ResourceCooker等）を自作または統合し、VRAMの節約とロード高速化を図る。
    - **Resourceパッケージ化**: DirectStorageの性能を最大限引き出すため、全アセットを直接GPUへ流せるレイアウト（D3D12placed-footprint等）に変換・パディングし、1つのパックファイルにまとめるシステムの構築。
- [ ] **AAA級 自動LOD生成＆バイナリアセットクッキング (Asset Pipeline)**
    - **オートLOD生成 (`meshoptimizer`の統合)**: オープンソースの最高峰C++ライブラリ `meshoptimizer` を組み込み、モデル読み込み時にQEMアルゴリズムによる高品質な「ポリゴン削減（デシメート）」を自動で行い、LOD1, LOD2 などを生成する機構。
    - **アセットクッキング (専用バイナリ化)**: 実行時の計算オーバーヘッドを無く滅するため、生成したLODデータや最適化済みの頂点配列を独自のバイナリ形式（例: `.irufemimodel`）としてエディタ側で保存（ベイク）する。本編ではこれを読み込むだけ（Zero-Copy Load）にし、ロード時間を一瞬にする。
    - **LODコンポーネント (距離ベースの動的切り替え)**: 実行時にカメラとオブジェクトの距離（平方根を避けた2乗距離による超高速判定）を測り、描画するモデル解像度を自動で切り替える機能。エディタ上でスライダー調整を可能にする。
- [ ] **Clustered Shading (Forward+) または Deferred Rendering の実装**
    - 現在の固定長定数バッファによるライト管理から脱却し、数百〜数千の動的ライトを効率的に処理するライティング基盤の構築。
- [ ] **Compute Skinning (GPUスキニング) の実装**
    - CPUで行っているアニメーションのボーン行列計算と頂点ブレンドを Compute Shader にオフロードする。
- [ ] **PSO (Pipeline State Object) キャッシュとバックグラウンドコンパイル**
    - ゲームプレイ中のカクつき（Stutter）を防ぐため、バックグラウンドでの事前コンパイルおよびディスクキャッシュ機構の構築。
- [ ] **GameObjectアーキテクチャの完全データ指向（ECS）への刷新（大手術）** (※現状パフォーマンス要件はクリアしているため、当面はデスコープ/保留)
    - 現在の「ツリー構造（親子関係）＋ポインタベースのComponent」というオブジェクト指向の限界を突破するため、AAA基準の純粋な ECS (Entity Component System) へとエンジン根幹のアーキテクチャを書き換える。
    - **【完全整数ID方式への移行】**: 現在の `std::type_index` と `unordered_map` によるコンポーネント取得を完全に廃止し、すべてのコンポーネント種類を整数ID（Enum等）で管理。`GetComponent<T>()` の取得コストをハッシュ計算ゼロ（1CPUサイクル）の超高速アクセスへと昇華させる。
    - EntityはただのID（数値）とし、全てのComponentを種類ごとの巨大な連続配列（SoA: Structure of Arrays）で管理。これによりCPUのキャッシュミスを極限まで減らし、数万〜数十万のオブジェクト更新をフレームレート低下なしで処理可能にする。
    - ※JSONの完全破壊的変更を伴うため、移行用のコンバータツールの作成か、またはエディタ上では従来の GameObject の見た目を保ちつつビルド時に内部で純粋なデータ配列へと自動変換（Baking）する「ハイブリッド方式」から導入する。
- [ ] **コンポーネント取得のポリモーフィズム対応 (GetComponentの最適化とリファクタリング)**
    - 現状の `GetComponent<T>()` が `typeid` の完全一致に依存しており、`OBBCollider` 等の派生クラスを基底型で取得できない問題をアーキテクチャレベルで解決する。
    - **解決案1 (カスタムリフレクション)**: コンポーネント登録時 (`AddComponent`) に基底クラスの型IDでもマップに登録し、キャストなしで $O(1)$ 取得可能にする。
    - **解決案2 (継承の廃止 / Union化)**: コライダー等を継承構造からフラットな `struct` (内部に形状の Enum と Union/Variant を持つ) へとリファクタリングする（これが将来のECS完全移行に向けた最も安全な布石となる）。
- [ ] **GameObject構造変更の遅延評価（Deferred Modification）機構の導入**
    - `BaseScene` で採用している `pendingAdds_` / `pendingRemoves_` と同等の遅延キュー機構を `GameObject` の親子付け（`AddChild` / `RemoveChild`）およびコンポーネント追加・削除にも実装する。
    - これにより、`Start()` や `Update()` などループ処理の最中に動的なオブジェクト生成・親子付けを行ってもイテレータが無効化されず、より安全でAAA基準なエンジン基盤となる。
- [ ] **空間分割 (AAA水準: TLAS & BLAS 動的BVH) の完全導入とGJK/EPAポリゴン判定**
    - 現在 `CollisionManager` が総当りループ（$O(N^2)$）で判定している問題の解消、および MeshCollider（メッシュ単位の高精度判定）への対応を見据えた次世代アーキテクチャ。
    - **【アーキテクチャ設計 (TLAS & BLAS)】**
      1. **BLAS (Bottom-Level Acceleration Structure)**:
         - 個別の3Dモデルが持つポリゴン（三角形）を内包する静的なBVH。
         - **SAH (Surface Area Heuristic)** を用いて構築コストよりも走査(Traversal)速度を最大化するようノードを分割する。ロード時に1回だけ構築。
      2. **TLAS (Top-Level Acceleration Structure)**:
         - ゲーム空間内の全コライダーの「World AABB」を管理する動的BVH（Dynamic AABB Tree）。
         - **Fattened AABB (マージン付きAABB) と Refitting**: AABBに10%の余白を持たせ、オブジェクトが微動した際のツリー再構築(Remove/Insert)をスキップして境界更新(Refit)のみで済ませる最適化を導入。
    - **【衝突判定アルゴリズムと現実的なスコープ (Narrow-Phase)】**
      - **スコープの限定 (限界ライン)**: 全てのオブジェクトでMesh判定を行うと破綻するため、BLASの適用は「変形しない静的な背景地形」のみに限定する。判定対象も「地形ポリゴン vs 球(Sphere)」および「地形ポリゴン vs 光線(Raycast)」に絞り、高負荷なポリゴン同士の衝突解決は行わない。
      - **外部ライブラリによる統合**: フルスクラッチによるGJK/EPA実装（バグの温床）は避け、業界標準のオープンソース物理ライブラリを `external` フォルダに配置・統合し、自作エンジンのECS/CollisionManagerと連携させる「即戦力エンジニア」としてのアプローチを採用する。
      - **ライブラリ候補**:
        - `ReactPhysics3D` (C++17ベースで軽量・依存なし。エンジン連携の相性が最高)
        - `Bullet Physics` (業界最強の実績と知名度)
        - `Jolt Physics` (マルチスレッド特化の次世代エンジン)
        - `DirectXMath` (簡易的なRay/Sphere交差判定関数の利用)
    - **【ゲーム開発者向けUXと最適化 (Convex Hullの事前ベイク)】**
      - 生の凹型(Concave)メッシュの物理判定は極めて不安定なため、エディタ上でモデルを読み込んだ際に **Convex Hull (凸包)** の頂点を自動計算し、ファイルに事前保存(ベイク)するツール機能（Quickhullアルゴリズム等）を提供する。ランタイムはベイク済みの軽い凸包データを読み込むだけで済む。
    - **【実装ステップとロードマップ】**
      - **Step 1: Broad-Phase (TLAS) の先行導入**
        - 現行のプリミティブ総当たりを TLAS に置き換え。「BVH ON/OFFトグル」と「処理時間のリアルタイム表示」で10,000オブジェクトのFPS改善を動画化する。
      - **Step 2: 外部ライブラリの統合基盤作成**
        - 候補ライブラリ（ReactPhysics3D 等）を導入し、既存のプリミティブ判定をライブラリ側に委譲・ラップするテストを行う。
      - **Step 3: 静的背景のBLAS構築とConvexベイクツールの作成**
        - エディタ側の事前ベイク機能を作成し、地形に対する「Sphere」および「Ray」の正確なポリゴン判定を完成させる。
- [ ] **SIMD (DirectXMath / SSE) を活用した算術ライブラリの刷新**
    - 現在の `Vector3` や行列計算がスカラ演算（float単位）で実装されているため、DirectXMath (`XMVECTOR`, `XMMATRIX`) などの SIMD 命令にバックエンドを差し替え、物理・Transform計算のボトルネックを解消する。
- [ ] **`StringId` (高速な文字列ハッシュ化) システムの導入**
    - `GameObject` の検索やタグ比較で多発している `std::string` のアロケーション・比較コストを削るため、コンパイル時ハッシュ等を利用したIDベースの文字列プールシステム（Unreal Engineの `FName` に相当）を導入する。
- [ ] **エンジングローバルなメモリアロケータの最適化**
    - `new / delete` のOS側のヒープ競合を防ぐため、汎用的なアロケータのオーバーライド（例: `mimalloc` や独自TLSFアロケータの組み込み）を行い、ゲームループ中の動的メモリ確保を高速化する。
- [ ] **アセットのバイナリベイク（JSONシリアライズからの脱却）**
    - 現在シーンデータや設定ファイルが `json` で保存・パースされているが、文字列パースはロード時間の大きなボトルネックになる。製品ビルド時には `FlatBuffers` や独自バイナリ形式にベイクし、ゼロコピーでメモリに展開する超高速ロードを実現する。
- [ ] **イベント駆動（Delegate / Message Bus）システムの構築**
    - コンポーネント同士のやり取り（例: 当たり判定から特定スクリプトを呼ぶ等）のハードコードな依存関係を無くすため、グローバルな Pub/Sub イベントバスや C++ Delegate 機構を導入し、疎結合なアーキテクチャにする。
- [ ] **固定フレームレート物理演算 (Fixed Timestep) の実装**
    - 現在のゲームループはフレームレート依存で Update が回っている。物理挙動の「抜け」や「デバイス毎の挙動の違い」を防ぐため、描画とは独立して必ず一定間隔（例: 60Hz/120Hz）で物理判定を回す `FixedUpdate` 機構を導入する。
- [ ] **FMOD (オーディオミドルウェア) の統合**
    - 現在のXAudio2ベースのオーディオ管理から、AAA業界標準である **FMOD** （NieR:Automata等で採用）へと完全に移行する。これにより、複雑なストリーミング再生や3D空間音響、動的なBGM遷移（インタラクティブオーディオ）を専用ツール(FMOD Studio)と連携して実現する。

### ✨ エディタの高度化と表現力の向上 (Editor & Rendering Features)
- [x] **マスクベースのカスタムアウトライン描画 (AAAアプローチ導入)**
    - 個別エフェクト(EffectMask)のアウトライン抽出を周辺ピクセル参照による拡張(Dilation)に変更し、背景境界でも途切れない太線・指定カラーでの描画に対応。
- [ ] **VFX (ComputeShader / パーティクル等) のエディタプレビュー機能**
    - インスペクターからの直感的なスライダー操作とリアルタイムプレビューによるエフェクト作成環境の強化。
    - Editモード（全体の時間が止まっている状態）でも、選択した GPUパーティクル や VoxelParticle、各種ComputeShaderアニメーションの挙動だけを独立して確認できる「強制プレビュー機能」や「専用プレビューウィンドウ」の実装。
- [ ] **マテリアルエディタ＆シェーダバリアント**
    - Normal Map、Emission等のPBRパラメータをエディタで設定・即時反映する機能。
- [ ] **タイムライン/Tweenアニメーションシステム**
    - UIやオブジェクトの移動・フェードなどをエディタ上でキーフレーム設定・再生できる仕組みの導入。
- [ ] **エディタの選択状態（Selection）の安全な破棄・クリア機構**
    - シーンからオブジェクトが `RemoveGameObject` または `Destroy` された際に、エディタのインスペクター描画対象（選択中オブジェクト）に残存してクラッシュ（`GetScene()` == nullptr 等）を引き起こす問題の根本解決。破棄イベントのフックや Weak_ptr による管理を導入する。

### 🖥️ ウィンドウ・解像度スケーリング基盤 (Window & Resolution Scaling)
- [ ] **レンダーターゲット (G-Buffer 等) の動的リサイズ機構**
    - 現在のスワップチェーンリサイズに加え、オフスクリーン描画用テクスチャや深度バッファを、ウィンドウ解像度の変更に合わせて動的に再生成・リサイズする仕組みの構築。
- [ ] **アスペクト比対応 (レターボックス / ピラーボックス処理)**
    - ウルトラワイド(21:9)やレガシー(4:3)環境での全画面表示時に、画面の歪みを防ぐための Viewport 調整と黒帯描画処理の実装。
- [ ] **可変解像度方式（Free Aspect / OSウィンドウサイズ＝描画解像度）への対応**
    - 内部解像度をウィンドウサイズに完全に同期させ、4Kやウルトラワイドモニタのピクセルを100%活かす描画機構の導入。
    - ※実現には後述のUIアンカーシステム（レスポンシブUI）の完全対応が前提となる。
- [ ] **UIのアンカー (画面端固定) 機能 (Canvas Scaler)**
    - 解像度やアスペクト比の変更に追従し、ミニマップを「右下固定」など画面端に吸着させるUIアンカーシステムの導入。
- [ ] **グラフィックコンフィグ・マネージャーの構築**
    - フルスクリーン/ウィンドウモードの切り替えや内部解像度設定を管理し、JSONやINIファイルへセーブ・ロードする設定基盤の作成。

### 🛠️ 開発環境とビルドパイプラインの最適化 (Development & Build Pipeline)
- [ ] **パッケージマネージャー (vcpkg等) の導入、または Git LFS による外部ライブラリ管理の刷新**
    - **現状の課題**: 現在手動で `externals/` に配置している巨大な `.lib` ファイル（assimp等）を Git リポジトリから排除したため、クローンしてきた各メンバーが自力でソースからビルド（nmake等）しなければならず、環境構築の手間とエラーの温床になっている。
    - **試行したこと**: Visual Studio 標準の NuGet を使ったリソースパッケージ化を試みたが、最新の C++ / VS2022 (vc143) 環境に対応した公式パッケージが存在せず、リンクエラーが多発したため**ロールバック（中止）**した。
    - **今後の解決策（検討案）**:
      1. **Git LFS (Large File Storage) の導入**: `.lib` 等のバイナリファイルのみを LFS で追跡し、今まで通り `externals/` で手動管理したまま GitHub にプッシュできるようにする。（★一番手軽で確実）
      2. **vcpkg の導入**: モダンな C++ の標準パッケージマネージャである vcpkg をプロジェクトに導入する。
      3. **セットアップスクリプトの作成**: Google Drive 等にビルド済みの `.zip` を置いておき、`Setup.bat` を叩くだけで自動ダウンロード＆展開されるようにする。

---
<br><br><br><br>

## 📦 アーカイブ### 完了済みのタスク
- [x] エンジンの主要コード群を `Irufemi` 名前空間に移行 (2026/07/31 完了)
  - エンジンコア・数学モジュール等すべてのクラス・関数を `namespace Irufemi` 内に格納。
  - `IrufemiEditor`, `Application_solo`, `Application_team` 等の全プロジェクトに名前空間を一括適用。
  - 名前衝突等のビルドエラー、文字列置換によるクラッシュ問題を全て解決。

以下は過去のフェーズ（フェーズ1〜6）で達成・実装済みの機能一覧です。

<details>
<summary>▶ 完了したアーキテクチャと機能一覧を開く</summary>

### フェーズ8: リソース管理システムの抜本的改革（AAA基準）
- [x] **`ResourceHandle` および `ResourceCachePool` の実装**
    - [x] `std::shared_ptr` の依存を排除し、独自の ID（Handle） ベースの高速なアロケータを構築。
- [x] **`TextureManager` の Handle 化**
    - [x] メモリリーク防止と非同期ロードの安定化。
- [x] **`ModelManager` の Handle 化**
    - [x] VRAM メモリパージ機構を想定したアーキテクチャへの刷新。
- [x] **各種コンポーネント（MeshRenderer, AnimationModel 等）の対応**

### フェーズ7: アーキテクチャとパフォーマンスの最適化
- [x] **GPUParticleSystemのパフォーマンス最適化と共有化**
    - [x] `GPUParticleManager`（または類似の共有機構）の設計・実装。
    - [x] 各 `ParticleEmitterComponent` から放出命令（Emit）を一つの中央バッファに集約する（Instanced Emission）アーキテクチャへの改修。
    - [x] パフォーマンス測定と、安定性の確認。
    - [x] 最適化完了後、旧 `ParticleSystem` (CPU) の完全削除とコードベース統合。

### フェーズ1: コンポーネント指向アーキテクチャの完成
- [x] すべてのエンティティのベースとなる `GameObject` クラスの作成
- [x] `GameObject` にアタッチできる `Component` 基底クラスの作成
- [x] 基本コンポーネント群（`Transform`, `MeshRenderer`, `PrimitiveRenderer`, `SpriteRenderer`, `Collider`等）の実装
- [x] 【UI分離】コンポーネントから ImGui 依存を完全に排除（`ComponentEditorRegistry` への移行）
- [x] 【Fat Class解消】`ObjClass` 等の内部トランスフォーム依存を排除し、コンポーネント主導の描画へ移行

### フェーズ2: エディタ基盤機能の構築
- [x] `EditorManager` によるエディタ機能の統括（`#ifdef EditorMode`）
- [x] 「ヒエラルキー（Hierarchy）パネル」による全GameObjectのツリー管理
- [x] 「インスペクター（Inspector）パネル」によるコンポーネントパラメータの動的編集
- [x] `ImGuizmo` を使用したシーンビュー上での直感的なトランスフォーム操作
- [x] 「シーンビュー（SceneView）パネル」による描画結果のプレビューとデバッグ表示切替

### フェーズ3: エディタの高度化と操作性の向上
- [x] **コマンドパターンによる Undo/Redo システムの実装**
- [x] **プロジェクトブラウザ（ProjectBrowser）の最適化** (メモリキャッシュと手動更新)
- [x] **自動ファイル監視システム (DirectoryWatcher) の構築** (Windows APIによる自動検知)
- [x] **ピッキング機能の精緻化** (アウトライン表示の追加)
- [x] **アセットのドラッグ＆ドロップ連携の強化** (シーンへのD&D生成)

### フェーズ4: シーン管理とシリアライズ
- [x] `GameObject` / `Component` の JSON シリアライズ・デシリアライズ基盤の実装
- [x] **エディタ上でのシーン保存・読込UI（File Menu）の実装**
- [x] **起動時の初期シーン自動ロード・自動生成機能**
- [x] プレハブ（Prefab）システムの導入

### フェーズ5: ランタイム連携とデプロイ
- [x] **Play Mode / Edit Mode の完全分離** (シーンの一時保存・復元)
- [x] **完全データ駆動化への移行 (スクリプトのコンポーネント化)**

### フェーズ6: ソロ制作向け・実戦用ゲームエンジンの完成
- [x] **自作スクリプト（Component）作成の効率化・自動化** (マクロ導入)
- [x] **ゲームロジック・イベント連携の強化** (`OnCollisionEnter` コールバック対応等)
- [x] **2D UI（キャンバス）システムの実装** (`ButtonComponent`, `CanvasComponent`)
- [x] **マルチメディアコンポーネントのエディタ対応** (`AudioSourceComponent` 等)
- [x] **シーン遷移のデータ駆動化** (`SceneManager::LoadScene`)

### その他 実装済みの高度な機能
- [x] **ランタイムMSDF動的生成によるテキスト描画 (`TextComponent`)**
- [x] **コライダーの視覚化と直感的なリサイズ操作** (ImGuizmo連携)
- [x] **アプリケーション終了時のリソースリーク（LIVE_DEVICE）の解消**
</details>

### 📝 コードレビュー対応（07/03指摘分）
- [x] **設計とパターンの改善**
  - [x] `Component.h` 等で、大きなオブジェクトの引数が値渡しになっている箇所を `const` 参照渡しに変更し、コピーコストを削減する
  - [x] アプリケーション側の複数コンポーネントで重複している処理（初期化・Transform同期など）を吸収できる、エンジン側の基底クラスやインターフェースの設計見直し
- [ ] **コード規則・構成の整理**
  - [x] エンジン層のコアクラス群に `Irufemi::` 等の名前空間を適用し、アプリケーション側のコードとの名前衝突を防ぐ
- [ ] **ドキュメントの拡充**
  - [x] エンジンのすべてのコアクラスに対して「目的」「責務」を明記したクラスコメントを追加する
  - [x] APIとして公開する関数に対して、引数・戻り値に関するDoxygenコメントを網羅する

## [中長期] RenderGraphの根本的リファクタリング計画
現状の `RenderGraph` はリソース状態（TransitionBarrier）の依存解決が完全に自動化されておらず、
エンジンの `DrawManager` 等で手動の `RegisterResourceState` やバリア発行が必要となっている（アンチパターン状態）。
今後の拡張性と安全性を高めるため、一線級のエンジンに見られる宣言的アプローチへと以下のリファクタリングを計画する。

- [x] **リソースインポートの一元化**
  - バックバッファやG-Buffer（Color, Mask, Normal, Material, Velocity等）のような外部管理リソースは、各フレームの開始時に `RenderGraph` に外部リソースとして登録（インポート）するのみとする。
  - `DrawManager::ExecuteRenderQueues` などの呼び出し元で行っている手動バリア発行を撤廃する。
- [x] **パスごとのリソース状態宣言の徹底**
  - `MainOpaquePass` や各 `PostProcessPass` 側で、対象テクスチャを `RENDER_TARGET` や `PIXEL_SHADER_RESOURCE` として利用することを、`RenderGraphBuilder` を介して明確に宣言（Require/Write/Read）する仕組みを徹底する。
- [x] **自動バリア解決のコンパイル機構の実装/修正**
  - `RenderGraph::Execute` 時（またはコンパイルフェーズ）に、各パスが宣言したリソースの利用ステートを解析し、パス間でステートが変わるタイミングでのみ `TransitionBarrier` を自動的に発行するロジックを実装・強化する。
  - 実行後に元のステートに戻す処理も、手動ではなく「最終ステートから期待される初期ステートに戻す」機能として `RenderGraph` 内部にカプセル化する。

## 将来の拡張: 可変解像度方式（ウィンドウサイズ完全同期）への対応
現在エンジンは内部解像度を1280x720に固定し、最終出力時にレターボックス化を行う「アスペクト比維持・解像度分離」アーキテクチャを採用している。
将来的に、OSのウィンドウサイズ変更に追従して内部描画解像度（およびアスペクト比）自体を動的に変更する「可変解像度方式」をオプション機能として導入することを検討する。
- メリット: ウルトラワイドモニター等で画面全体をフル活用したリッチな体験が可能になる。UIやHUDのスケーリングも柔軟に行える。
- 実装課題:
  - ResizeSwapChain発生時に、G-Buffer等の全中間テクスチャを再生成（または動的リサイズ）する必要がある。
  - UIのアンカー処理（画面端へのスナップ等）の完璧な対応が必須となる。
  - RenderGraphおよび各種Passにおいて、解像度依存の計算（Viewportなど）を動的に同期させる仕組みが必要。

## 将来の拡張: RenderGraphの抽象化とViewportの自動化（AAAアプローチ）
現状、各描画Pass（UIPass, TopMostPass, PostProcessRunnerなど）内に `#ifdef EditorMode` や `GetClientWidth()` などがハードコードされており、OSウィンドウの情報をPassが直接知る構造になっている。
これをより一線級のエンジン（AAAアーキテクチャ）に近づけるため、以下のリファクタリングを計画する。
- [ ] **Viewport/Scissor の自動設定**
  - Pass内で手動でViewportを計算するのではなく、`OMSetRenderTargets` される **描画先テクスチャ（RenderTarget）のサイズ** からRenderGraphが自動で Viewport と ScissorRect を設定する仕組みを構築する。
- [ ] **エディター依存コードの排除**
  - 各Passから `#ifdef EditorMode` を撤廃し、Passは「与えられたレンダーターゲット（1280x720など）にただ描画するだけ」という純粋な状態に抽象化する。
- [ ] **スケーリング/Blit専用パスの分離**
  - OSウィンドウとゲーム解像度のアスペクト比の差異（レターボックス計算など）を吸収する処理は、`PostProcessRunner` やその他のPassに散在させず、最終出力のみを担う専用の `FinalBlitPass`（または `PresentPass`）に集約する。

## 将来の拡張: Prefabモードの独立ウィンドウ化（マルチViewport対応）
現状のPrefabモードは、既存の `SceneView` パネルの中身を一時的にPrefab空間に切り替える方式（モダンUnity方式）を採用している。
将来的に、Unreal Engineのブループリントエディタのように、メインのScene画面と並べて見比べることができる「完全に独立したPrefab Viewタブ」の実装を検討する。
- メリット: 本番のシーンとPrefabを並行して確認・編集できる。
- 実装課題:
  - 2つの独立した3D空間を同時に描画するため、`SceneManager` を複数持てるようにするか、`Camera` と `RenderTarget` のマルチパスパイプラインを構築する必要がある。
  - RenderGraphアーキテクチャの根本的な拡張（複数カメラ・複数出力系統のサポート）が必要となる。

## 将来の拡張: 画面暗転とアセットロード状態の分離
現状の `SceneManager` は、シーン遷移時の「ロード中（暗転）」と、エンジン全体のアセットロード状態（`AsyncManager` 等）が密結合になりがちである。
将来的に、以下のようなリファクタリングを検討する。
- **フラグの完全分離**: 「アセットマネージャーの非同期ロードフラグ（本当に裏でアセットをロードしているか）」と「シーンマネージャーの画面暗転・ローディング画面表示フラグ（ユーザーにローディング中を見せているか）」を完全に分離し、状態管理を明確にする。
- メリット: ロードが早く終わっても演出上の最低暗転時間を確保しやすくなる。また、非同期ロード中のUI描画（インゲームのバックグラウンドロードなど）がより安全に行えるようになる。

## 将来の拡張: FontManager の根本的なロック粒度改修
現在、`FontManager::PrecacheText` によるテキストのSDFテクスチャ動的生成はバックグラウンドスレッドで行われているが、SDF生成（msdfgen）からGPUへのテクスチャアップロード完了まで、`cacheMutex` をロックしたままになっている。
このため、メインスレッドがその最中に初めて表示する文字を描画しようとすると、`cacheMutex` の取得待ちで長時間ブロックされ、強烈なFPSスパイク（カクつき）が発生する。
- **ロックのスコープ最小化**: `cacheMutex` は「キャッシュの検索」と「キャッシュへの登録」時のみロックし、SDFテクスチャの生成処理（`generateMSDF`）およびGPUアップロード（`ExecuteUploadCommands`）中はメインスレッドをブロックしないようにロック粒度を改修する。
- メリット: ゲーム中に新しいUIテキストやダメージ数値などが初めて画面に描画される際のFPS低下を完全に防止できる。

## 将来の拡張: IrufemiEngine (God Class) の解体と Subsystem パターンの導入
現在 `IrufemiEngine.cpp` は 1,100行を超え、DirectXデバイス・ウィンドウ・入力・オーディオ・物理・レンダーターゲット・ポストプロセス・シーン管理など、エンジン内のあらゆる機能の生成・初期化・毎フレーム更新・破棄（`Finalize`）を直に手動で管理しており、典型的な **God Class（神クラス）** の状態となっている。

特に `Finalize()` での破棄順序が手作業で直書きされているため、依存関係の順序が狂った際に「破棄済みCOMリソースやマネージャーへの不正アクセスによる終了時クラッシュ」「DirectXの未解放リーク警告」といった致命的な不具合を引き起こすリスクが高い。

商用エンジン（Unreal Engine や Unity 等）の標準的なアーキテクチャに倣い、**Subsystem（サブシステム）パターン** を導入してエンジンコアをスリム化・自律分散化する。

### 1. アーキテクチャ設計 (ISubsystem)
すべてのエンジンサブシステムが実装すべき共通基底インターフェースを新設する。

```cpp
namespace Irufemi {

enum class SubsystemPriority : int {
    CoreWindow   = 0,   // ウィンドウ・OS基盤
    GraphicsLow  = 100, // DirectX12 デバイス・CommandQueue・スワップチェーン
    Resource     = 200, // テクスチャ・モデル・シェーダーマネージャー
    Physics      = 300, // 物理演算・衝突判定
    Audio        = 400, // サウンドエンジン
    Input        = 500, // 入力管理
    Scene        = 600, // シーン・GameObjectマネージャー
    Editor       = 700  // エディタUI・ツール機能
};

class ISubsystem {
public:
    virtual ~ISubsystem() = default;

    virtual SubsystemPriority GetPriority() const = 0;
    virtual const char* GetName() const = 0;

    virtual void Initialize() = 0;
    virtual void Update(float deltaTime) {}
    virtual void Finalize() = 0;
};

} // namespace Irufemi
```

### 2. IrufemiEngine 側のライフサイクル委譲
`IrufemiEngine` 側は各マネージャーの初期化・破棄ロジックを直書きするのを廃止し、サブシステムのコンテナで統一管理する。

- **初期化 (`Initialize`)**:
  - 各サブシステムを Priority 順（昇順）にソートして自動初期化。
- **更新 (`Update`)**:
  - 毎フレーム、登録されたサブシステムの `Update(deltaTime)` を順次実行。
- **終了 (`Finalize`)**:
  - **初期化と完全に逆順（Priority 降順）** で自動的に `Finalize()` を呼び出し破棄。
  - これにより、「DirectXデバイスが破棄された後にテクスチャが解放されようとしてクラッシュする」といった破棄順序の逆転バグが構造的に 100% 発生しなくなる。

```cpp
class IrufemiEngine {
public:
    template <typename T, typename... Args>
    T* RegisterSubsystem(Args&&... args) {
        auto sub = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr = sub.get();
        subsystems_.push_back(std::move(sub));
        return ptr;
    }

    template <typename T>
    T* GetSubsystem() const {
        for (const auto& sub : subsystems_) {
            if (auto casted = dynamic_cast<T*>(sub.get())) {
                return casted;
            }
        }
        return nullptr;
    }

private:
    std::vector<std::unique_ptr<ISubsystem>> subsystems_;
};
```

### 3. 段階的移行計画 (Phased Migration Plan)
- **Phase 1 (独立サブシステムの先行分離)**:
  - 依存が少なく独立性の高い `AudioSubsystem`, `InputSubsystem`, `PhysicsSubsystem` を先行して切り出し、`ISubsystem` の運用実績を作る。
- **Phase 2 (グラフィックス・リソース層の分離)**:
  - `GraphicsCoreSubsystem` (Device, SwapChain), `RenderPipelineSubsystem` (RenderGraph, PostProcess), `ResourceManagerSubsystem` を分離。
- **Phase 3 (IrufemiEngine のファサード化)**:
  - `IrufemiEngine.cpp` のコード量を 100〜200行程度にスリム化し、単なるブートストラップ（起動エントリーポイント）とサブシステムへのアクセス窓口（Facade パターン）へ純化する。

### 📦 静的ポインタキャッシュ群のコンテキスト集約（Context Struct / Dependency Injection 改革）

#### 1. 現状の課題とアーキテクチャ背景
- [x] ~~`BaseResource::s_dxCommon_` / `BaseResource::GetDirectXCommon()`~~ (Phase 3完了: インスタンス優先メンバ移行)
- [x] ~~`Sprite::s_textureManager_` / `Sprite::GetTextureManager()`~~ (Phase 3完了: コンポーネントDI・インスタンスメンバ移行)
- [x] ~~`Text::s_fontManager_` / `Text::GetFontManager()`~~ (Phase 3完了: コンポーネントDI・インスタンスメンバ移行)
- [x] ~~`Primitive3DObject::s_textureManager_` / `Primitive3DObject::GetTextureManager()`~~ (Phase 3完了: コンポーネントDI・インスタンスメンバ移行)
- `ParticleObject::s_textureManager_` / `ParticleObject::GetTextureManager()`
- [x] ~~`BaseModel::s_engine_` / `BaseModel::GetIrufemiEngine()`~~ (完了: GetIrufemiEngine() を完全撤廃し、Component::GetEngine() 経由へ統一)
- `GPUParticleSystem::s_engine_`, `s_textureManager_`
- [x] ~~`Object2DResource::sTextureManager`, `Object3DResource::sTextureManager`, `GpuMaterial::sTextureManager`~~ (Phase 2完了: インスタンス所有へ移行)
- [x] ~~`RenderContext` 基盤新設と全レンダーパス移行~~ (Phase 1完了: IRenderPass / RenderGraph 統合)

これらは `IrufemiEngine::Initialize()` 時に一括注入されエンジン破棄時に解放されるため現状クラッシュのリスクはないが、**「将来のマルチスレッド描画（並列コマンドリスト構築）」「複数ワールド・エディタ同時実行（PIE）」** を導入する際にグローバル衝突の原因となる。

#### 2. 一線級エンジン（UE / Frostbite）準拠の解決策：`RenderContext`
個別の静的ポインタを排除し、描画パスやリソース操作時に軽量なコンテキスト構造体を参照渡しする設計へ移行する。

```cpp
struct RenderContext {
    DirectXCommon* dxCommon = nullptr;
    TextureManager* textureManager = nullptr;
    FontManager* fontManager = nullptr;
    PSOManager* psoManager = nullptr;
    ID3D12GraphicsCommandList* commandList = nullptr;
};
```

#### 3. 段階的移行フェーズ (Phased Roadmap)
- [x] ~~**Phase 1: 描画パイプライン（`DrawManager` / `RenderGraph`）のコンテキスト化**~~ (完了)
- [x] ~~**Phase 2: リソース層（`Object2DResource` / `Object3DResource` / `GpuMaterial`）の静的ポインタ撤廃**~~ (完了)
- [x] ~~**Phase 3: オブジェクト層（`Sprite` / `Text` / `Primitive3DObject` / `BaseResource`）の純化**~~ (完了: コンポーネント自動DIによる後方互換100%維持)

---

### 🛡️ レンダリングリソースのカプセル化刷新（Low-Level Resource Encapsulation）

#### 1. 課題と背景
- `Object3DResource`, `Object2DResource`, `LineResource` において、頂点バッファポインタ（`vertexData_`）、ビュー（`vertexBufferView_`）、インデックスバッファ（`indexBufferView_`）、トランスフォーム（`transform_`）などの低レベルメンバが `public` で公開されている。
- `DrawManager.cpp` や `StaticModelObject.cpp`, `AnimatedMeshObject.cpp`, `Effect.cpp`, `RenderData.cpp` などエンジン各所で直接生メンバがアクセスされているため、安全なカプセル化と一貫したアクセサ経由へのリファクタリングを段階的に実施する。

#### 2. 段階的実装計画
- [x] ~~**Step 1: アクセサ API（Getter / Read-Only View）の整備**~~ (完了: VBV, IBV, IndexCount, Transform, TextureHandle, GPU Resource アクセサ新設)
- [x] ~~**Step 2: 呼び出し側（`DrawManager`, 各 Model/Object クラス, 2D オブジェクト群, UI）の移行**~~ (完了: 全直接アクセスをアクセサ経由へ安全移行)
- [x] ~~**Step 3: メンバ変数の `protected` 化**~~ (完了: Object3DResource / Object2DResource / LineResource の低レベルリソースを完全カプセル化)

---

### 🛡️ パーティクルエミッターハンドルの安全化（Opaque Handle パターン刷新）

#### 1. 課題と背景
- `GPUParticleManager::EmitterHandle` および `VoxelParticleManager::EmitterHandle` において、内部システムへの生ポインタ（`GPUParticleSystem*`）を保持して外部（`ParticleObject`, 各シーン等）へ返却している。
- マネージャー側でシステムが再生成・解放された際に、ダングリングポインタが発生するリスクを排除するため、AAA水準の世代番号付き不透明ハンドル（Opaque Handle: SystemId + Generation + EmitterIndex）への刷新を検討する。

#### 2. 段階的実装計画
- [x] ~~**Step 1: Opaque Handle 型の設計と世代カウンタの実装**~~ (完了: `uint32_t systemId`, `uint16_t emitterIndex`, `uint16_t generation` を 64bit 値型として定義)
- [x] ~~**Step 2: GPUParticleManager / VoxelParticleManager でのハンドル解決・検証 API の整備**~~ (完了: `idLookup_` による O(1) 解決、世代番号インクリメントによる Use-After-Free/二重解放防止、VoxelParticleManagerの線形探索撤廃)
- [x] ~~**Step 3: 呼び出し元（`ParticleObject`, `DebugScene` 等）の移行と生ポインタ完全撤廃**~~ (完了: 100% 後方互換 API により呼び出し元無変更で生ポインタを完全排除)





