#include "Renderer/Object/2D/PrimitiveBatch/Primitive2DBatch.h"
#include "Renderer/DrawManager.h"
#include "Resource/Texture/TextureManager.h"
#include "Core/Math/MathFunction.h"
#include <cmath>
#include <algorithm>

Primitive2DBatch::Primitive2DBatch() {}

Primitive2DBatch::~Primitive2DBatch() {
    if (auto dxCommon = BaseResource::GetDirectXCommon()) {
        if (vertexResource_) {
            dxCommon->ReleaseAfterFence(std::move(vertexResource_));
        }
        if (indexResource_) {
            dxCommon->ReleaseAfterFence(std::move(indexResource_));
        }
    }
}

void Primitive2DBatch::Initialize(Irufemi::Primitive2DType type, const std::string& textureName) {
    type_ = type;
    isMeshDirty_ = true;

    if (textureManager_) {
        textureHandle_ = textureManager_->LoadTexture(textureName);
    }

    RebuildMesh();
}

void Primitive2DBatch::SetSubdivision(uint32_t subdiv) {
    if (subdivision_ != subdiv) {
        subdivision_ = subdiv;
        isMeshDirty_ = true;
    }
}

void Primitive2DBatch::SetThickness(float thickness) {
    if (thickness_ != thickness) {
        thickness_ = thickness;
        if (type_ == Irufemi::Primitive2DType::Ring || type_ == Irufemi::Primitive2DType::Line) {
            isMeshDirty_ = true;
        }
    }
}

void Primitive2DBatch::RebuildMesh() {
    vertexDataList_.clear();
    indexDataList_.clear();

    switch (type_) {
    case Irufemi::Primitive2DType::Rect:
        BuildRect();
        break;
    case Irufemi::Primitive2DType::Triangle:
        BuildTriangle();
        break;
    case Irufemi::Primitive2DType::Circle:
        BuildCircle(subdivision_);
        break;
    case Irufemi::Primitive2DType::Ring:
        BuildRing(subdivision_);
        break;
    case Irufemi::Primitive2DType::Line:
        BuildLine();
        break;
    }

    CreateResource();
    isMeshDirty_ = false;
}

void Primitive2DBatch::CreateResource() {
    if (!dx_) {
        return;
    }

    if (!vertexDataList_.empty()) {
        if (vertexResource_) {
            dx_->ReleaseAfterFence(std::move(vertexResource_));
        }
        vertexResource_ = dx_->CreateBufferResource(sizeof(VertexData) * vertexDataList_.size());
        vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
        vertexBufferView_.SizeInBytes = static_cast<UINT>(sizeof(VertexData) * vertexDataList_.size());
        vertexBufferView_.StrideInBytes = sizeof(VertexData);
    }

    if (!indexDataList_.empty()) {
        if (indexResource_) {
            dx_->ReleaseAfterFence(std::move(indexResource_));
        }
        indexResource_ = dx_->CreateBufferResource(sizeof(uint32_t) * indexDataList_.size());
        indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
        indexBufferView_.SizeInBytes = static_cast<UINT>(sizeof(uint32_t) * indexDataList_.size());
        indexBufferView_.Format = DXGI_FORMAT_R32_UINT;
        indexCount_ = static_cast<uint32_t>(indexDataList_.size());
    }
}

void Primitive2DBatch::SyncBeforeDraw() {
    BaseBatch::SyncBeforeDraw();

    if (isMeshDirty_) {
        RebuildMesh();

        // データのコピー（メッシュ形状変更時のみGPUへ転送）
        if (vertexResource_ && !vertexDataList_.empty()) {
            VertexData* mapped = nullptr;
            if (SUCCEEDED(vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&mapped))) && mapped) {
                std::memcpy(mapped, vertexDataList_.data(), sizeof(VertexData) * vertexDataList_.size());
                vertexResource_->Unmap(0, nullptr);
            }
        }

        if (indexResource_ && !indexDataList_.empty()) {
            uint32_t* mapped = nullptr;
            if (SUCCEEDED(indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&mapped))) && mapped) {
                std::memcpy(mapped, indexDataList_.data(), sizeof(uint32_t) * indexDataList_.size());
                indexResource_->Unmap(0, nullptr);
            }
        }
    }
}

void Primitive2DBatch::Draw() {
    if (!drawManager_ || GetInstanceCount() == 0) {
        return;
    }

    SyncBeforeDraw();

    RenderPackets::Primitive2DBatchPacket p{};
    p.vertexBufferView = vertexBufferView_;
    p.indexBufferView = indexBufferView_;
    p.materialAddress = GetMaterialVAddress();
    p.instancingSrvHandleGPU = GetInstancingSrvHandleGPU();
    p.indexCount = indexCount_;
    p.instanceCount = GetInstanceCount();
    p.blendMode = blendMode_;
    p.depthWrite = depthWrite_;
    p.cullMode = cullMode_;
    p.customPSO = customPSO_;
    p.customCBVAddress = customCBVAddress_;

    drawManager_->SubmitPrimitive2DBatch(p);
}

// --- メッシュ構築 (単位サイズ 1.0, ピボット中心 (0.5, 0.5) ベース) ---

void Primitive2DBatch::BuildRect() {
    float left = -0.5f;
    float right = 0.5f;
    float top = -0.5f;
    float bottom = 0.5f;

    vertexDataList_ = {{{left, top, 0.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
                       {{right, top, 0.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
                       {{left, bottom, 0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
                       {{right, bottom, 0.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}}};
    indexDataList_ = {0, 1, 2, 2, 1, 3};
}

void Primitive2DBatch::BuildTriangle() {
    float left = -0.5f;
    float right = 0.5f;
    float top = -0.5f;
    float bottom = 0.5f;
    float centerX = 0.0f;

    vertexDataList_ = {{{centerX, top, 0.0f, 1.0f}, {0.5f, 0.0f}, {0.0f, 0.0f, -1.0f}},
                       {{right, bottom, 0.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
                       {{left, bottom, 0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}}};
    indexDataList_ = {0, 1, 2};
}

void Primitive2DBatch::BuildCircle(uint32_t subdiv) {
    if (subdiv < 3) {
        subdiv = 3;
    }
    float radius = 0.5f;

    vertexDataList_.push_back({{0.0f, 0.0f, 0.0f, 1.0f}, {0.5f, 0.5f}, {0.0f, 0.0f, -1.0f}}); // Center

    for (uint32_t i = 0; i <= subdiv; ++i) {
        float angle = (static_cast<float>(i) / subdiv) * Irufemi::Math::TwoPI;
        float cosA = std::cos(angle);
        float sinA = std::sin(angle);

        float vx = cosA * radius;
        float vy = sinA * radius;
        float u = cosA * 0.5f + 0.5f;
        float v = sinA * 0.5f + 0.5f;
        vertexDataList_.push_back({{vx, vy, 0.0f, 1.0f}, {u, v}, {0.0f, 0.0f, -1.0f}});
    }

    for (uint32_t i = 0; i < subdiv; ++i) {
        indexDataList_.push_back(0);
        indexDataList_.push_back(i + 1);
        indexDataList_.push_back(i + 2);
    }
}

void Primitive2DBatch::BuildRing(uint32_t subdiv) {
    if (subdiv < 3) {
        subdiv = 3;
    }
    float outerRadius = 0.5f;
    // Ring の thickness は 0.0 ~ 1.0 の割合（0.5 = 半分が穴）として扱うか、
    // ここで固定値にするか。スケーリング時に太さもスケーリングされる仕様にする。
    float innerRadius = (std::max)(0.0f, outerRadius - thickness_ * 0.5f);

    for (uint32_t i = 0; i <= subdiv; ++i) {
        float angle = (static_cast<float>(i) / subdiv) * Irufemi::Math::TwoPI;
        float cosA = std::cos(angle);
        float sinA = std::sin(angle);

        float inX = cosA * innerRadius;
        float inY = sinA * innerRadius;
        float outX = cosA * outerRadius;
        float outY = sinA * outerRadius;

        float uOut = cosA * 0.5f + 0.5f;
        float vOut = sinA * 0.5f + 0.5f;
        float uIn = cosA * 0.5f * (innerRadius / outerRadius) + 0.5f;
        float vIn = sinA * 0.5f * (innerRadius / outerRadius) + 0.5f;

        vertexDataList_.push_back({{inX, inY, 0.0f, 1.0f}, {uIn, vIn}, {0.0f, 0.0f, -1.0f}});
        vertexDataList_.push_back({{outX, outY, 0.0f, 1.0f}, {uOut, vOut}, {0.0f, 0.0f, -1.0f}});
    }

    for (uint32_t i = 0; i < subdiv; ++i) {
        uint32_t p0 = i * 2;
        uint32_t p1 = i * 2 + 1;
        uint32_t p2 = i * 2 + 2;
        uint32_t p3 = i * 2 + 3;
        indexDataList_.push_back(p0);
        indexDataList_.push_back(p1);
        indexDataList_.push_back(p2);
        indexDataList_.push_back(p2);
        indexDataList_.push_back(p1);
        indexDataList_.push_back(p3);
    }
}

void Primitive2DBatch::BuildLine() {
    float length = 1.0f;
    float thick = thickness_; // 太さはTransformのYスケール等で補正できるが、ここでは基本値

    float left = -length * 0.5f;
    float right = length * 0.5f;
    float top = -thick * 0.5f;
    float bottom = thick * 0.5f;

    vertexDataList_ = {{{left, top, 0.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
                       {{right, top, 0.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
                       {{left, bottom, 0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
                       {{right, bottom, 0.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}}};
    indexDataList_ = {0, 1, 2, 2, 1, 3};
}
