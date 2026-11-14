---
name: source-reviewer
description: 学校の授業資料・レビュー基準（単元別カリキュラム）に基づき、作業場フォルダ（project/）全体のソースコードを一括診断・レビューするスキル。
---

# ソースレビュー (source-reviewer)

学校の授業資料および講義カリキュラムに準拠し、作業場フォルダ（`project/`）内のソースコード全体を単元ごとにレビュー・診断するスキルです。

---

## 1. 授業資料・単元インデックス (Curriculum Units)

現在登録されている単元一覧です。新しい講義資料が配布された際は、`references/` 配下に資料を追加して本インデックスを更新します。

| 単元ID | テーマ | リファレンス資料 | 主要な診断項目 |
| :--- | :--- | :--- | :--- |
| **`01_01`** | **カプセル化 (Encapsulation)** | [01_01_encapsulation.md](references/01_01_encapsulation.md) | アンチパターン5選（public変数、非const参照Getter、friend乱用、DTO不要カプセル化、神クラス） |
| **`01_02`** | **ポリモーフィズム (Polymorphism)** | [01_02_polymorphism.md](references/01_02_polymorphism.md) | 4大成立要件（virtual関数、override、基底ポインタ呼出、virtualデストラクタ）、is-a関係遵守、無理な基底拡張禁止 |
| **`01_03`** | **State Pattern (状態遷移パターン)** | [01_03_state_pattern.md](references/01_03_state_pattern.md) | 先生・AI診断4大合格要件、switch-caseベタ書き脱却、ポリモーフィズム呼び出し |
| **`01_04`** | **デザインパターン (Design Patterns)** | [01_04_design_patterns.md](references/01_04_design_patterns.md) | State/Singleton以外の各種パターン活用（Object Pool, Dirty Flag, Observer, Command, Factory等） |
| **`02_01`** | **データドリブン (Data-driven)** | [02_01_data_driven.md](references/02_01_data_driven.md) | 余分なif-else/switch排除、構造体・データテーブル化、共通初期化・処理の関数化 |
| **`02_02`** | **調整データの外部ファイル化 (External Data & Tooling)** | [02_02_external_data.md](references/02_02_external_data.md) | 配置・敵出現・ゲームバランス値の外部ファイル化（JSON/CSV）、エディタ/ツールの提供 |
| **`02_03`** | **関数化 (Functionalization & OAOO)** | [02_03_functionalization.md](references/02_03_functionalization.md) | OAOO原則、コピペ実装の根絶、重複ロジックの関数・ラムダ式抽出 |
| **`02_04`** | **引数のconst参照 (Pass by const-Reference)** | [02_04_const_reference.md](references/02_04_const_reference.md) | 64bit超の型のconst参照化、64bit以下プリミティブの無意味なconst参照排除、ダングリング参照防止 |
| **`02_05`** | **new / delete 禁止 (Smart Pointer & RAII)** | [02_05_no_new_delete.md](references/02_05_no_new_delete.md) | 生new/deleteの完全排除、unique_ptr優先、shared_ptr無意味濫用の抑制、Persistent Mapping理解 |
| **`02_06`** | **マジックナンバー撲滅 (Eliminate Magic Numbers)** | [02_06_magic_numbers.md](references/02_06_magic_numbers.md) | 3段階解消法（定数化/外部ファイル化/エディタ化）、許されるマジックナンバーの厳格適用、ロジック直書き排除 |


---

## 2. 診断対象範囲

- **主要作業場**: `project/` 配下の全C++自作コード
  - `project/IrufemiEngine/` (ゲームエンジンコア)
  - `project/Application_solo/` (ゲームアプリケーション・ロジック)
  - `project/IrufemiEditor/` (内製エディタ)
- **除外対象**: `externals/`, `thirdparty/` 等の外部サードパーティライブラリ

---

## 3. レビュー実行プロトコル

ユーザーから「ソースレビューして」「/source-review」「授業資料で診断して」等の指示を受けた際、以下の手順で自律実行します：

### Step 1: 単元の特定とリファレンス確認
- ユーザーが特定の単元（例: `01_01`）を指定した場合は該当単元を重点診断。
- 指定がない場合は、登録されている全単元（現在は `01_01 カプセル化`）を網羅的に診断。
- `references/` 配下の該当単元ドキュメントを読み込み、採点基準・アンチパターンを把握する。

### Step 2: 作業場の自動スキャン実行
- 単元に応じたスキル同梱スクリプトを実行して作業場全体を高速走査する：
  ```powershell
  # 01_01 カプセル化診断
  py -3 .agents\skills\source-reviewer\scripts\scan_encapsulation.py

  # 01_02 ポリモーフィズム診断
  py -3 .agents\skills\source-reviewer\scripts\scan_polymorphism.py

  # 01_03 State Pattern 診断
  py -3 .agents\skills\source-reviewer\scripts\scan_state_pattern.py

  # 02_01 データドリブン (Data-driven) 診断
  py -3 .agents\skills\source-reviewer\scripts\scan_data_driven.py

  # 02_02 調整データの外部ファイル化 (External Data & Tooling) 診断
  py -3 .agents\skills\source-reviewer\scripts\scan_external_data.py

  # 02_03 関数化 (Functionalization & OAOO) 診断
  py -3 .agents\skills\source-reviewer\scripts\scan_functionalization.py

  # 02_04 引数のconst参照 (Pass by const-Reference) 診断
  py -3 .agents\skills\source-reviewer\scripts\scan_const_reference.py

  # 02_05 new / delete 禁止 (Smart Pointer & RAII) 診断
  py -3 .agents\skills\source-reviewer\scripts\scan_new_delete.py

  # 02_06 マジックナンバー撲滅 (Eliminate Magic Numbers) 診断
  py -3 .agents\skills\source-reviewer\scripts\scan_magic_numbers.py
  ```
- スキャン結果に加え、直近の Git 変更差分（`git status -s`, `git diff`）も精査し、直近作業でアンチパターンが混入していないかを確認する。

### Step 3: レビューレポートの作成・提示
検出結果を以下のフォーマットで日本語で提示する：

```markdown
## ソースレビュー結果レポート

- **診断対象**: 作業場フォルダ (`project/`)
- **適用単元**: [単元ID] [単元テーマ]

### 1. 診断サマリー
- 適合状況: （合格 / 要改善 / 軽微な指摘あり）
- 検出件数: public変数 X件 / 非const参照Getter Y件 / friend Z件

### 2. 詳細な指摘事項（該当箇所がある場合）
- **[ファイル名:行番号]**:
  - **アンチパターン**: (例: アンチパターン② 非const参照を返すGetter)
  - **現状のコード**: `...`
  - **問題点**: なぜ授業基準で減点・不適切となるのか
  - **推奨される改善案**: C++20 モダンイディオムによる具体的な修正コード

### 3. 総評・アドバイス
...
```

### Step 4: 改善コードの提案（Proposal）
- 指摘事項がある場合、直接ファイルを書き換える前に必ず「改善案」をユーザーに提案し、承諾を得てから適用する。

---

## 4. 今後の単元追加ガイド
新しい授業資料が追加された場合：
1. `references/{単元ID}_{テーマ}.md` を作成し、スライドのアンチパターンやルールを整理。
2. 必要に応じて `scripts/` に自動検出スクリプトを追加。
3. `SKILL.md` の単元インデックスに追記。
