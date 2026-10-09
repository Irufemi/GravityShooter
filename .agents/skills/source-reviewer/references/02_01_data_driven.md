# 単元: 02_01 データドリブン (Data-driven)

学校の授業資料 `02_01. データドリブン` に基づく、ソースコードレビュー基準・アンチパターン集、および一線級商用ゲームエンジン・大手ゲーム会社の実装基準ガイドです。

---

## 1. 単元の目的と背景

> **「個人就職作品のプロジェクトについて、自分のコード内で値のみ異なるが処理内容は同じ部分を可能な限り if / else や switch からデータドリブンに置き換えよ」**

### なぜデータドリブンが重要視されるのか？
ゲーム企業の採用面接・作品審査において、**「無駄な if-else や switch によるハードコード分岐」** は最も頻出するダメ出し・減点ポイントの一つです。

余分な条件分岐を放置すると以下の重大な問題が発生します：
1. **可読性・保守性の低下**: 分岐がネスト・肥大化し、コード全体の把握やバグ調査が困難になる。
2. **重複コードの発生**: 処理は同一でパラメータのみが異なる似たコードが量産され、修正漏れ・仕様変更時のバグの原因となる。
3. **拡張性の低さ**: 新しいステージ、敵、武器、アイテム等を追加するたびにC++ソースコードの修正と再ビルドが必要になり、プランナーやデザイナーによるパラメータ調整が不可能になる。
4. **テストの困難さ**: 分岐ごとにテストケースが増大し、網羅的な検証が困難になる。

データ駆動型（Data-driven）設計を導入することで、ロジックとデータを完全に分離し、変更に強く柔軟なアーキテクチャを実現します。

---

## 2. 減点・指摘対象となるアンチパターン

### ❌ アンチパターン①: パラメータ違いの `if-else` / `switch` ベタ書き
- **症状**:
  ステージ初期化や敵生成、UI表示などで、実行する関数や手続き自体は全く同じなのに、条件分岐でパラメータ（座標、種別名、色、テキスト等）だけを変えてベタ書きしている。
  ```cpp
  // 悪い例（授業資料より）
  void InitStage(int stageId) {
      if (stageId == 0) /* 森ステージ */ {
          playerPosition = {0.0f, 0.0f, 0.0f};
          SpawnEnemy("EnemyA", {0.0f, 0.0f, 1.0f});
          SpawnEnemy("EnemyA", {2.0f, 0.0f, 1.0f});
      } else if (stageId == 1) /* 砂漠ステージ */ {
          playerPosition = {5.0f, 0.0f, -10.0f};
          SpawnEnemy("EnemyA", {0.0f, 0.0f, 1.0f});
          SpawnEnemy("EnemyB", {0.0f, 0.0f, -1.0f});
      } else if (stageId == 2) /* 氷山ステージ */ {
          playerPosition = {-3.0f, 0.0f, 2.0f};
          SpawnEnemy("EnemyB", {0.0f, 0.0f, -1.0f});
          SpawnEnemy("EnemyB", {2.0f, 0.0f, -1.0f});
          SpawnEnemy("EnemyB", {4.0f, 0.0f, -1.0f});
      }
  }
  ```
- **問題点**:
  ステージが10個、20個と増えるたびにC++コードの行数が激増し、プランナーがパラメータを調整できなくなる。

### ❌ アンチパターン②: コピペファイルの量産（もっとダメな例）
- **症状**:
  分岐をなくそうとして `Stage1.cpp`, `Stage2.cpp`, `Stage3.cpp` のように別ファイルを作成し、中身の処理はほぼ同じで値だけを変えたコードを分散配置する。
- **問題点**:
  全ステージ共通の仕様変更（例: プレイヤー初期化引数の変更、カメラ初期化の追加など）が発生した際、全ファイルを人手で修正しなければならず、修正ミスが多発する。

### ❌ アンチパターン③: データ取得関数の呼び出し側での分岐
- **症状**:
  データ構造体を作ったにもかかわらず、呼び出し側で `if (stage == 0) SetupStage(dataA); else if (stage == 1) SetupStage(dataB);` のように依然として分岐を書いている。
- **改善策**:
  インデックス添字やキー（ID文字列/enum）を用いてテーブルから一括取得し、呼び出し側では分岐を一切書かない。

---

## 3. 合格要件と推奨設計パターン（基礎カリキュラム基準）

### 要件1: 意味のあるまとまりごとの構造体定義
データテーブルを構築するため、属性やまとまりごとに構造体を分割・ネストして定義する。
```cpp
// プレイヤー初期化用データ
struct PlayerSpawnData {
    Vector3 position;
    Vector3 rotation;
};

// 敵スポーン用データ
struct EnemySpawnData {
    std::string enemyType;
    Vector3 position;
    float scale = 1.0f;
};

// 1ステージ全体のデータを集約する構造体
struct StageData {
    PlayerSpawnData playerSpawnData;
    std::vector<EnemySpawnData> enemySpawnDatas;
    int timeLimit = 0;
};
```

### 要件2: データテーブル（配列 / コンテナ / JSON）の一元化
全ステージや全タイプのデータを一つのデータテーブルにまとめる。
```cpp
inline const std::vector<StageData> kStageDataTable = {
    { // ステージ0 (森)
        { {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} },
        { {"EnemyA", {0.0f, 0.0f, 1.0f}}, {"EnemyA", {2.0f, 0.0f, 1.0f}} },
        180
    },
    { // ステージ1 (砂漠)
        { {5.0f, 0.0f, -10.0f}, {0.0f, 0.0f, 0.0f} },
        { {"EnemyA", {0.0f, 0.0f, 1.0f}}, {"EnemyB", {0.0f, 0.0f, -1.0f}} },
        240
    },
};
```

### 要件3: 共通初期化・処理の関数化
添字やIDを受け取ってデータを引き、単一の共通処理関数でセットアップを完了させる。分岐はテーブル境界の `assert` のみとする。
```cpp
void StageManager::SetupStage(int stageIndex) {
    assert(stageIndex >= 0 && stageIndex < static_cast<int>(kStageDataTable.size()));
    const auto& stage = kStageDataTable[stageIndex];

    // 分岐なしで共通処理を実行
    player_->SetPosition(stage.playerSpawnData.position);
    for (const auto& enemyData : stage.enemySpawnDatas) {
        enemySpawner_->SpawnEnemy(enemyData.enemyType, enemyData.position, enemyData.scale);
    }
}
```

---

## 4. 一線級エンジン・大手ゲーム会社の実装スタンダード (AAA Best Practices)

大手ゲーム会社（カプコン、スクウェア・エニックス、フロム・ソフトウェア等）や商用エンジン（Unreal Engine, Unity）では、データドリブンは単なる「配列化」に留まらず、以下の高度な設計パラダイムとして確立されています。

### ① Unreal Engine スタイル: `UDataTable` / `DataAsset` パターン
- **設計思想**: ゲームロジックコード（C++）は「データの受け皿と振る舞い」のみを定義し、具体的なパラメータ・出現テーブル・属性値はすべて外部データアセット（JSON / CSV / バイナリ）に追い出す。
- **メリット**:
  - プログラマを介さずにゲームデザイナー/プランナーがExcel/GoogleスプレッドシートやJSONエディタでバランス調整を完結できる。
  - ランタイムでの **Hot Reloading（ホットリロード）** が可能になり、ゲームを再起動・再ビルドせずにパラメータ変更を即時反映できる。

### ② コンパイル時データテーブル（`constexpr std::array` / C++20 lookup）
- **用途**: Enum ↔ 文字列変換、キーマッピング、固定ステータス定義など、固定値のルックアップ処理。
- **設計方針**:
  - `switch` 分岐を撤廃し、`constexpr` 配列による $O(1)$ 直接インデックス参照を行う。
  - 分岐命令（Branch Instruction）をなくすことで、CPUのブランチプレディクタ（分岐予測）のミスヒットをゼロにし、L1インストラクションキャッシュを保護する。
```cpp
// 一線級エンジン水準: 分岐ゼロ・コンパイル時解決の文字列マッピング
inline constexpr std::string_view ToString(GameAction action) noexcept {
    constexpr std::array<std::string_view, static_cast<size_t>(GameAction::Count)> kActionNames = {
        "Pull", "Fire", "LockOn", "ClearLock", "Pause", "ToggleFullscreen", "UI_Submit", "UI_Cancel"
    };
    const auto idx = static_cast<size_t>(action);
    return idx < kActionNames.size() ? kActionNames[idx] : "Unknown";
}
```

### ③ 動的ファクトリテーブル（Registration-based Factory）
- **用途**: 敵AIの行動戦略（Strategy）、弾幕パターン、エフェクト生成など、クラスの生成処理。
- **設計方針**:
  - `switch (type)` で `make_unique<ConcreteClass>()` を分岐生成するのではなく、生成ファクトリ関数（ラムダ式 / 関数ポインタ）をマップまたは配列に登録する。
  - 新しい戦略や敵タイプを追加する際、既存の switch 文を編集する必要がなくなり、**開放閉鎖原則 (Open-Closed Principle)** を完全に達成できる。
```cpp
// ファクトリ登録テーブル
using StrategyCreator = std::function<std::unique_ptr<IEnemyAttackStrategy>()>;
static const std::unordered_map<EnemyBehaviorType, StrategyCreator> kStrategyFactory = {
    { EnemyBehaviorType::StandardGunner,    [] { return std::make_unique<EnemyAttackStrategyNormal>(); } },
    { EnemyBehaviorType::PredictiveSniper, [] { return std::make_unique<EnemyAttackStrategyPredictiveSniper>(); } },
    { EnemyBehaviorType::DiveBomber,        [] { return std::make_unique<EnemyAttackStrategyDiveBomb>(); } },
};

void SetBehaviorType(EnemyBehaviorType type) {
    if (auto it = kStrategyFactory.find(type); it != kStrategyFactory.end()) {
        SetAttackStrategy(it->second());
    }
}
```

### ④ UI / プレゼンテーション層のデータ駆動化
- **用途**: ステータス色、メッセージ文言、アイコンテクスチャ、アニメーション曲線の表示。
- **設計方針**:
  - UIの `switch (state)` 分岐を排除し、状態と表示プロパティのマップテーブルを定義する。
  - カラーパレットやローカライズテキストを一元管理し、見た目の変更を容易にする。
