#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
単元 02_04 [引数のconst参照] 自動診断スクリプト
- 関数定義・宣言における「64bitを超える型の無駄な値渡し（Needless Copy）」の検出
- 関数定義・宣言における「64bit以下の型に対する無意味なconst参照（就活減点対象）」の検出
"""

import sys
import re
from pathlib import Path
from typing import List, Dict, Tuple

# 64bit (8バイト) を超える代表的な型名
LARGE_TYPES = {
    # 数学・ジオメトリ (12B以上)
    "Vector3", "Irufemi::Vector3", "Math::Vector3",
    "Vector4", "Irufemi::Vector4", "Math::Vector4",
    "Matrix4x4", "Irufemi::Matrix4x4", "Math::Matrix4x4",
    "Quaternion", "Irufemi::Quaternion", "Math::Quaternion",
    "AABB", "OBB", "Sphere",
    "Color", "Irufemi::Color",
    # コンテナ・文字列
    "string", "std::string",
    "wstring", "std::wstring",
    "json", "nlohmann::json",
}

# 64bit (8バイト) 以下の代表的なプリミティブ型
SMALL_TYPES = {
    "bool", "char", "signed char", "unsigned char",
    "short", "unsigned short", "int", "unsigned int",
    "long", "unsigned long", "long long", "unsigned long long",
    "float", "double",
    "int8_t", "uint8_t", "int16_t", "uint16_t",
    "int32_t", "uint32_t", "int64_t", "uint64_t",
    "size_t", "uintptr_t", "intptr_t",
}

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

def is_likely_function_decl_or_def(line_prefix: str) -> bool:
    """丸括弧の前が関数名および戻り値のパターンか判定（if, for, while, switch などを除外）"""
    prefix = line_prefix.strip()
    if not prefix:
        return False
    # 制御構文キーワードの除外
    control_keywords = {"if", "else if", "while", "for", "switch", "catch", "return"}
    tokens = re.findall(r'\b[A-Za-z0-9_:]+\b', prefix)
    if not tokens:
        return False
    if tokens[-1] in control_keywords or tokens[0] in control_keywords:
        return False
    # 代入文（= を含む）やメンバ呼び出し（-> や .）を除外
    if '=' in prefix or '->' in prefix or '.' in prefix:
        return False
    # 式・文としての関数呼び出し（例: Base::Func(arg);）を除外
    # 関数定義なら末尾が { か、クラス宣言内のプロトタイプ型指定があるはず
    if line_prefix.strip().endswith(';') and '::' in line_prefix:
        # クラス外でのプロトタイプ再宣言は通常ないため呼び出し文とみなす
        return False
    return True

def scan_file(file_path: Path) -> Tuple[List[Dict], List[Dict]]:
    needless_copies = []
    meaningless_const_refs = []

    try:
        raw_text = file_path.read_text(encoding="utf-8")
    except UnicodeDecodeError:
        try:
            raw_text = file_path.read_text(encoding="shift_jis")
        except Exception:
            return [], []

    clean_text = remove_comments_and_strings(raw_text)
    lines = clean_text.splitlines()

    # 関数シグネチャの正規表現: [戻り値型/修飾子] [関数名]([引数リスト]) [const/override等] [;|{]
    # 例: void SetPos(Vector3 pos);
    # 例: virtual bool CheckCollision(const AABB& a, Vector3 b) const;
    func_sig_pattern = re.compile(
        r'^(?P<prefix>[^;{}()]*?\b(?P<func_name>[A-Za-z0-9_~:]+))\s*\((?P<params>[^)]*)\)\s*(?P<suffix>[A-Za-z0-9_\s&*:=-]*)(?P<ending>[;{=])'
    )

    for line_idx, line in enumerate(lines, start=1):
        stripped = line.strip()
        if not stripped or stripped.startswith('#'):
            continue

        m = func_sig_pattern.search(stripped)
        if not m:
            continue

        prefix = m.group("prefix")
        ending = m.group("ending")
        if not is_likely_function_decl_or_def(prefix):
            continue

        # 例: Component::Deserialize(json); のような基底呼び出し文を除外
        # （戻り値の型がなく、いきなり Scope::Func で始まってセミコロンで終わる文）
        prefix_tokens = prefix.split()
        if ending == ';' and len(prefix_tokens) == 1 and '::' in prefix_tokens[0]:
            continue

        params_str = m.group("params").strip()
        if not params_str or params_str == "void":
            continue

        # カンマで引数を分割（テンプレートネスト考慮）
        param_tokens = []
        depth = 0
        curr = []
        for ch in params_str:
            if ch in '<(':
                depth += 1
            elif ch in '>)':
                depth -= 1
            elif ch == ',' and depth == 0:
                param_tokens.append(''.join(curr).strip())
                curr = []
                continue
            curr.append(ch)
        if curr:
            param_tokens.append(''.join(curr).strip())

        for param in param_tokens:
            param_clean = re.sub(r'=.*$', '', param).strip()
            if not param_clean:
                continue

            # 引数が式（四則演算やラムダ呼び出しなど）なら関数シグネチャではないためスキップ
            if any(op in param_clean for op in ['+', '-', '*', '/', '?', ':', '->', '&&', '||']):
                # ポインタの * や参照の & は除外してチェック
                clean_no_ptr = param_clean.replace('*', '').replace('&', '')
                if any(op in clean_no_ptr for op in ['+', '-', '/', '?', ':']):
                    continue

            # 1. 64bit以下の型に対する無意味な const 参照 (Meaningless const-Ref)
            # 例: const float&, const int&, const bool&
            for st in SMALL_TYPES:
                # const [st]& または [st] const&
                pattern_small = rf'\bconst\s+{re.escape(st)}\s*&|\b{re.escape(st)}\s+const\s*&'
                if re.search(pattern_small, param_clean):
                    meaningless_const_refs.append({
                        "file": file_path,
                        "line": line_idx,
                        "param": param_clean,
                        "type": st,
                        "code": line.strip()
                    })

            # 2. 64bit超の型の無駄な値渡し (Needless Copy)
            is_ref = '&' in param_clean
            is_ptr = '*' in param_clean
            if not is_ref and not is_ptr:
                words = re.findall(r'\b[A-Za-z0-9_:]+\b', param_clean)
                # std::vector, std::map などのコンテナ
                if re.search(r'\bstd::(vector|map|unordered_map|set|list|deque)\s*<', param_clean):
                    needless_copies.append({
                        "file": file_path,
                        "line": line_idx,
                        "param": param_clean,
                        "type": "STL Container",
                        "code": line.strip()
                    })
                else:
                    for word in words:
                        if word in LARGE_TYPES:
                            needless_copies.append({
                                "file": file_path,
                                "line": line_idx,
                                "param": param_clean,
                                "type": word,
                                "code": line.strip()
                            })
                            break

    return needless_copies, meaningless_const_refs

def main():
    target_dir = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(".")
    if not target_dir.exists():
        print(f"Error: Directory '{target_dir}' does not exist.")
        sys.exit(1)

    all_needless_copies = []
    all_meaningless_const_refs = []

    for ext in ("*.h", "*.hpp", "*.cpp"):
        for path in target_dir.rglob(ext):
            if any(part in EXCLUDE_DIRS for part in path.parts):
                continue
            copies, const_refs = scan_file(path.resolve())
            all_needless_copies.extend(copies)
            all_meaningless_const_refs.extend(const_refs)

    print("=" * 80)
    print("  単元 02_04 [引数のconst参照 (Pass by const-Reference)] 自動診断レポート")
    print(f"  診断対象ディレクトリ: {target_dir.resolve()}")
    print("=" * 80)
    print()

    print(f"【1. 64bit超の型の無駄な値渡し (Needless Copy)】: {len(all_needless_copies)} 件")
    if all_needless_copies:
        print("  ※以下の関数引数は 64bit を超えているため、const参照 (const T&) に変更すべきです：")
        for item in all_needless_copies[:20]:
            rel_path = item['file'].relative_to(target_dir.resolve())
            print(f"  - {rel_path}:{item['line']}")
            print(f"    引数: {item['param']} (型: {item['type']})")
            print(f"    宣言: {item['code']}")
        if len(all_needless_copies) > 20:
            print(f"    ... 他 {len(all_needless_copies) - 20} 件省略")
    else:
        print("  ✓ 検出なし: 64bitを超える型の無駄な値渡しは見つかりませんでした。")

    print()
    print(f"【2. 64bit以下の型に対する無意味なconst参照 (Meaningless const-Ref)】: {len(all_meaningless_const_refs)} 件")
    if all_meaningless_const_refs:
        print("  ※以下の関数引数は 64bit 以下のプリミティブ型です。const参照にすると間接参照コストが増加し、")
        print("    『理解せず使っている』として就活減点対象になります。値渡し (T) に変更すべきです：")
        for item in all_meaningless_const_refs[:20]:
            rel_path = item['file'].relative_to(target_dir.resolve())
            print(f"  - {rel_path}:{item['line']}")
            print(f"    引数: {item['param']} (型: {item['type']})")
            print(f"    宣言: {item['code']}")
        if len(all_meaningless_const_refs) > 20:
            print(f"    ... 他 {len(all_meaningless_const_refs) - 20} 件省略")
    else:
        print("  ✓ 検出なし: 64bit以下の型に対する無意味なconst参照は見つかりませんでした。")

    print()
    print("=" * 80)
    total_issues = len(all_needless_copies) + len(all_meaningless_const_refs)
    if total_issues == 0:
        print("  診断結果: 【合格】すべての関数引数が 64bit 境界に基づき最適に受け渡されています。")
    else:
        print(f"  診断結果: 【要改善】合計 {total_issues} 件の引数受け渡し改善候補が見つかりました。")
    print("=" * 80)

if __name__ == "__main__":
    main()
