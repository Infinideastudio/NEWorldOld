#include "ChunkRenderer.h"

namespace ChunkRenderer
{

    void renderPrimitive(QuadPrimitive &p)
    {
        double color = (double)p.brightness / world::BRIGHTNESSMAX;
        ubyte face = 0;
        int x = p.x, y = p.y, z = p.z, length = p.length;

        if (p.direction == 2)
        {
            face = 1;
        }
        else if (p.direction == 3)
        {
            face = 3;
        }
        else
        {
            face = 2;
        }

        renderer::TexCoord3d(0.0, 0.0, (Textures::getTextureIndex(p.block, face) - 0.5) / 64.0);

        if (p.direction == 0)
        {
            if (p.block != blocks::GLOWSTONE)
            {
                color *= 0.7;
            }

            renderer::Color3d(color, color, color);
            renderer::TexCoord2d(0.0, 0.0);
            renderer::Vertex3d(x + 0.5, y - 0.5, z - 0.5);
            renderer::TexCoord2d(0.0, 1.0);
            renderer::Vertex3d(x + 0.5, y + 0.5, z - 0.5);
            renderer::TexCoord2d(length + 1.0, 1.0);
            renderer::Vertex3d(x + 0.5, y + 0.5, z + length + 0.5);
            renderer::TexCoord2d(length + 1.0, 0.0);
            renderer::Vertex3d(x + 0.5, y - 0.5, z + length + 0.5);
        }
        else if (p.direction == 1)
        {
            if (p.block != blocks::GLOWSTONE)
            {
                color *= 0.7;
            }

            renderer::Color3d(color, color, color);
            renderer::TexCoord2d(0.0, 1.0);
            renderer::Vertex3d(x - 0.5, y + 0.5, z - 0.5);
            renderer::TexCoord2d(0.0, 0.0);
            renderer::Vertex3d(x - 0.5, y - 0.5, z - 0.5);
            renderer::TexCoord2d(length + 1.0, 0.0);
            renderer::Vertex3d(x - 0.5, y - 0.5, z + length + 0.5);
            renderer::TexCoord2d(length + 1.0, 1.0);
            renderer::Vertex3d(x - 0.5, y + 0.5, z + length + 0.5);
        }
        else if (p.direction == 2)
        {
            renderer::Color3d(color, color, color);
            renderer::TexCoord2d(0.0, 0.0);
            renderer::Vertex3d(x + 0.5, y + 0.5, z - 0.5);
            renderer::TexCoord2d(0.0, 1.0);
            renderer::Vertex3d(x - 0.5, y + 0.5, z - 0.5);
            renderer::TexCoord2d(length + 1.0, 1.0);
            renderer::Vertex3d(x - 0.5, y + 0.5, z + length + 0.5);
            renderer::TexCoord2d(length + 1.0, 0.0);
            renderer::Vertex3d(x + 0.5, y + 0.5, z + length + 0.5);
        }
        else if (p.direction == 3)
        {
            renderer::Color3d(color, color, color);
            renderer::TexCoord2d(0.0, 0.0);
            renderer::Vertex3d(x - 0.5, y - 0.5, z - 0.5);
            renderer::TexCoord2d(0.0, 1.0);
            renderer::Vertex3d(x + 0.5, y - 0.5, z - 0.5);
            renderer::TexCoord2d(length + 1.0, 1.0);
            renderer::Vertex3d(x + 0.5, y - 0.5, z + length + 0.5);
            renderer::TexCoord2d(length + 1.0, 0.0);
            renderer::Vertex3d(x - 0.5, y - 0.5, z + length + 0.5);
        }
        else if (p.direction == 4)
        {
            if (p.block != blocks::GLOWSTONE)
            {
                color *= 0.5;
            }

            renderer::Color3d(color, color, color);
            renderer::TexCoord2d(0.0, 1.0);
            renderer::Vertex3d(x - 0.5, y + 0.5, z + 0.5);
            renderer::TexCoord2d(0.0, 0.0);
            renderer::Vertex3d(x - 0.5, y - 0.5, z + 0.5);
            renderer::TexCoord2d(length + 1.0, 0.0);
            renderer::Vertex3d(x + length + 0.5, y - 0.5, z + 0.5);
            renderer::TexCoord2d(length + 1.0, 1.0);
            renderer::Vertex3d(x + length + 0.5, y + 0.5, z + 0.5);
        }
        else if (p.direction == 5)
        {
            if (p.block != blocks::GLOWSTONE)
            {
                color *= 0.5;
            }

            renderer::Color3d(color, color, color);
            renderer::TexCoord2d(0.0, 0.0);
            renderer::Vertex3d(x - 0.5, y - 0.5, z - 0.5);
            renderer::TexCoord2d(0.0, 1.0);
            renderer::Vertex3d(x - 0.5, y + 0.5, z - 0.5);
            renderer::TexCoord2d(length + 1.0, 1.0);
            renderer::Vertex3d(x + length + 0.5, y + 0.5, z - 0.5);
            renderer::TexCoord2d(length + 1.0, 0.0);
            renderer::Vertex3d(x + length + 0.5, y - 0.5, z - 0.5);
        }
    }

    void renderChunk(world::chunk *c)
    {
        int x, y, z;
        renderer::Init(2, 3);

        for (x = 0; x < 16; x++)
        {
            for (y = 0; y < 16; y++)
            {
                for (z = 0; z < 16; z++)
                {
                    block curr = c->getblock(x, y, z);

                    if (curr == blocks::AIR)
                    {
                        continue;
                    }

                    if (!BlockInfo(curr).isTranslucent())
                    {
                        renderblock(x, y, z, c);
                    }
                }
            }
        }

        renderer::Flush(c->vbuffer[0], c->vertexes[0]);
        renderer::Init(2, 3);

        for (x = 0; x < 16; x++)
        {
            for (y = 0; y < 16; y++)
            {
                for (z = 0; z < 16; z++)
                {
                    block curr = c->getblock(x, y, z);

                    if (curr == blocks::AIR)
                    {
                        continue;
                    }

                    if (BlockInfo(curr).isTranslucent() && BlockInfo(curr).isSolid())
                    {
                        renderblock(x, y, z, c);
                    }
                }
            }
        }

        renderer::Flush(c->vbuffer[1], c->vertexes[1]);
        renderer::Init(2, 3);

        for (x = 0; x < 16; x++)
        {
            for (y = 0; y < 16; y++)
            {
                for (z = 0; z < 16; z++)
                {
                    block curr = c->getblock(x, y, z);

                    if (curr == blocks::AIR)
                    {
                        continue;
                    }

                    if (!BlockInfo(curr).isSolid())
                    {
                        renderblock(x, y, z, c);
                    }
                }
            }
        }

        renderer::Flush(c->vbuffer[2], c->vertexes[2]);
    }

    void mergeFaceRender(world::chunk *c)
    {
        int cx = c->cx, cy = c->cy, cz = c->cz;
        int x = 0, y = 0, z = 0;
        QuadPrimitive cur;
        int cur_l_mx;
        block bl, neighbour;
        brightness br;
        bool valid = false;

        for (int steps = 0; steps < 3; steps++)
        {
            cur = QuadPrimitive();
            cur_l_mx = bl = neighbour = br = 0;
            //Linear merge
            renderer::Init(3, 3);

            for (int d = 0; d < 6; d++)
            {
                cur.direction = d;

                for (int i = 0; i < 16; i++) for (int j = 0; j < 16; j++)
                    {
                        for (int k = 0; k < 16; k++)
                        {
                            //Get position
                            if (d < 2)
                            {
                                x = i, y = j, z = k;
                            }
                            else if (d < 4)
                            {
                                x = i, y = j, z = k;
                            }
                            else
                            {
                                x = k, y = i, z = j;
                            }

                            //Get properties
                            bl = c->getblock(x, y, z);
                            //Get neighbour properties
                            int xx = x + delta[d][0], yy = y + delta[d][1], zz = z + delta[d][2];
                            int gx = cx * 16 + xx, gy = cy * 16 + yy, gz = cz * 16 + zz;

                            if (xx < 0 || xx >= 16 || yy < 0 || yy >= 16 || zz < 0 || zz >= 16)
                            {
                                neighbour = world::getblock(gx, gy, gz);
                                br = world::getbrightness(gx, gy, gz);
                            }
                            else
                            {
                                neighbour = c->getblock(xx, yy, zz);
                                br = c->getbrightness(xx, yy, zz);
                            }

                            //Render
                            if (bl == blocks::AIR || bl == neighbour && bl != blocks::LEAF || BlockInfo(neighbour).isOpaque() ||
                                steps == 0 && BlockInfo(bl).isTranslucent() ||
                                steps == 1 && (!BlockInfo(bl).isTranslucent() || !BlockInfo(bl).isSolid()) ||
                                steps == 2 && (!BlockInfo(bl).isTranslucent() || BlockInfo(bl).isSolid()))
                            {
                                //Not valid block
                                if (valid)
                                {
                                    if (BlockInfo(neighbour).isOpaque())
                                    {
                                        if (cur_l_mx < cur.length)
                                        {
                                            cur_l_mx = cur.length;
                                        }

                                        cur_l_mx++;
                                    }
                                    else
                                    {
                                        renderPrimitive(cur);
                                        valid = false;
                                    }
                                }

                                continue;
                            }

                            if (valid)
                            {
                                if (bl != cur.block || br != cur.brightness)
                                {
                                    renderPrimitive(cur);
                                    cur.x = x;
                                    cur.y = y;
                                    cur.z = z;
                                    cur.length = cur_l_mx = 0;
                                    cur.block = bl;
                                    cur.brightness = br;
                                }
                                else
                                {
                                    if (cur_l_mx > cur.length)
                                    {
                                        cur.length = cur_l_mx;
                                    }

                                    cur.length++;
                                }
                            }
                            else
                            {
                                valid = true;
                                cur.x = x;
                                cur.y = y;
                                cur.z = z;
                                cur.length = cur_l_mx = 0;
                                cur.block = bl;
                                cur.brightness = br;
                            }
                        }

                        if (valid)
                        {
                            renderPrimitive(cur);
                            valid = false;
                        }
                    }
            }

            renderer::Flush(c->vbuffer[steps], c->vertexes[steps]);
        }
    }

}