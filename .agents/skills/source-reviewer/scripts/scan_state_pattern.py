#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
単元 01_02（State Pattern）に基づく自動診断スクリプト
作業場フォルダ（project/）全体の C++ コードをスキャンし、
enum と switch-case による状態管理箇所（State Pattern 未適用リスク箇所）を検出します。
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

def scan_file_for_states(filepath):
    results = {
        "state_enums": [],     # {name, line, count, values}
        "switch_states": [],   # {line, var_name, cases_count}
        "state_pattern_classes": [] # {line, class_name}
    }

    try:
        with open(filepath, "r", encoding="utf-8", errors="ignore") as f:
            content = f.read()
    except Exception:
        return results

    # 1. State Pattern 適用済みクラスの検出 (class ... : public IState 等)
    state_class_matches = re.finditer(r"class\s+([A-Za-z0-9_]+)\s*:\s*public\s+(?:IState|[A-Za-z0-9_]*State)", content)
    for m in state_class_matches:
        line_num = content[:m.start()].count("\n") + 1
        results["state_pattern_classes"].append({
            "line": line_num,
            "class_name": m.group(1)
        })

    # 2. enum / enum class の検出 (State, Phase, Mode, Status, Action などを含むもの)
    enum_regex = re.compile(r"enum(?:\s+class)?\s+([A-Za-z0-9_]*(?:State|Phase|Mode|Status|Action|Step)[A-Za-z0-9_]*)\s*(?::\s*[A-Za-z0-9_]+\s*)?\{([^}]+)\}", re.DOTALL)
    for m in enum_regex.finditer(content):
        enum_name = m.group(1)
        body = m.group(2)
        line_num = content[:m.start()].count("\n") + 1
        
        # 項目数をカウント
        items = [item.strip() for item in body.split(",") if item.strip() and not item.strip().startswith("//")]
        results["state_enums"].append({
            "line": line_num,
            "name": enum_name,
            "count": len(items),
            "values": items[:5] # 最大5個プレビュー
        })

    # 3. switch (state) / switch (phase) 分岐の検出
    switch_regex = re.compile(r"switch\s*\(\s*([A-Za-z0-9_]*(?:state|phase|mode|status)[A-Za-z0-9_]*)\s*\)\s*\{", re.IGNORECASE)
    for m in switch_regex.finditer(content):
        var_name = m.group(1)
        line_num = content[:m.start()].count("\n") + 1
        
        # switch文内部の case 数を簡易カウント
        # switch の開始位置から次の対応する } までを探す
        start_idx = m.end()
        brace_count = 1
        sub_content = ""
        for i in range(start_idx, len(content)):
            if content[i] == '{':
                brace_count += 1
            elif content[i] == '}':
                brace_count -= 1
                if brace_count == 0:
                    sub_content = content[start_idx:i]
                    break
        
        case_matches = re.findall(r"\bcase\b", sub_content)
        results["switch_states"].append({
            "line": line_num,
            "var_name": var_name,
            "case_count": len(case_matches)
        })

    return results

def main():
    root_dir = Path(__file__).resolve().parent.parent.parent.parent.parent
    project_dir = root_dir / "project"

    if not project_dir.exists():
        print(f"Error: {project_dir} does not exist.")
        sys.exit(1)

    target_subdirs = ["IrufemiEngine", "Application_solo", "IrufemiEditor"]
    all_files = []
    for sub in target_subdirs:
        subpath = project_dir / sub
        if subpath.exists():
            for ext in ("*.h", "*.hpp", "*.cpp", "*.cc"):
                all_files.extend(list(subpath.rglob(ext)))

    print("=" * 80)
    print(" [単元 01_02 State Pattern] 先生・AI自動診断リスク事前スキャン")
    print(f" 走査対象ディレクトリ: {project_dir}")
    print(f" 対象ファイル総数: {len(all_files)} 件")
    print("=" * 80)

    total_enums = 0
    total_switches = 0
    total_pattern_classes = 0

    high_risk_candidates = []

    for f in all_files:
        # externals 等の除外
        if "externals" in str(f) or "thirdparty" in str(f):
            continue

        res = scan_file_for_states(f)
        rel_path = f.relative_to(project_dir)

        if res["state_pattern_classes"]:
            total_pattern_classes += len(res["state_pattern_classes"])

        if res["switch_states"]:
            for sw in res["switch_states"]:
                total_switches += 1
                # case数が4つ以上は「状態3つ以下」の例外規定を外れるため高リスク
                risk = "高 (case 4個以上: StatePattern強く推奨)" if sw["case_count"] >= 4 else "低 (case 3個以下: 単純状態例外の可能性)"
                high_risk_candidates.append({
                    "file": str(rel_path),
                    "line": sw["line"],
                    "var": sw["var_name"],
                    "cases": sw["case_count"],
                    "risk": risk
                })

        if res["state_enums"]:
            total_enums += len(res["state_enums"])

    print("\n--- スキャン集計結果 ---")
    print(f"・検出された State Pattern 適用クラス: {total_pattern_classes} 件")
    print(f"・検出された State/Phase 系 enum 定義: {total_enums} 件")
    print(f"・検出された switch (state) 等の分岐箇所: {total_switches} 件")

    print("\n--- 先生・AI診断で「設計不足」と指摘されるリスクがある箇所 ---")
    if high_risk_candidates:
        # 高リスク順（case数順）にソート
        high_risk_candidates.sort(key=lambda x: x["cases"], reverse=True)
        for cand in high_risk_candidates:
            print(f"[{cand['risk']}] {cand['file']}:{cand['line']} - switch({cand['var']}) [case数: {cand['cases']}]")
    else:
        print("高リスクな未適用 switch 分岐は検出されませんでした！")

    print("\n" + "=" * 80)

if __name__ == "__main__":
    main()
