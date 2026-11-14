#include "Resource/Model/AnimationImporter.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "Core/Utility/ErrorUtility.h"
#include <filesystem>
#include "Resource/Model/AssimpMutex.h"

using namespace Irufemi;

Animation AnimationImporter::Import(const std::string& fullPath) {
    Animation animation;
    if (!std::filesystem::exists(fullPath)) {
        return {};
    }

    std::lock_guard<std::mutex> lock(Irufemi::AssimpMutex::Get());
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(fullPath.c_str(), aiProcess_MakeLeftHanded);
    if (!scene || scene->mNumAnimations == 0) {
        return {};
    }
    aiAnimation* animationAssimp = scene->mAnimations[0];
    double ticksPerSecond = animationAssimp->mTicksPerSecond != 0.0 ? animationAssimp->mTicksPerSecond : 1.0;
    animation.duration = float(animationAssimp->mDuration / ticksPerSecond);

    for (uint32_t channelIndex = 0; channelIndex < animationAssimp->mNumChannels; ++channelIndex) {
        aiNodeAnim* nodeAnimationAssimp = animationAssimp->mChannels[channelIndex];
        NodeAnimation& nodeAnimation = animation.nodeAnimations[nodeAnimationAssimp->mNodeName.C_Str()];

        for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumPositionKeys; ++keyIndex) {
            aiVectorKey& keyAssimp = nodeAnimationAssimp->mPositionKeys[keyIndex];
            Keyframe<Irufemi::Vector3> keyframe;
            keyframe.time = float(keyAssimp.mTime / ticksPerSecond);
            keyframe.value = {keyAssimp.mValue.x, keyAssimp.mValue.y, keyAssimp.mValue.z};
            nodeAnimation.translate.keyframes.push_back(keyframe);
        }

        for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumRotationKeys; ++keyIndex) {
            aiQuatKey& keyAssimp = nodeAnimationAssimp->mRotationKeys[keyIndex];
            Keyframe<Irufemi::Quaternion> keyframe;
            keyframe.time = float(keyAssimp.mTime / ticksPerSecond);
            aiQuaternion& q = keyAssimp.mValue;
            keyframe.value = {q.x, q.y, q.z, q.w};
            nodeAnimation.rotate.keyframes.push_back(keyframe);
        }

        for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumScalingKeys; ++keyIndex) {
            aiVectorKey& keyAssimp = nodeAnimationAssimp->mScalingKeys[keyIndex];
            Keyframe<Irufemi::Vector3> keyframe;
            keyframe.time = float(keyAssimp.mTime / ticksPerSecond);
            keyframe.value = {keyAssimp.mValue.x, keyAssimp.mValue.y, keyAssimp.mValue.z};
            nodeAnimation.scale.keyframes.push_back(keyframe);
        }
    }
    return animation;
}
