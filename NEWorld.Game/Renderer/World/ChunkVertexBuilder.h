#pragma once

#include "Math/Vector2.h"
#include "Math/Vector3.h"
#include <memory>
#include <cstdint>

struct StandardBlockRect {
    UInt3 Position;
    UInt2 Texture;
    std::uint32_t Face;
    std::uint32_t Rotation;
    std::uint32_t Shade;
};

class ChunkVertexBuilder {
public:
    explicit ChunkVertexBuilder(int capacity);

    void Rect(const StandardBlockRect &rect) noexcept;

    [[nodiscard]] int Count() const noexcept { return mCount; }

    [[nodiscard]] const std::uint32_t *Data() const noexcept { return mData.get(); }

private:
    std::unique_ptr<std::uint32_t> mData;
    int mCount;
    std::uint32_t *mView;
};

class ChunkVertexExpander {
public:
    explicit ChunkVertexExpander(const ChunkVertexBuilder &builder) noexcept
            : mCount(builder.Count()), mView(builder.Data()) {}

    ChunkVertexExpander &SetTarget(float *target) noexcept { return (mTarget = target, *this); }

    ChunkVertexExpander &SetTexturePerLine(float val) noexcept { return (mTexturePerLine = val, *this); }

    ChunkVertexExpander &EnableNormal() noexcept { return (mNormalEnabled = true, *this); }

    ChunkVertexExpander &Expand() noexcept;

private:
    int mCount;
    const std::uint32_t *mView{};
    float *mTarget{};
    float mTexturePerLine{};
    bool mNormalEnabled{false};
};
