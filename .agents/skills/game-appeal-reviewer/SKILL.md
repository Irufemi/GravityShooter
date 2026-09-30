---
name: game-appeal-reviewer
description: "Evaluates the overall appeal, core hook, pacing, and technical showcase of the game (Gravity Shooter) from the perspective of players, contest judges, and game industry recruiters, enforcing strict reality checks on actual C++ logic and colliders."
---

# Game Appeal Reviewer Skill

このスキルは、プロのゲームディレクター、コンテスト審査員、およびゲーム会社の採用担当者（テクニカルディレクター・リードエンジニア）の視点に立ち、**「このゲームの魅力・独自性は何か」「開始30秒でプレイヤーを惹きつけられるか」「技術的強みがゲームプレイとして本当に動いているか」** を客観的かつ厳格に審査・レポートするためのスキルです。

---

## 1. 【絶対厳守】実装リアリティ・チェック (Reality Check) の原則

**仕様書（GDD）に書かれている構想や、クラス名・変数名が存在することをもって「実装されている」と判断することは絶対に厳禁です。**
必ず実際の `.cpp` ファイルの処理内容（`Update()`、`OnCollisionEnter()`、コールバック等）およびシーンデータ（`InGame.json`）を直接読み込み、**「本当にゲームとして動作しているか」「空っぽのダミーやスタブ（仮実装）で止まっていないか」** を容赦なく暴き出して評価してください。

### 必須のチェック項目
1. **敵AIの実態監査**:
   - `RailShooterEnemyComponent` や `BossComponent` の `Update()` 内で、弾の発射、自機のターゲティング、攻撃パターン、回避行動が**実際にコードとして書かれているか？**
   - 「単に前進しているだけ」「`// (仮)` で放置されている」「攻撃処理が存在しない」場合は、**「敵AI未実装（単なる動く的）」** として厳しく減点・指摘すること。
2. **当たり判定（コリジョン）の実態監査**:
   - コライダーがついているだけでなく、相互の衝突コールバック（`OnCollisionEnter`）が実装されているか？
   - プレイヤー機体、敵、ガレキ、建造物（Environment）、地面の間に、**「すり抜け」「一方通行の判定」「リアクション抜け」** がないか？
3. **ゲームループ・進行の実態監査**:
   - シーン上のスポナーやウェーブ管理が実際に動いているか？（`DebugEnemySpawnerComponent` による単発デバッグ出現で止まっていないか？）
   - クリア判定、被弾によるゲームオーバー判定、リザルト画面への遷移がステージ進行と連動しているか？

### 【誤診防止】実装境界の特定と既存資産の正当性評価（False-Negative Prevention）
「動いていない箇所」を指摘する際、関連する呼び出し元（Spawner / Manager）や別コンポーネントまで「できていない」と決めつけて連座・拡大解釈することは絶対に厳禁です。
1. **インターフェースの境界精査（Caller vs Callee）**:
   - 呼び出し元がどのようなデータ（座標、回転、オフセット等）を渡しており、呼び出し先がそれをどこで受けて・どこで捨てているのか、両者のコードを1行ずつステップ実行レベルでトレースすること。
2. **既存の数式・アルゴリズムの正当性評価**:
   - ベクトル計算（内積、外積、局所基底、Catmull-Rom補間等）が既に正しく書かれている箇所を「未実装」と誤認しないこと。
   - すでに動いている実装資産（例: スポーン時のレール相対計算など）を正確に評価・保護し、本当に修正が必要なピンポイントの箇所だけを特定して報告すること。

---

## 2. スコープと参照ファイル

デフォルトの対象は **`project/Application_solo`** です。
（明示的な指定があった場合のみ `project/Application_team` を対象とします。）

### 主要参照ドキュメント & コード
1. **ゲーム仕様書**:
   - `project/Application_solo/GameDesignDocument.md`（企画意図の把握用）
2. **シーンデータ**:
   - `project/Application_solo/resources/scenes/InGame.json`（実際の配置・コライダー・スポナー設定）
3. **戦闘・敵制御コード（実態精査対象）**:
   - `project/Application_solo/RailMechanics/RailShooterEnemyComponent.cpp`
   - `project/Application_solo/Combat/Boss/BossComponent.cpp`
   - `project/Application_solo/Combat/DebugEnemySpawnerComponent.cpp`
   - `project/Application_solo/Combat/EnemyBeamComponent.cpp`
4. **プレイヤー・ガレキ・当たり判定コード（実態精査対象）**:
   - `project/Application_solo/Player/GravityPlayerComponent.cpp`
   - `project/Application_solo/Player/PlayerHealthComponent.cpp`
   - `project/Application_solo/Environment/DebrisComponent.cpp`
   - `project/Application_solo/Environment/DebrisManagerComponent.cpp`
5. **演出・エンジン基盤**:
   - `project/Application_solo/Effects/EffectManagerComponent.cpp`
   - `project/IrufemiEngine/Renderer/System/VoxelParticle/VoxelParticleManager.cpp`
   - `project/IrufemiEngine/Renderer/System/ParticleGPU/GPUParticleManager.cpp`

---

## 3. 審査の4大評価軸

### 軸1: コアフックと独自性（Core Hook & Novelty）
- **操作の手触りと実装の真実**: ガレキの引き寄せ・投擲が気持ちよく動いているか？ 敵や障害物にぶつけたときの判定とリアクションが成立しているか？
- **ゲームサイクルの成立度**: 「ガレキを引き寄せる ➔ 狙う ➔ ぶつけて倒す ➔ 新たな敵が現れる」という最小ゲームループが実際に遊べる状態になっているか？

### 軸2: 敵AI・レベルデザインの実装度（AI & Level Reality）★重要
- **敵の脅威度と駆け引き**: 敵がただの置物や直進する的になっていないか？ プレイヤーを脅かす弾幕、突撃、レーザーなどの行動ルーチンが本当に稼働しているか？
- **当たり判定の完全性**: 衝突判定が片道通行になっていないか？ 被弾時のダメージやノックバック、破壊処理が双方向で正しく結線されているか？

### 軸3: 自作エンジン技術の視覚的結実（Tech-to-Visual Showcase）
- **エンジンの強みが画面に出ているか**:
  - Compute Shader による GPU パーティクル
  - `VoxelParticle` によるメッシュ粉砕演出
  - 空間分割（BVH）やオブジェクトプールによる 60fps 安定化
- **演出の説得力**: 技術的な苦労が、ただの自己満足ではなく「大量のガレキが舞い散る派手さ」「リッチなエフェクト」という目に見える体験になっているか？

### 軸4: ゲームテンポと演出の手応え（Pacing & Game Juice）
- **進行の起伏**: レール移動中のウェーブ構成に緩急があるか？（デバッグ配置のまま放置されていないか？）
- **ヒット感・フィードバック**: ヒットストップ、カメラシェイク、被弾フラッシュなど、商用クオリティとして不可欠な演出が組み込まれているか？

---

## 4. レポートの出力フォーマット

審査結果は必ず **日本語** で、お世辞や忖度のない率直かつ具体的な内容で出力してください。

```markdown
# 🎮 ゲーム魅力・実装実態（Reality Check）総合審査レポート

## ■ 総合評価
- **総合ランク**: 【 S / A / B / C / D 】
- **一言要約**: （現状の実態を端的に突いた総括）

## ■ 実装リアリティ・チェック結果（動いているもの vs ダミー・未実装）
| 要素 | 状態 | コード実態の診断結果 |
| :--- | :---: | :--- |
| **プレイヤー（重力アクション）** | ⭕ 実装済 | 引き寄せ、オービット、手動ロックオン、時間差投擲が動作 |
| **敵AI（行動・攻撃）** | ❌ 未実装 | 直進のみで攻撃ルーチン・弾発射処理が完全欠落（ただの的） |
| **当たり判定（コリジョン）** | ⚠️ 片道/不完全 | ガレキ➔敵は判定あるが、敵の衝突コールバックや地形判定が未実装 |
| **敵ウェーブ・進行** | ❌ 未実装 | DebugEnemySpawner の単発出現のみでウェーブ未稼働 |
| **自作エンジン演出（GPU/Voxel）** | ⭕ 実装済 | ComputeパーティクルとVoxel破砕がデータ駆動で正常稼働 |

## ■ 4大評価軸の詳細採点
| 評価軸 | ランク | 厳密な評価コメント |
| :--- | :---: | :--- |
| **1. コアフック・独自性** | ... | ... |
| **2. 敵AI・コリジョン実装度** | ... | ... |
| **3. 技術アピール度** | ... | ... |
| **4. テンポ・手応え（Juice）** | ... | ... |

## ■ 審査員・採用担当者が見たときの「致命的な見抜かれポイント」
- （面接官や審査員が触った瞬間に「あ、これ中身まだできてないな」とバレる箇所）

## ■ ポートフォリオとして完成させるための必須実装ロードマップ TOP 3
1. 【最優先】...
2. 【高優先】...
3. 【中優先】...
```
