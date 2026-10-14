#pragma once
#include <string>
#include <variant>
#include <unordered_map>
#include <functional>
#include <memory>
#include <type_traits>

#include "Core/Utility/Synchronized.h"

namespace Irufemi {

// ---------------------------------------------------------
// CVar (Console Variable) Data Structure
// ---------------------------------------------------------
struct CVar {
    enum class Type { Int, Float, Bool, String };

    std::string name;
    std::string description;
    Type type;

    std::variant<int, float, bool, std::string> value;
    std::variant<int, float, bool, std::string> defaultValue;

    // Optional callback when value changes
    std::function<void()> onChangeCallback;

    CVar() = default;
};

// ---------------------------------------------------------
// CVarSystem (Static Registry)
// ---------------------------------------------------------
class CVarSystem {
public:
    using RegistryType = std::unordered_map<std::string, std::unique_ptr<CVar>>;

    // マクロから自動登録される関数
    static void RegisterInt(const std::string& name, int defaultValue, const std::string& description);
    static void RegisterFloat(const std::string& name, float defaultValue, const std::string& description);
    static void RegisterBool(const std::string& name, bool defaultValue, const std::string& description);
    static void RegisterString(const std::string& name, const std::string& defaultValue,
                               const std::string& description);

    // 取得
    static int GetInt(const std::string& name);
    static float GetFloat(const std::string& name);
    static bool GetBool(const std::string& name);
    static std::string GetString(const std::string& name);

    // 更新
    static void SetInt(const std::string& name, int value);
    static void SetFloat(const std::string& name, float value);
    static void SetBool(const std::string& name, bool value);
    static void SetString(const std::string& name, const std::string& value);

    // コールバックの紐付け
    static void SetOnChangeCallback(const std::string& name, std::function<void()> callback);

    // ファイルI/O
    static void Load(const std::string& filepath);
    static void Save(const std::string& filepath);

    // スレッドセーフな内部辞書の取得
    static Synchronized<RegistryType>& GetSynchronizedRegistry();

    // 内部辞書の取得 (後方互換用)
    static RegistryType& GetRegistry();

    /**
     * @brief 登録されている全CVarを排他ロック下で安全に走査します（DebugUI用など）
     * @tparam Func 呼び出し可能オブジェクト型 (void(const std::string&, const CVar&))
     * @param func 各CVarを受け取る関数またはラムダ式
     */
    template <typename Func>
    static void ForEachCVar(Func&& func) {
        GetSynchronizedRegistry().WithRead([&](const RegistryType& reg) {
            for (const auto& [name, cvar] : reg) {
                if (cvar) {
                    func(name, *cvar);
                }
            }
        });
    }

    // CVarポインタの直接取得 (nullptr if not found)
    static CVar* GetCVar(const std::string& name);
};

// ---------------------------------------------------------
// 高速キャッシュアクセサ (O(1) Direct Access)
// ---------------------------------------------------------
template <typename T> class AutoCVarRef {
public:
    explicit AutoCVarRef(const std::string& name) : name_(name) {}

    T Get() {
        if (!cachedCVar_) {
            cachedCVar_ = CVarSystem::GetCVar(name_);
        }
        if (cachedCVar_ && std::holds_alternative<T>(cachedCVar_->value)) {
            return std::get<T>(cachedCVar_->value);
        }
        return T{};
    }

    T Get(T fallback) {
        if (!cachedCVar_) {
            cachedCVar_ = CVarSystem::GetCVar(name_);
        }
        if (cachedCVar_ && std::holds_alternative<T>(cachedCVar_->value)) {
            return std::get<T>(cachedCVar_->value);
        }
        return fallback;
    }

    void Set(const T& value) {
        if (!cachedCVar_) {
            cachedCVar_ = CVarSystem::GetCVar(name_);
        }
        if (cachedCVar_ && std::holds_alternative<T>(cachedCVar_->value)) {
            cachedCVar_->value = value;
            if (cachedCVar_->onChangeCallback) {
                cachedCVar_->onChangeCallback();
            }
        }
    }

private:
    std::string name_;
    CVar* cachedCVar_ = nullptr;
};

using AutoCVarInt = AutoCVarRef<int>;
using AutoCVarFloat = AutoCVarRef<float>;
using AutoCVarBool = AutoCVarRef<bool>;
using AutoCVarString = AutoCVarRef<std::string>;

} // namespace Irufemi

// ---------------------------------------------------------
// Macros for Global Registration in cpp files
// ---------------------------------------------------------
// (Meyers Singleton initialization pattern equivalent for macros is just direct struct instantiation,
// but we will put these macros ONLY in EngineCVars.cpp, so static order within that file is top-down).
// Wait, to allow any variable name characters, we can sanitize or just use __LINE__ token pasting.

#define CONCAT_IMPL(x, y) x##y
#define MACRO_CONCAT(x, y) CONCAT_IMPL(x, y)

#define DEFINE_CVAR_INT(name, defaultValue, description)                                                               \
    namespace {                                                                                                        \
    struct MACRO_CONCAT(CVarReg_, __LINE__) {                                                                          \
        MACRO_CONCAT(CVarReg_, __LINE__)() {                                                                           \
            Irufemi::CVarSystem::RegisterInt(name, defaultValue, description);                                         \
        }                                                                                                              \
    } MACRO_CONCAT(cvarInst_, __LINE__);                                                                               \
    }

#define DEFINE_CVAR_FLOAT(name, defaultValue, description)                                                             \
    namespace {                                                                                                        \
    struct MACRO_CONCAT(CVarReg_, __LINE__) {                                                                          \
        MACRO_CONCAT(CVarReg_, __LINE__)() {                                                                           \
            Irufemi::CVarSystem::RegisterFloat(name, defaultValue, description);                                       \
        }                                                                                                              \
    } MACRO_CONCAT(cvarInst_, __LINE__);                                                                               \
    }

#define DEFINE_CVAR_BOOL(name, defaultValue, description)                                                              \
    namespace {                                                                                                        \
    struct MACRO_CONCAT(CVarReg_, __LINE__) {                                                                          \
        MACRO_CONCAT(CVarReg_, __LINE__)() {                                                                           \
            Irufemi::CVarSystem::RegisterBool(name, defaultValue, description);                                        \
        }                                                                                                              \
    } MACRO_CONCAT(cvarInst_, __LINE__);                                                                               \
    }

#define DEFINE_CVAR_STRING(name, defaultValue, description)                                                            \
    namespace {                                                                                                        \
    struct MACRO_CONCAT(CVarReg_, __LINE__) {                                                                          \
        MACRO_CONCAT(CVarReg_, __LINE__)() {                                                                           \
            Irufemi::CVarSystem::RegisterString(name, defaultValue, description);                                      \
        }                                                                                                              \
    } MACRO_CONCAT(cvarInst_, __LINE__);                                                                               \
    }
