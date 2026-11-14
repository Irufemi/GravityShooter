#include "Resource/Model/AnimationSerializer.h"
#include <fstream>
#include <iostream>
#include "Core/Utility/Log.h"
#include <vector>

using namespace Irufemi;

namespace {
template <typename T> void WritePOD(std::ofstream& ofs, const T& value) {
    ofs.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

template <typename T> void ReadPOD(std::ifstream& ifs, T& value) {
    ifs.read(reinterpret_cast<char*>(&value), sizeof(T));
}

void WriteString(std::ofstream& ofs, const std::string& str) {
    uint32_t size = static_cast<uint32_t>(str.size());
    WritePOD(ofs, size);
    if (size > 0) {
        ofs.write(str.data(), size);
    }
}

void ReadString(std::ifstream& ifs, std::string& str) {
    uint32_t size = 0;
    ReadPOD(ifs, size);

    // サイズバリデーション（キャッシュ破損による巨大アロケーションクラッシュ防止）
    if (size > 4096) {
        Log::OutPutLog(std::cerr, "[AnimationSerializer] Error: Invalid string size detected (" + std::to_string(size) +
                                      " bytes). File might be corrupted.\n");
        str.clear();
        return;
    }

    if (size > 0) {
        str.resize(size);
        ifs.read(str.data(), size);
    } else {
        str.clear();
    }
}

template <typename T> void WriteCurve(std::ofstream& ofs, const AnimationCurve<T>& curve) {
    uint32_t size = static_cast<uint32_t>(curve.keyframes.size());
    WritePOD(ofs, size);
    if (size > 0) {
        ofs.write(reinterpret_cast<const char*>(curve.keyframes.data()), size * sizeof(Keyframe<T>));
    }
}

template <typename T> void ReadCurve(std::ifstream& ifs, AnimationCurve<T>& curve) {
    uint32_t size = 0;
    ReadPOD(ifs, size);
    if (size > 65536) {
        Log::OutPutLog(std::cerr, "[AnimationSerializer] Error: Unreasonable keyframe count (" + std::to_string(size) +
                                      "). File corrupted.\n");
        curve.keyframes.clear();
        return;
    }
    if (size > 0) {
        curve.keyframes.resize(size);
        ifs.read(reinterpret_cast<char*>(curve.keyframes.data()), size * sizeof(Keyframe<T>));
    } else {
        curve.keyframes.clear();
    }
}

void WriteNodeAnimation(std::ofstream& ofs, const NodeAnimation& na) {
    WriteCurve(ofs, na.translate);
    WriteCurve(ofs, na.rotate);
    WriteCurve(ofs, na.scale);
}

void ReadNodeAnimation(std::ifstream& ifs, NodeAnimation& na) {
    ReadCurve(ifs, na.translate);
    ReadCurve(ifs, na.rotate);
    ReadCurve(ifs, na.scale);
}
} // namespace

bool AnimationSerializer::Serialize(const std::string& filepath, const Animation& animation,
                                    uint64_t sourceLastWriteTime) {
    std::ofstream ofs(filepath, std::ios::binary);
    if (!ofs.is_open()) {
        Log::OutPutLog(std::cerr, "[AnimationSerializer] Error: Failed to open file for writing: " + filepath + "\n");
        return false;
    }

    Header header;
    header.magic = kMagicNumber;
    header.version = kVersion;
    header.sourceLastWriteTime = sourceLastWriteTime;
    WritePOD(ofs, header);

    WritePOD(ofs, animation.duration);

    uint32_t animCount = static_cast<uint32_t>(animation.nodeAnimations.size());
    WritePOD(ofs, animCount);
    for (const auto& pair : animation.nodeAnimations) {
        WriteString(ofs, pair.first);
        WriteNodeAnimation(ofs, pair.second);
    }

    return true;
}

bool AnimationSerializer::Deserialize(const std::string& filepath, Animation& outAnimation,
                                      uint64_t& outSourceLastWriteTime) {
    std::ifstream ifs(filepath, std::ios::binary);
    if (!ifs.is_open()) {
        Log::OutPutLog(std::cerr, "[AnimationSerializer] Error: Failed to open file for reading: " + filepath + "\n");
        return false;
    }

    Header header;
    if (!ReadHeader(filepath, header)) {
        return false;
    }

    ifs.seekg(sizeof(Header), std::ios::beg);
    outSourceLastWriteTime = header.sourceLastWriteTime;

    ReadPOD(ifs, outAnimation.duration);

    uint32_t animCount = 0;
    ReadPOD(ifs, animCount);
    outAnimation.nodeAnimations.clear();
    for (uint32_t i = 0; i < animCount; ++i) {
        std::string key;
        ReadString(ifs, key);
        NodeAnimation na;
        ReadNodeAnimation(ifs, na);
        outAnimation.nodeAnimations[key] = na;
    }

    return true;
}

bool AnimationSerializer::ReadHeader(const std::string& filepath, Header& outHeader) {
    std::ifstream ifs(filepath, std::ios::binary);
    if (!ifs.is_open()) {
        Log::OutPutLog(std::cerr,
                       "[AnimationSerializer] Error: Failed to open file for reading header: " + filepath + "\n");
        return false;
    }

    ReadPOD(ifs, outHeader);
    if (outHeader.magic != kMagicNumber) {
        return false;
    }
    if (outHeader.version != kVersion) {
        return false;
    }

    return true;
}
