#include "Platform/Input/InputMappingContext.h"

namespace Irufemi {

void InputMappingContext::AddBinding(const std::string& actionName, const InputBinding& binding) {
    mappings_[actionName].push_back(binding);
}

const std::vector<InputBinding>& InputMappingContext::GetBindings(const std::string& actionName) const {
    static const std::vector<InputBinding> emptyList;
    auto it = mappings_.find(actionName);
    if (it != mappings_.end()) {
        return it->second;
    }
    return emptyList;
}

std::vector<std::string> InputMappingContext::GetAllActionNames() const {
    std::vector<std::string> names;
    names.reserve(mappings_.size());
    for (const auto& pair : mappings_) {
        names.push_back(pair.first);
    }
    return names;
}

void InputMappingContext::Clear() {
    mappings_.clear();
}

} // namespace Irufemi
