#include "World.h"
#include "Textures.h"
#include "WorldGen.h"
#include "Particles.h"
#include <algorithm>
#include <filesystem>
#include "Player.h"
#include "Items.h"

namespace World {

    std::string worldname;
    Brightness skylight = 15;         //Sky light level
    Brightness BRIGHTNESSMAX = 15;    //Maximum brightness
    Brightness BRIGHTNESSMIN = 2;     //Mimimum brightness
    Brightness BRIGHTNESSDEC = 1;     //Brightness decrease
    Chunk *EmptyChunkPtr;
    unsigned int EmptyBuffer;
    int MaxChunkLoads = 64;
    int MaxChunkUnloads = 64;
    int MaxChunkRenders = 1;

    std::vector<Chunk *> chunks{};
    Chunk *cpCachePtr = nullptr;
    chunkid cpCacheID = 0;
    ChunkPtrArray cpArray;
    HeightMap HMap;
    int cloud[128][128];
    int rebuiltChunks, rebuiltChunksCount;
    int updatedChunks, updatedChunksCount;
    int unloadedChunks, unloadedChunksCount;
    int chunkBuildRenderList[256][2];
    int chunkLoadList[256][4];
    std::pair<Chunk *, int> chunkUnloadList[256];
    std::vector<unsigned int> vbuffersShouldDelete;
    int chunkBuildRenders, chunkLoads, chunkUnloads;

    void Init() {
        std::stringstream ss;
        ss << "Worlds/" << worldname << "/";
        std::filesystem::create_directories(ss.str());
        ss.clear();
        ss.str("");
        ss << "Worlds/" << worldname << "/chunks";
        std::filesystem::create_directories(ss.str());

        EmptyChunkPtr = (Chunk *) ~0;

        WorldGen::perlinNoiseInit(3404);
        cpCachePtr = nullptr;
        cpCacheID = 0;

        cpArray.Create((viewdistance + 2) * 2);

        HMap.setSize((viewdistance + 2) * 2 * 16);
        HMap.create();

    }

    auto LowerChunkBound(chunkid cid) noexcept {
        return std::lower_bound(chunks.begin(), chunks.end(), cid, [](auto& left, auto right) noexcept {
            return left->GetId() < right;
        });
    }

    Chunk *AddChunk(Int3 vec) {
        const auto cid = GetChunkId(vec);  //Chunk ID
        const auto chunkIter = LowerChunkBound(cid);
        if (chunkIter != chunks.end()) {
            if ((*chunkIter)->id == cid) {
                printf("[Console][Error]");
                printf("Chunk(%d,%d,%d)has been loaded,when adding Chunk.\n", vec.X, vec.Y, vec.Z);
                return *chunkIter;
            }
        }
        const auto newChunk = new Chunk(vec.X, vec.Y, vec.Z, cid);
        chunks.insert(chunkIter, newChunk);
        cpCacheID = cid;
        cpCachePtr = newChunk;
        cpArray.Add(newChunk, vec);
        return newChunk;
    }

    Chunk *GetChunk(Int3 vec) {
        const auto cid = GetChunkId(vec);
        if (cpCacheID == cid && cpCachePtr) return cpCachePtr;
        auto ret = cpArray.Get(vec);
        if (ret) {
            cpCacheID = cid;
            cpCachePtr = ret;
            return ret;
        }
        if (!chunks.empty()) {
            const auto iter = LowerChunkBound(cid);
            if (iter != chunks.end()) {
                const auto chunk = *iter;
                if (chunk->id == cid) {
                    ret = chunk;
                    cpCacheID = cid;
                    cpCachePtr = ret;
                    cpArray.Add(chunk, vec);
                    return ret;
                }
            }
        }
        return nullptr;
    }

    void DeleteChunk(Int3 vec) {
        const auto id = GetChunkId(vec);  //Chunk ID
        const auto chunkIter = LowerChunkBound(id);
        if (chunkIter != chunks.end()) {
            if ((*chunkIter)->id == id) {
                const auto chunk = *chunkIter;
                chunks.erase(chunkIter);
                delete chunk;
                if (cpCachePtr == chunk) {
                    cpCacheID = 0;
                    cpCachePtr = nullptr;
                }
                cpArray.Remove(vec);
            }
        }
    }


    std::vector<Hitbox::AABB> getHitboxes(const Hitbox::AABB &box) {
        //返回与box相交的所有方块AABB

        Hitbox::AABB blockbox;
        std::vector<Hitbox::AABB> Hitboxes;

        for (auto a = int(box.xmin + 0.5) - 1; a <= int(box.xmax + 0.5) + 1; a++) {
            for (auto b = int(box.ymin + 0.5) - 1; b <= int(box.ymax + 0.5) + 1; b++) {
                for (auto c = int(box.zmin + 0.5) - 1; c <= int(box.zmax + 0.5) + 1; c++) {
                    if (BlockInfo(GetBlock({a, b, c})).isSolid()) {
                        blockbox.xmin = a - 0.5;
                        blockbox.xmax = a + 0.5;
                        blockbox.ymin = b - 0.5;
                        blockbox.ymax = b + 0.5;
                        blockbox.zmin = c - 0.5;
                        blockbox.zmax = c + 0.5;
                        if (Hitbox::Hit(box, blockbox)) Hitboxes.push_back(blockbox);
                    }
                }
            }
        }
        return Hitboxes;
    }

    bool inWater(const Hitbox::AABB &box) {
        Hitbox::AABB blockbox;
        for (auto a = int(box.xmin + 0.5) - 1; a <= int(box.xmax + 0.5); a++) {
            for (auto b = int(box.ymin + 0.5) - 1; b <= int(box.ymax + 0.5); b++) {
                for (auto c = int(box.zmin + 0.5) - 1; c <= int(box.zmax + 0.5); c++) {
                    if (GetBlock({(a), (b), (c)}) == Blocks::WATER || GetBlock({(a), (b), (c)}) == Blocks::LAVA) {
                        blockbox.xmin = a - 0.5;
                        blockbox.xmax = a + 0.5;
                        blockbox.ymin = b - 0.5;
                        blockbox.ymax = b + 0.5;
                        blockbox.zmin = c - 0.5;
                        blockbox.zmax = c + 0.5;
                        if (Hitbox::Hit(box, blockbox)) return true;
                    }
                }
            }
        }
        return false;
    }

    void updateblock(int x, int y, int z, bool blockchanged, int depth) {
        //Blockupdate

        if (depth > 4096) return;
        depth++;

        auto updated = blockchanged;
        const auto cx = GetChunkPos(x);
        const auto cy = GetChunkPos(y);
        const auto cz = GetChunkPos(z);

        if (ChunkOutOfBound({(cx), (cy), (cz)})) return;

        const auto bx = GetBlockPos(x);
        const auto by = GetBlockPos(y);
        const auto bz = GetBlockPos(z);

        auto cptr = GetChunk({(cx), (cy), (cz)});
        if (cptr != nullptr) {
            if (cptr == EmptyChunkPtr) {
                cptr = AddChunk({(cx), (cy), (cz)});
                cptr->Load();
                cptr->Empty = false;
            }
            const auto oldbrightness = cptr->GetBrightness({(bx), (by), (bz)});
            auto skylighted = true;
            auto yi = y + 1;
            auto cyi = GetChunkPos(yi);
            if (y < 0) skylighted = false;
            else {
                while (!ChunkOutOfBound({(cx), (cyi + 1), (cz)}) && ChunkLoaded({(cx), (cyi + 1), (cz)}) &&
                       skylighted) {
                    if (BlockInfo(GetBlock({x, yi, z})).isOpaque() || GetBlock({(x), (yi), (z)}) == Blocks::WATER) {
                        skylighted = false;
                    }
                    yi++;
                    cyi = GetChunkPos(yi);
                }
            }

            if (!BlockInfo(GetBlock({x, y, z})).isOpaque()) {
                Block blks[7] = {0,
                                 (GetBlock({(x), (y), (z + 1)})),    //Front face
                                 (GetBlock({(x), (y), (z - 1)})),    //Back face
                                 (GetBlock({(x + 1), (y), (z)})),    //Right face
                                 (GetBlock({(x - 1), (y), (z)})),    //Left face
                                 (GetBlock({(x), (y + 1), (z)})),    //Top face
                                 (GetBlock({(x), (y - 1), (z)}))};  //Bottom face
                Brightness brts[7] = {0,
                                      getbrightness(x, y, z + 1),    //Front face
                                      getbrightness(x, y, z - 1),    //Back face
                                      getbrightness(x + 1, y, z),    //Right face
                                      getbrightness(x - 1, y, z),    //Left face
                                      getbrightness(x, y + 1, z),    //Top face
                                      getbrightness(x, y - 1, z)};  //Bottom face
                auto maxbrightness = 1;
                for (auto i = 2; i <= 6; i++) {
                    if (brts[maxbrightness] < brts[i]) maxbrightness = i;
                }
                auto br = brts[maxbrightness];
                if (blks[maxbrightness] == Blocks::WATER) {
                    if (br - 2 < BRIGHTNESSMIN) br = BRIGHTNESSMIN; else br -= 2;
                } else {
                    if (br - 1 < BRIGHTNESSMIN) br = BRIGHTNESSMIN; else br--;
                }

                if (skylighted) {
                    if (br < skylight) br = skylight;
                }
                if (br < BRIGHTNESSMIN) br = BRIGHTNESSMIN;
                //Set brightness
                cptr->SetBrightness({(bx), (by), (bz)}, br);

            } else {

                //Opaque block
                cptr->SetBrightness({(bx), (by), (bz)}, 0);
                if (GetBlock({(x), (y), (z)}) == Blocks::GLOWSTONE || GetBlock({(x), (y), (z)}) == Blocks::LAVA) {
                    cptr->SetBrightness({(bx), (by), (bz)}, BRIGHTNESSMAX);
                }

            }

            if (oldbrightness != cptr->GetBrightness({(bx), (by), (bz)})) updated = true;

            if (updated) {
                updateblock(x, y + 1, z, false, depth);
                updateblock(x, y - 1, z, false, depth);
                updateblock(x + 1, y, z, false, depth);
                updateblock(x - 1, y, z, false, depth);
                updateblock(x, y, z + 1, false, depth);
                updateblock(x, y, z - 1, false, depth);
            }

            setChunkUpdated(cx, cy, cz, true);
            if (bx == 15 && cx < worldsize - 1) setChunkUpdated(cx + 1, cy, cz, true);
            if (bx == 0 && cx > -worldsize) setChunkUpdated(cx - 1, cy, cz, true);
            if (by == 15 && cy < worldheight - 1) setChunkUpdated(cx, cy + 1, cz, true);
            if (by == 0 && cy > -worldheight) setChunkUpdated(cx, cy - 1, cz, true);
            if (bz == 15 && cz < worldsize - 1) setChunkUpdated(cx, cy, cz + 1, true);
            if (bz == 0 && cz > -worldsize) setChunkUpdated(cx, cy, cz - 1, true);

        }
    }

    bool ChunkHintMatch(const Int3 c, Chunk *const cptr) noexcept {
        return cptr && c.X == cptr->cx && c.Y == cptr->cy && c.Z == cptr->cz;
    }

    bool ChunkHintNoneEmptyMatch(const Int3 c, Chunk *const cptr) noexcept {
        return cptr != EmptyChunkPtr && ChunkHintMatch(c, cptr);
    }

    Block GetBlock(const Int3 v, Block mask, Chunk *const hint) {
        //获取方块
        const auto c = GetChunkPos(v);
        if (ChunkOutOfBound(c)) return Blocks::AIR;
        const auto b = GetBlockPos(v);
        if (ChunkHintMatch(c, hint)) { return hint->GetBlock(b); }
        const auto ci = GetChunk(c);
        if (ci == EmptyChunkPtr) return Blocks::AIR;
        if (ci) { return ci->GetBlock(b); }
        return mask;
    }

    Brightness GetBrightness(Int3 v, Chunk *const hint) {
        //获取亮度
        const auto c = GetChunkPos(v);
        if (ChunkOutOfBound(c)) return skylight;
        const auto b = GetBlockPos(v);
        if (ChunkHintMatch(c, hint)) { return hint->GetBrightness(b); }
        const auto ci = GetChunk(c);
        if (ci == EmptyChunkPtr) if (c.Y < 0) return BRIGHTNESSMIN; else return skylight;
        if (ci) { return ci->GetBrightness(b); }
        return skylight;
    }

    Chunk *GetChunkNoneLazy(const Int3 c) noexcept {
        auto i = GetChunk(c);
        if (i == EmptyChunkPtr) {
            i = AddChunk(c);
            i->Load();
            i->Empty = false;
        }
        return i;
    }

    void SetBlock(Int3 v, Block block, Chunk *hint) {
        //设置方块
        const auto c = GetChunkPos(v);
        const auto b = GetBlockPos(v);
        if (ChunkHintNoneEmptyMatch(c, hint)) {
            hint->SetBlock(b, block);
            updateblock(v.X, v.Y, v.Z, true);
        }
        else if (!ChunkOutOfBound(c)) {
            if (const auto i = GetChunkNoneLazy(c); i) {
                i->SetBlock(b, block);
                updateblock(v.X, v.Y, v.Z, true);
            }
        }
    }

    void SetBrightness(Int3 v, Brightness brightness, Chunk *hint) {
        //设置亮度
        const auto c = GetChunkPos(v);
        const auto b = GetBlockPos(v);
        if (ChunkHintNoneEmptyMatch(c, hint)) {
            hint->SetBrightness(b, brightness);
        }
        else if (!ChunkOutOfBound(c)) {
            if (const auto i = GetChunkNoneLazy(c); i) {
                i->SetBrightness(b, brightness);
            }
        }
    }

    bool chunkUpdated(const Int3 vec) {
        const auto i = GetChunk(vec);
        if (!i || i == EmptyChunkPtr) return false;
        return i->updated;
    }

    void setChunkUpdated(int x, int y, int z, bool value) {
        if (const auto i = GetChunkNoneLazy({(x), (y), (z)}); i) {
            i->updated = value;
        }
    }

    void sortChunkBuildRenderList(int xpos, int ypos, int zpos) {
        auto p = 0;

        const auto cxp = GetChunkPos(xpos);
        const auto cyp = GetChunkPos(ypos);
        const auto czp = GetChunkPos(zpos);

        for (auto ci = 0; ci < chunks.size(); ci++) {
            if (chunks[ci]->updated) {
                const auto cx = chunks[ci]->cx;
                const auto cy = chunks[ci]->cy;
                const auto cz = chunks[ci]->cz;
                if (!chunkInRange(cx, cy, cz, cxp, cyp, czp, viewdistance)) continue;
                const auto xd = cx * 16 + 7 - xpos;
                const auto yd = cy * 16 + 7 - ypos;
                const auto zd = cz * 16 + 7 - zpos;
                const auto distsqr = xd * xd + yd * yd + zd * zd;
                for (auto i = 0; i < MaxChunkRenders; i++) {
                    if (distsqr < chunkBuildRenderList[i][0] || p <= i) {
                        for (auto j = MaxChunkRenders - 1; j >= i + 1; j--) {
                            chunkBuildRenderList[j][0] = chunkBuildRenderList[j - 1][0];
                            chunkBuildRenderList[j][1] = chunkBuildRenderList[j - 1][1];
                        }
                        chunkBuildRenderList[i][0] = distsqr;
                        chunkBuildRenderList[i][1] = ci;
                        break;
                    }
                }
                if (p < MaxChunkRenders) p++;
            }
        }
        chunkBuildRenders = p;
    }

    void sortChunkLoadUnloadList(int xpos, int ypos, int zpos) {

        int cx, cy, cz, pl = 0, pu = 0, i;
        int xd, yd, zd, distsqr, first, middle, last;

        const auto cxp = GetChunkPos(xpos);
        const auto cyp = GetChunkPos(ypos);
        const auto czp = GetChunkPos(zpos);

        for (auto& chunk : chunks) {
            cx = chunk->cx;
            cy = chunk->cy;
            cz = chunk->cz;
            if (!chunkInRange(cx, cy, cz, cxp, cyp, czp, viewdistance + 1)) {
                xd = cx * 16 + 7 - xpos;
                yd = cy * 16 + 7 - ypos;
                zd = cz * 16 + 7 - zpos;
                distsqr = xd * xd + yd * yd + zd * zd;

                first = 0;
                last = pl - 1;
                while (first <= last) {
                    middle = (first + last) / 2;
                    if (distsqr > chunkUnloadList[middle].second)last = middle - 1;
                    else first = middle + 1;
                }
                if (first > pl || first >= MaxChunkUnloads) continue;
                i = first;

                for (auto j = MaxChunkUnloads - 1; j > i; j--) {
                    chunkUnloadList[j].first = chunkUnloadList[j - 1].first;
                    chunkUnloadList[j].second = chunkUnloadList[j - 1].second;
                }
                chunkUnloadList[i].first = chunk;
                chunkUnloadList[i].second = distsqr;

                if (pl < MaxChunkUnloads) pl++;
            }
        }
        chunkUnloads = pl;

        for (cx = cxp - viewdistance - 1; cx <= cxp + viewdistance; cx++) {
            for (cy = cyp - viewdistance - 1; cy <= cyp + viewdistance; cy++) {
                for (cz = czp - viewdistance - 1; cz <= czp + viewdistance; cz++) {
                    if (ChunkOutOfBound({(cx), (cy), (cz)})) continue;
                    if (cpArray.Get({cx, cy, cz}) == nullptr) {
                        xd = cx * 16 + 7 - xpos;
                        yd = cy * 16 + 7 - ypos;
                        zd = cz * 16 + 7 - zpos;
                        distsqr = xd * xd + yd * yd + zd * zd;

                        first = 0;
                        last = pu - 1;
                        while (first <= last) {
                            middle = (first + last) / 2;
                            if (distsqr < chunkLoadList[middle][0]) last = middle - 1;
                            else first = middle + 1;
                        }
                        if (first > pu || first >= MaxChunkLoads) continue;
                        i = first;

                        for (auto j = MaxChunkLoads - 1; j > i; j--) {
                            chunkLoadList[j][0] = chunkLoadList[j - 1][0];
                            chunkLoadList[j][1] = chunkLoadList[j - 1][1];
                            chunkLoadList[j][2] = chunkLoadList[j - 1][2];
                            chunkLoadList[j][3] = chunkLoadList[j - 1][3];
                        }
                        chunkLoadList[i][0] = distsqr;
                        chunkLoadList[i][1] = cx;
                        chunkLoadList[i][2] = cy;
                        chunkLoadList[i][3] = cz;
                        if (pu < MaxChunkLoads) pu++;
                    }
                }
            }
        }
        chunkLoads = pu;
    }

    void calcVisible(double xpos, double ypos, double zpos, Frustum &frus) {
        Chunk::setRelativeBase(xpos, ypos, zpos, frus);
        for (auto ci = 0; ci != chunks.size(); ci++) chunks[ci]->calcVisible();
    }

    void saveAllChunks() {
#ifndef NEWORLD_DEBUG_NO_FILEIO
        for (auto i = 0; i != chunks.size(); i++) {
            chunks[i]->SaveToFile();
        }
#endif
    }

    void destroyAllChunks() {

        for (auto i = 0; i != chunks.size(); i++) {
            if (!chunks[i]->Empty) {
                chunks[i]->destroyRender();
                chunks[i]->destroy();
                delete chunks[i];
            }
        }
        chunks.clear();
        chunks.shrink_to_fit();

        cpArray.Finalize();
        HMap.destroy();

        rebuiltChunks = 0;
        rebuiltChunksCount = 0;

        updatedChunks = 0;
        updatedChunksCount = 0;

        unloadedChunks = 0;
        unloadedChunksCount = 0;

        memset(chunkBuildRenderList, 0, 256 * 2 * sizeof(int));
        memset(chunkLoadList, 0, 256 * 4 * sizeof(int));

        chunkBuildRenders = 0;
        chunkLoads = 0;
        chunkUnloads = 0;

    }

    void buildtree(int x, int y, int z) {
        //对生成条件进行更严格的检测
        //一：正上方五格必须为空气
        for (auto i = y + 1; i < y + 6; i++) {
            if (GetBlock({(x), (i), (z)}) != Blocks::AIR)return;
        }
        //二：周围五格不能有树
        for (auto ix = x - 4; ix < x + 4; ix++) {
            for (auto iy = y - 4; iy < y + 4; iy++) {
                for (auto iz = z - 4; iz < z + 4; iz++) {
                    if (GetBlock({(ix), (iy), (iz)}) == Blocks::WOOD ||
                        GetBlock({(ix), (iy), (iz)}) == Blocks::LEAF)
                        return;
                }
            }
        }
        //终于可以开始生成了
        //设置泥土
        SetBlock({(x), (y), (z)}, Blocks::DIRT);
        //设置树干
        auto h = 0;//高度
        //测算泥土数量
        auto Dirt = 0;//泥土数
        for (auto ix = x - 4; ix < x + 4; ix++) {
            for (auto iy = y - 4; iy < y; iy++) {
                for (auto iz = z - 4; iz < z + 4; iz++) {
                    if (GetBlock({(ix), (iy), (iz)}) == Blocks::DIRT)Dirt++;
                }
            }
        }
        //测算最高高度
        for (auto i = y + 1; i < y + 16; i++) {
            if (GetBlock({(x), (i), (z)}) == Blocks::AIR) { h++; }
            else { break; };
        }
        //取最小值
        h = std::min(h, int(Dirt * 15 / 268 * std::max(rnd(), 0.8)));
        if (h < 7)return;
        //开始生成树干
        for (auto i = y + 1; i < y + h + 1; i++) {
            SetBlock({(x), (i), (z)}, Blocks::WOOD);
        }
        //设置树叶及枝杈
        //计算树叶起始生成高度
        const auto leafh = int(double(h) * 0.618) + 1;//黄金分割比大法好！！！
        const auto distancen2 = int(double((h - leafh + 1) * (h - leafh + 1))) + 1;
        for (auto iy = y + leafh; iy < y + int(double(h) * 1.382) + 2; iy++) {
            for (auto ix = x - 6; ix < x + 6; ix++) {
                for (auto iz = z - 6; iz < z + 6; iz++) {
                    const auto distancen = DistanceSquare(ix, iy, iz, x, y + leafh + 1, z);
                    if ((GetBlock({(ix), (iy), (iz)}) == Blocks::AIR) && (distancen < distancen2)) {
                        if ((distancen <= distancen2 / 9) && (rnd() > 0.3)) {
                            SetBlock({(ix), (iy), (iz)}, Blocks::WOOD);//生成枝杈
                        } else {
                            SetBlock({(ix), (iy), (iz)}, Blocks::LEAF); //生成树叶
                        }
                    }
                }
            }
        }
    }

    void explode(int x, int y, int z, int r, Chunk *c) {
        const double maxdistsqr = r * r;
        for (auto fx = x - r - 1; fx < x + r + 1; fx++) {
            for (auto fy = y - r - 1; fy < y + r + 1; fy++) {
                for (auto fz = z - r - 1; fz < z + r + 1; fz++) {
                    const auto distsqr = (fx - x) * (fx - x) + (fy - y) * (fy - y) + (fz - z) * (fz - z);
                    if (distsqr <= maxdistsqr * 0.75 ||
                        distsqr <= maxdistsqr && rnd() > (distsqr - maxdistsqr * 0.6) / (maxdistsqr * 0.4)) {
                        const auto e = GetBlock({(fx), (fy), (fz)});
                        if (e == Blocks::AIR) continue;
                        for (auto j = 1; j <= 12; j++) {
                            Particles::throwParticle(e,
                                                     float(fx + rnd() - 0.5f), float(fy + rnd() - 0.2f),
                                                     float(fz + rnd() - 0.5f),
                                                     float(rnd() * 0.2f - 0.1f), float(rnd() * 0.2f - 0.1f),
                                                     float(rnd() * 0.2f - 0.1f),
                                                     float(rnd() * 0.02 + 0.03), int(rnd() * 60) + 30);
                        }
                        SetBlock({(fx), (fy), (fz)}, Blocks::AIR, c);
                    }
                }
            }
        }
    }

    void pickleaf() {
        if (rnd() < 0.2) {
            if (rnd() < 0.5)Player::addItem(APPLE);
            else Player::addItem(STICK);
        } else {
            Player::addItem(Blocks::LEAF);
        }
    }

    void picktree(int x, int y, int z) {
        if (GetBlock({(x), (y), (z)}) != Blocks::LEAF) {
            Player::addItem(GetBlock({(x), (y), (z)}));
        } else pickleaf();
        for (auto j = 1; j <= 10; j++) {
            Particles::throwParticle(GetBlock({(x), (y), (z)}),
                                     float(x + rnd() - 0.5f), float(y + rnd() - 0.2f), float(z + rnd() - 0.5f),
                                     float(rnd() * 0.2f - 0.1f), float(rnd() * 0.2f - 0.1f), float(rnd() * 0.2f - 0.1f),
                                     float(rnd() * 0.02 + 0.03), int(rnd() * 60) + 30);
        }
        SetBlock({(x), (y), (z)}, Blocks::AIR);
        //上
        if ((GetBlock({(x), (y + 1), (z)}) == Blocks::WOOD) || (
                GetBlock({(x), (y + 1), (z)}) == Blocks::LEAF))
            picktree(x, y + 1, z);
        //前
        if ((GetBlock({(x), (y), (z + 1)}) == Blocks::WOOD) || (
                GetBlock({(x), (y), (z + 1)}) == Blocks::LEAF))
            picktree(x, y, z + 1);
        //后
        if ((GetBlock({(x), (y), (z - 1)}) == Blocks::WOOD) || (
                GetBlock({(x), (y), (z - 1)}) == Blocks::LEAF))
            picktree(x, y, z - 1);
        //左
        if ((GetBlock({(x + 1), (y), (z)}) == Blocks::WOOD) || (
                GetBlock({(x + 1), (y), (z)}) == Blocks::LEAF))
            picktree(x + 1, y, z);
        //右
        if ((GetBlock({(x - 1), (y), (z)}) == Blocks::WOOD) || (
                GetBlock({(x - 1), (y), (z)}) == Blocks::LEAF))
            picktree(x - 1, y, z);
    }

    void pickblock(int x, int y, int z) {
        if (GetBlock({(x), (y), (z)}) == Blocks::WOOD &&
            ((GetBlock({(x), (y + 1), (z)}) == Blocks::WOOD) || (
                    GetBlock({(x), (y + 1), (z)}) == Blocks::LEAF)) &&
            (GetBlock({(x), (y), (z + 1)}) == Blocks::AIR) && (
                    GetBlock({(x), (y), (z - 1)}) == Blocks::AIR) &&
            (GetBlock({(x + 1), (y), (z)}) == Blocks::AIR) && (
                    GetBlock({(x - 1), (y), (z)}) == Blocks::AIR) &&
            (GetBlock({(x), (y - 1), (z)}) != Blocks::AIR)
                ) { picktree(x, y + 1, z); }//触发砍树模式
        //击打树叶
        if (GetBlock({(x), (y), (z)}) != Blocks::LEAF) {
            Player::addItem(GetBlock({(x), (y), (z)}));
        } else pickleaf();

        SetBlock({(x), (y), (z)}, Blocks::AIR);
    }

}
