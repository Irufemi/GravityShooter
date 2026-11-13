#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
単元 02_02（調整データの外部ファイル化: External Data & Tooling）に基づく自動診断スクリプト
作業場フォルダ（project/）全体の C++ コードおよび外部データファイルをスキャンし、
配置情報・出現情報・バランス調整値の外部ファイル化（JSON/CSV）や
エディタプロパティ公開（OnRegisterProperties）の適合度を診断します。
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

def scan_external_data_coverage(project_root):
    results = {
        "external_data_files": [],       # 検出された外部データファイル (JSON/CSV)
        "json_loaders": [],              # JSON読み込み/パースを行っているC++クラス
        "registered_properties": [],     # OnRegisterProperties で公開されているプロパティ群
        "potential_hardcoded_spawns": [] # C++コード内で直書きされている可能性があるスポーン呼出
    }

    # 1. resources 配下のデータファイル検索
    for ext in ("*.json", "*.csv"):
        for p in project_root.rglob(ext):
            # intermediate/obj/generated は除外
            rel = str(p.relative_to(project_root.parent))
            if "generated" in rel.lower() or "obj" in rel.lower() or ".vscode" in rel.lower():
                continue
            if "gamedata" in rel.lower() or "level" in rel.lower() or "scenes" in rel.lower() or "prefabs" in rel.lower():
                results["external_data_files"].append({
                    "path": rel,
                    "size": p.stat().st_size
                })

    # 2. C++ ソースコード走査
    cpp_files = list(project_root.rglob("*.cpp")) + list(project_root.rglob("*.h"))
    for filepath in cpp_files:
        rel = str(filepath.relative_to(project_root.parent))
        if "externals" in rel.lower() or "thirdparty" in rel.lower():
            continue

        try:
            with open(filepath, "r", encoding="utf-8", errors="ignore") as f:
                content = f.read()
        except Exception:
            continue

        # A. JSON/データロード関数の検出
        if "LoadFromFile" in content or "nlohmann::json" in content or "from_json" in content:
            matches = re.findall(r'(?:Load[A-Za-z0-9_]*FromJson|LoadFromFile|LoadStatus[A-Za-z0-9_]*)', content)
            if matches:
                results["json_loaders"].append({
                    "file": rel,
                    "functions": list(set(matches))
                })

        # B. OnRegisterProperties 実装の検出
        if "OnRegisterProperties" in content and "RegisterProperty" in content:
            props = re.findall(r'RegisterProperty\s*\(\s*"([^"]+)"', content)
            if props:
                results["registered_properties"].append({
                    "file": rel,
                    "count": len(props),
                    "properties": props
                })

        # C. ハードコードされたスポーン/座標アンチパターンの検出 (SpawnEnemy 等に直接リテラル数値が渡されているか)
        hardcoded_spawns = re.findall(r'(?:SpawnEnemy|SpawnWave)\s*\(\s*"[^"]+"\s*,\s*\{[-0-9\.\s,f]+\}', content)
        if hardcoded_spawns:
            for s in hardcoded_spawns:
                results["potential_hardcoded_spawns"].append({
                    "file": rel,
                    "code": s.strip()
                })

    return results

def main():
    if len(sys.argv) > 1:
        target_dir = Path(sys.argv[1]).resolve()
    else:
        target_dir = Path(__file__).resolve().parents[4] / "project"

    print("=" * 80)
    print("  単元 02_02 [調整データの外部ファイル化] ソースコード・データ自動診断")
    print(f"  診断対象ディレクトリ: {target_dir}")
    print("=" * 80)

    if not target_dir.exists():
        print(f"エラー: ディレクトリが見つかりません: {target_dir}")
        sys.exit(1)

    res = scan_external_data_coverage(target_dir)

    print("\n【1. 検出された外部データファイル (JSON/CSV)】")
    if res["external_data_files"]:
        for f in res["external_data_files"]:
            print(f"  ✓ [{f['path']}] ({f['size']} bytes)")
    else:
        print("  ※ 外部データファイルが見つかりません")

    print("\n【2. 外部データ読み込み処理を実装しているC++クラス (Loaders)】")
    if res["json_loaders"]:
        for l in res["json_loaders"]:
            print(f"  ✓ [{l['file']}]: {', '.join(l['functions'])}")
    else:
        print("  ※ 該当なし")

    print("\n【3. エディタ公開プロパティを登録しているコンポーネント (OnRegisterProperties)】")
    if res["registered_properties"]:
        for p in res["registered_properties"]:
            print(f"  ✓ [{p['file']}]: {p['count']} 個のプロパティ登録 ({', '.join(p['properties'][:5])}{' ...' if p['count'] > 5 else ''})")
    else:
        print("  ※ 該当なし")

    print("\n【4. ハードコードされた敵・オブジェクト配置 (アンチパターン検出)】")
    if res["potential_hardcoded_spawns"]:
        for h in res["potential_hardcoded_spawns"]:
            print(f"  ▲ [{h['file']}]: {h['code']}")
    else:
        print("  ✓ なし (すべて外部データ・スクリプト・動的配置で制御されています)")

    print("\n" + "=" * 80)
    score_status = "合格 (優良水準)" if (len(res["external_data_files"]) > 0 and len(res["potential_hardcoded_spawns"]) == 0) else "要改善"
    print(f"  診断サマリー: 判定 = {score_status}")
    print(f"  - 外部データファイル数: {len(res['external_data_files'])} 件")
    print(f"  - データローダー実装クラス: {len(res['json_loaders'])} 件")
    print(f"  - エディタ調整プロパティ公開コンポーネント: {len(res['registered_properties'])} 件")
    print(f"  - ハードコード配置検出: {len(res['potential_hardcoded_spawns'])} 件")
    print("=" * 80)

if __name__ == "__main__":
    main()

