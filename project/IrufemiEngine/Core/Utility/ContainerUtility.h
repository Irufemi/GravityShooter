#pragma once
#include <vector>
#include <algorithm>
#include <utility>
#include <optional>
#include <cstddef>

/**
 * @namespace Irufemi::Container
 * @brief STLコンテナの高速操作（Swap&Pop等）や安全な検索ユーティリティを提供する名前空間
 */
namespace Irufemi::Container {

/**
 * @brief 順序非依存の高速要素削除（Swap & Pop 手法）
 * @details 指定した値を配列から O(1) で削除します。順序は保持されません。
 *          逆順ループ走査中の末尾要素であれば探索なしで即時 pop します。
 * @tparam T 要素型
 * @tparam U 比較対象の型
 * @param vec 対象の std::vector
 * @param value 削除したい値
 * @return 削除された場合は true、見つからなかった場合は false
 */
template <typename T, typename U> bool EraseSwap(std::vector<T>& vec, const U& value) {
    if (vec.empty()) {
        return false;
    }
    // 逆順ループ走査中の末尾要素なら探索不要で即時 pop (完全な O(1))
    if (vec.back() == value) {
        vec.pop_back();
        return true;
    }
    // 末尾以外の場合は末尾要素で上書きして pop_back (シフトなしの O(1))
    auto it = std::find(vec.begin(), vec.end(), value);
    if (it != vec.end()) {
        *it = std::move(vec.back());
        vec.pop_back();
        return true;
    }
    return false;
}

/**
 * @brief イテレータ指定での高速要素削除（Swap & Pop 手法）
 * @details 指定したイテレータ位置を末尾要素で上書きして pop_back します。順序は保持されません。
 * @tparam T 要素型
 * @param vec 対象の std::vector
 * @param it 削除したいイテレータ
 */
template <typename T> void EraseSwap(std::vector<T>& vec, typename std::vector<T>::iterator it) {
    if (vec.empty() || it == vec.end()) {
        return;
    }
    auto lastIt = std::prev(vec.end());
    if (it != lastIt) {
        *it = std::move(*lastIt);
    }
    vec.pop_back();
}

/**
 * @brief 条件一致による順序非依存の高速要素削除（Swap & Pop 手法）
 * @details 述語条件に一致する最初の要素を O(1) で削除します。順序は保持されません。
 * @tparam T 要素型
 * @tparam Predicate 条件述語の型
 * @param vec 対象の std::vector
 * @param pred 判定関数またはラムダ式
 * @return 削除された場合は true、見つからなかった場合は false
 */
template <typename T, typename Predicate> bool EraseSwapIf(std::vector<T>& vec, Predicate pred) {
    if (vec.empty()) {
        return false;
    }
    if (pred(vec.back())) {
        vec.pop_back();
        return true;
    }
    auto it = std::find_if(vec.begin(), vec.end(), pred);
    if (it != vec.end()) {
        *it = std::move(vec.back());
        vec.pop_back();
        return true;
    }
    return false;
}

/**
 * @brief 重複を避けて要素を末尾に追加 (AddUnique)
 * @details 配列内にまだ同一の要素が存在しない場合のみ追加します。
 * @tparam T 要素型
 * @tparam U 比較・追加する要素の型
 * @param vec 対象の std::vector
 * @param value 追加したい値
 * @return 新規に追加された場合は true、既に存在していた場合は false
 */
template <typename T, typename U> bool PushBackUnique(std::vector<T>& vec, U&& value) {
    if (std::find(vec.begin(), vec.end(), value) == vec.end()) {
        vec.push_back(std::forward<U>(value));
        return true;
    }
    return false;
}

/**
 * @brief 配列に特定の値が含まれているか判定 (Contains)
 * @tparam T 要素型
 * @tparam U 検索する値の型
 * @param vec 対象の std::vector
 * @param value 検索したい値
 * @return 含まれている場合は true
 */
template <typename T, typename U> bool Contains(const std::vector<T>& vec, const U& value) {
    return std::find(vec.begin(), vec.end(), value) != vec.end();
}

/**
 * @brief 条件に一致する要素が配列に含まれているか判定 (ContainsIf)
 * @tparam T 要素型
 * @tparam Predicate 条件述語の型
 * @param vec 対象の std::vector
 * @param pred 判定関数またはラムダ式
 * @return 条件に一致する要素が存在する場合は true
 */
template <typename T, typename Predicate> bool ContainsIf(const std::vector<T>& vec, Predicate pred) {
    return std::find_if(vec.begin(), vec.end(), pred) != vec.end();
}

/**
 * @brief 配列内における特定の値のインデックスを取得 (FindIndex)
 * @tparam T 要素型
 * @tparam U 検索する値の型
 * @param vec 対象の std::vector
 * @param value 検索したい値
 * @return 見つかった場合は std::optional<size_t>、見つからなかった場合は std::nullopt
 */
template <typename T, typename U> std::optional<size_t> FindIndex(const std::vector<T>& vec, const U& value) {
    auto it = std::find(vec.begin(), vec.end(), value);
    if (it != vec.end()) {
        return static_cast<size_t>(std::distance(vec.begin(), it));
    }
    return std::nullopt;
}

/**
 * @brief 配列内における特定の値のインデックスを取得 (IndexOf: 互換用)
 * @tparam T 要素型
 * @tparam U 検索する値の型
 * @param vec 対象の std::vector
 * @param value 検索したい値
 * @return 見つかった場合は 0 以上のインデックス、見つからなかった場合は -1
 */
template <typename T, typename U> std::ptrdiff_t IndexOf(const std::vector<T>& vec, const U& value) {
    auto it = std::find(vec.begin(), vec.end(), value);
    return (it != vec.end()) ? std::distance(vec.begin(), it) : -1;
}

} // namespace Irufemi::Container
