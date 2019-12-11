#include "ChunkVertexBuilder.h"

namespace {
    constexpr std::uint32_t Rotation[][4] = {
            {0x0000, 0x0001, 0x0101, 0x0100},
            {0x0001, 0x0101, 0x0100, 0x0000},
            {0x0101, 0x0100, 0x0000, 0x0001},
            {0x0100, 0x0000, 0x0001, 0x0101}
    };

    constexpr std::uint32_t Faces[][4] = {
            {0x010101, 0x010001, 0x010000, 0x010100},
            {0x000100, 0x000000, 0x000001, 0x000101},
            {0x000100, 0x000101, 0x010101, 0x010100},
            {0x000001, 0x000000, 0x010000, 0x010001},
            {0x000101, 0x000001, 0x010001, 0x010101},
            {0x010100, 0x010000, 0x000000, 0x000100}
    };

    constexpr float Normals[][3] = {
            {1, 0, 0}, {-1, 0, 0},
            {0, 1, 0}, {0, -1, 0},
            {0, 0, 1}, {0, 0, -1}
    };
}

ChunkVertexBuilder::ChunkVertexBuilder(int capacity)
        : mData(new std::uint32_t[capacity]), mCount(0), mView(mData.get()) {}

void ChunkVertexBuilder::Rect(const StandardBlockRect &rect) noexcept {
    const auto high = (rect.Shade << 24u) | (rect.Position.X << 16u) | (rect.Position.Y << 8u) | rect.Position.Z;
    const auto low = (rect.Face << 16u) | (rect.Texture.X << 8u) | rect.Texture.Y;
    for (auto i = 0; i < 4; ++i) {
        *(mView++) = high + Faces[rect.Face][i];
        *(mView++) = low + Rotation[rect.Rotation][i];
    }
    mCount += 4;
}

ChunkVertexExpander &ChunkVertexExpander::Expand() noexcept {
    for (auto i = 0; i < mCount; ++i) {
        const auto high = *mView++;
        const auto low = *mView++;
        *(mTarget++) = static_cast<float>((high >> 16u) & 0xFFu);
        *(mTarget++) = static_cast<float>((high >> 8u) & 0xFFu);
        *(mTarget++) = static_cast<float>(high & 0xFFu);
        *(mTarget++) = static_cast<float>((low >> 8u) & 0xFFu) / mTexturePerLine;
        *(mTarget++) = static_cast<float>(low & 0xFFu) / mTexturePerLine;
        *(mTarget++) = static_cast<float>((((high >> 24u) & 0xFFu) + 1)) * 0.0625f;
        if (mNormalEnabled) {
            const auto normal = (low >> 16u) & 0xFFu;
            mTarget[0] = Normals[normal][0];
            mTarget[1] = Normals[normal][1];
            mTarget[2] = Normals[normal][2];
            mTarget += 3;
        }
    }
    return *this;
}
