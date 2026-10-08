#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
単元 01_01（カプセル化）に基づく自動診断スクリプト
作業場フォルダ（project/）全体の C++ ソース・ヘッダーをスキャンし、
アンチパターン（publicメンバ変数、非const参照Getter、friend宣言等）を検出します。
"""

import os
import re
import sys
from pathlib import Path

# Windows 環境でのコンソール文字化け防止
if sys.platform == "win32":
    try:
        sys.stdout.reconfigure(encoding="utf-8")
    except Exception:
        pass

def scan_file(filepath):
    results = {
        "public_vars": [],
        "non_const_ref_getters": [],
        "friends": []
    }

    try:
        with open(filepath, "r", encoding="utf-8", errors="ignore") as f:
            lines = f.readlines()
    except Exception as e:
        return results

    in_class = False
    class_name = ""
    current_access = "private" # C++ class のデフォルトは private
    brace_depth = 0
    class_brace_depth = 0

    # 単純パターンの正規表現
    getter_ref_regex = re.compile(r"^\s*(?:inline\s+)?(?:virtual\s+)?([A-Za-z0-9_:<>\*]+)\s*&\s+([A-Za-z0-9_]+)\s*\((.*?)\)(?:\s*const)?\s*(\{|;)")
    friend_regex = re.compile(r"^\s*friend\s+(?:class\s+)?([A-Za-z0-9_:]+)")

    for line_idx, line in enumerate(lines, 1):
        stripped = line.strip()

        # コメント行スキップ
        if stripped.startswith("//") or stripped.startswith("/*") or stripped.startswith("*"):
            continue

        # クラス定義開始の検出 (class Name : ... { または class Name {)
        class_match = re.match(r"^\s*class\s+(?:[A-Za-z0-9_]+\s+)?([A-Za-z0-9_]+)\s*(?::|{|\n|$)", line)
        if class_match and not stripped.endswith(";") and not "friend class" in line:
            in_class = True
            class_name = class_match.group(1)
            current_access = "private"
            class_brace_depth = brace_depth

        # 波括弧の深さ管理
        opens = line.count("{")
        closes = line.count("}")
        prev_depth = brace_depth
        brace_depth += opens - closes

        if in_class and brace_depth <= class_brace_depth:
            in_class = False
            class_name = ""
            current_access = "private"

        # アクセス指定子の検出
        if in_class:
            if re.match(r"^\s*public\s*:", line):
                current_access = "public"
                continue
            elif re.match(r"^\s*private\s*:", line):
                current_access = "private"
                continue
            elif re.match(r"^\s*protected\s*:", line):
                current_access = "protected"
                continue

            # アンチパターン①: public メンバ変数
            # クラスの直下（brace_depth == class_brace_depth + 1）にあり、関数内ではない行のみを判定
            is_directly_in_class = (brace_depth == class_brace_depth + 1 and closes == 0) or (prev_depth == class_brace_depth + 1 and closes > 0)
            if current_access == "public" and is_directly_in_class:
                # 関数宣言・定義、マクロ、型エイリアス等を除外
                if (";" in line and "(" not in line and ")" not in line and
                    not stripped.startswith("using ") and not stripped.startswith("typedef ") and
                    not stripped.startswith("enum ") and not stripped.startswith("static constexpr ") and
                    not stripped.startswith("static const ") and not stripped.startswith("friend ") and
                    not stripped.startswith("template") and not stripped.startswith("struct ") and
                    not stripped.startswith("return ") and not stripped.startswith("case ") and
                    not stripped.startswith("default:") and not stripped.startswith("break;") and
                    not stripped.startswith("continue;")):
                    
                    results["public_vars"].append({
                        "line": line_idx,
                        "class": class_name,
                        "content": stripped
                    })

            # アンチパターン③: friend 宣言
            f_match = friend_regex.search(line)
            if f_match and not stripped.startswith("//"):
                friend_target = f_match.group(1)
                results["friends"].append({
                    "line": line_idx,
                    "class": class_name if in_class else "Global",
                    "target": friend_target,
                    "content": stripped
                })

        # アンチパターン②: 非const参照を返す Getter（クラス内外問わず走査）
        g_match = getter_ref_regex.search(line)
        if g_match:
            ret_type = g_match.group(1).strip()
            func_name = g_match.group(2).strip()
            params = g_match.group(3).strip()
            is_const = "const" in line.split(")")[-1] if ")" in line else False

            # const参照でなく、かつGetで始まる関数
            if not ret_type.startswith("const ") and func_name.startswith("Get"):
                results["non_const_ref_getters"].append({
                    "line": line_idx,
                    "func": func_name,
                    "type": ret_type + "&",
                    "is_const": is_const,
                    "content": stripped
                })

    return results

def main():
    root_dir = Path(__file__).resolve().parents[4] / "project"
    if not root_dir.exists():
        root_dir = Path.cwd() / "project"

    # 対象ディレクトリ（自作コードのみに限定）
    target_dirs = [
        root_dir / "IrufemiEngine",
        root_dir / "Application_solo",
        root_dir / "IrufemiEditor"
    ]

    print(f"=== [01_01 カプセル化] ソースレビュー診断開始 ===")
    
    total_files = 0
    total_public_vars = 0
    total_non_const_getters = 0
    total_friends = 0

    for t_dir in target_dirs:
        if not t_dir.exists():
            continue
        for ext in ("*.h", "*.hpp", "*.cpp"):
            for filepath in t_dir.rglob(ext):
                # externals などの外部コードを除外
                if "externals" in filepath.parts or "thirdparty" in filepath.parts:
                    continue

                total_files += 1
                res = scan_file(filepath)
                rel_path = filepath.relative_to(root_dir)

                if res["public_vars"]:
                    print(f"\n[アンチパターン1: public メンバ変数] {rel_path}:")
                    for item in res["public_vars"]:
                        print(f"  Line {item['line']} (class {item['class']}): {item['content']}")
                        total_public_vars += 1

                if res["non_const_ref_getters"]:
                    print(f"\n[アンチパターン2: 非const参照 Getter] {rel_path}:")
                    for item in res["non_const_ref_getters"]:
                        print(f"  Line {item['line']}: {item['content']}")
                        total_non_const_getters += 1

                if res["friends"]:
                    print(f"\n[アンチパターン3: friend 宣言] {rel_path}:")
                    for item in res["friends"]:
                        print(f"  Line {item['line']} (in {item['class']}): {item['content']}")
                        total_friends += 1

    print("\n" + "=" * 50)
    print(f"診断完了 (対象ファイル数: {total_files})")
    print(f"- public メンバ変数: {total_public_vars} 件")
    print(f"- 非const参照 Getter: {total_non_const_getters} 件")
    print(f"- friend 宣言: {total_friends} 件")
    print("=" * 50)

if __name__ == "__main__":
    main()
