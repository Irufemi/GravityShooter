#include "Renderer/Camera/CameraManager.h"
#include <cassert>

void CameraManager::AddCamera(const std::string& name, std::shared_ptr<Camera> camera) {
    if (camera) {
        cameras_[name] = camera;
        // 初めて追加されたカメラをアクティブにする
        if (activeCameraName_.empty()) {
            activeCameraName_ = name;
            activeCameraCache_ = camera.get();
        } else if (activeCameraName_ == name) {
            activeCameraCache_ = camera.get();
        }
    }
}

void CameraManager::RemoveCamera(const std::string& name) {
    auto it = cameras_.find(name);
    if (it != cameras_.end()) {
        cameras_.erase(it);
        if (activeCameraName_ == name) {
            activeCameraName_.clear();
            activeCameraCache_ = nullptr;
            // 代わりのカメラを適当に設定する
            if (!cameras_.empty()) {
                activeCameraName_ = cameras_.begin()->first;
                activeCameraCache_ = cameras_.begin()->second.get();
            }
        }
    }
}

void CameraManager::RemoveCamera(const std::shared_ptr<Camera>& camera) {
    if (!camera) {
        return;
    }
    for (auto it = cameras_.begin(); it != cameras_.end(); ++it) {
        if (it->second == camera) {
            std::string name = it->first;
            cameras_.erase(it);
            if (activeCameraName_ == name) {
                activeCameraName_.clear();
                activeCameraCache_ = nullptr;
                if (!cameras_.empty()) {
                    activeCameraName_ = cameras_.begin()->first;
                    activeCameraCache_ = cameras_.begin()->second.get();
                }
            }
            break;
        }
    }
}

void CameraManager::SetActiveCamera(const std::string& name) {
    auto it = cameras_.find(name);
    if (it != cameras_.end()) {
        activeCameraName_ = name;
        activeCameraCache_ = it->second.get();
    }
}

Camera* CameraManager::GetActiveCamera() const {
    return activeCameraCache_;
}

const std::string& CameraManager::GetActiveCameraName() const {
    return activeCameraName_;
}

Camera* CameraManager::GetCamera(const std::string& name) const {
    auto it = cameras_.find(name);
    if (it != cameras_.end()) {
        return it->second.get();
    }
    return nullptr;
}

void CameraManager::Update() {
    for (auto& pair : cameras_) {
        if (pair.second) {
            pair.second->Update();
        }
    }
}

void CameraManager::OnResize(int width, int height) {
    for (auto& pair : cameras_) {
        if (pair.second) {
            pair.second->Initialize(width, height);
        }
    }
}

void CameraManager::Clear() {
    cameras_.clear();
    activeCameraName_.clear();
    activeCameraCache_ = nullptr;
}
