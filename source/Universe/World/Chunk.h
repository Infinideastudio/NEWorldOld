#pragma once

#include "Definitions.h"
#include "Hitbox.h"
#include "Blocks.h"
#include "Frustum.h"
#include <cstring>
#include <Math/Vector3.h>

class Object;

namespace World {

    extern std::string worldname;
    extern Brightness BRIGHTNESSMIN;
    extern Brightness skylight;

    class Chunk;

    void explode(int x, int y, int z, int r, Chunk *c);

    constexpr unsigned int ChunkEdgeSizeLog2 = 4;
    constexpr unsigned int ChunkPlaneSizeLog2 = ChunkEdgeSizeLog2 * 2u;
    constexpr unsigned int ChunkCubicSizeLog2 = ChunkEdgeSizeLog2 * 3u;
    constexpr unsigned int ChunkEdgeSize = 1u << ChunkEdgeSizeLog2;
    constexpr unsigned int ChunkPlaneSize = 1u << ChunkPlaneSizeLog2;
    constexpr unsigned int ChunkCubicSize = 1u << ChunkCubicSizeLog2;

    class Chunk {
    private:
        Block *mBlock;
        Brightness *mBrightness;
        std::vector<Object *> objects;
        static double relBaseX, relBaseY, relBaseZ;
        static Frustum TestFrustum;

        static constexpr unsigned int GetIndex(const Int3 vec) noexcept {
            const auto v = UInt3(vec);
            return (v.X << ChunkPlaneSizeLog2) | (v.Y << ChunkEdgeSizeLog2) | v.Z;
        }
    public:
        //竟然一直都没有构造函数/析构函数 还要手动调用Init...我受不了啦(╯‵□′)╯︵┻━┻ --Null
        //2333 --qiaozhanrong
        Chunk(int cxi, int cyi, int czi, chunkid idi) : cx(cxi), cy(cyi), cz(czi), id(idi),
                                                        Modified(false), Empty(false), updated(false),
                                                        renderBuilt(false), DetailGenerated(false), loadAnim(0.0) {
            memset(vertexes, 0, sizeof(vertexes));
            memset(vbuffer, 0, sizeof(vbuffer));
        }

        int cx, cy, cz;
        Hitbox::AABB aabb;
        bool Empty, updated, renderBuilt, Modified, DetailGenerated;
        chunkid id;
        vtxCount vertexes[4];
        VBOID vbuffer[4];
        double loadAnim;
        bool visible;

        [[nodiscard]] chunkid GetId() const noexcept { return id; }

        void create();

        void destroy();

        void Load(bool initIfEmpty = true);

        void Unload();

        void buildTerrain(bool initIfEmpty = true);

        void buildDetail();

        void build(bool initIfEmpty = true);

        std::string getChunkPath() {
            //assert(Empty == false);
            std::stringstream ss;
            ss << "Worlds/" << worldname << "/chunks/chunk_" << cx << "_" << cy << "_" << cz << ".NEWorldChunk";
            return ss.str();
        }

        std::string getObjectsPath() {
            std::stringstream ss;
            ss << "Worlds/" << worldname << "/objects/chunk_" << cx << "_" << cy << "_" << cz << ".NEWorldObjects";
            return ss.str();
        }

        bool LoadFromFile(); //返回true代表区块文件打开成功

        void SaveToFile();

        void buildRender();

        void destroyRender();

        Block GetBlock(const Int3 vec) noexcept {return mBlock[GetIndex(vec)];}

        Brightness GetBrightness(const Int3 vec) noexcept {return mBrightness[GetIndex(vec)];}

        void SetBlock(const Int3 vec, Block block) noexcept {
            if (block == Blocks::TNT) {
                World::explode(cx * 16 + vec.X, cy * 16 + vec.Y, cz * 16 + vec.Z, 8, this);
                return;
            }
            mBlock[GetIndex(vec)] = block;
            Modified = true;
        }

        void SetBrightness(const Int3 vec, Brightness brightness) noexcept {
            mBrightness[GetIndex(vec)] = brightness;
            Modified = true;
        }

        static void setRelativeBase(double x, double y, double z, Frustum &frus) {
            relBaseX = x;
            relBaseY = y;
            relBaseZ = z;
            TestFrustum = frus;
        }

        Hitbox::AABB getBaseAABB();

        Frustum::ChunkBox getRelativeAABB();

        void calcVisible() { visible = TestFrustum.FrustumTest(getRelativeAABB()); }

    };
}
