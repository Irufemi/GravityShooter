# Gravity Shooter / Irufemi Engine

[![GitHub Pages Docs](https://img.shields.io/badge/Docs-GitHub%20Pages-2ea44f?logo=github&style=flat)](https://irufemi.github.io/GravityShooter/)
[![DebugBuild](https://github.com/Irufemi/CG3/actions/workflows/DebugBuild.yml/badge.svg)](https://github.com/Irufemi/CG3/actions/workflows/DebugBuild.yml)
[![ReleaseBuild](https://github.com/Irufemi/CG3/actions/workflows/ReleaseBuild.yml/badge.svg)](https://github.com/Irufemi/CG3/actions/workflows/ReleaseBuild.yml)
[![DevelopmentBuild](https://github.com/Irufemi/CG3/actions/workflows/DevelopmentBuild.yml/badge.svg)](https://github.com/Irufemi/CG3/actions/workflows/DevelopmentBuild.yml)
[![CheckUnwantedFiles](https://github.com/Irufemi/GravityShooter/actions/workflows/CheckUnwantedFiles.yml/badge.svg)](https://github.com/Irufemi/GravityShooter/actions/workflows/CheckUnwantedFiles.yml)

> 📖 **[【公式公開中】オンラインAPIドキュメント＆開発マニュアル (GitHub Pages)](https://irufemi.github.io/GravityShooter/)**

C++ と DirectX 12 を用いてスクラッチから構築した、**GPU-Driven Rendering** および **Data-Oriented Design** 指向の自作3Dゲームエンジンプロジェクトです。
大量の動的オブジェクト処理やロード時間の短縮、空間分割による最適化など、パフォーマンスと拡張性を意識したアーキテクチャの実装を目的としています。

## ⚡ クイックスタート（ワンクリックでビルド ＆ 起動）

本リポジトリはリポジトリの健全性および技術審査規約（バイナリ配布制限・アンチパターン回避）に基づき、**コンパイル済み実行ファイル（.exe）やビルド中間生成物はコミットしていません**。

リポジトリ直下のバッチファイルをダブルクリックするだけで、MSBuildによる依存関係解決・NuGetパッケージ自動復元・シェーダー事前コンパイルが走り、即座にビルド・配置・起動します。

| バッチファイル | 動作内容 |
| :--- | :--- |
| **`BuildAndRun.bat`** | **ゲーム本編（Release構成）** を最適化ビルドして即座に起動 |
| **`Run_Benchmark.bat`** | **10,000オブジェクト負荷検証 ＆ 外部テレメトリ監視** をワンクリック連動起動 |
| **`Launch_Telemetry.bat`** | **C# WPF 外部パフォーマンス監視ツール** をビルド・起動し、リアルタイムメトリクスを表示 |
| **`PackageForSubmission.bat`** | 審査提出用パッケージ（製品版・**自作エディタ版**・ソースコード）をワンクリック自動生成 |

> ※ **ビルド環境**: Visual Studio 2026 (または MSBuild がインストールされた Windows 環境)。Visual Studio を手動で開くことなく、バッチ単体で完全自動ビルドが可能です。  
> ※ **自作エディタの動作確認**: `PackageForSubmission.bat` でエディタ体験パッケージ（`02_EditorBuild`）を一括生成するか、Visual Studio で `Editor` 構成を選択してビルド可能です。

---

## 🎨 ポートフォリオ技術発表資料
本エンジン、および実装ゲームシステムに関する詳細な技術解説・スライド資料を公開しています。

> **【要約】自作エンジン「Irufemi Engine」の低レイヤにおける最適化（世代付きHandle、GPUフラスタムカリング、Bindless Resources）や、ゲーム制作のイテレーションを支えるエディタ・開発支援機能の解説資料です。**
* **📄 [末廣航一_ポートフォリオ (PDF)](docs/Portfolio_Koichi_Suehiro.pdf)**
* **📄 [末廣航一_プログラム説明書 (PDF)](docs/ProgramSpec_Koichi_Suehiro.pdf)**
* **🌐 [公式オンラインAPIリファレンス＆マニュアル (GitHub Pages)](https://irufemi.github.io/GravityShooter/)**
  * ※PDF資料は本リポジトリの `docs/` ディレクトリ内に格納しています。

---

## 🎮 ショーケース：Irufemi Engineで開発されたゲーム群

本エンジンの大きな特徴は、「技術的なデモ」に留まらず、実際に個人制作やチーム制作の基盤として運用しながら機能拡張を重ねており、ゲームロジックをスムーズに実装できるAPI設計と実用性を重視している点にあります。（※自身の累計制作経験：個人制作4作品（本作含む）、チーム制作10作品 / 計14作品。過去13作品はすべて提出・完遂済み）

| プロジェクト名 | 概要・ジャンル | 開発体制・期間 | 実装した主なエンジン機能 |
| :--- | :--- | :--- | :--- |
| **Gravity Shooter** | 奥スクロール型 重力シューティング | 1人 (鋭意開発中) | 最大1万個ベンチマーク耐性のガレキ制御（本編では視認性重視の最適密度循環）、スプライン相対追従、マルチロックオン、Dynamic BVH |
| **七転び八転び** | 3Dアクションゲーム | 4人 (約4ヶ月) | 非同期ローダー、Compute Shaderによる数十万ボクセル破壊、シーン管理・リソース管理 |
| **血管壊回** | 見下ろし型 疑似3Dアクション | 3人 (約1ヶ月) | 3Dパーティクルによる血流表現、XY平面ベースの2D/3D融合コリジョン処理 |
| **纏当て** | 3Dボスバトルアクション | 4人 (約1ヶ月) | DirectX 12 描画パイプラインの構築、デバッグUI環境の提供、描画用HLSLシェーダー |

---

## 🎯 コアゲームプレイ & エネミーAIシステム (Core Gameplay & AI)

『Gravity Shooter』は、空間に漂う大量の瓦礫（ガレキ）を重力波で引き寄せ、攻防一体のシールドや高速弾丸として撃ち出す奥スクロール型の重力アクション・レールシューターです。

### 🔄 最小ゲームループの確立 (Core Loop)
```mermaid
graph LR
    A["🧲 ガレキ引き寄せ<br>(周囲の破片を自機周囲へ)"] --> B["🛡️ オービット展開<br>(旋回シールドで弾幕防御)"]
    B --> C["🎯 マルチロックオン<br>(照準で複数の敵を捕捉)"]
    C --> D["💥 時間差一斉射出<br>(敵機体を粉砕・ドロップ)"]
    D --> A
    style A fill:#2b6cb0,color:#fff
    style B fill:#2c5282,color:#fff
    style C fill:#9b2c2c,color:#fff
    style D fill:#c53030,color:#fff
```

### 👾 3大エネミーAIと駆け引き (Enemy AI Behaviors)
単に直進するだけの「動く的」を排し、自機の機動や速度ベクトルを認識して攻撃を仕掛ける3系統のAI行動ルーチンをC++ステートマシン・ストラテジーパターンで実装しています。

| 敵タイプ | AI行動アルゴリズム | プレイヤーとの駆け引き・演出 (Game Juice) |
| :--- | :--- | :--- |
| **通常砲台<br>(`StandardGunner`)** | レールスプライン上の局所直交基底（Frenet-Serret）を維持しながら編隊飛行し、自機座標へ向けて正確に射撃。 | ガレキシールドを展開して正面から弾幕を受け止めつつ、隙を見て撃破する基礎駆け引き。 |
| **未来予測スナイパー<br>(`PredictiveSniper`)** | 自機の移動速度ベクトルを計算し、弾速から逆算した「未来着弾予測地点」をロックオン。赤色シリンダーAOE予告線（Telegraph Laser）が高速パルス点滅。 | 予告線の射線軸から軸ずらしダッシュで回避するか、ガレキを前方に構えて直撃を防ぐ緊迫感のある駆け引き。 |
| **ダイブボマー<br>(`DiveBomber`)** | 接近ステートから突撃ステートへ遷移し、機体を急加速させながら自機へ体当たり自爆を敢行。突入直前の機体ロール旋回挙動を数学的に制御。 | 自爆前の高速接近中に素早くロックオンし、カウンターでガレキをぶつけて粉砕・自爆阻止する爽快感。 |

- 🔗 **関連コード原本**: [`RailShooterEnemyComponent.cpp`](project/Application_solo/RailMechanics/RailShooterEnemyComponent.cpp) / [`WaveManagerComponent.cpp`](project/Application_solo/Level/WaveManagerComponent.cpp)

---

## 🕹️ プレイ操作ガイド & キオスクモード (Controls & Kiosk Mode)

『Gravity Shooter』および本エンジン製ゲームは、展示会（BitSummit / TGS）や審査試遊での快適性を最重視した入力・ウィンドウ制御を標準搭載しています。

### 基本操作 (Controls)
| アクション | キーボード & マウス | コントローラー (XInput) |
| :--- | :--- | :--- |
| **移動** | `W` / `A` / `S` / `D` | 左スティック |
| **照準 (Aim)** | マウス移動 | 右スティック |
| **ガレキ引き寄せ (Pull)** | `マウス右クリック` / `E` | `LT` / `L1` |
| **ガレキ射撃 (Fire)** | `マウス左クリック` / `Space` | `RT` / `R1` |
| **ロックオン (Lock-On)** | `Shift` | `LB` |
| **ポーズ (Pause)** | `ESC` | `Start / Menu` |
| **全画面切替 (Fullscreen)** | `F11` または `Alt + Enter` | - |

### 🔒 展示・審査向けキオスクモード & 安全な終了導線 (Fail-Safe Exit)
- **Windowsキー誤爆防止**: 激しい操作によるOSスタートメニューの飛び出しを防ぐため、ゲーム実行中はWindowsキーを低レベルフック（`WH_KEYBOARD_LL`）で安全に無効化しています（フォーカスが外れると自動復帰します）。
- **ゲームの終了方法 (3系統の脱出経路)**:
  1. **ポーズメニュー**: `ESC` キーを押してポーズ画面を開き、**[QUIT]** を選択してクリーン終了。
  2. **ESCキー 2秒長押し**: 万が一の画面スタック時でも、`ESC` を2秒長押しすることでエンジンメインループから安全に強制シャットダウン（`PostQuitMessage(0)`）を発動。
  3. **`Alt + F4`**: OS標準の即時終了ショートカットは常に保証されています。

---

## 📊 技術的アピールポイント（最適化とモダンアーキテクチャ）

本エンジンにおける中核技術について、「実現したかった要件」「比較検討したトレードオフ」「結果と定量データ」の構成で解説します。

### <a id="desc-perf1"></a>🔹 1. 「処理落ち」の限界を広げるパフォーマンス最適化
> **【要約】最大10,000個の負荷耐性と、本編での視認性を最優先した最適循環チューニング**

- **【課題・実現したかったこと】**
  『Gravity Shooter』の基礎設計において、将来的な戦闘規模の拡大や破壊表現に耐えうる処理能力を確保したかった。また実際のゲーム本編では、画面がオブジェクトで埋まり弾幕が見えなくなるのを防ぎつつ、数十〜100個規模のガレキが「引き寄せ ➔ 旋回防御 ➔ 射出 ➔ 敵撃破でドロップ」という心地よい密度で循環するゲームループの実現を目指した。
- **【比較と技術選定 (トレードオフ)】**
  ポインタベースの `GameObject` (OOP) では、メモリ断片化によるキャッシュミスでCPUの限界（16.6ms）がすぐに訪れる。そこで、ゲーム進行に必要な当たり判定（OBB等）はCPUに残し、視覚的な「大量の破片の物理演算」のみを Compute Shader にオフロードした。さらに、ガレキ制御等の演算は真面目に行わず `sin/cos` を用いた「騙しの物理演算（Fake Physics）」で軽量化。データ管理には、座標データを密配列で保持する `VirtualEntityManagerComponent` (Sparse Set アーキテクチャ) を採用した。
- **【結果と定量的成果】**
  ・**【大量オブジェクト負荷テスト】**: 10,000オブジェクトの同時追尾ベンチマークにおいて、CPU側のディスパッチ負荷を **平均0.066ms** に抑え、高負荷時でも安定して動作することを確認。
  ・**【ゲーム本編の実運用】**: プレイヤーの視認性（弾幕の視認、敵の予兆検知）とゲームテンポを最優先し、本編中では数十〜100個のガレキが高密度かつ快適に物理循環するよう意図的に調整。敵撃破時に飛び散る破片を即座に次の武器としてリサイクルする、リッチで爽快な弾幕循環を実現。
- 🔗 **関連コード原本**: [`VirtualEntityManagerComponent.h`](project/IrufemiEngine/Framework/Component/VirtualEntity/VirtualEntityManagerComponent.h) / [`DebrisManagerComponent.cpp`](project/Application_solo/Environment/DebrisManagerComponent.cpp)
> **💡 ワンクリック負荷検証の手順 & ハイブリッド通信設計**:  
> リポジトリ直下の **`Run_Benchmark.bat`** を実行すると、Releaseビルド（`/O3` 最適化）でゲーム本体と外部ツール `TelemetryMonitor` が連動起動します。  
> 
> ```
> [TelemetryMonitor (WPF)] ──(UDP: 8888 高頻度メトリクス受信 / ゼロ負荷)──> [Game Engine]
>                          <──(TCP: 8889 到達保証コマンド送信 ＆ ログ返信)───> [Game Engine]
> ```
> 
> - **UDP (Port: 8888)**: 毎フレームの描画負荷やFPSなどの数値を非同期ストリームし、ゲーム描画ループへの負荷を一切かけずにリアルタイムグラフ化。
> - **TCP (Port: 8889)**: 外部ツールから `spawn_debris 10000` などの命令を確実に届かせ、ゲーム側の実行結果ログ（`[Success]` / `[Error]`）を双方向ストリーミング。接続状態（Connected / Disconnected）もリアルタイム可視化。
> - `TelemetryMonitor` 下部の **【💥 10,000 Debris】** ボタンをクリックするだけで、誰でも即座に1万個ガレキの60FPS安定動作（平均0.066ms）を追試・確認できます（インゲーム内でも `F1` キーでコンソールを開閉可能）。

**【図解: Sparse SetとInstance Promotionによるキャッシュ最適化】**
```mermaid
graph TD
    subgraph Data-Oriented ["Virtual Entity 密配列キャッシュ"]
        Dense["連続メモリ上の座標データ配列 (キャッシュヒット率: 極高)"]
    end
    subgraph Object-Oriented ["GameObject プール"]
        GO["実体化されたGameObject群 (コンポーネントによる複雑な処理)"]
    end
    Dense -->|"干渉時・描画時のみ Promote() で実体化"| GO
    GO -->|"不要になれば Demote() で返却"| Dense
    style Dense fill:#2b6cb0,stroke:#3182ce,stroke-width:2px,color:#fff
    style GO fill:#2d3748,stroke:#4a5568,stroke-width:1px,color:#a0aec0
```

---

### <a id="desc-bvh"></a>🔹 2. Dynamic BVH とハードコードの排除
> **【要約】O(N log N)の動的AABBツリーとデータ駆動設計による衝突判定**

- **【課題・実現したかったこと】**
  マップごとの敵の出現位置や大量の背景をコードに直書きすると保守性が下がる。また、万単位のオブジェクトによる総当たり衝突判定（$O(N^2)$）のCPU負荷が課題だった。
- **【比較と技術選定 (トレードオフ)】**
  衝突判定を $O(N \log N)$ に落とすため動的AABBツリー (BVH) を実装。さらに、ツリーのノード管理をポインタで実装するとキャッシュミスが起きるため、あえてポインタを排し **「インデックスベースの配列プール」** を用いた Data-Oriented な実装を選択した。また、ウェーブや環境は JSON/CSV で外部定義し、`EnvironmentManagerComponent` でプーリングとバッチ描画（インスタンシング）を自動化するデータ駆動設計を導入した。
- **【結果と定量的成果】**
  プログラマ視点での高い保守性とシーンファイルの肥大化防止を達成。大量のガレキや敵弾が密集する激しい弾幕戦でも、処理落ち（フレーム落ち）による操作遅延をゼロにし、プレイヤーの反射神経を裏切らない安定した実行速度（CPUバウンドの解消）を実現。
- 🔗 **関連コード原本**: [`EnvironmentManagerComponent.h`](project/Application_solo/components/EnvironmentManagerComponent.h) / [`DynamicBVH.cpp`](project/IrufemiEngine/Physics/Collision/DynamicBVH.cpp)

---

### <a id="desc-memory"></a>🔹 3. 開発を止めない堅牢なメモリ管理と非同期・遅延処理
> **【要約】自前スレッドプールによる非同期ローダーと「世代付きHandle」の導入**

- **【課題・実現したかったこと】**
  高解像度テクスチャのロードがメインスレッドを占有し、アクションゲームで最も重要な「テンポ」を削ぐ数秒間のフリーズ（スパイク）を無くすこと。また、生成・消滅を繰り返す弾などのオブジェクトにおけるダングリングポインタのクラッシュ（アクセス違反）を未然に防止したかった。
- **【比較と技術選定 (トレードオフ)】**
  OS依存の `std::async` ではなく、ゲーム進行を極力阻害しないよう自前でワーカースレッド数と待機状態を制御できるスレッドプール (`ThreadPool`) を `std::mutex` を用いてスクラッチ実装。
  さらに、オブジェクトプールは生ポインタではなく **「世代(generation)付きHandle」** による管理へ移行。また、Updateループ中の直接削除による配列崩壊を防ぐため、**遅延削除キュー (Pending Kill)** を実装し、フレームの最後で一括返却するアーキテクチャを採用した。
- **【結果と定量的成果】**
  ロード中の最大スパイクを **約4100ms から 約51ms に激減**。また、古い世代のHandleアクセスを `nullptr` で弾き、遅延削除によってマルチスレッド環境や複雑なUpdateループ下でも安全で堅牢なゼロ・アロケーション基盤を構築できた。
- 🔗 **関連コード原本**: [`ThreadPool.h`](project/IrufemiEngine/Engine/Core/System/ThreadPool.h) / [`ObjectPool.h`](project/IrufemiEngine/Engine/Core/Utility/ObjectPool.h)

**【図解: 世代付きHandleによるダングリングポインタの防御】**
```mermaid
graph LR
    subgraph ObjectPool ["ObjectPool メモリプール"]
        Slot0["Slot 0 (Gen: 1, Active)"]
        Slot1["Slot 1 (Gen: 2, Free, next: 2)"]
        Slot2["Slot 2 (Gen: 1, Free, next: -1)"]
    end
    User["ユーザーのHandle (Index: 1, Gen: 1)"] -.->|"古い世代でアクセス"| Slot1
    Slot1 -.->|"世代アンマッチで 安全に弾く (nullptr返却)"| User
    style Slot0 fill:#2b6cb0,color:#fff
    style Slot1 fill:#c53030,color:#fff
    style Slot2 fill:#2d3748,color:#fff
```

---

### <a id="desc-graphics"></a>🔹 4. G-Bufferを活用したグラフィックス表現とGPU最適化
> **【要約】G-Bufferへの直接アクセス、GPU Culling、および Bindless Resources への対応**

- **【課題・実現したかったこと】**
  単なる3Dモデルではなく手描きコミックのようなリッチな線画表現や、内部で光が散乱する「煙の厚み」を描画したかった。また、数万のパーティクルや背景を描画する際のCPU側のカリング負荷・テクスチャバインド負荷を大幅に削減したかった。
- **【比較と技術選定 (トレードオフ)】**
  単一の輪郭線抽出ではなく、「深度」「法線」「輝度」の3つの G-Buffer から複合的にエッジを抽出するマルチレイヤー・アウトラインシェーダーを独自に実装。煙については、HLSLによるレイマーチングを実装し、「境界球（Bounding Sphere）」との交差判定を数学的に事前計算してサンプリング区間を最小限に絞り込んだ。
  最適化面では、**Compute Shader による GPU フラスタムカリング** を導入し `ExecuteIndirect` で一括描画。さらに、テクスチャバインドのオーバーヘッドをゼロにするため、**Bindless Resources (Descriptor Indexing)** への移行を達成した。
- **【結果と定量的成果】**
  自作エンジンだからこそ可能な G-Buffer への直接アクセスを活かし、3D空間ノイズ（FBM等）を用いた濃密な爆煙の質感と、コミックライクなマルチレイヤー・アウトラインを実現。Compute Shader によるボクセル粉砕演出と連動し、敵機体を撃破した瞬間の「重厚な破壊感」を視覚的に最大化。同時に、数万のオブジェクトが描画されてもCPU側にカリング負荷・バインド負荷を大きく抑えたパイプラインを確立した。
- 🔗 **関連シェーダー**: [`ParticleGPU.PS.hlsl`](project/IrufemiEngine/EngineResources/shaders/ParticleGPU.PS.hlsl) / [`DepthBasedOutline.PS.hlsl`](project/IrufemiEngine/EngineResources/shaders/DepthBasedOutline.PS.hlsl)

---

### <a id="desc-manual"></a>🔹 5. 開発効率化を支えるエディタツールとドキュメンテーション
> **【要約】Facadeパターン、リフレクションUI、Prefab分離編集、および開発ガイドライン**

- **【課題・実現したかったこと】**
  ゲームを実行したままパラメータをリアルタイム調整できる環境を整え、イテレーション速度を高めること。また、多数のオブジェクトが配置されたシーン上ではなく、敵や弾などのアセット単体を独立したクリーンな空間で調整・確認できる編集ワークフローを確立すること。
- **【比較と技術選定 (トレードオフ)】**
  C++側で変数を登録するだけで自動的にImGuiのインスペクタUIが生成・JSON保存されるリフレクション基盤を提供。また、パーティクル生成や描画準備をカプセル化した直感的なAPI設計を導入した。
  さらに、**「Prefab分離編集モード (Isolation Stage)」** を独自実装。原点 (0, 0, 0) XZ平面グリッドの自動描画、パンくずリストによる階層インジケータ、編集中のPlayモード移行防止、突入・離脱時のUndo履歴クリアを導入し、シーンから完全に独立した状態でアセットを安全にオーサリングできる環境を整備した。
  暗黙の前提によるバグを防ぐため、メモリ管理やエディタの操作ルールをまとめた **Manual (取扱説明書)** を作成・維持している。
- **【結果と定量的成果】**
  再コンパイルの待ち時間が減少し、ゲームバランス調整や演出作成のイテレーション速度が向上。2万文字規模（2100行以上）のマニュアルを執筆・整備し、将来の拡張や機能追加にも迷わず取り組める開発基盤を構築できた。
- 📄 **関連ドキュメント原本**: [`Manual.md`](Manual.md)

---

### <a id="desc-dx-error"></a>🔹 6. DirectX 12 エラーハンドリングとビルド別フェイルセーフ
> **【要約】`[[nodiscard]]` による静的型安全性と、用途に合わせたビルド別エラー処理**

- **【課題・実現したかったこと】**
  DirectX 12 における `HRESULT` エラーは、不適切なハンドリングにより即座に「Direct3D デバイスロスト」や無言のアプリケーション強制終了を引き起こす。開発中に即座にブレークしたい要求と、エディタ編集中に「未保存データを保護してクラッシュを防ぎたい」要求、製品版で「画面フリーズせず安全に終了したい」要求を用途に応じて両立させる必要があった。
- **【比較と技術選定 (トレードオフ)】**
  単純な `assert()` や共通の例外スローを廃止し、回復可能なエラー用の **`IRUFEMI_CHECK_HR`** と致命的エラー用の **`IRUFEMI_ASSERT_HR`** に明確に二分。
  さらに `IRUFEMI_CHECK_HR` に C++20 の **`[[nodiscard]]` 属性を付与** し、呼出元が成否を判定（Early Return / リカバリ）していないコードをコンパイル時に検出する静的検証を導入した。
  ビルド構成（Debug / Editor / Development / Release）ごとに、プログラマ向け即時ブレーク・エディタUI保護・展示向けログ記録・製品版クリーン終了を動的に切り替える耐障害性マトリクスを構築した。
- **【結果と定量的成果】**
  非致命的なGPUエラー発生時でもエディタ本体をクラッシュさせずに未保存シーンや編集データを保護し、コンパイル時検査により実行時クラッシュの見落としを未然に防止できる堅牢な運用体制を整えた。
- 🔗 **関連コード原本**: [`ErrorUtility.h`](project/IrufemiEngine/Core/Utility/ErrorUtility.h) / [`ErrorUtility.cpp`](project/IrufemiEngine/Core/Utility/ErrorUtility.cpp)

---

### <a id="desc-rendergraph"></a>🔹 7. RenderGraph による自動リソースバリアと描画パイプラインの最適化
> **【要約】パス宣言型 RenderGraph による手動バリアの解消と、動的パスカリングによるメモリ・帯域の最適化**

- **【課題・実現したかったこと】**
  ポストプロセスやシャドウ、G-Buffer等の複数パス間において、DirectX 12 のリソースバリア（`TransitionBarrier`）を手動管理すると、状態の不整合や指定漏れによる描画乱れ、冗長なバリア発行による描画遅延の原因となる。また、特定のエフェクトが無効なフレームでも不要なテクスチャコピーやパス処理が実行される負荷を解消したかった。
- **【比較と技術選定 (トレードオフ)】**
  各パスが個別にバリアを発行する従来方式を改め、パスが必要とする入出力リソースの状態（`SRV` / `RTV` / `COPY_DEST` 等）を事前宣言する **RenderGraph** を構築。
  ポストプロセスの各エフェクト（Bloomの抽出・ブラー・合成等）も単一のパスにまとめず、内部ステップを個別のRenderPassとしてRenderGraphへ展開。
  フレーム実行時にリソースの直前状態と要求状態を追跡し、ステート遷移が必要な境界でのみ最小限のバリアを自動計算・一括発行するアーキテクチャを採用した。
  また、有効な描画要求に応じて毎フレーム動的にパスを登録・カリング（除外）する仕組みと、一時テクスチャ（Transient Resource）のヒープエイリアシングによるメモリ再利用を統合した。
- **【結果と定量的成果】**
  ポストプロセス各エフェクトや描画パス内部から手動バリアを解消し、バリア競合による不具合リスクを低減。エフェクトOFF時には不要なテクスチャコピーやバリア発行をスキップすることで、無駄な帯域消費と処理負荷を削減した。
- 🔗 **関連コード原本**: [`RenderGraph.h`](project/IrufemiEngine/Renderer/Pipeline/RenderGraph/RenderGraph.h) / [`PostProcessRunner.h`](project/IrufemiEngine/Renderer/PostProcess/PostProcessRunner.h)

---

## 🚀 開発ロードマップ & 実装機能一覧

本エンジンで実証・実装された主な機能群です。

- [x] **入力システム (Enhanced Input)**: JSONデータ駆動の論理アクション、厳格修飾子排他 (Strict Modifier Matching)、入力消費 (`ConsumeKey`)
- [x] **ウィンドウ & キオスク制御**: `F11` / `Alt+Enter` エンジンコア集約トグル、低レベルフックによるWindowsキー無効化と3重の安全脱出導線
- [x] **PSOキャッシュ & 並列PreWarm**: 全CPUコアによる事前マルチスレッド並列JITコンパイル（初回到達時スタッター完全ゼロ化）
- [x] **DirectX 12 描画基礎**: パイプライン、シェーダバインド、定数バッファ管理
- [x] **RenderGraph 描画パイプライン**: リソースステート追跡によるバリア自動計算・一括発行、動的パスカリング、一時テクスチャのメモリ再利用 (Transient Resource)
- [x] **DirectX 12 エラー耐性**: `[[nodiscard]]` 静的検査付き `IRUFEMI_CHECK_HR` / `IRUFEMI_ASSERT_HR` と4大ビルド構成連動フェイルセーフ
- [x] **アーキテクチャ最適化**: Bindless Resources (Descriptor Indexing) への対応
- [x] **メモリ管理**: 世代(Generation)付きHandleによる安全なObjectPool、スタックアロケータ、遅延削除(Pending Kill)キュー
- [x] **非同期・マルチスレッド**: 自作 `ThreadPool` による非同期ローダー（Time-Slicing化）、非同期レイキャスト `RaycastAsync`
- [x] **衝突最適化**: Dynamic BVH (O(N log N)) 空間分割、データ指向の密配列キャッシュ (Sparse Set)
- [x] **グラフィックス強化**: G-Buffer (MRT) ベースの高品質マルチレイヤーアウトライン、GPU Skinning、GPU フラスタムカリング (`ExecuteIndirect`)、Compute Shader パーティクル
- [x] **デバッグ・開発ツール**: ImGui の統合（3カラムインスペクター）、リフレクションによる自動UI生成、Prefab (JSON) の動的ロード、Prefab分離編集モード (原点グリッド / Undo履歴分離 / 誤操作防止ガード)

---

## 📖 API利用例 (API Usage)

直感的に利用でき、内部の処理を安全にカプセル化したAPIの設計例です。

### 1. リフレクションによるエディタUIの自動生成
変数を登録するだけで、ImGuiのインスペクタにUIが自動生成され、実行中のパラメータ変更とJSON保存が可能になります。

```cpp
#include "Framework/Component/Component.h"

class PlayerStatusComponent : public Component {
public:
    int hp_ = 100;
    float speed_ = 5.0f;

    std::string GetComponentName() const override { return "PlayerStatusComponent"; }

    void OnRegisterProperties() override {
        // これだけで Inspector に UI が自動生成＆ JSON に連動
        RegisterProperty("Max HP", &hp_);
        RegisterProperty("Move Speed", &speed_);
    }
};
```

### 2. スレッドプールを用いた非同期レイキャスト (Async Raycast)
メインスレッドを停止させない分散処理（Time-Slicing）による当たり判定の例です。

```cpp
// 毎フレーム判定するのではなく、一定時間ごとに非同期でRaycastを発行
if (timeSinceLastCheck > 0.1f) {
    auto* collisionManager = engine_->GetCollisionManager();
    // 非同期でレイキャストを発行し、結果を future として受け取る
    raycastFuture_ = collisionManager->RaycastAsync(engine_->GetThreadPool(), ray, maxDistance, layerMask);
    timeSinceLastCheck = 0.0f;
}

// 別のフレームで、判定が完了しているかチェックして結果を受け取る (ノンブロッキング)
if (raycastFuture_.valid() && raycastFuture_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
    auto [isHit, hitInfo] = raycastFuture_.get();
    if (isHit) {
        // ヒットした場合の処理
    }
}
```

---

## 📁 プロジェクト構成 (Project Structure)

本ソリューション (`Irufemi.slnx`) は、エンジンコアとアプリケーション（ゲームロジック）、およびツール群を明確に分離（関心の分離）した以下の4プロジェクトで構成されています。

| プロジェクト | 種別 | 役割 |
| :--- | :--- | :--- |
| **IrufemiEngine** | 静的ライブラリ (.lib) | 描画・物理・リソース・コンポーネント基盤を提供するエンジンコア |
| **IrufemiEditor** | 静的ライブラリ (.lib) | ImGuiベースのレベルエディタ・デバッグツール群 |
| **Application_solo** | 実行ファイル (.exe) | 個人制作ゲームのロジック・固有シーン・アセンブリ |
| **Application_team** | 実行ファイル (.exe) | チーム制作ゲームのロジック・固有シーン・アセンブリ |

### ⚙️ IrufemiEngine (`project/IrufemiEngine/`)
エンジンのコアモジュール群です。特定のゲームに依存する処理は一切含みません。

| ディレクトリ | 役割 |
| :--- | :--- |
| **Engine/** | DirectX12ラッパー、ウィンドウ管理、スレッドプール、各種Manager群 |
| **Renderer/** | RenderGraph、G-Buffer、Bindless Resources パイプライン |
| **Resource/** | Handleシステムを用いた非同期テクスチャ・モデル・オーディオ管理 |
| **Framework/** | Componentを用いた高速なECS基盤、Dynamic BVH 衝突判定 |

### 🎮 Application (`project/Application_solo/` 等)
ゲーム固有のロジックとリソースを格納します。

| ディレクトリ | 役割 |
| :--- | :--- |
| **components/** | ゲーム固有の振る舞い (DebrisManager, Boss 等) |
| **scene/** | 各シーンの初期化と状態管理 |
| **resources/** | このゲーム専用のテクスチャ、モデル、JSON等のアセット群 |

### 🛠️ Tools & External Editors (`tools/`)
ゲーム制作イテレーションを高速化する外部ツール群です。
- **`tools/BlenderLevelEditor/`**: Blenderを3Dレベルエディタとして拡張する公式アドオン群。
  - レールシューターのスプライン軌道編集、敵配置オーサリング、JSONエクスポートに対応。
  - 環境非依存の多層自動検出ランチャー（`Launch_BlenderLevelEditor.bat`、`Sync_BlenderAddon.bat`）および説明書PDF（`docs/`）を完備。

### 💾 派生データとアセットの3層分離設計
Unreal EngineのDDC（Derived Data Cache）思想を取り入れ、リポジトリの健全性を保つ3層分離を徹底しています。
- **`asset_src/` (DCC原本層)**: `.blend` 元データ、原本フォント、高解像度テクスチャ（ランタイムには含めない）。
- **`project/Application_solo/resources/` (ランタイム層)**: ゲーム実行に必要な最適化済みアセットのみを格納（未参照ファイルの混入ゼロ）。
- **`generated/cache/` (派生データ・キャッシュ層)**: 高速パース済みモデル（`.model.ibin`）やDirectX12 PSOキャッシュ（`*.pso`）をアセット領域から完全隔離。

---

## 🛠️ 開発効率化を支える自作ツール・エコシステム

エンジン単体にとどまらず、ゲーム開発・プロファイリングのイテレーションを加速させるための独立ツール群を開発・統合しています。

| ツール / エディタ | 技術スタック / プロトコル | 特徴と設計のこだわり |
| :--- | :--- | :--- |
| **双方向テレメトリ ＆ リモートコントロール (`TelemetryMonitor`)** | C# / WPF (.NET 10)<br>**UDP双方向通信 (受信: 8888 / 送信: 8889)** | **【ゲームループ無負荷 ＆ リアルタイム遠隔介入】** プロファイリング自体がゲームの動作を重くしては本末転倒であるため、軽量なUDP通信を採用。エンジン側は専用の独立ワーカースレッドから非同期送信することで、**ゲーム本体のメインループ・フレームレートに一切の負荷を与えない監視環境（Releaseビルド対応）**を実現。さらにツール下部のリモートパネルから `[💥 10,000 Debris]` などのコマンドを送信し、ゲーム画面の操作を邪魔することなく外部からリアルタイムに負荷実験を行える双方向司令塔として設計。ゲーム内でも `F1` キーで開発者コンソールを開いて同様の操作が可能。 |
| **インハウス・ゲームエンジンエディタ (`IrufemiEditor`)** | C++ / DirectX 12 / ImGui | Commandパターンによる**Undo/Redo**、ドラッグ＆ドロップによる階層移動、3Dシーンビュー、階層ツリー、プロジェクトブラウザを完備。ボス行動ステートやスプラインウェイポイントを視覚的に編集できるカスタムインスペクター拡張基盤を搭載。**Prefab分離編集モード (Isolation Stage)** により、シーンから隔離されたクリーンな空間で原点グリッドや階層インジケータを参照しながら単体アセットの調整が可能。 |

---

## 🛠️ 自動ビルド＆パッケージングパイプライン

ビルドやアセット整理、提出用パッケージ作成を効率化するスクリプト群をリポジトリルートに配備しています。

| ツール / スクリプト | 役割・特徴 |
| :--- | :--- |
| **`CompileShaders.bat`** | 全77種以上のHLSLシェーダーを `/O3` 最適化で先行オフラインコンパイル。審査員環境でのDirectXランタイムDLL依存クラッシュを根本から防止します。 |
| **`CleanAssets.bat`** | `.gitignore` に準拠し、`generated/cache/` や `logs/` などの一時生成物を安全に一括消去するアセット＆ワークスペース健全化ツール。 |
| **`scripts/PackageForSubmission.ps1`** | 作品提出規定に準拠したパッケージ自動生成スクリプト。「ゲーム本編（製品版）」「自作エディタ環境（Editor版）」「クリーンソースコード」「提出資料・動画」を1つの審査提出用ZIPへ全自動で集約します。 |

---

## 🎨 アセットパイプライン

### 1. 3Dモデルのエクスポート
- **ルール**: Blender等のツールでは **デフォルト設定（Y-up / 右手座標系）** でエクスポートしてください。
- **処理**: エンジン内部（Assimp読み込み時）でDirectX用の左手座標系へ自動変換されます。

### 2. テクスチャ命名規則 (Linear Workflow)
リニアワークフローを正確に行うため、ファイル名による自動判別を行っています。
- **数値データ (Linear)**: 接尾辞 `_n`, `_ao`, `_m`, `_r` を含めることでガンマ補正をスキップします。
- **カラーデータ (sRGB)**: 上記以外はすべて色として扱われ、自動的にリニアライズされます。

---

## ⚙️ 動作環境・計測環境

- **OS**: Windows 10 / 11
- **IDE**: Visual Studio 2022 (v17.10以降) / Visual Studio 2026
- **SDK**: Windows SDK 10.0.26100.7175 以上推奨
- **計測環境**: 
  - CPU: Intel Core i7 / AMD Ryzen 7 相当以上
  - GPU: NVIDIA RTX 2060 相当以上を想定

**ビルド手順**:
1. リポジトリをクローン後、Visual Studio 2022 (v17.10以降) または VS2026 で `Irufemi.slnx` を開きます。
2. 構成を選択しビルドを実行します。
  - `Debug` / `Development`: エンジン実行時に動的コンパイルされ、迅速なイテレーションが可能。
  - `Editor`: 自作エンジン内蔵エディタ（Prefab分離編集モード・インスペクタ・ギズモ操作）が有効化された開発モード。
  - `Release`: Visual Studio の PreBuild イベントで最高レベルの最適化 (`/O3`) を適用したオフラインコンパイルが行われます。

---

## 📚 使用ライブラリ

- [Assimp](https://github.com/assimp/assimp)
- [Dear ImGui](https://github.com/ocornut/imgui)
- [DirectX 12 Agility SDK](https://devblogs.microsoft.com/directx/directx12agility/)

---

## 📄 ライセンス

このエンジンのソースコードは [MIT License](LICENSE.txt) の下で提供されています。各ライブラリのライセンスについては個別の規定に従ってください。
