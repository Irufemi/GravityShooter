#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
単元 02_06 [マジックナンバー撲滅] 自動診断スクリプト
- ロジック内（条件式・計算式・関数引数）の裸のマジックナンバー (Needless Literal) の検出
- メンバ変数のハードコード初期値 (Hardcoded Default) の検出
- 許されるマジックナンバー (0, 1, 2, -1, 100等) や定数宣言 (constexpr, const, enum) の精密除外
"""

import sys
import re
from pathlib import Path
from typing import List, Dict, Tuple

EXCLUDE_DIRS = {
    "externals", "packages", "generated", ".git", ".vs", "build", "out", "bin", "obj"
}

# 授業資料スライド 4 に基づく「許されるマジックナンバー」
ALLOWED_NUMBERS = {
    "0", "1", "2", "-1", "100",
    "0.0", "1.0", "2.0", "-1.0", "100.0",
    "0.0f", "1.0f", "2.0f", "-1.0f", "100.0f",
    "0f", "1f", "2f", "-1f", "100f",
    "0.5f", "0.5",  # 半分・中心座標
}

def remove_comments_and_strings(content: str) -> str:
    """コメントと文字列リテラルを除去（行番号を保つため改行は保持）"""
    def replacer(match):
        s = match.group(0)
        if s.startswith('/') or s.startswith('*'):
            return '\n' * s.count('\n')
        else:
            return ' ' * len(s)
    pattern = re.compile(
        r'//.*?$|/\*.*?\*/|\'(?:\\.|[^\\\'])*\'|"(?:\\.|[^\\"])*"',
        re.DOTALL | re.MULTILINE
    )
    return re.sub(pattern, replacer, content)

def is_constant_declaration(line: str) -> bool:
    """定数定義行 (constexpr, const, enum, #define, static const) か判定"""
    stripped = line.strip()
    if stripped.startswith('#define'):
        return True
    if any(k in stripped for k in ['constexpr', 'const ', 'const\t', 'enum ']):
        return True
    return False

def is_member_or_struct_init(line: str) -> bool:
    """メンバ変数の初期化宣言（例: float speed_ = 10.0f; または int count = 5;）か判定"""
    stripped = line.strip()
    # 末尾にセミコロンがあり、型名 + 変数名 + = + 数値 の形式
    member_pattern = re.compile(r'^\s*(?:float|int|double|size_t|uint32_t|int32_t|Vector\d|Irufemi::Vector\d)\s+[A-Za-z0-9_]+\s*=\s*[^;]+;\s*$')
    return bool(member_pattern.match(stripped))

def scan_file(file_path: Path) -> Tuple[List[Dict], List[Dict]]:
    logic_literals = []
    member_defaults = []

    try:
        raw_text = file_path.read_text(encoding="utf-8")
    except UnicodeDecodeError:
        try:
            raw_text = file_path.read_text(encoding="shift_jis")
        except Exception:
            return [], []

    clean_text = remove_comments_and_strings(raw_text)
    lines = clean_text.splitlines()

    num_pattern = re.compile(r'(?<![A-Za-z0-9_])(?:-)?\b\d+(?:\.\d+)?(?:f|F)?\b')

    for line_idx, line in enumerate(lines, start=1):
        stripped = line.strip()
        if not stripped or stripped.startswith('#'):
            continue

        if is_constant_declaration(stripped):
            continue

        if re.search(r'\b(pragma|include|version)\b', stripped):
            continue

        line_no_hex = re.sub(r'0x[0-9A-Fa-f]+', '', stripped)
        is_member_decl = is_member_or_struct_init(line_no_hex)

        for match in num_pattern.finditer(line_no_hex):
            token = match.group(0)
            token_clean = token.lower()

            if token_clean in ALLOWED_NUMBERS:
                continue

            if token_clean.isdigit() and int(token_clean) in {0, 1, 2, 3, 4}:
                continue

            info = {
                "file": file_path,
                "line": line_idx,
                "token": token,
                "code": stripped
            }

            if is_member_decl:
                member_defaults.append(info)
            else:
                logic_literals.append(info)

    return logic_literals, member_defaults

def main():
    target_dir = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(".")
    if not target_dir.exists():
        print(f"Error: Directory '{target_dir}' does not exist.")
        sys.exit(1)

    all_logic = []
    all_member = []

    for ext in ("*.h", "*.hpp", "*.cpp"):
        for path in target_dir.rglob(ext):
            if any(part in EXCLUDE_DIRS for part in path.parts):
                continue
            logic, member = scan_file(path.resolve())
            all_logic.extend(logic)
            all_member.extend(member)

    print("=" * 80)
    print("  単元 02_06 [マジックナンバー撲滅 (Eliminate Magic Numbers)] 自動診断レポート")
    print(f"  診断対象ディレクトリ: {target_dir.resolve()}")
    print("=" * 80)
    print()

    print(f"【1. ロジック内の裸のマジックナンバー (式・条件判定・引数)】: {len(all_logic)} 件")
    if all_logic:
        print("  ※以下の箇所はロジック中に数値が直接書かれています。最優先で名前付き定数 (constexpr) 化、")
        print("    または調整値として外部ファイル (JSON) 化すべきです：")
        for item in all_logic[:20]:
            rel_path = item['file'].relative_to(target_dir.resolve())
            print(f"  - {rel_path}:{item['line']}")
            print(f"    数値: {item['token']}")
            print(f"    コード: {item['code']}")
        if len(all_logic) > 20:
            print(f"    ... 他 {len(all_logic) - 20} 件省略")
    else:
        print("  ✓ 検出なし: ロジック内の裸のマジックナンバーは見つかりませんでした。")

    print()
    print(f"【2. メンバ変数のハードコード初期値】: {len(all_member)} 件")
    print("  ※メンバ変数名は付いていますが、ゲームバランス調整項目の場合は JSON 等の外部ファイル化 (Level 2/3) が推奨されます。")
    if all_member:
        for item in all_member[:10]:
            rel_path = item['file'].relative_to(target_dir.resolve())
            print(f"  - {rel_path}:{item['line']} -> {item['code']}")
        if len(all_member) > 10:
            print(f"    ... 他 {len(all_member) - 10} 件省略")

    print()
    print("=" * 80)
    if len(all_logic) == 0:
        print("  診断結果: 【完全合格】ロジック内の裸のマジックナンバーが 0 件です。")
    else:
        print(f"  診断結果: 【要改善】合計 {len(all_logic)} 件のロジック内マジックナンバーが検出されました。")
    print("=" * 80)

if __name__ == "__main__":
    main()
