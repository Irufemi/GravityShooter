#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
単元 02_05 [new/delete禁止] 自動診断スクリプト
- コード内の生の new / delete 演算子の検出
- 特殊メンバ関数 (= delete;) やコメント・文字列リテラルの精密除外
- スマートポインタ (unique_ptr / shared_ptr) の使用統計
"""

import sys
import re
from pathlib import Path
from typing import List, Dict, Tuple

EXCLUDE_DIRS = {
    "externals", "packages", "generated", ".git", ".vs", "build", "out", "bin", "obj"
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

def scan_file(file_path: Path) -> Tuple[List[Dict], List[Dict], int, int]:
    new_violations = []
    delete_violations = []
    unique_ptr_count = 0
    shared_ptr_count = 0

    try:
        raw_text = file_path.read_text(encoding="utf-8")
    except UnicodeDecodeError:
        try:
            raw_text = file_path.read_text(encoding="shift_jis")
        except Exception:
            return [], [], 0, 0

    clean_text = remove_comments_and_strings(raw_text)
    lines = clean_text.splitlines()

    for line_idx, line in enumerate(lines, start=1):
        stripped = line.strip()
        if not stripped or stripped.startswith('#'):
            continue

        # スマートポインタの使用集計
        unique_ptr_count += len(re.findall(r'\b(?:std::)?(?:unique_ptr|make_unique)\b', stripped))
        shared_ptr_count += len(re.findall(r'\b(?:std::)?(?:shared_ptr|make_shared)\b', stripped))

        # 1. delete 判定
        # `= delete;` や `=delete;`（特殊メンバ関数禁止）を除外
        if re.search(r'=\s*delete\s*;', stripped):
            # 特殊メンバ関数の delete 宣言は正当
            pass
        else:
            # delete ptr; または delete[] ptr;
            # operator delete のオーバーロード宣言は除外
            if not re.search(r'\boperator\s+delete\b', stripped):
                m_del = re.search(r'\bdelete(?:\s*\[\s*\])?\s+([A-Za-z0-9_*>]+)', stripped)
                if m_del:
                    delete_violations.append({
                        "file": file_path,
                        "line": line_idx,
                        "token": m_del.group(0),
                        "code": line.strip()
                    })

        # 2. new 判定
        # new Type(...) や new Type[...]
        # operator new のオーバーロード宣言、placement new、std::make_unique 内の new は除外
        if not re.search(r'\boperator\s+new\b', stripped):
            # placement new (::new(buf) Type) や通常の new Type
            # ただし、関数名や変数名 (e.g. newObj, NewLayer) は単語境界 \b で除外
            m_new = re.search(r'(?<!::)\bnew\s+([A-Za-z0-9_:]+)(?:\s*\[|\s*\(|\s*;|\s*\{)', stripped)
            if m_new:
                target_type = m_new.group(1)
                # make_unique や make_shared の定義内などのライブラリ的記述は自作コードではほぼないが判定
                new_violations.append({
                    "file": file_path,
                    "line": line_idx,
                    "type": target_type,
                    "code": line.strip()
                })

    return new_violations, delete_violations, unique_ptr_count, shared_ptr_count

def main():
    target_dir = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(".")
    if not target_dir.exists():
        print(f"Error: Directory '{target_dir}' does not exist.")
        sys.exit(1)

    all_new = []
    all_delete = []
    total_unique_ptr = 0
    total_shared_ptr = 0

    for ext in ("*.h", "*.hpp", "*.cpp"):
        for path in target_dir.rglob(ext):
            if any(part in EXCLUDE_DIRS for part in path.parts):
                continue
            news, deletes, u_cnt, s_cnt = scan_file(path.resolve())
            all_new.extend(news)
            all_delete.extend(deletes)
            total_unique_ptr += u_cnt
            total_shared_ptr += s_cnt

    print("=" * 80)
    print("  単元 02_05 [new/delete禁止 (Smart Pointer & RAII)] 自動診断レポート")
    print(f"  診断対象ディレクトリ: {target_dir.resolve()}")
    print("=" * 80)
    print()

    print(f"【スマートポインタ活用統計】")
    print(f"  - std::unique_ptr / make_unique 使用箇所 : {total_unique_ptr} 件")
    print(f"  - std::shared_ptr / make_shared 使用箇所 : {total_shared_ptr} 件")
    if total_unique_ptr + total_shared_ptr > 0:
        ratio = (total_unique_ptr / (total_unique_ptr + total_shared_ptr)) * 100
        print(f"  - unique_ptr 採用比率: {ratio:.1f}% (単独所有が徹底されているかの目安)")
    print()

    print(f"【1. 生の new 演算子の検出】: {len(all_new)} 件")
    if all_new:
        print("  ※以下の箇所で生の new が使用されています。std::make_unique 等に置き換えてください：")
        for item in all_new:
            rel_path = item['file'].relative_to(target_dir.resolve())
            print(f"  - {rel_path}:{item['line']}")
            print(f"    型: {item['type']}")
            print(f"    コード: {item['code']}")
    else:
        print("  ✓ 検出なし: 自作コード内に生の new 演算子は存在しません。")

    print()
    print(f"【2. 生の delete 演算子の検出】: {len(all_delete)} 件")
    if all_delete:
        print("  ※以下の箇所で生の delete が使用されています。スマートポインタによる自動解放に委譲してください：")
        for item in all_delete:
            rel_path = item['file'].relative_to(target_dir.resolve())
            print(f"  - {rel_path}:{item['line']}")
            print(f"    トークン: {item['token']}")
            print(f"    コード: {item['code']}")
    else:
        print("  ✓ 検出なし: 自作コード内に生の delete 演算子は存在しません。")

    print()
    print("=" * 80)
    total_violations = len(all_new) + len(all_delete)
    if total_violations == 0:
        print("  診断結果: 【完全合格】コードベース全体で生の new / delete が 0 件です。")
        print("            RAII原則に基づき、スマートポインタによる近代的なメモリ管理が完徹されています。")
    else:
        print(f"  診断結果: 【要改善】合計 {total_violations} 件の生 new / delete が見つかりました。")
    print("=" * 80)

if __name__ == "__main__":
    main()
