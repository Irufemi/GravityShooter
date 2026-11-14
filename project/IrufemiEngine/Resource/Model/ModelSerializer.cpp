#include "Resource/Model/ModelSerializer.h"
#include <fstream>
#include <filesystem>
#include <iostream>
#include "Core/Utility/Log.h"
#include <vector>

using namespace Irufemi;

namespace {
template <typename T> void WritePOD(std::ofstream& ofs, const T& value) {
    ofs.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

template <typename T> bool ReadPOD(std::ifstream& ifs, T& value) {
    ifs.read(reinterpret_cast<char*>(&value), sizeof(T));
    return !ifs.fail();
}

void WriteString(std::ofstream& ofs, const std::string& str) {
    uint32_t size = static_cast<uint32_t>(str.size());
    WritePOD(ofs, size);
    if (size > 0) {
        ofs.write(str.data(), size);
    }
}

bool ReadString(std::ifstream& ifs, std::string& str, uint64_t fileSize) {
    uint32_t size = 0;
    if (!ReadPOD(ifs, size)) {
        str.clear();
        return false;
    }
    if (size == 0) {
        str.clear();
        return true;
    }

    // 残りファイルサイズによるバリデーション（シークを行わずに計算）
    auto currentPos = ifs.tellg();
    if (currentPos == std::streampos(-1) || static_cast<uint64_t>(currentPos) > fileSize ||
        static_cast<uint64_t>(size) > (fileSize - static_cast<uint64_t>(currentPos))) {
        Log::OutPutLog(std::cerr, "[ModelSerializer] Error: String size validation failed.\n");
        str.clear();
        return false;
    }

    str.resize(size);
    ifs.read(str.data(), size);
    if (ifs.fail()) {
        str.clear();
        return false;
    }
    return true;
}

template <typename T> void WriteVectorPOD(std::ofstream& ofs, const std::vector<T>& vec) {
    uint32_t size = static_cast<uint32_t>(vec.size());
    WritePOD(ofs, size);
    if (size > 0) {
        ofs.write(reinterpret_cast<const char*>(vec.data()), size * sizeof(T));
    }
}

template <typename T> bool ReadVectorPOD(std::ifstream& ifs, std::vector<T>& vec, uint64_t fileSize) {
    uint32_t size = 0;
    if (!ReadPOD(ifs, size)) {
        vec.clear();
        return false;
    }
    if (size == 0) {
        vec.clear();
        return true;
    }

    // 残りファイルサイズによるバリデーション（シークを行わずに計算）
    auto currentPos = ifs.tellg();
    uint64_t bytesNeeded = static_cast<uint64_t>(size) * sizeof(T);
    if (currentPos == std::streampos(-1) || static_cast<uint64_t>(currentPos) > fileSize ||
        bytesNeeded > (fileSize - static_cast<uint64_t>(currentPos))) {
        Log::OutPutLog(std::cerr, "[ModelSerializer] Error: Vector size validation failed.\n");
        vec.clear();
        return false;
    }

    vec.resize(size);
    ifs.read(reinterpret_cast<char*>(vec.data()), bytesNeeded);
    if (ifs.fail()) {
        vec.clear();
        return false;
    }
    return true;
}

void WriteMaterial(std::ofstream& ofs, const ObjMaterial& mat) {
    WritePOD(ofs, mat.color);
    WritePOD(ofs, mat.ambient);
    WritePOD(ofs, mat.specular);
    WritePOD(ofs, mat.roughness);
    WritePOD(ofs, mat.metallic);
    WritePOD(ofs, mat.alpha);
    WritePOD(ofs, mat.enableLighting);
    WritePOD(ofs, mat.lightingMode);
    WritePOD(ofs, mat.useClampSampler);
    WritePOD(ofs, mat.environmentCoefficient);
    WritePOD(ofs, mat.alphaReference);
    WritePOD(ofs, mat.uvTransform);
    WriteString(ofs, mat.textureFilePath);
    WriteString(ofs, mat.normalMapFilePath);
}

bool ReadMaterial(std::ifstream& ifs, ObjMaterial& mat, uint64_t fileSize) {
    if (!ReadPOD(ifs, mat.color)) {
        return false;
    }
    if (!ReadPOD(ifs, mat.ambient)) {
        return false;
    }
    if (!ReadPOD(ifs, mat.specular)) {
        return false;
    }
    if (!ReadPOD(ifs, mat.roughness)) {
        return false;
    }
    if (!ReadPOD(ifs, mat.metallic)) {
        return false;
    }
    if (!ReadPOD(ifs, mat.alpha)) {
        return false;
    }
    if (!ReadPOD(ifs, mat.enableLighting)) {
        return false;
    }
    if (!ReadPOD(ifs, mat.lightingMode)) {
        return false;
    }
    if (!ReadPOD(ifs, mat.useClampSampler)) {
        return false;
    }
    if (!ReadPOD(ifs, mat.environmentCoefficient)) {
        return false;
    }
    if (!ReadPOD(ifs, mat.alphaReference)) {
        return false;
    }
    if (!ReadPOD(ifs, mat.uvTransform)) {
        return false;
    }
    if (!ReadString(ifs, mat.textureFilePath, fileSize)) {
        return false;
    }
    if (!ReadString(ifs, mat.normalMapFilePath, fileSize)) {
        return false;
    }
    return true;
}

void WriteNode(std::ofstream& ofs, const Node& node) {
    WritePOD(ofs, node.transform);
    WritePOD(ofs, node.localMatrix);
    WriteString(ofs, node.name);

    uint32_t childrenCount = static_cast<uint32_t>(node.children.size());
    WritePOD(ofs, childrenCount);
    for (const auto& child : node.children) {
        WriteNode(ofs, child);
    }
}

bool ReadNode(std::ifstream& ifs, Node& node, uint64_t fileSize) {
    if (!ReadPOD(ifs, node.transform)) {
        return false;
    }
    if (!ReadPOD(ifs, node.localMatrix)) {
        return false;
    }
    if (!ReadString(ifs, node.name, fileSize)) {
        return false;
    }

    uint32_t childrenCount = 0;
    if (!ReadPOD(ifs, childrenCount)) {
        return false;
    }

    auto currentPos = ifs.tellg();
    if (currentPos == std::streampos(-1) || static_cast<uint64_t>(currentPos) > fileSize) {
        return false;
    }
    // ノード1つあたり最低でも transform + localMatrix + name長(4B) + childrenCount(4B) が必要
    uint64_t minNodeSize = sizeof(node.transform) + sizeof(node.localMatrix) + sizeof(uint32_t) + sizeof(uint32_t);
    if (static_cast<uint64_t>(childrenCount) * minNodeSize > (fileSize - static_cast<uint64_t>(currentPos))) {
        Log::OutPutLog(std::cerr, "[ModelSerializer] Error: Node children count validation failed.\n");
        return false;
    }

    node.children.resize(childrenCount);
    for (uint32_t i = 0; i < childrenCount; ++i) {
        if (!ReadNode(ifs, node.children[i], fileSize)) {
            return false;
        }
    }
    return true;
}
} // namespace

bool ModelSerializer::Serialize(const std::string& filepath, const ObjModel& model, uint64_t sourceLastWriteTime) {
    std::ofstream ofs(filepath, std::ios::binary);
    if (!ofs.is_open()) {
        Log::OutPutLog(std::cerr, "[ModelSerializer] Error: Failed to open file for writing: " + filepath + "\n");
        return false;
    }

    // ヘッダー書き込み
    Header header;
    header.magic = kMagicNumber;
    header.version = kVersion;
    header.sourceLastWriteTime = sourceLastWriteTime;
    WritePOD(ofs, header);

    // メッシュ
    uint32_t meshCount = static_cast<uint32_t>(model.meshes.size());
    WritePOD(ofs, meshCount);
    for (const auto& mesh : model.meshes) {
        WriteVectorPOD(ofs, mesh.vertices);
        WriteVectorPOD(ofs, mesh.indices);
        WriteMaterial(ofs, mesh.material);
        WriteString(ofs, mesh.nodeName);
    }

    // ノードツリー
    WriteNode(ofs, model.rootNode);

    // スキンクラスター
    uint32_t skinCount = static_cast<uint32_t>(model.skinClusterData.size());
    WritePOD(ofs, skinCount);
    for (const auto& pair : model.skinClusterData) {
        WriteString(ofs, pair.first);
        WritePOD(ofs, pair.second.inverseBindPoseMatrix);
        WriteVectorPOD(ofs, pair.second.vertexWeights);
    }

    // 境界ボリューム
    WritePOD(ofs, model.boundingSphere);
    WritePOD(ofs, model.boundingBox);

    return true;
}

bool ModelSerializer::Deserialize(const std::string& filepath, ObjModel& outModel, uint64_t& outSourceLastWriteTime) {
    std::ifstream ifs(filepath, std::ios::binary);
    if (!ifs.is_open()) {
        Log::OutPutLog(std::cerr, "[ModelSerializer] Error: Failed to open file for reading: " + filepath + "\n");
        return false;
    }

    Header header;
    if (!ReadHeader(filepath, header)) {
        return false; // ヘッダーチェック
    }

    // ファイルサイズを一度だけ計算してキャッシュ
    ifs.seekg(0, std::ios::end);
    uint64_t fileSize = static_cast<uint64_t>(ifs.tellg());
    ifs.seekg(sizeof(Header), std::ios::beg); // ヘッダーの次から読み込む

    outSourceLastWriteTime = header.sourceLastWriteTime;

    // メッシュ
    uint32_t meshCount = 0;
    if (!ReadPOD(ifs, meshCount)) {
        return false;
    }

    auto currentPos = ifs.tellg();
    if (currentPos == std::streampos(-1) || static_cast<uint64_t>(currentPos) > fileSize) {
        return false;
    }

    outModel.meshes.resize(meshCount);
    for (uint32_t i = 0; i < meshCount; ++i) {
        if (!ReadVectorPOD(ifs, outModel.meshes[i].vertices, fileSize)) {
            return false;
        }
        if (!ReadVectorPOD(ifs, outModel.meshes[i].indices, fileSize)) {
            return false;
        }
        if (!ReadMaterial(ifs, outModel.meshes[i].material, fileSize)) {
            return false;
        }
        if (!ReadString(ifs, outModel.meshes[i].nodeName, fileSize)) {
            return false;
        }
    }

    // ノードツリー
    if (!ReadNode(ifs, outModel.rootNode, fileSize)) {
        return false;
    }

    // スキンクラスター
    uint32_t skinCount = 0;
    if (!ReadPOD(ifs, skinCount)) {
        return false;
    }
    outModel.skinClusterData.clear();
    for (uint32_t i = 0; i < skinCount; ++i) {
        std::string key;
        if (!ReadString(ifs, key, fileSize)) {
            return false;
        }
        JointWeightData data;
        if (!ReadPOD(ifs, data.inverseBindPoseMatrix)) {
            return false;
        }
        if (!ReadVectorPOD(ifs, data.vertexWeights, fileSize)) {
            return false;
        }
        outModel.skinClusterData[key] = std::move(data);
    }

    // 境界ボリューム
    if (!ReadPOD(ifs, outModel.boundingSphere)) {
        return false;
    }
    if (!ReadPOD(ifs, outModel.boundingBox)) {
        return false;
    }

    if (ifs.fail()) {
        return false;
    }

    return true;
}

bool ModelSerializer::ReadHeader(const std::string& filepath, Header& outHeader) {
    std::ifstream ifs(filepath, std::ios::binary);
    if (!ifs.is_open()) {
        Log::OutPutLog(std::cerr,
                       "[ModelSerializer] Error: Failed to open file for reading header: " + filepath + "\n");
        return false;
    }

    if (!ReadPOD(ifs, outHeader)) {
        return false;
    }
    if (outHeader.magic != kMagicNumber) {
        return false;
    }
    if (outHeader.version != kVersion) {
        return false;
    }

    return true;
}
