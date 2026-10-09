#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
単元 02_01（データドリブン: Data-driven）に基づく自動診断スクリプト
作業場フォルダ（project/）全体の C++ コードをスキャンし、
値のみ異なる余分な if-else や switch 分岐（データドリブン化の候補）を検出します。
また、外部データ（JSON等）やデータテーブルを活用している優良設計箇所も合わせて評価します。
"""

import os
import re
import sys
from pathlib import Path

if sys.platform == "win32":
    try:
        sys.stdout.reconfigure(encoding="utf-8")
    except Exception:
        pass

def scan_file_for_data_driven(filepath):
    results = {
        "enum_to_string_switches": [],   # switch で文字列リテラルを返している箇所
        "factory_switches": [],          # switch で make_unique / new している箇所
        "ui_presentation_switches": [],  # switch で UI 色や文字列を表示している箇所
        "general_value_switches": [],    # switch で値代入のみ行っている箇所
        "data_tables": [],               # 優良設計: データテーブル構造体 / JSON 活用
    }

    try:
        with open(filepath, "r", encoding="utf-8", errors="ignore") as f:
            lines = f.readlines()
            content = "".join(lines)
    except Exception:
        return results

    # 1. 優良設計の検出: JSON 駆動・データテーブル構造体
    if "json" in content.lower() and ("parameters" in content or "loaddata" in content.lower() or "from_json" in content):
        results["data_tables"].append({
            "line": 1,
            "type": "JSON 外部データ連携",
            "desc": "JSON パラメータによるデータ駆動型処理を検出"
        })

    # 2. switch ブロックの解析
    switch_regex = re.compile(r"switch\s*\(([^)]+)\)\s*\{", re.MULTILINE)
    for m in switch_regex.finditer(content):
        switch_start = m.start()
        line_num = content[:switch_start].count("\n") + 1
        var_name = m.group(1).strip()

        # switch ブロックの終端を探す（簡易波括弧カウント）
        brace_count = 1
        idx = m.end()
        block_lines = []
        while idx < len(content) and brace_count > 0:
            ch = content[idx]
            if ch == '{':
                brace_count += 1
            elif ch == '}':
                brace_count -= 1
            idx += 1
        block_text = content[m.end():idx-1]
        block_lines = block_text.splitlines()

        # ケース数をカウント
        cases = re.findall(r"\bcase\b", block_text)
        if len(cases) < 2:
            continue

        # A. Enum-to-String パターン: 各 case で return "..." している
        return_strings = re.findall(r'return\s+"([^"]+)";', block_text)
        if len(return_strings) >= 2 and len(return_strings) >= len(cases) - 1:
            results["enum_to_string_switches"].append({
                "line": line_num,
                "var": var_name,
                "cases_count": len(cases),
                "preview": f"return 文字列リテラル ({len(return_strings)} 箇所)"
            })
            continue

        # B. Factory パターン: make_unique / new している
        factory_matches = re.findall(r'(?:std::make_unique|new\s+[A-Za-z0-9_]+)', block_text)
        if len(factory_matches) >= 2:
            results["factory_switches"].append({
                "line": line_num,
                "var": var_name,
                "cases_count": len(cases),
                "preview": f"具象インスタンス生成 ({len(factory_matches)} 箇所)"
            })
            continue

        # C. UI / 表示パターン: ImGui::TextColored / Text 等を呼んでいる
        imgui_matches = re.findall(r'ImGui::(?:TextColored|Text|TextWrapped)', block_text)
        if len(imgui_matches) >= 2:
            results["ui_presentation_switches"].append({
                "line": line_num,
                "var": var_name,
                "cases_count": len(cases),
                "preview": f"ImGui テキスト・カラー出力 ({len(imgui_matches)} 箇所)"
            })
            continue

        # D. 一般的なパラメータ代入パターン (各 case 内に 1〜2行の代入・関数呼出しかない)
        # 行数が cases * 4 未満なら単純な値分岐の可能性が高い
        if len(block_lines) <= len(cases) * 5:
            results["general_value_switches"].append({
                "line": line_num,
                "var": var_name,
                "cases_count": len(cases),
                "preview": f"パラメータ分岐 ({len(cases)} cases)"
            })

    return results

def main():
    workspace_root = Path(__file__).resolve().parents[4]
    project_dir = workspace_root / "project"

    if not project_dir.exists():
        print(f"Error: Project directory not found: {project_dir}")
        sys.exit(1)

    print("================================================================================")
    print("  単元 02_01 [データドリブン (Data-driven)] ソースコード自動診断")
    print(f"  診断対象ディレクトリ: {project_dir}")
    print("================================================================================\n")

    ignored_dirs = {"externals", "thirdparty", "generated", "imgui", ".git", ".vs"}
    target_exts = {".cpp", ".h"}

    all_enum_strings = []
    all_factories = []
    all_ui_switches = []
    all_general_switches = []
    all_data_tables = []

    for root, dirs, files in os.walk(project_dir):
        # 除外ディレクトリのスキップ
        dirs[:] = [d for d in dirs if d.lower() not in ignored_dirs]
        for f in files:
            if Path(f).suffix.lower() in target_exts:
                full_path = Path(root) / f
                rel_path = full_path.relative_to(project_dir)
                res = scan_file_for_data_driven(full_path)

                for item in res["enum_to_string_switches"]:
                    all_enum_strings.append((rel_path, item))
                for item in res["factory_switches"]:
                    all_factories.append((rel_path, item))
                for item in res["ui_presentation_switches"]:
                    all_ui_switches.append((rel_path, item))
                for item in res["general_value_switches"]:
                    all_general_switches.append((rel_path, item))
                for item in res["data_tables"]:
                    all_data_tables.append((rel_path, item))

    # レポート出力
    print("【1. 優良設計: 外部データ・データテーブル活用箇所 (Good Patterns)】")
    if all_data_tables:
        for path, item in all_data_tables:
            print(f"  ✓ [{path}] {item['type']} - {item['desc']}")
    else:
        print("  なし")
    print()

    print("【2. 要改善: Enum ↔ 文字列変換の switch 分岐 (constexpr 配列化推奨)】")
    if all_enum_strings:
        for path, item in all_enum_strings:
            print(f"  ▲ [{path}:{item['line']}] switch ({item['var']}) - {item['cases_count']} cases ({item['preview']})")
    else:
        print("  なし (合格)")
    print()

    print("【3. 要改善: UI 表示・テキスト・カラーの switch 分岐 (UIテーブル引き推奨)】")
    if all_ui_switches:
        for path, item in all_ui_switches:
            print(f"  ▲ [{path}:{item['line']}] switch ({item['var']}) - {item['cases_count']} cases ({item['preview']})")
    else:
        print("  なし (合格)")
    print()

    print("【4. 改善検討: 具象クラス生成の switch 分岐 (ファクトリ登録マップ推奨)】")
    if all_factories:
        for path, item in all_factories:
            print(f"  ▲ [{path}:{item['line']}] switch ({item['var']}) - {item['cases_count']} cases ({item['preview']})")
    else:
        print("  なし (合格)")
    print()

    print("【5. その他: 単純パラメータ分岐の可能性がある switch】")
    if all_general_switches:
        for path, item in all_general_switches:
            print(f"  ・ [{path}:{item['line']}] switch ({item['var']}) - {item['cases_count']} cases")
    else:
        print("  なし")
    print()

    total_candidates = len(all_enum_strings) + len(all_ui_switches) + len(all_factories)
    print("================================================================================")
    print(f"  診断サマリー: データドリブン改善推奨箇所 計 {total_candidates} 件")
    print("================================================================================")

if __name__ == "__main__":
    main()
