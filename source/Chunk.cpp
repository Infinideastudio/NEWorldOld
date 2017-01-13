#include "Chunk.h"
#include "WorldGen.h"
#include "World.h"
#include "Renderer.h"

namespace ChunkRenderer
{
    void renderChunk(world::chunk *c);
    void mergeFaceRender(world::chunk *c);
}

namespace world
{

    void chunk::build()
    {
        //生成地形
        int x, y, z, height, h = 0, sh = 0;

        Empty = true;

        if (cy > 8)
        {
            memset(pblocks, 0, 4096 * sizeof(block));

            for (int index = 0; index < 4096; index++)
            {
                pbrightness[index] = skylight;
            }
        }
        else if (cy < 0)
        {
            memset(pblocks, 0, 4096 * sizeof(block));

            for (int index = 0; index < 4096; index++)
            {
                pbrightness[index] = BRIGHTNESSMIN;
            }
        }
        else
        {

            int hm[16][16];

            for (x = 0; x < 16; x++)
            {
                for (z = 0; z < 16; z++)
                {
                    hm[x][z] = HMap.getHeight(cx * 16 + x, cz * 16 + z);
                }
            }

            sh = WorldGen::WaterLevel + 2;

            int index = 0;

            for (x = 0; x < 16; x++)
            {
                for (y = 0; y < 16; y++)
                {
                    for (z = 0; z < 16; z++)
                    {

                        h = hm[x][z];
                        height = cy * 16 + y;
                        pbrightness[index] = 0;

                        if (height == 0)
                        {
                            pblocks[index] = blocks::BEDROCK;
                        }
                        else if (height == h && height > sh && height > WorldGen::WaterLevel + 1)
                        {
                            pblocks[index] = blocks::GRASS;
                        }
                        else if (height < h && height > sh && height > WorldGen::WaterLevel + 1)
                        {
                            pblocks[index] = blocks::DIRT;
                        }
                        else if ((height >= sh - 5 || height >= h - 5) && height <= h && (height <= sh || height <= WorldGen::WaterLevel + 1))
                        {
                            pblocks[index] = blocks::SAND;
                        }
                        else if ((height < sh - 5 && height < h - 5) && height >= 1 && height <= h)
                        {
                            pblocks[index] = blocks::ROCK;
                        }
                        else
                        {
                            if (height <= WorldGen::WaterLevel)
                            {
                                pblocks[index] = blocks::WATER;

                                if (skylight - (WorldGen::WaterLevel - height) * 2 < BRIGHTNESSMIN)
                                {
                                    pbrightness[index] = BRIGHTNESSMIN;
                                }
                                else
                                {
                                    pbrightness[index] = skylight - (brightness)((WorldGen::WaterLevel - height) * 2);
                                }
                            }
                            else
                            {
                                pblocks[index] = blocks::AIR;
                                pbrightness[index] = skylight;
                            }
                        }

                        if (pblocks[index] != blocks::AIR)
                        {
                            Empty = false;
                        }

                        index++;

                    }
                }
            }
        }

    }

    void chunk::Load()
    {
        pblocks = new block[4096];
        pbrightness = new brightness[4096];

        if (fileExist())
        {
            LoadFromFile();
        }
        else
        {
            build();
        }

        if (!Empty)
        {
            updated = true;
        }
    }

    void chunk::Unload()
    {
        unloadedChunksCount++;
        SaveToFile();
        destroyRender();
        delete[] pblocks;
        delete[] pbrightness;
        pblocks = nullptr;
        pbrightness = nullptr;
        updated = false;
        unloadedChunks++;
    }

    void chunk::LoadFromFile()
    {
        std::ifstream file(getFileName().c_str(), std::ios::in | std::ios::binary);
        file.read((char *)pblocks, 4096 * sizeof(block));
        file.read((char *)pbrightness, 4096 * sizeof(brightness));
        file.close();
    }

    void chunk::SaveToFile()
    {
        if (!Modified)
        {
            return;
        }

        std::ofstream file(getFileName().c_str(), std::ios::out | std::ios::binary);
        file.write((char *)pblocks, 4096 * sizeof(block));
        file.write((char *)pbrightness, 4096 * sizeof(brightness));
        file.close();
    }

    void chunk::buildRender()
    {
        //建立chunk显示列表
        int x, y, z;

        for (x = -1; x <= 1; x++)
        {
            for (y = -1; y <= 1; y++)
            {
                for (z = -1; z <= 1; z++)
                {
                    if (x == 0 && y == 0 && z == 0)
                    {
                        continue;
                    }

                    if (chunkOutOfBound(cx + x, cy + y, cz + z))
                    {
                        continue;
                    }

                    if (!chunkLoaded(cx + x, cy + y, cz + z))
                    {
                        return;
                    }
                }
            }
        }

        rebuiltChunks++;
        updatedChunks++;

        if (renderBuilt == false)
        {
            renderBuilt = true;
            loadAnim = cy * 16.0f + 16.0f;
        }

        if (MergeFace)
        {
            ChunkRenderer::mergeFaceRender(this);
        }
        else
        {
            ChunkRenderer::renderChunk(this);
        }

        updated = false;

    }

    void chunk::destroyRender()
    {
        if (!renderBuilt)
        {
            return;
        }

        if (vbuffer[0] != 0)
        {
            vbuffersShouldDelete.push_back(vbuffer[0]);
        }

        if (vbuffer[1] != 0)
        {
            vbuffersShouldDelete.push_back(vbuffer[1]);
        }

        if (vbuffer[2] != 0)
        {
            vbuffersShouldDelete.push_back(vbuffer[2]);
        }

        vbuffer[0] = vbuffer[1] = vbuffer[2] = 0;
        renderBuilt = false;
    }

    Hitbox::AABB chunk::getChunkAABB()
    {
        return Hitbox::AABB
        { 
            Vec3d{ cx * 16 - 0.5, cy * 16 - loadAnim - 0.5, cz * 16 - 0.5 },
            Vec3d{ cx * 16 + 16 - 0.5, cy * 16 - loadAnim + 16 - 0.5, cz * 16 + 16 - 0.5 }
        };
    }

    Hitbox::AABB chunk::getRelativeAABB(double &x, double &y, double &z)
    {
        return Hitbox::AABB
        {
            Vec3d{ cx * 16 - 0.5 - x, cy * 16 - loadAnim - 0.5 - y, cz * 16 - 0.5 - z },
            Vec3d{ cx * 16 + 16 - 0.5 - x, cy * 16 - loadAnim + 16 - 0.5 - y, cz * 16 + 16 - 0.5 - z }
        };
    }

}
