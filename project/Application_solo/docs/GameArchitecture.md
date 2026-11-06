# Application (Rail Shooter) 固有アーキテクチャ仕様書

本書は、`project/Application_solo/` におけるレールシューター＆重力アクションゲームの固有ロジック・ステートマシン・ゲームプレイ連携仕様をまとめた開発資料です。（※汎用エンジン機能については `docs/Manual.md` を参照してください）

---

## 1. レール移動＆相対オフセット制御 (SplineFollowerComponent, RailRelativeFollowerComponent)

本ゲームは、古典的な奥スクロールレールシューター（スターフォックス、パンツァードラグーン等）の体験を最新の Direct3D 12 基盤で実現するため、**「レール前進」と「画面内相対移動」の完全な階層分離** を採用しています。

### 1. 親子階層アーキテクチャ (`PlayerCart` -> `Player`)
- **`PlayerCart` (親オブジェクト)**:
  - `SplineFollowerComponent` をアタッチ。ステージ空間に敷設されたスプライン曲線に沿って前進・旋回し、自機全体の基準ワールド座標系を提供します。
- **`Player` (子オブジェクト)**:
  - `RailRelativeFollowerComponent` と `GravityPlayerComponent` をアタッチ。カートのローカル空間において、スティック/WASD入力によるXYオフセット移動を行います。

```
[World Root]
  └── PlayerCart (SplineFollowerComponent: レール前進・旋回)
        └── Player (RailRelativeFollowerComponent: XY相対移動・バンク・振動)
```

### 2. 慣性ベース補間と姿勢制御
- **加速度と摩擦**: 入力ベクトルに対する即値移動ではなく、`Acceleration` と `Friction` による滑らかな速度補間を行い、宇宙船特有の慣性を表現します。
- **バンキング（ロール回転）**: 水平移動速度に応じて機体を最大35度ロール回転させ、重力航行のダイナミクスを強調します。
- **常時ホバリング振動**: 静止時でもサイン波を用いた微小な上下浮遊を行い、生きている機体の質感を演出します。
- **移動可能範囲クランプ**: 画面外への突出を防ぐため、カメラ視野角に収まる楕円/矩形境界で座標を安全にクランプします。

---

## 2. マルチロックオン＆ターゲティング (PlayerTargetingComponent, TargetableComponent)

プレイヤーの照準操作と連動し、複数を同時にロックオンして一斉射出するための直感的なマーキング・マルチロックオンシステムです。

### 1. データ駆動なターゲット管理 (`TargetableComponent`)
毎フレーム全 `GameObject` を再帰探索する重い処理を廃止し、ターゲット可能な全エネミーやボスコアに `TargetableComponent` をアタッチします。
- コンポーネントの `OnEnable()` / `OnDisable()` に連動して共有レジストリへ自動登録・解除されるため、定数時間 $O(1)$ でのアクティブターゲット走査が可能です。

### 2. 直感的マーキングと多重ロック
- **直感的マーキング**: 右クリック（またはパッド用ボタン）を押している間、レティクル近傍のターゲットをキューへ高速登録します。
- **多重ロックオン**: 同一の敵・コアに対して、撃ち込む弾数として複数のロックオンを重ね掛けできます。
- **2D+3D深度スコアリング**: スクリーン中心距離と3D深度の重み付け評価により、プレイヤーが狙いたい敵を直感的に選び出します。
- **遮蔽物レイキャスト判定**: 壁や岩の裏に隠れている敵に対しては、`CollisionManager::Raycast` により視界が通っているかを判定し、誤ロックを防止します。
- **サテライトUIアニメーション (`LockonMarkerUIComponent`)**: 多重ロック数に応じて、ターゲット枠の周囲を衛星のように旋回・展開するUIアニメーションを描画します。

---

## 3. 重力アクション＆ステートマシン (GravityPlayerComponent, DebrisManagerComponent)

周囲のデブリを重力で引き寄せ、保持し、ロックオン対象へ撃ち出す本作のコアアクションです。

### 1. 自機アクションステートマシン
- **State 1: [引き寄せ (Pull)]**:
  - ボタン長押しトリガーにより、一定半径内の浮遊デブリを検知して自機周囲へ求心移動させます。
  - ロックオン中またはエイム中のボスのシールド破片を優先して引き剥がす「シールド優先吸引」機能を備えています。
- **State 2: [自機周囲で保持 (Hold)]**:
  - 自機ローカル座標系において、`sin/cos` による有機的な浮遊アニメーションを行いながらデブリを追従保持します（軌道スロット管理による安全な配置）。
- **State 3: [一斉投擲 (Throw)]**:
  - ロックオンキューに積まれた対象へ向けて、デブリをホーミング弾道で高速射出します。
  - キューが空（ノーロック）の場合は、レティクルの照準点へ向けて直進弾として射出します。

### 2. ライフサイクルプール (AAA基準ライフサイクル管理)
- デブリの生成・破棄によるヒープ断片化とGCスパイクを防ぐため、`DebrisManagerComponent` がリングバッファ型のオブジェクトプールを事前確保し、高速にアクティブ化/非アクティブ化を行います。

---

## 4. ボス戦・エネミーAIシステム (BossComponent, EnemySpawnerComponent)

### 1. ボスステートマシン (`BossComponent`)
- **多層シールド**: ボス本体を護る複数の浮遊シールド。自機の重力アクションで1つずつ引き剥がすことが可能。
- **ドローン編隊展開 (`DroneManagerComponent`)**: 自機周囲を旋回する小型攻撃ドローンの生成・突撃指示。
- **弱点コア露出と重力特異点攻撃**: シールド全破壊時の弱点コア露出、およびステージ全域を巻き込む重力ディストーション攻撃。
- **部位破壊連動爆発**: ボスHPゼロ時、パーツごとの順次爆破、スローモーション演出、およびボクセル破片爆散。

### 2. プレイヒッチを排除する事前ウォームアップ
- `EnemySpawnerComponent::Start()` において、ステージに登場する全敵種（ザコ、突撃ドローン、スナイパー艦艇等）のGPUバッチ描画とプレハブプールをシーン開始前に事前確保（Prewarm）します。
- `VoxelParticleManager::ReservePool()` により、ボクセル頂点バッファやGPUコンピュートバッファをローディング中に先行確定し、戦闘中のDirectX12フレームドロップを徹底排除します。

---

## 5. ゲームループと演出連携 (GameLoopManagerComponent, StageIntroDirectorComponent)

### 1. ステージ開始演出 (`StageIntroDirectorComponent`)
- ステージ開始時、自機が宇宙空間の「臨界境界面（重力の壁）」を突破するシネマティック演出を行います。
- 境界面突破の瞬間に `PostProcessManager::TriggerGravityDistortion` を飛ばし、空間全体の強力な波打ち歪みと色収差で突入の衝撃を演出します。

### 2. プレイヤー死亡シークエンス (`PlayerDamageVisualizerComponent`)
- **被弾演出**: 残りHPに応じた赤色ビネットの明滅とノイズグリッチ（`PostProcessMode::Glitch`）。
- **重力スーパーノヴァ死亡シークエンス**:
  1. 自機HPがゼロになった瞬間、自機のスクリーンUV座標をロック。
  2. `PostProcessMode::BlackHoleSupernova` が起動し、周辺の地平線と背景空間が自機へ重力レンズにより吸い込まれる。
  3. クライマックスで超新星プラズマ爆発と重力衝撃波が画面端まで拡大。
  4. 画面を覆う瞬間に全画面ホワイトアウト（透明度1.0固定）へ移行し、そのままシームレスに `ResultScene` へ遷移します。

---

## 6. データ駆動サウンド再生基盤 (SoundHelper & sound_actions.json)
- **SoundHelper**: `#include "Audio/SoundHelper.h"`
- **GameSoundAction**: `#include "Audio/GameSoundAction.h"`
- **設定ファイル**: `resources/config/sound_actions.json`

各コンポーネントやシーン内で音声ファイルのパス文字列（例: `"resources/audio/se/shot.wav"`）をハードコードすることは禁止されています。
キーバインド設計（`input_actions.json`）と同様に、サウンドもデータドリブンな一元管理基盤（`SoundHelper`）を使用してください。

1. **サウンドの登録 (`sound_actions.json`)**:
   `GameSoundAction.h` の `enum class GameSE`, `GameBGM` と対になるキー（文字列）で設定を記述します。
   ```json
   {
     "se": {
       "PlayerShoot": { "path": "resources/audio/se/shot.wav", "volume": 0.8, "pitch": 1.0 },
       "Explosion": { "path": "resources/audio/se/explosion.wav", "volume": 1.0 }
     }
   }
   ```

2. **再生コード (C++)**:
   ```cpp
   #include "Audio/SoundHelper.h"

   // ワンショット再生 (音量・ピッチ・実在ファイルは自動解決)
   SoundHelper::Play(GameSE::PlayerShoot);

   // 音量を一時的にオーバーライドして再生
   SoundHelper::Play(GameSE::Explosion, 0.5f);
   ```

3. **事前ロード (ヒッチ防止)**:
   ゲーム起動時やシーン初期化時に `SoundHelper::PreloadAll()` を呼び出すことで、全サウンドファイルがメモリキャッシュされ、初弾発射時などのフレームレート低下（ヒッチ）を完全に防ぎます。
