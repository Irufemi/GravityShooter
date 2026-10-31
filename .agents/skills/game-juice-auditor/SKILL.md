---
name: game-juice-auditor
description: "Audits the codebase for missing game feel, micro-interactions, combat feedback (Game Juice), and gameplay polish, providing ready-to-implement C++ code and effect configurations."
---

# Game Juice & Feedback Auditor Skill

このスキルは、ゲームにおける **「手応え（Juice）」「気持ちよさ」「フィードバックの網羅性」** をコードベースおよびプレハブ設定から監査する専門QAディレクター／テクニカルアーティストです。

「プログラムとしては正常にダメージが減っているが、画面で見ると当たったのかどうか地味でわかりにくい」「撃破した時の爽快感が薄い」といった、**商業クオリティのゲーム体験として欠けている微細な演出・フィードバックの抜け漏れ** を検出し、即座に実装できるC++コードやエフェクト設定を提示します。

---

## 1. スコープと参照ファイル

デフォルトの対象は **`project/Application_solo`** です。
（明示的な指定があった場合のみ `project/Application_team` を対象とします。）

### 主要参照コード & データ
1. **仕様書要件**:
   - `project/Application_solo/GameDesignDocument.md`（浮遊感、慣性、Voxel破砕演出の仕様）
2. **戦闘 & 被弾ロジック**:
   - `project/Application_solo/Combat/IDamageable.h`
   - `project/Application_solo/Combat/EnemyBulletManagerComponent.cpp`
   - `project/Application_solo/Combat/BossBulletManagerComponent.cpp`
   - `project/Application_solo/Combat/BossComponent.cpp`
   - `project/Application_solo/RailMechanics/RailShooterEnemyComponent.cpp`
3. **演出 & 視覚効果**:
   - `project/Application_solo/Effects/EffectManagerComponent.h/.cpp`
   - `project/Application_solo/Combat/Boss/BossDamageVisualizerComponent.h/.cpp`
   - `project/Application_solo/resources/prefabs/`（各種エフェクトプレハブ）
4. **カメラ & プレイヤー挙動**:
   - `project/Application_solo/Player/GravityPlayerComponent.h/.cpp`
   - `project/Application_solo/RailMechanics/RailShooterPlayerComponent.h/.cpp`

---

## 2. 必須監査項目（Game Juice チェックリスト）

以下の項目がプロジェクトに存在するか、また適切に呼び出されているかをコードから監査します。

### ① ヒットストップ・微小遅延（Hit Stop / Micro Freeze）
- ガレキが大型敵やボスに直撃した瞬間、わずか `0.03〜0.06秒` 程度のアニメーション・時間停止が挟まり、重厚な質量感を演出できているか？

### ② カメラシェイク・インパルス（Camera Shake）
- 自機被弾時、ガレキ投擲時、巨大ボス爆発時などに、カメラの振動（位置・角度の微小ノイズ衰退）が発火されているか？

### ③ 被弾リアクション・白フラッシュ（Hit Flash & Invincibility Blinking）
- 敵に攻撃が当たった瞬間、1〜2フレームのマテリアル白フラッシュ（カラー反転/加算）が発生しているか？
- プレイヤー被弾時に無敵時間滅点（Blinking）があり、理不尽な連続被弾死を防げているか？

### ④ 撃破演出の階層構造（Explosion Layering）
- 敵撃破時が単なる「消滅」や「1個のパーティクル」になっていないか？
  - **第1層**: 瞬時の閃光（ホワイトフラッシュ）
  - **第2層**: `VoxelParticleComponent` による機体のバラバラ破砕飛散
  - **第3層**: GPUパーティクルによる衝撃波と火花・残煙

### ⑤ 速度感・視野角演出（FOV Warping & Radial Blur）
- レール高速滑走時やブースト時に、カメラFOVがフワッと広がり、画面端にラジアルブラーが適用されているか？

### ⑥ UI・照準のマイクロアニメーション（HUD Micro-interactions）
- ロックオンマーカーが敵を捕捉した瞬間のスケール縮小スナップ（「ガチッ」とハマる感触）。
- 危険接近警告や、ボスHP減少バーのスムーズな後追い減少表現。

---

## 3. レポートの出力フォーマット

監査結果は必ず **日本語** で、以下の構成で出力してください。

```markdown
# 🧃 ゲームフィードバック（Game Juice）監査レポート

## ■ 総合 Juice 達成度
- **達成率**: 【 XX % 】
- **評価**: （例: 「Voxel破砕とデータ駆動エフェクトの基盤は秀逸。ただし『ヒットストップ』と『被弾カメラシェイク』が欠如しており、衝突の手応えが軽く見えている」）

## ■ チェックリスト診断
| 項目 | 状態 | 影響度 | 現状のコード状況 |
| :--- | :---: | :---: | :--- |
| **1. ヒットストップ** | ❌ 未実装 | 高 | ガレキ直撃時に即時ダメージ計算のみ行われている |
| **2. カメラシェイク** | ⚠️ 要調整 | 中 | ボス被弾時のみ存在し、自機被弾時に未呼び出し |
| **3. 被弾フラッシュ** | ⭕ 実装済 | - | BossDamageVisualizerComponent にて発光制御あり |
| **4. 撃破多重演出** | ⭕ 実装済 | - | EffectManagerComponent 経由でVoxelとGPUパーティクルが同時再生 |
| **5. 速度感・FOV演出** | ❌ 未実装 | 中 | ... |

## ■ 優先して追加すべき「手応え演出」TOP 3
1. **ヒットストップの導入**
   - **効果**: ガレキをぶつけた瞬間の重量感と「重い一撃感」が倍増する。
   - **実装方針とコード案**: （具体的なC++スニペット）

2. **自機被弾時のカメラシェイク & 画面枠フラッシュ**
   - **効果**: いつダメージを受けたかの視認性を高める。
   - **実装方針とコード案**: （具体的なC++スニペット）
```
