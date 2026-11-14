#include "Resource/Model/ModelImporter.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/material.h>
#include "Core/Utility/ErrorUtility.h"
#include <filesystem>
#include <algorithm>
#include <limits>
#include <cmath>
#include "Resource/Model/AssimpMutex.h"
#include "Core/Utility/Log.h"
#include <iostream>

using namespace Irufemi;

namespace {
// ノードとメッシュの関連を解析するヘルパー関数
void ProcessNode(aiNode* node, const aiScene* scene, std::vector<ObjMesh>& meshes) {
    for (UINT i = 0; i < node->mNumMeshes; i++) {
        UINT meshIndex = node->mMeshes[i];
        if (meshIndex < meshes.size()) {
            meshes[meshIndex].nodeName = node->mName.C_Str();
        }
    }
    for (UINT i = 0; i < node->mNumChildren; i++) {
        ProcessNode(node->mChildren[i], scene, meshes);
    }
}

Node ReadNode(aiNode* node) {
    Node result;
    aiMatrix4x4 aiLocalMatrix = node->mTransformation;
    aiLocalMatrix.Transpose();

    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            result.localMatrix.m[r][c] = aiLocalMatrix[r][c];
        }
    }

    aiVector3D scale, translate;
    aiQuaternion rotate;
    node->mTransformation.Decompose(scale, rotate, translate);
    result.transform.scale = {scale.x, scale.y, scale.z};
    result.transform.rotate = {rotate.x, rotate.y, rotate.z, rotate.w};
    result.transform.translate = {translate.x, translate.y, translate.z};

    result.name = node->mName.C_Str();
    result.children.resize(node->mNumChildren);
    for (uint32_t childIndex = 0; childIndex < node->mNumChildren; ++childIndex) {
        result.children[childIndex] = ReadNode(node->mChildren[childIndex]);
    }
    return result;
}

void CalculateBoundingSphere(ObjModel& model) {
    Irufemi::Vector3 minV = {(std::numeric_limits<float>::max)(), (std::numeric_limits<float>::max)(),
                             (std::numeric_limits<float>::max)()};
    Irufemi::Vector3 maxV = {(std::numeric_limits<float>::lowest)(), (std::numeric_limits<float>::lowest)(),
                             (std::numeric_limits<float>::lowest)()};
    bool hasVertices = false;

    for (const auto& mesh : model.meshes) {
        for (const auto& vertex : mesh.vertices) {
            minV.x = (std::min)(minV.x, vertex.position.x);
            minV.y = (std::min)(minV.y, vertex.position.y);
            minV.z = (std::min)(minV.z, vertex.position.z);
            maxV.x = (std::max)(maxV.x, vertex.position.x);
            maxV.y = (std::max)(maxV.y, vertex.position.y);
            maxV.z = (std::max)(maxV.z, vertex.position.z);
            hasVertices = true;
        }
    }

    if (!hasVertices) {
        model.boundingBox = Irufemi::AABB{{0, 0, 0}, {0, 0, 0}};
        model.boundingSphere.center = {0, 0, 0};
        model.boundingSphere.radius = 0.0f;
        return;
    }

    model.boundingBox = Irufemi::AABB{minV, maxV};
    model.boundingSphere.center = (minV + maxV) * 0.5f;

    float maxDistSq = 0.0f;
    for (const auto& mesh : model.meshes) {
        for (const auto& vertex : mesh.vertices) {
            Irufemi::Vector3 pos = {vertex.position.x, vertex.position.y, vertex.position.z};
            Irufemi::Vector3 diff = pos - model.boundingSphere.center;
            float distSq = Irufemi::Math::Dot(diff, diff);
            maxDistSq = (std::max)(maxDistSq, distSq);
        }
    }
    model.boundingSphere.radius = std::sqrt(maxDistSq);
}
} // namespace

ObjModel ModelImporter::Import(const std::string& fullPath) {
    ObjModel objModel;

    const std::string filePath = fullPath;

    const unsigned int flags =
        aiProcess_Triangulate | aiProcess_FlipWindingOrder | aiProcess_FlipUVs | aiProcess_MakeLeftHanded;

    std::lock_guard<std::mutex> lock(Irufemi::AssimpMutex::Get());
    Assimp::Importer importer;
    Log::OutPutLog(std::cout, "[ModelImporter] Assimp ReadFile START: " + filePath + "\n");
    const aiScene* scene = importer.ReadFile(filePath.c_str(), flags);
    Log::OutPutLog(std::cout, "[ModelImporter] Assimp ReadFile FINISH: " + filePath + "\n");
    if (!scene || !scene->HasMeshes()) {
        IRUFEMI_WARNING(false,
                        "Assimp failed to load model or no meshes found: " + std::string(importer.GetErrorString()));
        return ObjModel();
    }

    std::vector<ObjMaterial> convertedMaterials(scene->mNumMaterials);

    for (uint32_t i = 0; i < scene->mNumMaterials; ++i) {
        const aiMaterial* m = scene->mMaterials[i];
        ObjMaterial out{};

        out.textureFilePath = "";
        out.color = {1.0f, 1.0f, 1.0f, 1.0f};
        out.ambient = {0.0f, 0.0f, 0.0f};
        out.specular = {0.0f, 0.0f, 0.0f};
        out.roughness = 0.5f;
        out.metallic = 0.0f;
        out.alpha = 1.0f;
        out.enableLighting = true;
        out.lightingMode = 3;
        out.environmentCoefficient = 0.0f;
        out.uvTransform = Irufemi::Math::MakeAffineMatrix({1.0f, 1.0f, 1.0f}, Irufemi::Vector3{0, 0, 0}, {0, 0, 0});

        if (m->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
            aiString texPath;
            if (m->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == aiReturn_SUCCESS) {
                std::string p = texPath.C_Str();
                if (!p.empty() && p[0] != '*') {
                    std::filesystem::path modelPath(filePath);
                    std::filesystem::path texturePath = modelPath.parent_path() / p;
                    out.textureFilePath = texturePath.string();
                    std::replace(out.textureFilePath.begin(), out.textureFilePath.end(), '\\', '/');
                }
            }
        }

        if (m->GetTextureCount(aiTextureType_NORMALS) > 0 || m->GetTextureCount(aiTextureType_HEIGHT) > 0) {
            aiString texPath;
            if (m->GetTexture(aiTextureType_NORMALS, 0, &texPath) == aiReturn_SUCCESS ||
                m->GetTexture(aiTextureType_HEIGHT, 0, &texPath) == aiReturn_SUCCESS) {
                std::string p = texPath.C_Str();
                if (!p.empty() && p[0] != '*') {
                    std::filesystem::path modelPath(filePath);
                    std::filesystem::path texturePath = modelPath.parent_path() / p;
                    out.normalMapFilePath = texturePath.string();
                    std::replace(out.normalMapFilePath.begin(), out.normalMapFilePath.end(), '\\', '/');
                }
            }
        }

        aiColor3D kd;
        if (m->Get(AI_MATKEY_COLOR_DIFFUSE, kd) == aiReturn_SUCCESS) {
            out.color.x = kd.r;
            out.color.y = kd.g;
            out.color.z = kd.b;
            out.color.w = 1.0f;
        }
        aiColor3D ka;
        if (m->Get(AI_MATKEY_COLOR_AMBIENT, ka) == aiReturn_SUCCESS) {
            out.ambient.x = ka.r;
            out.ambient.y = ka.g;
            out.ambient.z = ka.b;
        }
        aiColor3D ks;
        if (m->Get(AI_MATKEY_COLOR_SPECULAR, ks) == aiReturn_SUCCESS) {
            out.specular.x = ks.r;
            out.specular.y = ks.g;
            out.specular.z = ks.b;
        }
        float shininess = 0.0f;
        if (m->Get(AI_MATKEY_SHININESS, shininess) == aiReturn_SUCCESS) {
            out.roughness = std::clamp(std::sqrt(2.0f / (shininess + 2.0f)), 0.0f, 1.0f);
        }
        float opacity = 1.0f;
        if (m->Get(AI_MATKEY_OPACITY, opacity) == aiReturn_SUCCESS) {
            out.alpha = opacity;
            out.color.w = opacity;
        }

        convertedMaterials[i] = out;
    }

    uint32_t vertexOffset = 0;
    for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
        aiMesh* mesh = scene->mMeshes[meshIndex];
        ObjMesh outMesh;

        if (mesh->mMaterialIndex < convertedMaterials.size()) {
            outMesh.material = convertedMaterials[mesh->mMaterialIndex];
        }

        outMesh.vertices.resize(mesh->mNumVertices);
        for (uint32_t i = 0; i < mesh->mNumVertices; ++i) {
            const aiVector3D& p = mesh->mVertices[i];
            const aiVector3D& n = mesh->HasNormals() ? mesh->mNormals[i] : aiVector3D(0, 1, 0);
            const aiVector3D& t = mesh->HasTextureCoords(0) ? mesh->mTextureCoords[0][i] : aiVector3D(0.5f, 0.5f, 0);

            VertexData& v = outMesh.vertices[i];
            v.position = {p.x, p.y, p.z, 1.0f};
            v.normal = {n.x, n.y, n.z};
            v.texcoord = {t.x, t.y};
        }

        for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {
            const aiFace& face = mesh->mFaces[faceIndex];
            IRUFEMI_ASSERT(face.mNumIndices == 3);
            outMesh.indices.push_back(face.mIndices[0]);
            outMesh.indices.push_back(face.mIndices[1]);
            outMesh.indices.push_back(face.mIndices[2]);
        }

        for (uint32_t boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex) {
            aiBone* bone = mesh->mBones[boneIndex];
            std::string jointName = bone->mName.C_Str();
            JointWeightData& jointWeightData = objModel.skinClusterData[jointName];

            aiMatrix4x4 bindPoseMatrixAssimp = bone->mOffsetMatrix.Inverse();
            aiVector3D scale, translate;
            aiQuaternion rotate;
            bindPoseMatrixAssimp.Decompose(scale, rotate, translate);
            Irufemi::Matrix4x4 bindPoseMatrix =
                Irufemi::Math::MakeAffineMatrix({scale.x, scale.y, scale.z}, {rotate.x, rotate.y, rotate.z, rotate.w},
                                                {translate.x, translate.y, translate.z});
            jointWeightData.inverseBindPoseMatrix = Irufemi::Math::Inverse(bindPoseMatrix);

            for (uint32_t weightIndex = 0; weightIndex < bone->mNumWeights; ++weightIndex) {
                jointWeightData.vertexWeights.push_back(
                    {bone->mWeights[weightIndex].mWeight, bone->mWeights[weightIndex].mVertexId + vertexOffset});
            }
        }

        objModel.meshes.push_back(std::move(outMesh));
        vertexOffset += mesh->mNumVertices;
    }

    ProcessNode(scene->mRootNode, scene, objModel.meshes);

    objModel.rootNode = ReadNode(scene->mRootNode);

    CalculateBoundingSphere(objModel);

    return objModel;
}
