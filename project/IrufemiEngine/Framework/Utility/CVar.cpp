#include "CVar.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

namespace Irufemi {

// Meyer's Singleton pattern internally to hold the registry safely
Synchronized<CVarSystem::RegistryType>& CVarSystem::GetSynchronizedRegistry() {
    static Synchronized<RegistryType> registry;
    return registry;
}

std::unordered_map<std::string, std::unique_ptr<CVar>>& CVarSystem::GetRegistry() {
    static std::unordered_map<std::string, std::unique_ptr<CVar>> fallbackRegistry;
    return fallbackRegistry;
}

// ---------------------------------------------------------
// Registration
// ---------------------------------------------------------
void CVarSystem::RegisterInt(const std::string& name, int defaultValue, const std::string& description) {
    GetSynchronizedRegistry().WithWrite([&](RegistryType& reg) {
        if (reg.find(name) == reg.end()) {
            auto cvar = std::make_unique<CVar>();
            cvar->name = name;
            cvar->description = description;
            cvar->type = CVar::Type::Int;
            cvar->defaultValue = defaultValue;
            cvar->value = defaultValue;
            reg[name] = std::move(cvar);
        }
    });
}

void CVarSystem::RegisterFloat(const std::string& name, float defaultValue, const std::string& description) {
    GetSynchronizedRegistry().WithWrite([&](RegistryType& reg) {
        if (reg.find(name) == reg.end()) {
            auto cvar = std::make_unique<CVar>();
            cvar->name = name;
            cvar->description = description;
            cvar->type = CVar::Type::Float;
            cvar->defaultValue = defaultValue;
            cvar->value = defaultValue;
            reg[name] = std::move(cvar);
        }
    });
}

void CVarSystem::RegisterBool(const std::string& name, bool defaultValue, const std::string& description) {
    GetSynchronizedRegistry().WithWrite([&](RegistryType& reg) {
        if (reg.find(name) == reg.end()) {
            auto cvar = std::make_unique<CVar>();
            cvar->name = name;
            cvar->description = description;
            cvar->type = CVar::Type::Bool;
            cvar->defaultValue = defaultValue;
            cvar->value = defaultValue;
            reg[name] = std::move(cvar);
        }
    });
}

void CVarSystem::RegisterString(const std::string& name, const std::string& defaultValue,
                                const std::string& description) {
    GetSynchronizedRegistry().WithWrite([&](RegistryType& reg) {
        if (reg.find(name) == reg.end()) {
            auto cvar = std::make_unique<CVar>();
            cvar->name = name;
            cvar->description = description;
            cvar->type = CVar::Type::String;
            cvar->defaultValue = defaultValue;
            cvar->value = defaultValue;
            reg[name] = std::move(cvar);
        }
    });
}

// ---------------------------------------------------------
// Getters
// ---------------------------------------------------------
int CVarSystem::GetInt(const std::string& name) {
    return GetSynchronizedRegistry().WithRead([&](const RegistryType& reg) -> int {
        auto it = reg.find(name);
        if (it != reg.end() && it->second->type == CVar::Type::Int) {
            return std::get<int>(it->second->value);
        }
        return 0;
    });
}

float CVarSystem::GetFloat(const std::string& name) {
    return GetSynchronizedRegistry().WithRead([&](const RegistryType& reg) -> float {
        auto it = reg.find(name);
        if (it != reg.end() && it->second->type == CVar::Type::Float) {
            return std::get<float>(it->second->value);
        }
        return 0.0f;
    });
}

bool CVarSystem::GetBool(const std::string& name) {
    return GetSynchronizedRegistry().WithRead([&](const RegistryType& reg) -> bool {
        auto it = reg.find(name);
        if (it != reg.end() && it->second->type == CVar::Type::Bool) {
            return std::get<bool>(it->second->value);
        }
        return false;
    });
}

std::string CVarSystem::GetString(const std::string& name) {
    return GetSynchronizedRegistry().WithRead([&](const RegistryType& reg) -> std::string {
        auto it = reg.find(name);
        if (it != reg.end() && it->second->type == CVar::Type::String) {
            return std::get<std::string>(it->second->value);
        }
        return "";
    });
}

// ---------------------------------------------------------
// Setters
// ---------------------------------------------------------
void CVarSystem::SetInt(const std::string& name, int value) {
    std::function<void()> callbackToInvoke;
    GetSynchronizedRegistry().WithWrite([&](RegistryType& reg) {
        auto it = reg.find(name);
        if (it != reg.end() && it->second->type == CVar::Type::Int) {
            it->second->value = value;
            callbackToInvoke = it->second->onChangeCallback;
        }
    });
    if (callbackToInvoke) {
        callbackToInvoke();
    }
}

void CVarSystem::SetFloat(const std::string& name, float value) {
    std::function<void()> callbackToInvoke;
    GetSynchronizedRegistry().WithWrite([&](RegistryType& reg) {
        auto it = reg.find(name);
        if (it != reg.end() && it->second->type == CVar::Type::Float) {
            it->second->value = value;
            callbackToInvoke = it->second->onChangeCallback;
        }
    });
    if (callbackToInvoke) {
        callbackToInvoke();
    }
}

void CVarSystem::SetBool(const std::string& name, bool value) {
    std::function<void()> callbackToInvoke;
    GetSynchronizedRegistry().WithWrite([&](RegistryType& reg) {
        auto it = reg.find(name);
        if (it != reg.end() && it->second->type == CVar::Type::Bool) {
            it->second->value = value;
            callbackToInvoke = it->second->onChangeCallback;
        }
    });
    if (callbackToInvoke) {
        callbackToInvoke();
    }
}

void CVarSystem::SetString(const std::string& name, const std::string& value) {
    std::function<void()> callbackToInvoke;
    GetSynchronizedRegistry().WithWrite([&](RegistryType& reg) {
        auto it = reg.find(name);
        if (it != reg.end() && it->second->type == CVar::Type::String) {
            it->second->value = value;
            callbackToInvoke = it->second->onChangeCallback;
        }
    });
    if (callbackToInvoke) {
        callbackToInvoke();
    }
}

// ---------------------------------------------------------
// Callback Setup
// ---------------------------------------------------------
void CVarSystem::SetOnChangeCallback(const std::string& name, std::function<void()> callback) {
    std::function<void()> callbackToInvoke;
    GetSynchronizedRegistry().WithWrite([&](RegistryType& reg) {
        auto it = reg.find(name);
        if (it != reg.end()) {
            it->second->onChangeCallback = std::move(callback);
            callbackToInvoke = it->second->onChangeCallback;
        }
    });
    // Call it immediately once when registered so current values apply
    if (callbackToInvoke) {
        callbackToInvoke();
    }
}

// ---------------------------------------------------------
// File I/O
// ---------------------------------------------------------
void CVarSystem::Load(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return; // File doesn't exist yet, that's fine
    }

    nlohmann::json j;
    try {
        file >> j;
    } catch (...) {
        return; // Parse error
    }

    std::vector<std::function<void()>> callbacksToInvoke;
    GetSynchronizedRegistry().WithWrite([&](RegistryType& reg) {
        for (auto& [key, val] : j.items()) {
            auto it = reg.find(key);
            if (it != reg.end()) {
                auto& cvar = it->second;
                if (cvar->type == CVar::Type::Int && val.is_number_integer()) {
                    cvar->value = val.get<int>();
                } else if (cvar->type == CVar::Type::Float && val.is_number_float()) {
                    cvar->value = val.get<float>();
                } else if (cvar->type == CVar::Type::Bool && val.is_boolean()) {
                    cvar->value = val.get<bool>();
                } else if (cvar->type == CVar::Type::String && val.is_string()) {
                    cvar->value = val.get<std::string>();
                }

                if (cvar->onChangeCallback) {
                    callbacksToInvoke.push_back(cvar->onChangeCallback);
                }
            }
        }
    });

    for (const auto& cb : callbacksToInvoke) {
        if (cb) {
            cb();
        }
    }
}

void CVarSystem::Save(const std::string& filepath) {
    nlohmann::json j;

    GetSynchronizedRegistry().WithRead([&](const RegistryType& reg) {
        for (const auto& pair : reg) {
            const auto& cvar = pair.second;
            if (cvar->type == CVar::Type::Int) {
                j[cvar->name] = std::get<int>(cvar->value);
            } else if (cvar->type == CVar::Type::Float) {
                j[cvar->name] = std::get<float>(cvar->value);
            } else if (cvar->type == CVar::Type::Bool) {
                j[cvar->name] = std::get<bool>(cvar->value);
            } else if (cvar->type == CVar::Type::String) {
                j[cvar->name] = std::get<std::string>(cvar->value);
            }
        }
    });

    std::ofstream file(filepath);
    if (file.is_open()) {
        file << j.dump(4);
    }
}

// ---------------------------------------------------------
// Direct Access
// ---------------------------------------------------------
CVar* CVarSystem::GetCVar(const std::string& name) {
    return GetSynchronizedRegistry().WithRead([&](const RegistryType& reg) -> CVar* {
        auto it = reg.find(name);
        if (it != reg.end()) {
            return it->second.get();
        }
        return nullptr;
    });
}

} // namespace Irufemi
