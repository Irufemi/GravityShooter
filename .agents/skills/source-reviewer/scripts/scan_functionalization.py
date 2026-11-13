#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
単元 02_03（関数化: Functionalization & OAOO）に基づく自動診断スクリプト
作業場フォルダ（project/）全体の C++ コードをスキャンし、
OAOO原則（Once And Only Once）に反するコピペ実装・類似重複ブロックを検出します。
"""

import os
import re
import sys
from pathlib import Path
from collections import defaultdict

if sys.platform == "win32":
    try:
        sys.stdout.reconfigure(encoding="utf-8")
    except Exception:
        pass

def normalize_line(line):
    """コメント、空白、変数リテラルの簡易正規化"""
    line = re.sub(r'//.*$', '', line)
    line = re.sub(r'/\*.*?\*/', '', line)
    line = line.strip()
    if line.startswith('*') or line.startswith('/*') or line.endswith('*/'):
        return ''
    return line

def extract_code_blocks(filepath, min_lines=5):
    """ファイルを読み込み、連続する意味のあるコード行のブロックを抽出"""
    try:
        with open(filepath, "r", encoding="utf-8", errors="ignore") as f:
            lines = f.readlines()
    except Exception:
        return []

    clean_lines = []
    line_numbers = []
    for i, raw_line in enumerate(lines):
        norm = normalize_line(raw_line)
        if norm and norm not in ("{", "}", "};", "public:", "private:", "protected:", "#pragma once"):
            if not norm.startswith("#include") and not norm.startswith("#error") and not norm.startswith("#endif"):
                clean_lines.append(norm)
                line_numbers.append(i + 1)

    blocks = []
    for i in range(len(clean_lines) - min_lines + 1):
        block_text = "\n".join(clean_lines[i:i + min_lines])
        blocks.append((block_text, line_numbers[i], min_lines))
    return blocks

def scan_for_duplicates(target_dir, min_lines=5):
    """プロジェクト全体から完全一致・高類似度のコードブロックを検出"""
    cpp_files = list(target_dir.rglob("*.cpp")) + list(target_dir.rglob("*.h"))
    block_map = defaultdict(list)

    for f in cpp_files:
        rel = str(f.relative_to(target_dir.parent))
        if any(ign in rel.lower() for ign in ["externals", "thirdparty", "packages", "generated", "outputs", ".vscode"]):
            continue

        blocks = extract_code_blocks(f, min_lines=min_lines)
        for b_text, l_num, n_lines in blocks:
            block_map[b_text].append((rel, l_num))

    # 複数ファイルまたは複数箇所に現れるブロックを抽出
    duplicates = []
    for b_text, occurrences in block_map.items():
        # 同一ファイル内の同一近傍行の重複（スライディングウィンドウの重複）を除去
        filtered_occ = []
        last_file = None
        last_line = -999
        for f_path, l_num in occurrences:
            if f_path != last_file or abs(l_num - last_line) >= min_lines:
                filtered_occ.append((f_path, l_num))
                last_file = f_path
                last_line = l_num

        if len(filtered_occ) >= 2:
            duplicates.append({
                "block": b_text,
                "occurrences": filtered_occ,
                "count": len(filtered_occ)
            })

    # 重複ブロックの出現数・行数順にソート
    duplicates.sort(key=lambda x: (x["count"], len(x["block"])), reverse=True)
    return duplicates

def main():
    if len(sys.argv) > 1:
        target_dir = Path(sys.argv[1]).resolve()
    else:
        target_dir = Path(__file__).resolve().parents[4] / "project"

    print("=" * 80)
    print("  単元 02_03 [関数化 (Functionalization & OAOO)] コピペ・重複コード自動診断")
    print(f"  診断対象ディレクトリ: {target_dir}")
    print("=" * 80)

    if not target_dir.exists():
        print(f"エラー: ディレクトリが見つかりません: {target_dir}")
        sys.exit(1)

    duplicates = scan_for_duplicates(target_dir, min_lines=5)

    print(f"\n【1. 検出されたコピペ・重複コードブロック (5行以上の完全一致)】: {len(duplicates)} パターン")

    if not duplicates:
        print("  ✓ 5行以上の重複コードブロックは検出されませんでした（高水準なOAOO達成）。")
    else:
        # 上位の重複を表示
        for i, dup in enumerate(duplicates[:10], 1):
            print(f"\n[重複パターン {i}] ({dup['count']} 箇所で検出):")
            for f_path, l_num in dup["occurrences"][:4]:
                print(f"  - {f_path}:{l_num}")
            if len(dup["occurrences"]) > 4:
                print(f"  - ...他 {len(dup['occurrences']) - 4} 箇所")
            print("  ▼ コード抜粋:")
            for line in dup["block"].splitlines()[:4]:
                print(f"    | {line}")
            if len(dup["block"].splitlines()) > 4:
                print("    | ...")

    print("\n" + "=" * 80)
    status = "合格" if len(duplicates) <= 5 else "要改善"
    print(f"  診断サマリー: 判定 = {status} (検出重複パターン: {len(duplicates)} 件)")
    print("=" * 80)

if __name__ == "__main__":
    main()
