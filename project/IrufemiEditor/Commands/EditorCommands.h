#pragma once

#ifdef EditorMode
#include "Core/ICommand.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Scene/BaseScene.h"
#include "Core/EditorManager.h"
#include <functional>
#include <memory>
#include <limits>

using Irufemi::BaseScene;
using Irufemi::Component;
using Irufemi::GameObject;

/**
 * @class ChangeValueCommand
 * @brief 値の変更を記録し、Undo/Redo を行う汎用コマンド
 */
template <typename T> class ChangeValueCommand : public ICommand {
public:
    ChangeValueCommand(const T& oldValue, const T& newValue, std::function<void(const T&)> setter)
        : oldValue_(oldValue), newValue_(newValue), setter_(setter) {}

    void Do() override {
        setter_(newValue_);
    }
    void Undo() override {
        setter_(oldValue_);
    }

private:
    T oldValue_;
    T newValue_;
    std::function<void(const T&)> setter_;
};

/**
 * @class CreateObjectCommand
 * @brief オブジェクトの生成を記録するコマンド
 */
class CreateObjectCommand : public ICommand {
public:
    CreateObjectCommand(BaseScene* scene, std::shared_ptr<GameObject> object,
                        std::shared_ptr<GameObject> parent = nullptr,
                        size_t index = (std::numeric_limits<size_t>::max)())
        : scene_(scene), object_(object), parent_(parent), index_(index) {}

    void Do() override {
        if (parent_) {
            if (index_ == (std::numeric_limits<size_t>::max)()) {
                parent_->AddChild(object_);
            } else {
                parent_->InsertChild(object_, index_);
            }
        } else {
            if (index_ == (std::numeric_limits<size_t>::max)()) {
                scene_->AddGameObject(object_);
            } else {
                scene_->InsertGameObject(object_, index_);
            }
        }
    }

    void Undo() override {
        if (parent_) {
            parent_->RemoveChild(object_);
        } else {
            scene_->RemoveGameObject(object_);
        }
    }

private:
    BaseScene* scene_;
    std::shared_ptr<GameObject> object_;
    std::shared_ptr<GameObject> parent_;
    size_t index_;
};

/**
 * @class DeleteObjectCommand
 * @brief オブジェクトの削除を記録するコマンド
 */
class DeleteObjectCommand : public ICommand {
public:
    DeleteObjectCommand(BaseScene* scene, std::shared_ptr<GameObject> object) : scene_(scene), object_(object) {
        parent_ = object->GetParent();
        if (parent_) {
            index_ = parent_->GetChildIndex(object);
        } else {
            index_ = scene_->GetGameObjectIndex(object);
        }
    }

    void Do() override {
        if (parent_) {
            parent_->RemoveChild(object_);
        } else {
            scene_->RemoveGameObject(object_);
        }
    }

    void Undo() override {
        if (parent_) {
            parent_->InsertChild(object_, index_);
        } else {
            scene_->InsertGameObject(object_, index_);
        }
    }

private:
    BaseScene* scene_;
    std::shared_ptr<GameObject> object_;
    std::shared_ptr<GameObject> parent_;
    size_t index_;
};

/**
 * @class AddComponentCommand
 * @brief コンポーネントの追加を記録するコマンド
 */
class AddComponentCommand : public ICommand {
public:
    AddComponentCommand(std::shared_ptr<GameObject> target, std::shared_ptr<Component> component)
        : target_(target), component_(component) {}

    void Do() override {
        target_->AddComponent(component_);
    }

    void Undo() override {
        target_->RemoveComponent(component_.get());
    }

private:
    std::shared_ptr<GameObject> target_;
    std::shared_ptr<Component> component_;
};

/**
 * @class RemoveComponentCommand
 * @brief コンポーネントの削除を記録するコマンド
 */
class RemoveComponentCommand : public ICommand {
public:
    RemoveComponentCommand(std::shared_ptr<GameObject> target, std::shared_ptr<Component> component)
        : target_(target), component_(component) {}

    void Do() override {
        target_->RemoveComponent(component_.get());
    }

    void Undo() override {
        target_->AddComponent(component_);
    }

private:
    std::shared_ptr<GameObject> target_;
    std::shared_ptr<Component> component_;
};

#endif // EditorMode
