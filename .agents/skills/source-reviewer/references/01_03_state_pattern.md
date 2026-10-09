# 単元: 01_03 State Pattern (状態遷移パターン)

学校の授業資料 `[確認課題] 01_03.State Pattern` に基づく、先生およびAI自動採点システムによる設計審査基準とアンチパターン集です。

---

## 1. 課題の前提と目的

> **「個人就職作品のプロジェクトについて、自分のコード内でState Patternを適用できそうなところは片っ端から適用せよ。」**

ゲーム開発において、オブジェクト指向の三大要素の一つである「ポリモーフィズム（多態性）」を実践的に活用できているかを厳格に審査する単元です。  
手続き型の `switch (state)` 分岐を放置していると、先生やAI診断から「オブジェクト指向の設計ができていない」と判定され、減点・再提出の対象となります。

---

## 2. 先生・AI診断の合格基準 (4大必須チェック項目)

実装が State Pattern として「設計合格」と判定されるためには、以下の4条件をすべて満たしている必要があります。

| # | 審査項目 | 合格要件 | 減点・不合格となる状態 |
| :--- | :--- | :--- | :--- |
| **1** | **状態クラスの存在** | 状態ごとに独立したクラス（派生クラス）が存在している | `enum class` の値だけで状態を表現している |
| **2** | **共通関数のoverride** | 基底状態クラスの純粋仮想関数（`Enter`, `Update`, `Exit` 等）を具象クラスがオーバーライドしている | 基底クラスに関数が定義されていない、または空関数を呼ぶだけ |
| **3** | **ポリモーフィズム呼び出し** | 状態を持つ本体（Context: Player, Enemy, Boss等）が、基底ポインタ経由で仮想関数を呼び出している | Context 内で `dynamic_cast` や enum 判定を行って分岐している |
| **4** | **クラス内部での状態遷移** | 次の状態への遷移条件を、状態クラス自身（または状態内部ロジック）が判断・要請している | Context 側や外部クラスが場当たり的に状態を上書きしている |

---

## 3. 減点・指摘対象となるアンチパターン

### ❌ アンチパターン①: `enum class` + `switch-case` のベタ書き放置
- **症状**:
  ```cpp
  // 先生・AI診断で「設計不足」と判定される典型例
  void Enemy::Update(float dt) {
      switch (state_) {
      case State::Idling:   UpdateIdle(dt);   break;
      case State::Chasing:  UpdateChase(dt);  break;
      case State::Attacking:UpdateAttack(dt); break;
      case State::Dying:    UpdateDead(dt);   break;
      }
  }
  ```
- **問題点**: 状態が増えるたびに `switch` 文が肥大化し、Open-Closed Principle（開放閉鎖原則）に反する。

### ❌ アンチパターン②: 状態クラス内部での型判別（多態性の破壊）
- **症状**: 状態クラスを作ったものの、関数内部で `if (type == AttackState)` のように分岐している。
- **改善策**: 状態ごとの固有ロジックはそれぞれの具象派生クラスの仮想関数内に完全に閉じ込める。

### ❌ アンチパターン③: 外部からの強制状態書き換え
- **症状**: 外部クラスが `context->SetState(NewState)` を直接呼び出して内部状態を乱暴に変更している。
- **改善策**: 状態遷移の契機（イベントや条件）は Context のメソッドとして提供し、現在の状態クラスが次の状態インスタンスを生成・返却する構造にする。

---

## 4. 例外規定 (State Pattern を適用しなくても減点されないケース)

スライド資料により、以下の条件に該当する場合は State Pattern の適用を免除（または代替手法で可）とされます：

1. **状態数が3つ以下で極めて単純な場合**（クラス分割のオーバーヘッド・手間の方が大きい）
2. **極限まで抽象化した結果、アプリ層にステートマシンが残っていない場合**
3. **データ指向設計（DoD）やパフォーマンス最適化のため、連続メモリ配列でフラットに更新すべき場合**（例: `DebrisComponent` のような数千個のガレキバッチ更新）
   * ※明確な技術的理由を口頭またはコード注釈で説明できることが条件。

---

## 5. 本プロジェクト（GravityShooter）での合格実績

本単元に基づき、以下の3大コンポーネントを完全に State Pattern へ移行完了済み：
1. **`BossComponent`**: `IBossState` 基底および具象状態クラス群（既存）
2. **`RailShooterEnemyComponent`**: `IRailShooterEnemyState` 基底および `Approach`, `Combat`, `Dive`, `Disengage`（本改修）
3. **`TitleMenuControllerComponent`**: `ITitleMenuState` 基底および `Idle`, `OpeningModal`, `Suspended`, `Launching`（本改修）
4. **`TitleSceneDirectorComponent`**: `ITitleLaunchState` 基底および `Charge`, `Accelerate`, `Break`, `Afterglow`（本改修）
