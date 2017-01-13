#include "World.h"
#include "Textures.h"
#include "Renderer.h"
#include "WorldGen.h"

extern int viewdistance;

namespace world
{
    string worldname;
    brightness skylight = 15;         //Sky light level
    brightness BRIGHTNESSMAX = 15;    //Maximum brightness
    brightness BRIGHTNESSMIN = 2;     //Mimimum brightness
    brightness BRIGHTNESSDEC = 1;     //Brightness decrease
    World mWorld;

    HeightMap HMap;
    vector<unsigned int> vbuffersShouldDelete;

    void World::tryLoadUnloadChunks(const Vec3i& centre)
    {
        OrderedList<int, Vec3i, MaxChunkLoads> chunkLoadList;
        OrderedList<int, chunk*, MaxChunkUnloads, std::greater> chunkUnloadList;
        Vec3i ccentre = centre / 16, c;

        for (auto&& chk : mWorld)
        {
            c = Vec3i{ chk.second.cx, chk.second.cy, chk.second.cz };
            if (!chunkInRange(c.x, c.y, c.z, ccentre.x, ccentre.y, ccentre.z ,viewdistance + 1))
                chunkUnloadList.insert(((c + Vec3i{ 7, 7, 7 }) *16 - centre).lengthSqr(), &chk.second);
        }

        for (c.x = ccentre.x - viewdistance - 1; c.x <= ccentre.x + viewdistance; c.x++)
            for (c.y = ccentre.y - viewdistance - 1; c.y <= ccentre.y + viewdistance; c.y++)
                for (c.z = ccentre.z - viewdistance - 1; c.z <= ccentre.z + viewdistance; c.z++)
                    if (!chunkLoaded(c.x, c.y, c.z))
                        chunkLoadList.insert(((c + Vec3i{ 7, 7, 7 }) *16 - centre).lengthSqr(), c);

        // Unload chunks
        for (auto&& iter : chunkUnloadList)
        {
            int cx = iter.second->cx, cy = iter.second->cy, cz = iter.second->cz;
            iter.second->Unload();
            eraseChunk(cx, cy, cz);
        }

        // Load chunks
        for (auto&& iter : chunkLoadList)
        {
            auto *chk = insertChunk(iter.second.x, iter.second.y, iter.second.z);
            chk->Load();
            if (chk->Empty)
                chk->Unload();
        }
    }

    void World::tryUpdateRenderers(const Vec3i& centre)
    {
        OrderedList<int, chunk*, MaxChunkRenders> chunkBuildRenderList;
        Vec3i ccentre = centre / 16, c;
        for (auto&& chk : mWorld)
        {
            if (chk.second.updated)
            {
                c = Vec3i{ chk.second.cx, chk.second.cy, chk.second.cz };
                if (chunkInRange(c.x, c.y, c.z, ccentre.x, ccentre.y, ccentre.z, viewdistance))
                    chunkBuildRenderList.insert(((c + Vec3i{ 7, 7, 7 }) * 16 - centre).lengthSqr(), &chk.second);
            }
        }
        for (auto&& iter : chunkBuildRenderList)
            iter.second->buildRender();
    }

    chunk *World::insertChunk(int x, int y, int z)
    {
        mChunks[{x, y, z}] = chunk(x, y, z, getChunkID({ x, y, z }));
        return &mChunks[{x, y, z}];
    }

    void World::eraseChunk(int x, int y, int z)
    {
        mChunks.erase({ x, y, z });
    }


    void Init()
    {
        std::stringstream ss;
        ss << "md \"Worlds/" << worldname << "\"";
        system(ss.str().c_str());
        ss.clear();
        ss.str("");
        ss << "md \"Worlds/" << worldname << "/chunks\"";
        system(ss.str().c_str());

        WorldGen::perlinNoiseInit(3404);

        HMap.setSize((viewdistance + 2) * 2 * 16);
        HMap.create();
    }

    chunk *getChunkPtr(int x, int y, int z)
    {
        auto iter = mWorld.mChunks.find({ x, y, z });
        return (iter != mWorld.mChunks.end()) ? &(iter->second) : nullptr;
    }

    void renderblock(int x, int y, int z, chunk *chunkptr)
    {

        double colors, color1, color2, color3, color4, tcx, tcy, size, EPS = 0.0;
        int cx = chunkptr->cx, cy = chunkptr->cy, cz = chunkptr->cz;
        int gx = cx * 16 + x, gy = cy * 16 + y, gz = cz * 16 + z;
        block blk[7] = { chunkptr->getblock(x, y, z),
                         z < 15 ? chunkptr->getblock(x, y, z + 1) : getblock(gx, gy, gz + 1, blocks::ROCK),
                         z > 0 ? chunkptr->getblock(x, y, z - 1) : getblock(gx, gy, gz - 1, blocks::ROCK),
                         x < 15 ? chunkptr->getblock(x + 1, y, z) : getblock(gx + 1, gy, gz, blocks::ROCK),
                         x > 0 ? chunkptr->getblock(x - 1, y, z) : getblock(gx - 1, gy, gz, blocks::ROCK),
                         y < 15 ? chunkptr->getblock(x, y + 1, z) : getblock(gx, gy + 1, gz, blocks::ROCK),
                         y > 0 ? chunkptr->getblock(x, y - 1, z) : getblock(gx, gy - 1, gz, blocks::ROCK)
                       };

        brightness brt[7] = { chunkptr->getbrightness(x, y, z),
                              z < 15 ? chunkptr->getbrightness(x, y, z + 1) : getbrightness(gx, gy, gz + 1),
                              z > 0 ? chunkptr->getbrightness(x, y, z - 1) : getbrightness(gx, gy, gz - 1),
                              x < 15 ? chunkptr->getbrightness(x + 1, y, z) : getbrightness(gx + 1, gy, gz),
                              x > 0 ? chunkptr->getbrightness(x - 1, y, z) : getbrightness(gx - 1, gy, gz),
                              y < 15 ? chunkptr->getbrightness(x, y + 1, z) : getbrightness(gx, gy + 1, gz),
                              y > 0 ? chunkptr->getbrightness(x, y - 1, z) : getbrightness(gx, gy - 1, gz)
                            };

        size = 1 / 8.0f - EPS;

        if (NiceGrass && blk[0] == blocks::GRASS && getblock(gx, gy - 1, gz + 1, blocks::ROCK, chunkptr) == blocks::GRASS)
        {
            tcx = Textures::getTexcoordX(blk[0], 1) + EPS;
            tcy = Textures::getTexcoordY(blk[0], 1) + EPS;
        }
        else
        {
            tcx = Textures::getTexcoordX(blk[0], 2) + EPS;
            tcy = Textures::getTexcoordY(blk[0], 2) + EPS;
        }

        // Front Face
        if (!(BlockInfo(blk[1]).isOpaque() || (blk[1] == blk[0] && BlockInfo(blk[0]).isOpaque() == false)) || blk[0] == blocks::LEAF)
        {

            colors = brt[1];
            color1 = colors;
            color2 = colors;
            color3 = colors;
            color4 = colors;

            if (blk[0] != blocks::GLOWSTONE && SmoothLighting)
            {
                color1 = (colors + getbrightness(gx, gy - 1, gz + 1) + getbrightness(gx - 1, gy, gz + 1) + getbrightness(gx - 1, gy - 1, gz + 1)) / 4.0;
                color2 = (colors + getbrightness(gx, gy - 1, gz + 1) + getbrightness(gx + 1, gy, gz + 1) + getbrightness(gx + 1, gy - 1, gz + 1)) / 4.0;
                color3 = (colors + getbrightness(gx, gy + 1, gz + 1) + getbrightness(gx + 1, gy, gz + 1) + getbrightness(gx + 1, gy + 1, gz + 1)) / 4.0;
                color4 = (colors + getbrightness(gx, gy + 1, gz + 1) + getbrightness(gx - 1, gy, gz + 1) + getbrightness(gx - 1, gy + 1, gz + 1)) / 4.0;
            }

            color1 /= BRIGHTNESSMAX;
            color2 /= BRIGHTNESSMAX;
            color3 /= BRIGHTNESSMAX;
            color4 /= BRIGHTNESSMAX;

            if (blk[0] != blocks::GLOWSTONE)
            {
                color1 *= 0.5;
                color2 *= 0.5;
                color3 *= 0.5;
                color4 *= 0.5;
            }

            renderer::Color3d(color1, color1, color1);
            renderer::TexCoord2d(tcx, tcy);
            renderer::Vertex3d(-0.5 + x, -0.5 + y, 0.5 + z);
            renderer::Color3d(color2, color2, color2);
            renderer::TexCoord2d(tcx + size, tcy);
            renderer::Vertex3d(0.5 + x, -0.5 + y, 0.5 + z);
            renderer::Color3d(color3, color3, color3);
            renderer::TexCoord2d(tcx + size, tcy + size);
            renderer::Vertex3d(0.5 + x, 0.5 + y, 0.5 + z);
            renderer::Color3d(color4, color4, color4);
            renderer::TexCoord2d(tcx, tcy + size);
            renderer::Vertex3d(-0.5 + x, 0.5 + y, 0.5 + z);

        }

        if (NiceGrass && blk[0] == blocks::GRASS && getblock(gx, gy - 1, gz - 1, blocks::ROCK, chunkptr) == blocks::GRASS)
        {
            tcx = Textures::getTexcoordX(blk[0], 1) + EPS;
            tcy = Textures::getTexcoordY(blk[0], 1) + EPS;
        }
        else
        {
            tcx = Textures::getTexcoordX(blk[0], 2) + EPS;
            tcy = Textures::getTexcoordY(blk[0], 2) + EPS;
        }

        // Back Face
        if (!(BlockInfo(blk[2]).isOpaque() || (blk[2] == blk[0] && BlockInfo(blk[0]).isOpaque() == false)) || blk[0] == blocks::LEAF)
        {

            colors = brt[2];
            color1 = colors;
            color2 = colors;
            color3 = colors;
            color4 = colors;

            if (blk[0] != blocks::GLOWSTONE && SmoothLighting)
            {
                color1 = (colors + getbrightness(gx, gy - 1, gz - 1) + getbrightness(gx - 1, gy, gz - 1) + getbrightness(gx - 1, gy - 1, gz - 1)) / 4.0;
                color2 = (colors + getbrightness(gx, gy + 1, gz - 1) + getbrightness(gx - 1, gy, gz - 1) + getbrightness(gx - 1, gy + 1, gz - 1)) / 4.0;
                color3 = (colors + getbrightness(gx, gy + 1, gz - 1) + getbrightness(gx + 1, gy, gz - 1) + getbrightness(gx + 1, gy + 1, gz - 1)) / 4.0;
                color4 = (colors + getbrightness(gx, gy - 1, gz - 1) + getbrightness(gx + 1, gy, gz - 1) + getbrightness(gx + 1, gy - 1, gz - 1)) / 4.0;
            }

            color1 /= BRIGHTNESSMAX;
            color2 /= BRIGHTNESSMAX;
            color3 /= BRIGHTNESSMAX;
            color4 /= BRIGHTNESSMAX;

            if (blk[0] != blocks::GLOWSTONE)
            {
                color1 *= 0.5;
                color2 *= 0.5;
                color3 *= 0.5;
                color4 *= 0.5;
            }

            renderer::Color3d(color1, color1, color1);
            renderer::TexCoord2d(tcx + size * 1.0, tcy + size * 0.0);
            renderer::Vertex3d(-0.5 + x, -0.5 + y, -0.5 + z);
            renderer::Color3d(color2, color2, color2);
            renderer::TexCoord2d(tcx + size * 1.0, tcy + size * 1.0);
            renderer::Vertex3d(-0.5 + x, 0.5 + y, -0.5 + z);
            renderer::Color3d(color3, color3, color3);
            renderer::TexCoord2d(tcx + size * 0.0, tcy + size * 1.0);
            renderer::Vertex3d(0.5 + x, 0.5 + y, -0.5 + z);
            renderer::Color3d(color4, color4, color4);
            renderer::TexCoord2d(tcx + size * 0.0, tcy + size * 0.0);
            renderer::Vertex3d(0.5 + x, -0.5 + y, -0.5 + z);

        }

        if (NiceGrass && blk[0] == blocks::GRASS && getblock(gx + 1, gy - 1, gz, blocks::ROCK, chunkptr) == blocks::GRASS)
        {
            tcx = Textures::getTexcoordX(blk[0], 1) + EPS;
            tcy = Textures::getTexcoordY(blk[0], 1) + EPS;
        }
        else
        {
            tcx = Textures::getTexcoordX(blk[0], 2) + EPS;
            tcy = Textures::getTexcoordY(blk[0], 2) + EPS;
        }

        // Right face
        if (!(BlockInfo(blk[3]).isOpaque() || (blk[3] == blk[0] && !BlockInfo(blk[0]).isOpaque())) || blk[0] == blocks::LEAF)
        {

            colors = brt[3];
            color1 = colors;
            color2 = colors;
            color3 = colors;
            color4 = colors;

            if (blk[0] != blocks::GLOWSTONE && SmoothLighting)
            {
                color1 = (colors + getbrightness(gx + 1, gy - 1, gz) + getbrightness(gx + 1, gy, gz - 1) + getbrightness(gx + 1, gy - 1, gz - 1)) / 4.0;
                color2 = (colors + getbrightness(gx + 1, gy + 1, gz) + getbrightness(gx + 1, gy, gz - 1) + getbrightness(gx + 1, gy + 1, gz - 1)) / 4.0;
                color3 = (colors + getbrightness(gx + 1, gy + 1, gz) + getbrightness(gx + 1, gy, gz + 1) + getbrightness(gx + 1, gy + 1, gz + 1)) / 4.0;
                color4 = (colors + getbrightness(gx + 1, gy - 1, gz) + getbrightness(gx + 1, gy, gz + 1) + getbrightness(gx + 1, gy - 1, gz + 1)) / 4.0;
            }

            color1 /= BRIGHTNESSMAX;
            color2 /= BRIGHTNESSMAX;
            color3 /= BRIGHTNESSMAX;
            color4 /= BRIGHTNESSMAX;

            if (blk[0] != blocks::GLOWSTONE)
            {
                color1 *= 0.7;
                color2 *= 0.7;
                color3 *= 0.7;
                color4 *= 0.7;
            }

            renderer::Color3d(color1, color1, color1);
            renderer::TexCoord2d(tcx + size * 1.0, tcy + size * 0.0);
            renderer::Vertex3d(0.5 + x, -0.5 + y, -0.5 + z);
            renderer::Color3d(color2, color2, color2);
            renderer::TexCoord2d(tcx + size * 1.0, tcy + size * 1.0);
            renderer::Vertex3d(0.5 + x, 0.5 + y, -0.5 + z);
            renderer::Color3d(color3, color3, color3);
            renderer::TexCoord2d(tcx + size * 0.0, tcy + size * 1.0);
            renderer::Vertex3d(0.5 + x, 0.5 + y, 0.5 + z);
            renderer::Color3d(color4, color4, color4);
            renderer::TexCoord2d(tcx + size * 0.0, tcy + size * 0.0);
            renderer::Vertex3d(0.5 + x, -0.5 + y, 0.5 + z);

        }

        if (NiceGrass && blk[0] == blocks::GRASS && getblock(gx - 1, gy - 1, gz, blocks::ROCK, chunkptr) == blocks::GRASS)
        {
            tcx = Textures::getTexcoordX(blk[0], 1) + EPS;
            tcy = Textures::getTexcoordY(blk[0], 1) + EPS;
        }
        else
        {
            tcx = Textures::getTexcoordX(blk[0], 2) + EPS;
            tcy = Textures::getTexcoordY(blk[0], 2) + EPS;
        }

        // Left Face
        if (!(BlockInfo(blk[4]).isOpaque() || (blk[4] == blk[0] && BlockInfo(blk[0]).isOpaque() == false)) || blk[0] == blocks::LEAF)
        {

            colors = brt[4];
            color1 = colors;
            color2 = colors;
            color3 = colors;
            color4 = colors;

            if (blk[0] != blocks::GLOWSTONE && SmoothLighting)
            {
                color1 = (colors + getbrightness(gx - 1, gy - 1, gz) + getbrightness(gx - 1, gy, gz - 1) + getbrightness(gx - 1, gy - 1, gz - 1)) / 4.0;
                color2 = (colors + getbrightness(gx - 1, gy - 1, gz) + getbrightness(gx - 1, gy, gz + 1) + getbrightness(gx - 1, gy - 1, gz + 1)) / 4.0;
                color3 = (colors + getbrightness(gx - 1, gy + 1, gz) + getbrightness(gx - 1, gy, gz + 1) + getbrightness(gx - 1, gy + 1, gz + 1)) / 4.0;
                color4 = (colors + getbrightness(gx - 1, gy + 1, gz) + getbrightness(gx - 1, gy, gz - 1) + getbrightness(gx - 1, gy + 1, gz - 1)) / 4.0;
            }

            color1 /= BRIGHTNESSMAX;
            color2 /= BRIGHTNESSMAX;
            color3 /= BRIGHTNESSMAX;
            color4 /= BRIGHTNESSMAX;

            if (blk[0] != blocks::GLOWSTONE)
            {
                color1 *= 0.7;
                color2 *= 0.7;
                color3 *= 0.7;
                color4 *= 0.7;
            }

            renderer::Color3d(color1, color1, color1);
            renderer::TexCoord2d(tcx + size * 0.0, tcy + size * 0.0);
            renderer::Vertex3d(-0.5 + x, -0.5 + y, -0.5 + z);
            renderer::Color3d(color2, color2, color2);
            renderer::TexCoord2d(tcx + size * 1.0, tcy + size * 0.0);
            renderer::Vertex3d(-0.5 + x, -0.5 + y, 0.5 + z);
            renderer::Color3d(color3, color3, color3);
            renderer::TexCoord2d(tcx + size * 1.0, tcy + size * 1.0);
            renderer::Vertex3d(-0.5 + x, 0.5 + y, 0.5 + z);
            renderer::Color3d(color4, color4, color4);
            renderer::TexCoord2d(tcx + size * 0.0, tcy + size * 1.0);
            renderer::Vertex3d(-0.5 + x, 0.5 + y, -0.5 + z);

        }

        tcx = Textures::getTexcoordX(blk[0], 1);
        tcy = Textures::getTexcoordY(blk[0], 1);

        // Top Face
        if (!(BlockInfo(blk[5]).isOpaque() || (blk[5] == blk[0] && BlockInfo(blk[0]).isOpaque() == false)) || blk[0] == blocks::LEAF)
        {

            colors = brt[5];
            color1 = colors;
            color2 = colors;
            color3 = colors;
            color4 = colors;

            if (blk[0] != blocks::GLOWSTONE && SmoothLighting)
            {
                color1 = (color1 + getbrightness(gx, gy + 1, gz - 1) + getbrightness(gx - 1, gy + 1, gz) + getbrightness(gx - 1, gy + 1, gz - 1)) / 4.0;
                color2 = (color2 + getbrightness(gx, gy + 1, gz + 1) + getbrightness(gx - 1, gy + 1, gz) + getbrightness(gx - 1, gy + 1, gz + 1)) / 4.0;
                color3 = (color3 + getbrightness(gx, gy + 1, gz + 1) + getbrightness(gx + 1, gy + 1, gz) + getbrightness(gx + 1, gy + 1, gz + 1)) / 4.0;
                color4 = (color4 + getbrightness(gx, gy + 1, gz - 1) + getbrightness(gx + 1, gy + 1, gz) + getbrightness(gx + 1, gy + 1, gz - 1)) / 4.0;
            }

            color1 /= BRIGHTNESSMAX;
            color2 /= BRIGHTNESSMAX;
            color3 /= BRIGHTNESSMAX;
            color4 /= BRIGHTNESSMAX;

            renderer::Color3d(color1, color1, color1);
            renderer::TexCoord2d(tcx + size * 0.0, tcy + size * 1.0);
            renderer::Vertex3d(-0.5 + x, 0.5 + y, -0.5 + z);
            renderer::Color3d(color2, color2, color2);
            renderer::TexCoord2d(tcx + size * 0.0, tcy + size * 0.0);
            renderer::Vertex3d(-0.5 + x, 0.5 + y, 0.5 + z);
            renderer::Color3d(color3, color3, color3);
            renderer::TexCoord2d(tcx + size * 1.0, tcy + size * 0.0);
            renderer::Vertex3d(0.5 + x, 0.5 + y, 0.5 + z);
            renderer::Color3d(color4, color4, color4);
            renderer::TexCoord2d(tcx + size * 1.0, tcy + size * 1.0);
            renderer::Vertex3d(0.5 + x, 0.5 + y, -0.5 + z);

        }

        tcx = Textures::getTexcoordX(blk[0], 3);
        tcy = Textures::getTexcoordY(blk[0], 3);

        // Bottom Face
        if (!(BlockInfo(blk[6]).isOpaque() || (blk[6] == blk[0] && BlockInfo(blk[0]).isOpaque() == false)) || blk[0] == blocks::LEAF)
        {

            colors = brt[6];
            color1 = colors;
            color2 = colors;
            color3 = colors;
            color4 = colors;

            if (blk[0] != blocks::GLOWSTONE && SmoothLighting)
            {
                color1 = (colors + getbrightness(gx, gy - 1, gz - 1) + getbrightness(gx - 1, gy - 1, gz) + getbrightness(gx - 1, gy - 1, gz - 1)) / 4.0;
                color2 = (colors + getbrightness(gx, gy - 1, gz - 1) + getbrightness(gx + 1, gy - 1, gz) + getbrightness(gx + 1, gy - 1, gz - 1)) / 4.0;
                color3 = (colors + getbrightness(gx, gy - 1, gz + 1) + getbrightness(gx + 1, gy - 1, gz) + getbrightness(gx + 1, gy - 1, gz + 1)) / 4.0;
                color4 = (colors + getbrightness(gx, gy - 1, gz + 1) + getbrightness(gx - 1, gy - 1, gz) + getbrightness(gx - 1, gy - 1, gz + 1)) / 4.0;
            }

            color1 /= BRIGHTNESSMAX;
            color2 /= BRIGHTNESSMAX;
            color3 /= BRIGHTNESSMAX;
            color4 /= BRIGHTNESSMAX;

            renderer::Color3d(color1, color1, color1);
            renderer::TexCoord2d(tcx + size * 1.0, tcy + size * 1.0);
            renderer::Vertex3d(-0.5 + x, -0.5 + y, -0.5 + z);
            renderer::Color3d(color2, color2, color2);
            renderer::TexCoord2d(tcx + size * 0.0, tcy + size * 1.0);
            renderer::Vertex3d(0.5 + x, -0.5 + y, -0.5 + z);
            renderer::Color3d(color3, color3, color3);
            renderer::TexCoord2d(tcx + size * 0.0, tcy + size * 0.0);
            renderer::Vertex3d(0.5 + x, -0.5 + y, 0.5 + z);
            renderer::Color3d(color4, color4, color4);
            renderer::TexCoord2d(tcx + size * 1.0, tcy + size * 0.0);
            renderer::Vertex3d(-0.5 + x, -0.5 + y, 0.5 + z);

        }
    }

    vector<Hitbox::AABB> getHitboxes(Hitbox::AABB box)
    {
        //返回与box相交的所有方块AABB

        Hitbox::AABB blockbox;
        vector<Hitbox::AABB> hitBoxes;

        for (int a = int(box.min.x + 0.5) - 1; a <= int(box.max.x + 0.5) + 1; a++)
        {
            for (int b = int(box.min.y + 0.5) - 1; b <= int(box.max.y + 0.5) + 1; b++)
            {
                for (int c = int(box.min.z + 0.5) - 1; c <= int(box.max.z + 0.5) + 1; c++)
                {
                    if (BlockInfo(getblock(a, b, c)).isSolid())
                    {
                        blockbox.min.x = a - 0.5;
                        blockbox.max.x = a + 0.5;
                        blockbox.min.y = b - 0.5;
                        blockbox.max.y = b + 0.5;
                        blockbox.min.z = c - 0.5;
                        blockbox.max.z = c + 0.5;

                        if (Hitbox::hit(box, blockbox))
                        {
                            hitBoxes.push_back(blockbox);
                        }
                    }
                }
            }
        }

        return hitBoxes;
    }

    bool inWater(Hitbox::AABB box)
    {
        Hitbox::AABB blockbox;
        int a, b, c;

        for (a = int(box.min.x + 0.5) - 1; a <= int(box.max.x + 0.5); a++)
        {
            for (b = int(box.min.y + 0.5) - 1; b <= int(box.max.y + 0.5); b++)
            {
                for (c = int(box.min.z + 0.5) - 1; c <= int(box.max.z + 0.5); c++)
                {
                    if (getblock(a, b, c) == blocks::WATER || getblock(a, b, c) == blocks::LAVA)
                    {
                        blockbox.min.x = a - 0.5;
                        blockbox.max.x = a + 0.5;
                        blockbox.min.y = b - 0.5;
                        blockbox.max.y = b + 0.5;
                        blockbox.min.z = c - 0.5;
                        blockbox.max.z = c + 0.5;

                        if (Hitbox::hit(box, blockbox))
                        {
                            return true;
                        }
                    }
                }
            }
        }

        return false;
    }

    void updateblock(int x, int y, int z, bool blockchanged)
    {
        //Blockupdate

        int cx, cy, cz, bx, by, bz;
        bool updated = blockchanged;

        cx = getchunkpos(x);
        cy = getchunkpos(y);
        cz = getchunkpos(z);

        bx = getblockpos(x);
        by = getblockpos(y);
        bz = getblockpos(z);

        chunk *cptr = getChunkPtr(cx, cy, cz);

        if (cptr != nullptr)
        {
            brightness oldbrightness = cptr->getbrightness(bx, by, bz);
            bool skylighted = true;
            int yi, cyi;
            yi = y + 1;
            cyi = getchunkpos(yi);

            if (y < 0)
            {
                skylighted = false;
            }
            else
            {
                while (chunkLoaded(cx, cyi + 1, cz) && skylighted)
                {
                    if (BlockInfo(getblock(x, yi, z)).isOpaque() || getblock(x, yi, z) == blocks::WATER)
                    {
                        skylighted = false;
                    }

                    yi++;
                    cyi = getchunkpos(yi);
                }
            }

            if (!BlockInfo(getblock(x, y, z)).isOpaque())
            {

                brightness br;
                int maxbrightness;
                block blks[7] = { 0,
                                  getblock(x, y, z + 1),    //Front face
                                  getblock(x, y, z - 1),    //Back face
                                  getblock(x + 1, y, z),    //Right face
                                  getblock(x - 1, y, z),    //Left face
                                  getblock(x, y + 1, z),    //Top face
                                  getblock(x, y - 1, z)
                };     //Bottom face
                brightness brts[7] = { 0,
                                       getbrightness(x, y, z + 1),    //Front face
                                       getbrightness(x, y, z - 1),    //Back face
                                       getbrightness(x + 1, y, z),    //Right face
                                       getbrightness(x - 1, y, z),    //Left face
                                       getbrightness(x, y + 1, z),    //Top face
                                       getbrightness(x, y - 1, z)
                };     //Bottom face
                maxbrightness = 1;

                for (int i = 2; i <= 6; i++)
                {
                    if (brts[maxbrightness] < brts[i])
                    {
                        maxbrightness = i;
                    }
                }

                br = brts[maxbrightness];

                if (blks[maxbrightness] == blocks::WATER)
                {
                    if (br - 2 < BRIGHTNESSMIN)
                    {
                        br = BRIGHTNESSMIN;
                    }
                    else
                    {
                        br -= 2;
                    }
                }
                else
                {
                    if (br - 1 < BRIGHTNESSMIN)
                    {
                        br = BRIGHTNESSMIN;
                    }
                    else
                    {
                        br--;
                    }
                }

                if (skylighted)
                {
                    if (br < skylight)
                    {
                        br = skylight;
                    }
                }

                if (br < BRIGHTNESSMIN)
                {
                    br = BRIGHTNESSMIN;
                }

                //Set brightness
                cptr->setbrightness(bx, by, bz, br);

            }
            else
            {

                //Opaque block
                cptr->setbrightness(bx, by, bz, 0);

                if (getblock(x, y, z) == blocks::GLOWSTONE || getblock(x, y, z) == blocks::LAVA)
                {
                    cptr->setbrightness(bx, by, bz, BRIGHTNESSMAX);
                }

            }

            if (oldbrightness != cptr->getbrightness(bx, by, bz))
            {
                updated = true;
            }

            if (updated)
            {
                updateblock(x, y + 1, z, false);
                updateblock(x, y - 1, z, false);
                updateblock(x + 1, y, z, false);
                updateblock(x - 1, y, z, false);
                updateblock(x, y, z + 1, false);
                updateblock(x, y, z - 1, false);
            }

            setChunkUpdated(cx, cy, cz, true);

            if (bx == 15)
            {
                setChunkUpdated(cx + 1, cy, cz, true);
            }

            if (bx == 0)
            {
                setChunkUpdated(cx - 1, cy, cz, true);
            }

            if (by == 15)
            {
                setChunkUpdated(cx, cy + 1, cz, true);
            }

            if (by == 0)
            {
                setChunkUpdated(cx, cy - 1, cz, true);
            }

            if (bz == 15)
            {
                setChunkUpdated(cx, cy, cz + 1, true);
            }

            if (bz == 0)
            {
                setChunkUpdated(cx, cy, cz - 1, true);
            }
        }
    }

    block getblock(int x, int y, int z, block mask, chunk *cptr)
    {
        //获取XYZ的方块
        int cx, cy, cz;
        cx = getchunkpos(x);
        cy = getchunkpos(y);
        cz = getchunkpos(z);
        int bx, by, bz;
        bx = getblockpos(x);
        by = getblockpos(y);
        bz = getblockpos(z);

        if (cptr != nullptr && cx == cptr->cx && cy == cptr->cy && cz == cptr->cz)
        {
            return cptr->getblock(bx, by, bz);
        }

        chunk *ci = getChunkPtr(cx, cy, cz);

        if (ci != nullptr)
        {
            return ci->getblock(bx, by, bz);
        }

        return mask;
    }

    brightness getbrightness(int x, int y, int z, chunk *cptr)
    {
        //获取XYZ的亮度
        int cx, cy, cz;
        cx = getchunkpos(x);
        cy = getchunkpos(y);
        cz = getchunkpos(z);

        int bx, by, bz;
        bx = getblockpos(x);
        by = getblockpos(y);
        bz = getblockpos(z);

        if (cptr != nullptr && cx == cptr->cx && cy == cptr->cy && cz == cptr->cz)
        {
            return cptr->getbrightness(bx, by, bz);
        }

        chunk *ci = getChunkPtr(cx, cy, cz);

        if (ci != nullptr)
        {
            return ci->getbrightness(bx, by, bz);
        }

        return skylight;
    }

    void setblock(int x, int y, int z, block Blockname)
    {

        //设置方块
        int cx, cy, cz, bx, by, bz;

        cx = getchunkpos(x);
        cy = getchunkpos(y);
        cz = getchunkpos(z);

        bx = getblockpos(x);
        by = getblockpos(y);
        bz = getblockpos(z);

        chunk *i = getChunkPtr(cx, cy, cz);

        if (i != nullptr)
        {
            i->setblock(bx, by, bz, Blockname);
            updateblock(x, y, z, true);
        }
    }

    void setbrightness(int x, int y, int z, brightness Brightness)
    {

        //设置XYZ的亮度
        int cx, cy, cz, bx, by, bz;

        cx = getchunkpos(x);
        cy = getchunkpos(y);
        cz = getchunkpos(z);

        bx = getblockpos(x);
        by = getblockpos(y);
        bz = getblockpos(z);

        chunk *i = getChunkPtr(cx, cy, cz);

        if (i != nullptr)
        {
            i->setbrightness(bx, by, bz, Brightness);
        }
    }

    bool chunkUpdated(int x, int y, int z)
    {
        chunk *i = getChunkPtr(x, y, z);
        return i->updated;
    }

    void setChunkUpdated(int x, int y, int z, bool value)
    {
        chunk *i = getChunkPtr(x, y, z);
        if (i != nullptr)
        {
            i->updated = value;
        }
    }

    void saveAllChunks()
    {
        for (auto&& chk : mWorld)
            chk.second.SaveToFile();
    }

    void destroyAllChunks()
    {
        for (auto&& chk : mWorld)
            if (!chk.second.Empty)
                chk.second.destroyRender();
        HMap.destroy();
    }

    void buildtree(int x, int y, int z)
    {

        block trblock = getblock(x, y, z), tublock = getblock(x, y - 1, z);
        ubyte xt, yt, zt;
        ubyte th = ubyte(rnd() * 3) + 4;

        if (trblock != blocks::AIR || tublock != blocks::GRASS || tublock == blocks::AIR)
        {
            return;
        }

        for (yt = 0; yt != th; yt++)
        {
            setblock(x, y + yt, z, blocks::WOOD);
        }

        setblock(x, y - 1, z, blocks::DIRT);

        for (xt = 0; xt != 4; xt++)
        {
            for (zt = 0; zt != 4; zt++)
            {
                for (yt = 0; yt != 1; yt++)
                {
                    if (getblock(x + xt - 2, y + th - 2 - yt, z + zt - 2) == blocks::AIR)
                    {
                        setblock(x + xt - 2, y + th - 2 - yt, z + zt - 2, blocks::LEAF);
                    }
                }
            }
        }

        for (xt = 0; xt != 2; xt++)
        {
            for (zt = 0; zt != 2; zt++)
            {
                for (yt = 0; yt != 1; yt++)
                {
                    if (getblock(x + xt - 1, y + th - 1 + yt, z + zt - 1) == blocks::AIR && abs(xt - 1) != abs(zt - 1))
                    {
                        setblock(x + xt - 1, y + th - 1 + yt, z + zt - 1, blocks::LEAF);
                    }
                }
            }
        }

        setblock(x, y + th, z, blocks::LEAF);

    }

}
