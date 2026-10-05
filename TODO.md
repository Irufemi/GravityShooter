# TODO & 今後の実装ロードマップ

本ドキュメントは、プロジェクトの設計仕様、数式モデル、将来的な実装タスクを外部資料を参照することなく実装可能なレベルで集約・管理するものです。

---

## 1. ステージ突入・戦闘開始シークエンス（B案：ブートアップ＆整流演出）

### 概要
タイトル出撃（`TitleSceneDirectorComponent`）からゲームシーン（`StageIntroDirectorComponent`）への突入時、自機がカメラ後方から飛来して到着するモーメント（`Arrival` $\to$ `Active`）において、機体制御の整流とFCS（火器管制システム）起動の臨場感を高める演出。

### 現在の状態（フック配備済み）
- A案適用済み: 誤解を招くメニューカーソル選択音（`se_menu_cursor`）を撤去。
- `StageIntroDirectorComponent` 内に以下のフック関数を配備済み:
  - `ApplyArrivalInertia(float progress)`
  - `TriggerFCSBootupSequence()`

---

### 詳細設計仕様と数式モデル

#### (1) 急制動時のノーズダイブ・整流バネ挙動（`ApplyArrivalInertia`）
自機が超高速（Z: -25m）から急減速して定位置（Z: 0m）に停止した瞬間、慣性力によって機首が下がり、サスペンションのようにバウンドして水平へ収束する減衰振動（Damped Harmonic Oscillation）を適用する。

- **減衰振動の計算式**:
  $$\theta_{pitch}(t) = \theta_{max} \cdot e^{-\lambda t} \cdot \cos(\omega t)$$
  - $\theta_{max}$: 最大ピッチ傾斜角（推奨値: 約 $3.0^\circ \sim 5.0^\circ \approx 0.05 \sim 0.08$ rad）
  - $\lambda$: 減衰係数（Damping Factor: 推奨値 $6.0 \sim 8.0$、約0.5秒で収束）
  - $\omega$: 固有角振動数（Natural Frequency: 推奨値 $20.0 \sim 25.0$ rad/s、約1.5〜2周期の揺れ戻し）
  - $t$: `progress \times arrivalDuration_`（秒）

- **C++実装スニペット（予定）**:
  ```cpp
  void StageIntroDirectorComponent::ApplyArrivalInertia(float progress) {
      if (auto ship = shipObj_.lock()) {
          if (auto transform = ship->GetTransform()) {
              float t = progress * arrivalDuration_;
              const float maxPitch = -0.07f; // ノーズダイブ方向（ラジアン）
              const float damping = 7.0f;
              const float omega = 22.0f;
              float pitchOffset = maxPitch * std::exp(-damping * t) * std::cos(omega * t);
              
              Irufemi::Vector3 rot = initialShipLocalRot_;
              rot.x += pitchOffset;
              transform->SetRotation(rot);
          }
      }
  }
  ```

#### (2) FCSブートアップ演出（`TriggerFCSBootupSequence`）
単なる汎用UI音ではなく、機体システムが戦闘モードに移行したことを示す音響とUIアニメーション。

1. **専用音響アセットの要件**:
   - アセット名例: `resources/audio/SE/se_fcs_online.mp3`（または `.wav`）
   - 音響特性: サイバネティックな高周波チャイム、またはサーボの起動音（立ち上がり約0.1秒、ディケイ約0.6秒、SF感のあるピッチ急上昇チャイム）。
   - 音量: $0.7 \sim 0.85$。

2. **ReticleUIComponent（照準器）のスプリング展開アニメーション**:
   - 初期状態: 拡大（Scale 2.0x）＋透明度0.0。
   - `Active` 突入時: 0.25秒で Scale 1.0x へスプリング収縮（オーバーシュート 0.9x $\to$ 1.0x）し、同時にカラー輝度（エミッシブ）が瞬間的に最大化してから定常値へ整流。
   - レティクル内の中央レティクルと外周リングが時間差（0.05秒ディレイ）で組み上がることでメカニカルな展開感を演出。

---

## 2. 関連コンポーネント一覧
- `StageIntroDirectorComponent` ([`StageIntroDirectorComponent.h`](file:///f:/school/3_0/WP0/WP0/project/Application_solo/Level/StageIntroDirectorComponent.h) / [`.cpp`](file:///f:/school/3_0/WP0/WP0/project/Application_solo/Level/StageIntroDirectorComponent.cpp))
- `TitleSceneDirectorComponent` ([`TitleSceneDirectorComponent.h`](file:///f:/school/3_0/WP0/WP0/project/Application_solo/Scenes/title/TitleSceneDirectorComponent.h) / [`.cpp`](file:///f:/school/3_0/WP0/WP0/project/Application_solo/Scenes/title/TitleSceneDirectorComponent.cpp))
- `RailShooterPlayerComponent` ([`RailShooterPlayerComponent.h`](file:///f:/school/3_0/WP0/WP0/project/Application_solo/RailMechanics/RailShooterPlayerComponent.h) / [`.cpp`](file:///f:/school/3_0/WP0/WP0/project/Application_solo/RailMechanics/RailShooterPlayerComponent.cpp))
- `ReticleUIComponent` ([`ReticleUIComponent.h`](file:///f:/school/3_0/WP0/WP0/project/Application_solo/UI/ReticleUIComponent.h))
