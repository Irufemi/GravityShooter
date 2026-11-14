import os
import re
import sys
from pathlib import Path

def scan_engine_namespace(project_dir: Path):
    engine_dir = project_dir / "project" / "IrufemiEngine"
    all_project_dir = project_dir / "project"

    print("=" * 80)
    print("  単元 02_07 [エンジンnamespace (Engine Namespace)] 自動診断レポート")
    print(f"  診断対象ディレクトリ: {project_dir}")
    print("=" * 80)
    print()

    # 1. ヘッダーファイル内の using namespace 汚染チェック (最優先)
    header_pollutions = []
    for p in all_project_dir.rglob("*.h"):
        if "externals" in p.parts:
            continue
        try:
            lines = p.read_text(encoding="utf-8", errors="ignore").splitlines()
        except Exception:
            continue
        for idx, line in enumerate(lines, 1):
            s = line.strip()
            if s.startswith("//") or s.startswith("/*") or s.startswith("*"):
                continue
            if re.search(r"^\s*using\s+namespace\s+[A-Za-z0-9_:]+\s*;", s):
                header_pollutions.append((p.relative_to(project_dir), idx, s))

    print(f"【1. ヘッダーファイルでの using namespace 汚染】: {len(header_pollutions)} 件")
    if header_pollutions:
        print("  ※【重大な減点対象】ヘッダーでの using namespace は全インクルード先を名前空間汚染します：")
        for loc, idx, code in header_pollutions:
            print(f"  - {loc}:{idx} -> {code}")
    else:
        print("  -> 合格！すべてのヘッダーで using namespace は使用されていません（名前空間汚染 0 件）。")
    print()

    # 2. IrufemiEngine 内のクラスが namespace に属しているかのチェック
    unnamespaced_engine_headers = []
    namespaced_engine_headers = []

    if engine_dir.exists():
        for p in engine_dir.rglob("*.h"):
            try:
                content = p.read_text(encoding="utf-8", errors="ignore")
            except Exception:
                continue
            rel = p.relative_to(project_dir)
            # クラスまたは構造体の宣言があるか
            has_type = re.search(r"\b(?:class|struct)\s+[A-Za-z0-9_]+\b", content)
            if not has_type:
                continue

            # namespace で囲まれているか
            # コメント以外の namespace 定義を探す
            cleaned = re.sub(r"//.*", "", content)
            cleaned = re.sub(r"/\*.*?\*/", "", cleaned, flags=re.DOTALL)
            if re.search(r"\bnamespace\s+(?:Irufemi|IrufemiEngine)\b", cleaned):
                namespaced_engine_headers.append(rel)
            else:
                unnamespaced_engine_headers.append(rel)

        total_engine = len(namespaced_engine_headers) + len(unnamespaced_engine_headers)
        coverage = (len(namespaced_engine_headers) / total_engine * 100.0) if total_engine > 0 else 100.0

        print(f"【2. IrufemiEngine 層の名前空間 (namespace Irufemi) 所属率】: {coverage:.1f}% ({len(namespaced_engine_headers)} / {total_engine} ヘッダー)")
        if unnamespaced_engine_headers:
            print("  ※以下のエンジンクラスはまだグローバル名前空間に定義されています。順次 `namespace Irufemi` 内へ移行推奨：")
            for h in unnamespaced_engine_headers[:15]:
                print(f"  - {h}")
            if len(unnamespaced_engine_headers) > 15:
                print(f"  ... 他 {len(unnamespaced_engine_headers) - 15} 件省略")
        else:
            print("  -> 完璧です！すべてのエンジンクラスが namespace Irufemi 内に正しく定義されています。")
    print()

    print("=" * 80)
    if header_pollutions:
        print("  診断結果: 【要修正】ヘッダーファイルでの名前空間汚染が検出されました。最優先で解消してください。")
    elif unnamespaced_engine_headers:
        print("  診断結果: 【改善推奨】ヘッダー汚染は 0 件です。エンジン層クラスの名前空間所属率を高める移行が推奨されます。")
    else:
        print("  診断結果: 【完全合格】ヘッダー汚染 0 件、エンジン層の namespace 所属 100% を達成しています！")
    print("=" * 80)

if __name__ == "__main__":
    current = Path.cwd()
    scan_engine_namespace(current)
