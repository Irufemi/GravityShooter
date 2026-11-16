#include "Framework/Scene/SceneObjectRegistry.h"
#include "Framework/GameObject/GameObject.h"
#include <algorithm>
#include <regex>

namespace Irufemi {

void SceneObjectRegistry::Register(const std::shared_ptr<GameObject>& obj) {
    if (!obj) {
        return;
    }
    std::lock_guard<std::recursive_mutex> lock(registryMutex_);

    auto registerInternal = [this](auto& self, const std::shared_ptr<GameObject>& target) -> void {
        if (!target) {
            return;
        }
        if (!target->GetName().empty()) {
            nameIndex_[target->GetName()].push_back(target);
        }
        idIndex_[target->GetInstanceID()] = target;

        for (const auto& child : target->GetChildren()) {
            self(self, child);
        }
    };
    registerInternal(registerInternal, obj);
}

void SceneObjectRegistry::Unregister(const std::shared_ptr<GameObject>& obj) {
    if (!obj) {
        return;
    }
    std::lock_guard<std::recursive_mutex> lock(registryMutex_);

    auto unregisterInternal = [this](auto& self, const std::shared_ptr<GameObject>& target) -> void {
        if (!target) {
            return;
        }
        auto nameIt = nameIndex_.find(target->GetName());
        if (nameIt != nameIndex_.end()) {
            auto& list = nameIt->second;
            list.erase(std::remove_if(list.begin(), list.end(),
                                      [&target](const std::weak_ptr<GameObject>& weakPtr) {
                                          auto locked = weakPtr.lock();
                                          return !locked || locked == target;
                                      }),
                       list.end());
            if (list.empty()) {
                nameIndex_.erase(nameIt);
            }
        }
        idIndex_.erase(target->GetInstanceID());

        for (const auto& child : target->GetChildren()) {
            self(self, child);
        }
    };
    unregisterInternal(unregisterInternal, obj);
}

void SceneObjectRegistry::Clear() {
    std::lock_guard<std::recursive_mutex> lock(registryMutex_);
    nameIndex_.clear();
    idIndex_.clear();
}

std::shared_ptr<GameObject> SceneObjectRegistry::FindByName(const std::string& name) {
    std::lock_guard<std::recursive_mutex> lock(registryMutex_);
    auto it = nameIndex_.find(name);
    if (it != nameIndex_.end()) {
        auto& list = it->second;
        for (auto listIt = list.begin(); listIt != list.end();) {
            if (auto obj = listIt->lock()) {
                if (!obj->IsDestroyed()) {
                    return obj;
                } else {
                    listIt = list.erase(listIt);
                }
            } else {
                listIt = list.erase(listIt);
            }
        }
    }
    return nullptr;
}

std::vector<std::shared_ptr<GameObject>> SceneObjectRegistry::FindAllByName(const std::string& name) {
    std::vector<std::shared_ptr<GameObject>> result;
    std::lock_guard<std::recursive_mutex> lock(registryMutex_);
    auto it = nameIndex_.find(name);
    if (it != nameIndex_.end()) {
        auto& list = it->second;
        for (auto listIt = list.begin(); listIt != list.end();) {
            if (auto obj = listIt->lock()) {
                if (!obj->IsDestroyed()) {
                    result.push_back(obj);
                    ++listIt;
                } else {
                    listIt = list.erase(listIt);
                }
            } else {
                listIt = list.erase(listIt);
            }
        }
    }
    return result;
}

std::shared_ptr<GameObject> SceneObjectRegistry::FindById(uint64_t instanceId) {
    std::lock_guard<std::recursive_mutex> lock(registryMutex_);
    auto it = idIndex_.find(instanceId);
    if (it != idIndex_.end()) {
        if (auto obj = it->second.lock()) {
            if (!obj->IsDestroyed()) {
                return obj;
            } else {
                idIndex_.erase(it);
            }
        } else {
            idIndex_.erase(it);
        }
    }
    return nullptr;
}

std::vector<std::shared_ptr<GameObject>>
SceneObjectRegistry::FindByTag(const std::string& tag, const std::vector<std::shared_ptr<GameObject>>& allObjects) {
    std::vector<std::shared_ptr<GameObject>> result;
    std::lock_guard<std::recursive_mutex> lock(registryMutex_);
    for (const auto& obj : allObjects) {
        if (obj && !obj->IsDestroyed() && obj->GetTag() == tag) {
            result.push_back(obj);
        }
    }
    return result;
}

void SceneObjectRegistry::OnNameChanged(const std::shared_ptr<GameObject>& obj, const std::string& oldName,
                                        const std::string& newName) {
    if (!obj) {
        return;
    }
    std::lock_guard<std::recursive_mutex> lock(registryMutex_);
    auto itOld = nameIndex_.find(oldName);
    if (itOld != nameIndex_.end()) {
        auto& list = itOld->second;
        list.erase(std::remove_if(list.begin(), list.end(),
                                  [&obj](const std::weak_ptr<GameObject>& weakPtr) {
                                      auto locked = weakPtr.lock();
                                      return !locked || locked == obj;
                                  }),
                   list.end());
        if (list.empty()) {
            nameIndex_.erase(itOld);
        }
    }
    if (!newName.empty()) {
        nameIndex_[newName].push_back(obj);
    }
}

std::string SceneObjectRegistry::GenerateUniqueName(const std::string& baseName,
                                                    const std::vector<std::shared_ptr<GameObject>>& pendingAdds) {
    std::lock_guard<std::recursive_mutex> lock(registryMutex_);

    auto NameExists = [&](const std::string& name) {
        auto it = nameIndex_.find(name);
        if (it != nameIndex_.end()) {
            for (const auto& weakObj : it->second) {
                if (auto obj = weakObj.lock()) {
                    if (!obj->IsDestroyed()) {
                        return true;
                    }
                }
            }
        }
        for (const auto& obj : pendingAdds) {
            if (obj && !obj->IsDestroyed() && obj->GetName() == name) {
                return true;
            }
        }
        return false;
    };

    if (!NameExists(baseName)) {
        return baseName;
    }

    std::string prefix = baseName;
    int nextIndex = 1;

    static const std::regex re(R"(^(.*) \((\d+)\)$)");
    std::smatch match;
    if (std::regex_match(baseName, match, re)) {
        prefix = match[1].str();
        nextIndex = std::stoi(match[2].str()) + 1;
    }

    // 同一プレフィックスの最大インデックスを走査して試行回数を最小化
    int maxIndex = nextIndex - 1;
    std::string prefixTag = prefix + " (";
    auto ScanMaxIndex = [&](const std::string& name) {
        if (name.rfind(prefixTag, 0) == 0 && name.back() == ')') {
            size_t start = prefixTag.length();
            size_t len = name.length() - start - 1;
            if (len > 0) {
                try {
                    int val = std::stoi(name.substr(start, len));
                    if (val > maxIndex) {
                        maxIndex = val;
                    }
                } catch (...) {
                }
            }
        }
    };

    for (const auto& [name, objList] : nameIndex_) {
        ScanMaxIndex(name);
    }
    for (const auto& obj : pendingAdds) {
        if (obj && !obj->IsDestroyed()) {
            ScanMaxIndex(obj->GetName());
        }
    }

    nextIndex = maxIndex + 1;

    std::string candidate;
    do {
        candidate = prefix + " (" + std::to_string(nextIndex) + ")";
        nextIndex++;
    } while (NameExists(candidate));

    return candidate;
}

} // namespace Irufemi

