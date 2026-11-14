# 単元: 02_07 エンジンnamespace (Engine Namespace)

学校の授業資料 `02_07. エンジンnamespace` に基づく、自作ゲームエンジン層の名前空間設計・レビュー基準およびアンチパターン集です。

---

## 1. 単元の目的と背景

> **「個人就職作品のプロジェクトについて、エンジン層を専用の namespace 内に定義せよ」**

### なぜ自作エンジン層を namespace で囲むべきなのか？
1. **識別子の衝突回避（タケシの衝突問題）**:
   - 自作ゲームエンジンは汎用的なユーティリティクラスや共通基盤（`Vector3`, `Renderer`, `Timer`, `Camera`, `TextureManager` 等）を多く持ちます。
   - ゲーム層（アプリケーション固有コード）や外部ライブラリ、C++標準ライブラリ（`std::`）と同名のクラスが存在しても、`Irufemi::Vector3` のようにスコープを限定することで名前の衝突（衝突エラーや意図しないオーバーロード解決）を完全に防ぐことができます。
2. **エンジンとゲーム（アプリケーション）の明確な境界線の確立**:
   - 名前空間を分けることで、どのクラスがエンジン基盤（共通機能）で、どのクラスがゲーム固有ロジックなのかがコード上で一目瞭然になります。
3. **ゲーム会社・審査員からの評価**:
   - 「ライブラリやミドルウェアとして再利用・配布可能なレベルのアーキテクチャ設計・スコープ管理ができているか」を判断する重要な指標となります。

---

## 2. 実装基準とファイル分割ルール

### ① クラス宣言とメンバ関数定義の囲み方
ヘッダーファイル（.h）とソースファイル（.cpp）のそれぞれで、`namespace EngineName { ... }` で囲みます。

```cpp
// UIManager.h
namespace Irufemi {
class UIManager {
public:
    void ShowMessage(const std::string& msg);
};
} // namespace Irufemi

// UIManager.cpp
#include "UIManager.h"
#include <iostream>

namespace Irufemi {
void UIManager::ShowMessage(const std::string& msg) {
    std::cout << "[UI] " << msg << "\n";
}
} // namespace Irufemi
```

> [!TIP]
> **インデントの抑制**:  
> namespace 内でコード全体のインデントが 1 段階深くなるのを防ぎたい場合は、`.clang-format` に `NamespaceIndentation: None` を設定することで、namespace 直下のインデントを抑制できます。

---

### ② 無名名前空間 (unnamed namespace) の活用
ソースファイル（.cpp）の内部だけで完結する定数やヘルパー関数は、**無名名前空間** で囲んでファイルスコープ（内部リンケージ）に限定します。

```cpp
// Player.cpp
namespace {
constexpr float kDefaultMoveSpeed = 5.5f;
constexpr int kMaxRetryCount = 3;

bool ValidatePosition(const Irufemi::Vector3& pos) {
    return pos.y >= 0.0f;
}
} // namespace
```
- 他のソースファイルと定数名・ヘルパー関数名が重複しても名前衝突が起こりません。
- 外部への不要なシンボル公開を防ぎ（隠蔽・カプセル化）、リンカの負担を軽減します。

---

## 3. 【最重要・厳禁】ヘッダーファイルの名前空間汚染 (Namespace Pollution)

> [!CAUTION]
> **「ヘッダーファイルに using namespace を書くな」**

### なぜヘッダーファイルでの `using namespace` が厳禁なのか？
- `using namespace` をヘッダーファイルに書くと、そのヘッダーをインクルードした**すべてのソースファイル（および後続のインクルード先）全体にその名前空間が強制展開**されます。
- これにより、インクルードした側で予期せぬ名前衝突が発生し、名前空間による分離機能が完全に崩壊します。

```cpp
// ❌ 厳禁：ヘッダーファイルで using namespace
// Common.h
#pragma once
using namespace std;     // 厳禁！全インクルード先で std:: が省略されてしまう
using namespace Irufemi; // 厳禁！

// ⭕ 正解：ヘッダーファイルでは名前空間を明示して書く
#pragma once
#include <string>
#include "Math/Vector3.h"

class Player {
    Irufemi::Vector3 position_;
    std::string name_;
};
```

※ソースファイル（.cpp）内での `using namespace` や `using Irufemi::Vector3;` は、スコープがそのファイル内に閉じるため禁止まではされませんが、多用は避け必要最小限に留めることが推奨されます。

---

## 4. 減点・指摘対象となるアンチパターン

| アンチパターン | 症状 | 改善案 |
| :--- | :--- | :--- |
| **❌ ヘッダーでの `using namespace`** | `.h` 内に `using namespace std;` や `using namespace Irufemi;` が書かれている。 | 直ちに削除し、ヘッダー内では型名に明示的な修飾（`std::`, `Irufemi::`）を付与する。 |
| **❌ エンジン層クラスのグローバル定義** | `IrufemiEngine/` 配下のクラスが namespace 外のグローバル空間に剥き出しで定義されている。 | `namespace Irufemi { ... }` 内に定義し、ゲーム側からは `Irufemi::ClassName` で参照する。 |
| **❌ .cpp 内のプライベート定数の裸定義** | ソースファイル内にファイル限定の定数や関数が名前空間なしでグローバルに置かれている。 | `namespace { ... }`（無名名前空間）で囲み、ファイルスコープに限定する。 |
