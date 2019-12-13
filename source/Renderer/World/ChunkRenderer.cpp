#include "ChunkRenderer.h"
#include "Renderer.h"
#include "Universe/World/World.h"

namespace ChunkRenderer {
    using World::getbrightness;

    void renderblock(int x, int y, int z, World::Chunk *chunkptr) {
        double colors, color1, color2, color3, color4, tcx, tcy, size, EPS = 0.0;
        auto cx = chunkptr->cx, cy = chunkptr->cy, cz = chunkptr->cz;
        auto gx = cx * 16 + x, gy = cy * 16 + y, gz = cz * 16 + z;
        Block blk[7] = {(chunkptr->GetBlock({x, y, z})),
                        z < 15 ? chunkptr->GetBlock({(x), (y), (z + 1)}) : World::GetBlock({(gx), (gy), (gz + 1)}, Blocks::ROCK),
                        z > 0 ? chunkptr->GetBlock({(x), (y), (z - 1)}) : World::GetBlock({(gx), (gy), (gz - 1)}, Blocks::ROCK),
                        x < 15 ? chunkptr->GetBlock({(x + 1), (y), (z)}) : World::GetBlock({(gx + 1), (gy), (gz)}, Blocks::ROCK),
                        x > 0 ? chunkptr->GetBlock({(x - 1), (y), (z)}) : World::GetBlock({(gx - 1), (gy), (gz)}, Blocks::ROCK),
                        y < 15 ? chunkptr->GetBlock({(x), (y + 1), (z)}) : World::GetBlock({(gx), (gy + 1), (gz)}, Blocks::ROCK),
                        y > 0 ? chunkptr->GetBlock({(x), (y - 1), (z)}) : World::GetBlock({(gx), (gy - 1), (gz)}, Blocks::ROCK)};

        Brightness brt[7] = {(chunkptr->GetBrightness({(x), (y), (z)})),
                             z < 15 ? chunkptr->GetBrightness({(x), (y), (z + 1)}) : World::getbrightness(gx, gy, gz + 1),
                             z > 0 ? chunkptr->GetBrightness({(x), (y), (z - 1)}) : World::getbrightness(gx, gy, gz - 1),
                             x < 15 ? chunkptr->GetBrightness({(x + 1), (y), (z)}) : World::getbrightness(gx + 1, gy, gz),
                             x > 0 ? chunkptr->GetBrightness({(x - 1), (y), (z)}) : World::getbrightness(gx - 1, gy, gz),
                             y < 15 ? chunkptr->GetBrightness({(x), (y + 1), (z)}) : World::getbrightness(gx, gy + 1, gz),
                             y > 0 ? chunkptr->GetBrightness({(x), (y - 1), (z)}) : World::getbrightness(gx, gy - 1, gz)};

        size = 1 / 8.0f - EPS;

        if (NiceGrass && blk[0] == Blocks::GRASS &&
            World::GetBlock({(gx), (gy - 1), (gz + 1)}, Blocks::ROCK, chunkptr) == Blocks::GRASS) {
            tcx = Textures::getTexcoordX(blk[0], 1) + EPS;
            tcy = Textures::getTexcoordY(blk[0], 1) + EPS;
        } else {
            tcx = Textures::getTexcoordX(blk[0], 2) + EPS;
            tcy = Textures::getTexcoordY(blk[0], 2) + EPS;
        }

        // Front Face
        if (!(BlockInfo(blk[1]).isOpaque() || (blk[1] == blk[0] && !BlockInfo(blk[0]).isOpaque())) ||
            blk[0] == Blocks::LEAF) {

            colors = brt[1];
            color1 = colors;
            color2 = colors;
            color3 = colors;
            color4 = colors;

            if (blk[0] != Blocks::GLOWSTONE && SmoothLighting) {
                color1 = (colors + World::getbrightness(gx, gy - 1, gz + 1) + World::getbrightness(gx - 1, gy, gz + 1) +
                          World::getbrightness(gx - 1, gy - 1, gz + 1)) / 4.0;
                color2 = (colors + World::getbrightness(gx, gy - 1, gz + 1) + World::getbrightness(gx + 1, gy, gz + 1) +
                          World::getbrightness(gx + 1, gy - 1, gz + 1)) / 4.0;
                color3 = (colors + World::getbrightness(gx, gy + 1, gz + 1) + World::getbrightness(gx + 1, gy, gz + 1) +
                          World::getbrightness(gx + 1, gy + 1, gz + 1)) / 4.0;
                color4 = (colors + World::getbrightness(gx, gy + 1, gz + 1) + World::getbrightness(gx - 1, gy, gz + 1) +
                          World::getbrightness(gx - 1, gy + 1, gz + 1)) / 4.0;
            }

            color1 /= World::BRIGHTNESSMAX;
            color2 /= World::BRIGHTNESSMAX;
            color3 /= World::BRIGHTNESSMAX;
            color4 /= World::BRIGHTNESSMAX;
            if (blk[0] != Blocks::GLOWSTONE && !Renderer::AdvancedRender) {
                color1 *= 0.5;
                color2 *= 0.5;
                color3 *= 0.5;
                color4 *= 0.5;
            }

            if (Renderer::AdvancedRender) Renderer::Attrib1f(0.0f);
            Renderer::Color3d(color1, color1, color1);
            Renderer::TexCoord2d(tcx, tcy);
            Renderer::Vertex3d(-0.5 + x, -0.5 + y, 0.5 + z);
            Renderer::Color3d(color2, color2, color2);
            Renderer::TexCoord2d(tcx + size, tcy);
            Renderer::Vertex3d(0.5 + x, -0.5 + y, 0.5 + z);
            Renderer::Color3d(color3, color3, color3);
            Renderer::TexCoord2d(tcx + size, tcy + size);
            Renderer::Vertex3d(0.5 + x, 0.5 + y, 0.5 + z);
            Renderer::Color3d(color4, color4, color4);
            Renderer::TexCoord2d(tcx, tcy + size);
            Renderer::Vertex3d(-0.5 + x, 0.5 + y, 0.5 + z);

        }

        if (NiceGrass && blk[0] == Blocks::GRASS &&
            World::GetBlock({(gx), (gy - 1), (gz - 1)}, Blocks::ROCK, chunkptr) == Blocks::GRASS) {
            tcx = Textures::getTexcoordX(blk[0], 1) + EPS;
            tcy = Textures::getTexcoordY(blk[0], 1) + EPS;
        } else {
            tcx = Textures::getTexcoordX(blk[0], 2) + EPS;
            tcy = Textures::getTexcoordY(blk[0], 2) + EPS;
        }

        // Back Face
        if (!(BlockInfo(blk[2]).isOpaque() || (blk[2] == blk[0] && !BlockInfo(blk[0]).isOpaque())) ||
            blk[0] == Blocks::LEAF) {

            colors = brt[2];
            color1 = colors;
            color2 = colors;
            color3 = colors;
            color4 = colors;

            if (blk[0] != Blocks::GLOWSTONE && SmoothLighting) {
                color1 = (colors + World::getbrightness(gx, gy - 1, gz - 1) + World::getbrightness(gx - 1, gy, gz - 1) +
                          World::getbrightness(gx - 1, gy - 1, gz - 1)) / 4.0;
                color2 = (colors + World::getbrightness(gx, gy + 1, gz - 1) + World::getbrightness(gx - 1, gy, gz - 1) +
                          World::getbrightness(gx - 1, gy + 1, gz - 1)) / 4.0;
                color3 = (colors + World::getbrightness(gx, gy + 1, gz - 1) + World::getbrightness(gx + 1, gy, gz - 1) +
                          World::getbrightness(gx + 1, gy + 1, gz - 1)) / 4.0;
                color4 = (colors + World::getbrightness(gx, gy - 1, gz - 1) + World::getbrightness(gx + 1, gy, gz - 1) +
                          World::getbrightness(gx + 1, gy - 1, gz - 1)) / 4.0;
            }

            color1 /= World::BRIGHTNESSMAX;
            color2 /= World::BRIGHTNESSMAX;
            color3 /= World::BRIGHTNESSMAX;
            color4 /= World::BRIGHTNESSMAX;
            if (blk[0] != Blocks::GLOWSTONE && !Renderer::AdvancedRender) {
                color1 *= 0.5;
                color2 *= 0.5;
                color3 *= 0.5;
                color4 *= 0.5;
            }

            if (Renderer::AdvancedRender) Renderer::Attrib1f(1.0f);
            Renderer::Color3d(color1, color1, color1);
            Renderer::TexCoord2d(tcx + size * 1.0, tcy + size * 0.0);
            Renderer::Vertex3d(-0.5 + x, -0.5 + y, -0.5 + z);
            Renderer::Color3d(color2, color2, color2);
            Renderer::TexCoord2d(tcx + size * 1.0, tcy + size * 1.0);
            Renderer::Vertex3d(-0.5 + x, 0.5 + y, -0.5 + z);
            Renderer::Color3d(color3, color3, color3);
            Renderer::TexCoord2d(tcx + size * 0.0, tcy + size * 1.0);
            Renderer::Vertex3d(0.5 + x, 0.5 + y, -0.5 + z);
            Renderer::Color3d(color4, color4, color4);
            Renderer::TexCoord2d(tcx + size * 0.0, tcy + size * 0.0);
            Renderer::Vertex3d(0.5 + x, -0.5 + y, -0.5 + z);

        }

        if (NiceGrass && blk[0] == Blocks::GRASS &&
            World::GetBlock({(gx + 1), (gy - 1), (gz)}, Blocks::ROCK, chunkptr) == Blocks::GRASS) {
            tcx = Textures::getTexcoordX(blk[0], 1) + EPS;
            tcy = Textures::getTexcoordY(blk[0], 1) + EPS;
        } else {
            tcx = Textures::getTexcoordX(blk[0], 2) + EPS;
            tcy = Textures::getTexcoordY(blk[0], 2) + EPS;
        }

        // Right face
        if (!(BlockInfo(blk[3]).isOpaque() || (blk[3] == blk[0] && !BlockInfo(blk[0]).isOpaque())) ||
            blk[0] == Blocks::LEAF) {

            colors = brt[3];
            color1 = colors;
            color2 = colors;
            color3 = colors;
            color4 = colors;

            if (blk[0] != Blocks::GLOWSTONE && SmoothLighting) {
                color1 = (colors + World::getbrightness(gx + 1, gy - 1, gz) + World::getbrightness(gx + 1, gy, gz - 1) +
                          World::getbrightness(gx + 1, gy - 1, gz - 1)) / 4.0;
                color2 = (colors + World::getbrightness(gx + 1, gy + 1, gz) + World::getbrightness(gx + 1, gy, gz - 1) +
                          World::getbrightness(gx + 1, gy + 1, gz - 1)) / 4.0;
                color3 = (colors + World::getbrightness(gx + 1, gy + 1, gz) + World::getbrightness(gx + 1, gy, gz + 1) +
                          World::getbrightness(gx + 1, gy + 1, gz + 1)) / 4.0;
                color4 = (colors + World::getbrightness(gx + 1, gy - 1, gz) + World::getbrightness(gx + 1, gy, gz + 1) +
                          World::getbrightness(gx + 1, gy - 1, gz + 1)) / 4.0;
            }

            color1 /= World::BRIGHTNESSMAX;
            color2 /= World::BRIGHTNESSMAX;
            color3 /= World::BRIGHTNESSMAX;
            color4 /= World::BRIGHTNESSMAX;
            if (blk[0] != Blocks::GLOWSTONE && !Renderer::AdvancedRender) {
                color1 *= 0.7;
                color2 *= 0.7;
                color3 *= 0.7;
                color4 *= 0.7;
            }

            if (Renderer::AdvancedRender) Renderer::Attrib1f(2.0f);
            Renderer::Color3d(color1, color1, color1);
            Renderer::TexCoord2d(tcx + size * 1.0, tcy + size * 0.0);
            Renderer::Vertex3d(0.5 + x, -0.5 + y, -0.5 + z);
            Renderer::Color3d(color2, color2, color2);
            Renderer::TexCoord2d(tcx + size * 1.0, tcy + size * 1.0);
            Renderer::Vertex3d(0.5 + x, 0.5 + y, -0.5 + z);
            Renderer::Color3d(color3, color3, color3);
            Renderer::TexCoord2d(tcx + size * 0.0, tcy + size * 1.0);
            Renderer::Vertex3d(0.5 + x, 0.5 + y, 0.5 + z);
            Renderer::Color3d(color4, color4, color4);
            Renderer::TexCoord2d(tcx + size * 0.0, tcy + size * 0.0);
            Renderer::Vertex3d(0.5 + x, -0.5 + y, 0.5 + z);

        }

        if (NiceGrass && blk[0] == Blocks::GRASS &&
            World::GetBlock({(gx - 1), (gy - 1), (gz)}, Blocks::ROCK, chunkptr) == Blocks::GRASS) {
            tcx = Textures::getTexcoordX(blk[0], 1) + EPS;
            tcy = Textures::getTexcoordY(blk[0], 1) + EPS;
        } else {
            tcx = Textures::getTexcoordX(blk[0], 2) + EPS;
            tcy = Textures::getTexcoordY(blk[0], 2) + EPS;
        }

        // Left Face
        if (!(BlockInfo(blk[4]).isOpaque() || (blk[4] == blk[0] && !BlockInfo(blk[0]).isOpaque())) ||
            blk[0] == Blocks::LEAF) {

            colors = brt[4];
            color1 = colors;
            color2 = colors;
            color3 = colors;
            color4 = colors;

            if (blk[0] != Blocks::GLOWSTONE && SmoothLighting) {
                color1 = (colors + World::getbrightness(gx - 1, gy - 1, gz) + World::getbrightness(gx - 1, gy, gz - 1) +
                          World::getbrightness(gx - 1, gy - 1, gz - 1)) / 4.0;
                color2 = (colors + World::getbrightness(gx - 1, gy - 1, gz) + World::getbrightness(gx - 1, gy, gz + 1) +
                          World::getbrightness(gx - 1, gy - 1, gz + 1)) / 4.0;
                color3 = (colors + World::getbrightness(gx - 1, gy + 1, gz) + World::getbrightness(gx - 1, gy, gz + 1) +
                          World::getbrightness(gx - 1, gy + 1, gz + 1)) / 4.0;
                color4 = (colors + World::getbrightness(gx - 1, gy + 1, gz) + World::getbrightness(gx - 1, gy, gz - 1) +
                          World::getbrightness(gx - 1, gy + 1, gz - 1)) / 4.0;
            }

            color1 /= World::BRIGHTNESSMAX;
            color2 /= World::BRIGHTNESSMAX;
            color3 /= World::BRIGHTNESSMAX;
            color4 /= World::BRIGHTNESSMAX;
            if (blk[0] != Blocks::GLOWSTONE && !Renderer::AdvancedRender) {
                color1 *= 0.7;
                color2 *= 0.7;
                color3 *= 0.7;
                color4 *= 0.7;
            }

            if (Renderer::AdvancedRender) Renderer::Attrib1f(3.0f);
            Renderer::Color3d(color1, color1, color1);
            Renderer::TexCoord2d(tcx + size * 0.0, tcy + size * 0.0);
            Renderer::Vertex3d(-0.5 + x, -0.5 + y, -0.5 + z);
            Renderer::Color3d(color2, color2, color2);
            Renderer::TexCoord2d(tcx + size * 1.0, tcy + size * 0.0);
            Renderer::Vertex3d(-0.5 + x, -0.5 + y, 0.5 + z);
            Renderer::Color3d(color3, color3, color3);
            Renderer::TexCoord2d(tcx + size * 1.0, tcy + size * 1.0);
            Renderer::Vertex3d(-0.5 + x, 0.5 + y, 0.5 + z);
            Renderer::Color3d(color4, color4, color4);
            Renderer::TexCoord2d(tcx + size * 0.0, tcy + size * 1.0);
            Renderer::Vertex3d(-0.5 + x, 0.5 + y, -0.5 + z);

        }

        tcx = Textures::getTexcoordX(blk[0], 1);
        tcy = Textures::getTexcoordY(blk[0], 1);

        // Top Face
        if (!(BlockInfo(blk[5]).isOpaque() || (blk[5] == blk[0] && !BlockInfo(blk[0]).isOpaque())) ||
            blk[0] == Blocks::LEAF) {

            colors = brt[5];
            color1 = colors;
            color2 = colors;
            color3 = colors;
            color4 = colors;

            if (blk[0] != Blocks::GLOWSTONE && SmoothLighting) {
                color1 = (color1 + World::getbrightness(gx, gy + 1, gz - 1) + World::getbrightness(gx - 1, gy + 1, gz) +
                          World::getbrightness(gx - 1, gy + 1, gz - 1)) / 4.0;
                color2 = (color2 + World::getbrightness(gx, gy + 1, gz + 1) + World::getbrightness(gx - 1, gy + 1, gz) +
                          World::getbrightness(gx - 1, gy + 1, gz + 1)) / 4.0;
                color3 = (color3 + World::getbrightness(gx, gy + 1, gz + 1) + World::getbrightness(gx + 1, gy + 1, gz) +
                          World::getbrightness(gx + 1, gy + 1, gz + 1)) / 4.0;
                color4 = (color4 + World::getbrightness(gx, gy + 1, gz - 1) + World::getbrightness(gx + 1, gy + 1, gz) +
                          World::getbrightness(gx + 1, gy + 1, gz - 1)) / 4.0;
            }

            color1 /= World::BRIGHTNESSMAX;
            color2 /= World::BRIGHTNESSMAX;
            color3 /= World::BRIGHTNESSMAX;
            color4 /= World::BRIGHTNESSMAX;

            if (Renderer::AdvancedRender) Renderer::Attrib1f(4.0f);
            Renderer::Color3d(color1, color1, color1);
            Renderer::TexCoord2d(tcx + size * 0.0, tcy + size * 1.0);
            Renderer::Vertex3d(-0.5 + x, 0.5 + y, -0.5 + z);
            Renderer::Color3d(color2, color2, color2);
            Renderer::TexCoord2d(tcx + size * 0.0, tcy + size * 0.0);
            Renderer::Vertex3d(-0.5 + x, 0.5 + y, 0.5 + z);
            Renderer::Color3d(color3, color3, color3);
            Renderer::TexCoord2d(tcx + size * 1.0, tcy + size * 0.0);
            Renderer::Vertex3d(0.5 + x, 0.5 + y, 0.5 + z);
            Renderer::Color3d(color4, color4, color4);
            Renderer::TexCoord2d(tcx + size * 1.0, tcy + size * 1.0);
            Renderer::Vertex3d(0.5 + x, 0.5 + y, -0.5 + z);

        }

        tcx = Textures::getTexcoordX(blk[0], 3);
        tcy = Textures::getTexcoordY(blk[0], 3);

        // Bottom Face
        if (!(BlockInfo(blk[6]).isOpaque() || (blk[6] == blk[0] && !BlockInfo(blk[0]).isOpaque())) ||
            blk[0] == Blocks::LEAF) {

            colors = brt[6];
            color1 = colors;
            color2 = colors;
            color3 = colors;
            color4 = colors;

            if (blk[0] != Blocks::GLOWSTONE && SmoothLighting) {
                color1 = (colors + World::getbrightness(gx, gy - 1, gz - 1) + World::getbrightness(gx - 1, gy - 1, gz) +
                          World::getbrightness(gx - 1, gy - 1, gz - 1)) / 4.0;
                color2 = (colors + World::getbrightness(gx, gy - 1, gz - 1) + World::getbrightness(gx + 1, gy - 1, gz) +
                          World::getbrightness(gx + 1, gy - 1, gz - 1)) / 4.0;
                color3 = (colors + World::getbrightness(gx, gy - 1, gz + 1) + World::getbrightness(gx + 1, gy - 1, gz) +
                          World::getbrightness(gx + 1, gy - 1, gz + 1)) / 4.0;
                color4 = (colors + World::getbrightness(gx, gy - 1, gz + 1) + World::getbrightness(gx - 1, gy - 1, gz) +
                          World::getbrightness(gx - 1, gy - 1, gz + 1)) / 4.0;
            }

            color1 /= World::BRIGHTNESSMAX;
            color2 /= World::BRIGHTNESSMAX;
            color3 /= World::BRIGHTNESSMAX;
            color4 /= World::BRIGHTNESSMAX;

            if (Renderer::AdvancedRender) Renderer::Attrib1f(5.0f);
            Renderer::Color3d(color1, color1, color1);
            Renderer::TexCoord2d(tcx + size * 1.0, tcy + size * 1.0);
            Renderer::Vertex3d(-0.5 + x, -0.5 + y, -0.5 + z);
            Renderer::Color3d(color2, color2, color2);
            Renderer::TexCoord2d(tcx + size * 0.0, tcy + size * 1.0);
            Renderer::Vertex3d(0.5 + x, -0.5 + y, -0.5 + z);
            Renderer::Color3d(color3, color3, color3);
            Renderer::TexCoord2d(tcx + size * 0.0, tcy + size * 0.0);
            Renderer::Vertex3d(0.5 + x, -0.5 + y, 0.5 + z);
            Renderer::Color3d(color4, color4, color4);
            Renderer::TexCoord2d(tcx + size * 1.0, tcy + size * 0.0);
            Renderer::Vertex3d(-0.5 + x, -0.5 + y, 0.5 + z);

        }
    }


    /*
    合并面的顶点顺序（以0到3标出）：

    The vertex order of merge face render
    Numbered from 0 to 3:

    (k++)
    ...
    |    |
    +----+--
    |    |
    |    |    |
    3----2----+-
    |curr|    |   ...
    |face|    |   (j++)
    0----1----+--

    --qiaozhanrong
    */

    void RenderPrimitive(QuadPrimitive &p) {
        auto col0 = static_cast<float>(p.col0) * 0.25f / World::BRIGHTNESSMAX;
        auto col1 = static_cast<float>(p.col1) * 0.25f / World::BRIGHTNESSMAX;
        auto col2 = static_cast<float>(p.col2) * 0.25f / World::BRIGHTNESSMAX;
        auto col3 = static_cast<float>(p.col3) * 0.25f / World::BRIGHTNESSMAX;
        const auto x = p.x, y = p.y, z = p.z, length = p.length;
#ifdef NERDMODE1
        Renderer::TexCoord3d(0.0, 0.0, (p.tex + 0.5) / 64.0);
        if (p.direction == 0) {
            if (Renderer::AdvancedRender) Renderer::Attrib1f(2.0f);
            else col0 *= 0.7f, col1 *= 0.7f, col2 *= 0.7f, col3 *= 0.7f;
            Renderer::Color3f(col0, col0, col0);
            Renderer::TexCoord2d(0.0, 0.0);
            Renderer::Vertex3d(x + 0.5, y - 0.5, z - 0.5);
            Renderer::Color3f(col1, col1, col1);
            Renderer::TexCoord2d(0.0, 1.0);
            Renderer::Vertex3d(x + 0.5, y + 0.5, z - 0.5);
            Renderer::Color3f(col2, col2, col2);
            Renderer::TexCoord2d(length + 1.0, 1.0);
            Renderer::Vertex3d(x + 0.5, y + 0.5, z + length + 0.5);
            Renderer::Color3f(col3, col3, col3);
            Renderer::TexCoord2d(length + 1.0, 0.0);
            Renderer::Vertex3d(x + 0.5, y - 0.5, z + length + 0.5);
        } else if (p.direction == 1) {
            if (Renderer::AdvancedRender) Renderer::Attrib1f(3.0f);
            else col0 *= 0.7f, col1 *= 0.7f, col2 *= 0.7f, col3 *= 0.7f;
            Renderer::Color3f(col0, col0, col0);
            Renderer::TexCoord2d(0.0, 1.0);
            Renderer::Vertex3d(x - 0.5, y + 0.5, z - 0.5);
            Renderer::Color3f(col1, col1, col1);
            Renderer::TexCoord2d(0.0, 0.0);
            Renderer::Vertex3d(x - 0.5, y - 0.5, z - 0.5);
            Renderer::Color3f(col2, col2, col2);
            Renderer::TexCoord2d(length + 1.0, 0.0);
            Renderer::Vertex3d(x - 0.5, y - 0.5, z + length + 0.5);
            Renderer::Color3f(col3, col3, col3);
            Renderer::TexCoord2d(length + 1.0, 1.0);
            Renderer::Vertex3d(x - 0.5, y + 0.5, z + length + 0.5);
        } else if (p.direction == 2) {
            if (Renderer::AdvancedRender) Renderer::Attrib1f(4.0f);
            Renderer::Color3f(col0, col0, col0);
            Renderer::TexCoord2d(0.0, 0.0);
            Renderer::Vertex3d(x + 0.5, y + 0.5, z - 0.5);
            Renderer::Color3f(col1, col1, col1);
            Renderer::TexCoord2d(0.0, 1.0);
            Renderer::Vertex3d(x - 0.5, y + 0.5, z - 0.5);
            Renderer::Color3f(col2, col2, col2);
            Renderer::TexCoord2d(length + 1.0, 1.0);
            Renderer::Vertex3d(x - 0.5, y + 0.5, z + length + 0.5);
            Renderer::Color3f(col3, col3, col3);
            Renderer::TexCoord2d(length + 1.0, 0.0);
            Renderer::Vertex3d(x + 0.5, y + 0.5, z + length + 0.5);
        } else if (p.direction == 3) {
            if (Renderer::AdvancedRender) Renderer::Attrib1f(5.0f);
            Renderer::Color3f(col0, col0, col0);
            Renderer::TexCoord2d(0.0, 0.0);
            Renderer::Vertex3d(x - 0.5, y - 0.5, z - 0.5);
            Renderer::Color3f(col1, col1, col1);
            Renderer::TexCoord2d(0.0, 1.0);
            Renderer::Vertex3d(x + 0.5, y - 0.5, z - 0.5);
            Renderer::Color3f(col2, col2, col2);
            Renderer::TexCoord2d(length + 1.0, 1.0);
            Renderer::Vertex3d(x + 0.5, y - 0.5, z + length + 0.5);
            Renderer::Color3f(col3, col3, col3);
            Renderer::TexCoord2d(length + 1.0, 0.0);
            Renderer::Vertex3d(x - 0.5, y - 0.5, z + length + 0.5);
        } else if (p.direction == 4) {
            if (Renderer::AdvancedRender) Renderer::Attrib1f(0.0f);
            else col0 *= 0.5f, col1 *= 0.5f, col2 *= 0.5f, col3 *= 0.5f;
            Renderer::Color3f(col0, col0, col0);
            Renderer::TexCoord2d(0.0, 1.0);
            Renderer::Vertex3d(x - 0.5, y + 0.5, z + 0.5);
            Renderer::Color3f(col1, col1, col1);
            Renderer::TexCoord2d(0.0, 0.0);
            Renderer::Vertex3d(x - 0.5, y - 0.5, z + 0.5);
            Renderer::Color3f(col2, col2, col2);
            Renderer::TexCoord2d(length + 1.0, 0.0);
            Renderer::Vertex3d(x + length + 0.5, y - 0.5, z + 0.5);
            Renderer::Color3f(col3, col3, col3);
            Renderer::TexCoord2d(length + 1.0, 1.0);
            Renderer::Vertex3d(x + length + 0.5, y + 0.5, z + 0.5);
        } else if (p.direction == 5) {
            if (Renderer::AdvancedRender) Renderer::Attrib1f(1.0f);
            else col0 *= 0.5f, col1 *= 0.5f, col2 *= 0.5f, col3 *= 0.5f;
            Renderer::Color3f(col0, col0, col0);
            Renderer::TexCoord2d(0.0, 0.0);
            Renderer::Vertex3d(x - 0.5, y - 0.5, z - 0.5);
            Renderer::Color3f(col1, col1, col1);
            Renderer::TexCoord2d(0.0, 1.0);
            Renderer::Vertex3d(x - 0.5, y + 0.5, z - 0.5);
            Renderer::Color3f(col2, col2, col2);
            Renderer::TexCoord2d(length + 1.0, 1.0);
            Renderer::Vertex3d(x + length + 0.5, y + 0.5, z - 0.5);
            Renderer::Color3f(col3, col3, col3);
            Renderer::TexCoord2d(length + 1.0, 0.0);
            Renderer::Vertex3d(x + length + 0.5, y - 0.5, z - 0.5);
        }
#else
        float T3d = (Textures::getTextureIndex(p.block, face) - 0.5) / 64.0;
        switch (p.direction)
        {
        case 0: {
            if (p.block != Blocks::GLOWSTONE) color *= 0.7;
            float geomentry[] = {
                0.0, 0.0, T3d, color, color, color, x + 0.5, y - 0.5, z - 0.5,
                0.0, 1.0, T3d, color, color, color, x + 0.5, y + 0.5, z - 0.5,
                length + 1.0, 1.0, T3d, color, color, color, x + 0.5, y + 0.5, z + length + 0.5,
                length + 1.0, 0.0, T3d, color, color, color, x + 0.5, y - 0.5, z + length + 0.5
            };
            Renderer::Quad(geomentry);
        }
                break;
        case 1: {
            if (p.block != Blocks::GLOWSTONE) color *= 0.7;
            float geomentry[] = {
                0.0, 1.0, T3d, color, color, color, x - 0.5, y + 0.5, z - 0.5,
                0.0, 0.0, T3d, color, color, color, x - 0.5, y - 0.5, z - 0.5,
                length + 1.0, 0.0, T3d, color, color, color, x - 0.5, y - 0.5, z + length + 0.5,
                length + 1.0, 1.0, T3d, color, color, color, x - 0.5, y + 0.5, z + length + 0.5
            };
            Renderer::Quad(geomentry);
        }
                break;
        case 2: {
            float geomentry[] = {
                0.0, 0.0, T3d, color, color, color, x + 0.5, y + 0.5, z - 0.5,
                0.0, 1.0, T3d, color, color, color, x - 0.5, y + 0.5, z - 0.5,
                length + 1.0, 1.0, T3d, color, color, color, x - 0.5, y + 0.5, z + length + 0.5,
                length + 1.0, 0.0, T3d, color, color, color, x + 0.5, y + 0.5, z + length + 0.5
            };
            Renderer::Quad(geomentry);
        }
                break;
        case 3: {
            float geomentry[] = {
                0.0, 0.0, T3d, color, color, color, x - 0.5, y - 0.5, z - 0.5,
                0.0, 1.0, T3d, color, color, color, x + 0.5, y - 0.5, z - 0.5,
                length + 1.0, 1.0, T3d, color, color, color, x + 0.5, y - 0.5, z + length + 0.5,
                length + 1.0, 0.0, T3d, color, color, color, x - 0.5, y - 0.5, z + length + 0.5
            };
            Renderer::Quad(geomentry);
        }
                break;
        case 4: {
            if (p.block != Blocks::GLOWSTONE) color *= 0.5;
            float geomentry[] = {
                0.0, 1.0, T3d, color, color, color, x - 0.5, y + 0.5, z + 0.5,
                0.0, 0.0, T3d, color, color, color, x - 0.5, y - 0.5, z + 0.5,
                length + 1.0, 0.0, T3d, color, color, color, x + length + 0.5, y - 0.5, z + 0.5,
                length + 1.0, 1.0, T3d, color, color, color, x + length + 0.5, y + 0.5, z + 0.5
            };
            Renderer::Quad(geomentry);
        }
                break;
        case 5: {
            if (p.block != Blocks::GLOWSTONE) color *= 0.5;
            float geomentry[] = {
                0.0, 0.0, T3d, color, color, color, x - 0.5, y - 0.5, z - 0.5,
                0.0, 1.0, T3d, color, color, color, x - 0.5, y + 0.5, z - 0.5,
                length + 1.0, 1.0, T3d, color, color, color, x + length + 0.5, y + 0.5, z - 0.5,
                length + 1.0, 0.0, T3d, color, color, color, x + length + 0.5, y - 0.5, z - 0.5
            };
            Renderer::Quad(geomentry);
        }
                break;
        }
#endif // NERDMODE1

    }

    void RenderPrimitive_Depth(QuadPrimitive_Depth &p) {
        const auto x = p.x, y = p.y, z = p.z, length = p.length;
        if (p.direction == 0) {
            Renderer::Vertex3d(x + 0.5, y - 0.5, z - 0.5);
            Renderer::Vertex3d(x + 0.5, y + 0.5, z - 0.5);
            Renderer::Vertex3d(x + 0.5, y + 0.5, z + length + 0.5);
            Renderer::Vertex3d(x + 0.5, y - 0.5, z + length + 0.5);
        } else if (p.direction == 1) {
            Renderer::Vertex3d(x - 0.5, y + 0.5, z - 0.5);
            Renderer::Vertex3d(x - 0.5, y - 0.5, z - 0.5);
            Renderer::Vertex3d(x - 0.5, y - 0.5, z + length + 0.5);
            Renderer::Vertex3d(x - 0.5, y + 0.5, z + length + 0.5);
        } else if (p.direction == 2) {
            Renderer::Vertex3d(x + 0.5, y + 0.5, z - 0.5);
            Renderer::Vertex3d(x - 0.5, y + 0.5, z - 0.5);
            Renderer::Vertex3d(x - 0.5, y + 0.5, z + length + 0.5);
            Renderer::Vertex3d(x + 0.5, y + 0.5, z + length + 0.5);
        } else if (p.direction == 3) {
            Renderer::Vertex3d(x - 0.5, y - 0.5, z - 0.5);
            Renderer::Vertex3d(x + 0.5, y - 0.5, z - 0.5);
            Renderer::Vertex3d(x + 0.5, y - 0.5, z + length + 0.5);
            Renderer::Vertex3d(x - 0.5, y - 0.5, z + length + 0.5);
        } else if (p.direction == 4) {
            Renderer::Vertex3d(x - 0.5, y + 0.5, z + 0.5);
            Renderer::Vertex3d(x - 0.5, y - 0.5, z + 0.5);
            Renderer::Vertex3d(x + length + 0.5, y - 0.5, z + 0.5);
            Renderer::Vertex3d(x + length + 0.5, y + 0.5, z + 0.5);
        } else if (p.direction == 5) {
            Renderer::Vertex3d(x - 0.5, y - 0.5, z - 0.5);
            Renderer::Vertex3d(x - 0.5, y + 0.5, z - 0.5);
            Renderer::Vertex3d(x + length + 0.5, y + 0.5, z - 0.5);
            Renderer::Vertex3d(x + length + 0.5, y - 0.5, z - 0.5);
        }
    }

    void RenderChunk(World::Chunk *c) {
        int x, y, z;
        if (Renderer::AdvancedRender) Renderer::Init(2, 3, 1); else Renderer::Init(2, 3);
        for (x = 0; x < 16; x++) {
            for (y = 0; y < 16; y++) {
                for (z = 0; z < 16; z++) {
                    const auto curr = c->GetBlock({x, y, z});
                    if (curr == Blocks::AIR) continue;
                    if (!BlockInfo(curr).isTranslucent()) renderblock(x, y, z, c);
                }
            }
        }
        Renderer::Flush(c->vbuffer[0], c->vertexes[0]);
        if (Renderer::AdvancedRender) Renderer::Init(2, 3, 1); else Renderer::Init(2, 3);
        for (x = 0; x < 16; x++) {
            for (y = 0; y < 16; y++) {
                for (z = 0; z < 16; z++) {
                    const auto curr = c->GetBlock({x, y, z});
                    if (curr == Blocks::AIR) continue;
                    if (BlockInfo(curr).isTranslucent() && BlockInfo(curr).isSolid()) renderblock(x, y, z, c);
                }
            }
        }
        Renderer::Flush(c->vbuffer[1], c->vertexes[1]);
        if (Renderer::AdvancedRender) Renderer::Init(2, 3, 1); else Renderer::Init(2, 3);
        for (x = 0; x < 16; x++) {
            for (y = 0; y < 16; y++) {
                for (z = 0; z < 16; z++) {
                    const auto curr = c->GetBlock({x, y, z});
                    if (curr == Blocks::AIR) continue;
                    if (!BlockInfo(curr).isSolid()) renderblock(x, y, z, c);
                }
            }
        }
        Renderer::Flush(c->vbuffer[2], c->vertexes[2]);
    }

    //合并面大法好！！！
    void MergeFaceRender(World::Chunk *c) {
        //话说我注释一会中文一会英文是不是有点奇怪。。。
        // -- qiaozhanrong

        auto cx = c->cx, cy = c->cy, cz = c->cz;
        auto gx = 0, gy = 0, gz = 0;
        int x = 0, y = 0, z = 0, cur_l_mx, br;
        auto col0 = 0, col1 = 0, col2 = 0, col3 = 0;
        QuadPrimitive cur;
        Block bl, neighbour;
        ubyte face = 0;
        TextureID tex;
        auto valid = false;
        for (auto steps = 0; steps < 3; steps++) {
            cur = QuadPrimitive();
            cur_l_mx = bl = neighbour = 0;
            //Linear merge
            if (Renderer::AdvancedRender) Renderer::Init(3, 3, 1); else Renderer::Init(3, 3);
            for (auto d = 0; d < 6; d++) {
                cur.direction = d;
                if (d == 2) face = 1;
                else if (d == 3) face = 3;
                else face = 2;
                //Render current face
                for (auto i = 0; i < 16; i++)
                    for (auto j = 0; j < 16; j++) {
                        for (auto k = 0; k < 16; k++) {
                            //Get position & brightness
                            if (d == 0) { //x+
                                x = i, y = j, z = k;
                                gx = cx * 16 + x;
                                gy = cy * 16 + y;
                                gz = cz * 16 + z;
                                br = getbrightness(gx + 1, gy, gz, c);
                                if (SmoothLighting) {
                                    col0 = br + getbrightness(gx + 1, gy - 1, gz, c) +
                                           getbrightness(gx + 1, gy, gz - 1, c) +
                                           getbrightness(gx + 1, gy - 1, gz - 1, c);
                                    col1 = br + getbrightness(gx + 1, gy + 1, gz, c) +
                                           getbrightness(gx + 1, gy, gz - 1, c) +
                                           getbrightness(gx + 1, gy + 1, gz - 1, c);
                                    col2 = br + getbrightness(gx + 1, gy + 1, gz, c) +
                                           getbrightness(gx + 1, gy, gz + 1, c) +
                                           getbrightness(gx + 1, gy + 1, gz + 1, c);
                                    col3 = br + getbrightness(gx + 1, gy - 1, gz, c) +
                                           getbrightness(gx + 1, gy, gz + 1, c) +
                                           getbrightness(gx + 1, gy - 1, gz + 1, c);
                                } else col0 = col1 = col2 = col3 = br * 4;
                            } else if (d == 1) { //x-
                                x = i, y = j, z = k;
                                gx = cx * 16 + x;
                                gy = cy * 16 + y;
                                gz = cz * 16 + z;
                                br = getbrightness(gx - 1, gy, gz, c);
                                if (SmoothLighting) {
                                    col0 = br + getbrightness(gx - 1, gy + 1, gz, c) +
                                           getbrightness(gx - 1, gy, gz - 1, c) +
                                           getbrightness(gx - 1, gy + 1, gz - 1, c);
                                    col1 = br + getbrightness(gx - 1, gy - 1, gz, c) +
                                           getbrightness(gx - 1, gy, gz - 1, c) +
                                           getbrightness(gx - 1, gy - 1, gz - 1, c);
                                    col2 = br + getbrightness(gx - 1, gy - 1, gz, c) +
                                           getbrightness(gx - 1, gy, gz + 1, c) +
                                           getbrightness(gx - 1, gy - 1, gz + 1, c);
                                    col3 = br + getbrightness(gx - 1, gy + 1, gz, c) +
                                           getbrightness(gx - 1, gy, gz + 1, c) +
                                           getbrightness(gx - 1, gy + 1, gz + 1, c);
                                } else col0 = col1 = col2 = col3 = br * 4;
                            } else if (d == 2) { //y+
                                x = j, y = i, z = k;
                                gx = cx * 16 + x;
                                gy = cy * 16 + y;
                                gz = cz * 16 + z;
                                br = getbrightness(gx, gy + 1, gz, c);
                                if (SmoothLighting) {
                                    col0 = br + getbrightness(gx + 1, gy + 1, gz, c) +
                                           getbrightness(gx, gy + 1, gz - 1, c) +
                                           getbrightness(gx + 1, gy + 1, gz - 1, c);
                                    col1 = br + getbrightness(gx - 1, gy + 1, gz, c) +
                                           getbrightness(gx, gy + 1, gz - 1, c) +
                                           getbrightness(gx - 1, gy + 1, gz - 1, c);
                                    col2 = br + getbrightness(gx - 1, gy + 1, gz, c) +
                                           getbrightness(gx, gy + 1, gz + 1, c) +
                                           getbrightness(gx - 1, gy + 1, gz + 1, c);
                                    col3 = br + getbrightness(gx + 1, gy + 1, gz, c) +
                                           getbrightness(gx, gy + 1, gz + 1, c) +
                                           getbrightness(gx + 1, gy + 1, gz + 1, c);
                                } else col0 = col1 = col2 = col3 = br * 4;
                            } else if (d == 3) { //y-
                                x = j, y = i, z = k;
                                gx = cx * 16 + x;
                                gy = cy * 16 + y;
                                gz = cz * 16 + z;
                                br = getbrightness(gx, gy - 1, gz, c);
                                if (SmoothLighting) {
                                    col0 = br + getbrightness(gx - 1, gy - 1, gz, c) +
                                           getbrightness(gx, gy - 1, gz - 1, c) +
                                           getbrightness(gx - 1, gy - 1, gz - 1, c);
                                    col1 = br + getbrightness(gx + 1, gy - 1, gz, c) +
                                           getbrightness(gx, gy - 1, gz - 1, c) +
                                           getbrightness(gx + 1, gy - 1, gz - 1, c);
                                    col2 = br + getbrightness(gx + 1, gy - 1, gz, c) +
                                           getbrightness(gx, gy - 1, gz + 1, c) +
                                           getbrightness(gx + 1, gy - 1, gz + 1, c);
                                    col3 = br + getbrightness(gx - 1, gy - 1, gz, c) +
                                           getbrightness(gx, gy - 1, gz + 1, c) +
                                           getbrightness(gx - 1, gy - 1, gz + 1, c);
                                } else col0 = col1 = col2 = col3 = br * 4;
                            } else if (d == 4) { //z+
                                x = k, y = j, z = i;
                                gx = cx * 16 + x;
                                gy = cy * 16 + y;
                                gz = cz * 16 + z;
                                br = getbrightness(gx, gy, gz + 1, c);
                                if (SmoothLighting) {
                                    col0 = br + getbrightness(gx - 1, gy, gz + 1, c) +
                                           getbrightness(gx, gy + 1, gz + 1, c) +
                                           getbrightness(gx - 1, gy + 1, gz + 1, c);
                                    col1 = br + getbrightness(gx - 1, gy, gz + 1, c) +
                                           getbrightness(gx, gy - 1, gz + 1, c) +
                                           getbrightness(gx - 1, gy - 1, gz + 1, c);
                                    col2 = br + getbrightness(gx + 1, gy, gz + 1, c) +
                                           getbrightness(gx, gy - 1, gz + 1, c) +
                                           getbrightness(gx + 1, gy - 1, gz + 1, c);
                                    col3 = br + getbrightness(gx + 1, gy, gz + 1, c) +
                                           getbrightness(gx, gy + 1, gz + 1, c) +
                                           getbrightness(gx + 1, gy + 1, gz + 1, c);
                                } else col0 = col1 = col2 = col3 = br * 4;
                            } else if (d == 5) { //z-
                                x = k, y = j, z = i;
                                gx = cx * 16 + x;
                                gy = cy * 16 + y;
                                gz = cz * 16 + z;
                                br = getbrightness(gx, gy, gz - 1, c);
                                if (SmoothLighting) {
                                    col0 = br + getbrightness(gx - 1, gy, gz - 1, c) +
                                           getbrightness(gx, gy - 1, gz - 1, c) +
                                           getbrightness(gx - 1, gy - 1, gz - 1, c);
                                    col1 = br + getbrightness(gx - 1, gy, gz - 1, c) +
                                           getbrightness(gx, gy + 1, gz - 1, c) +
                                           getbrightness(gx - 1, gy + 1, gz - 1, c);
                                    col2 = br + getbrightness(gx + 1, gy, gz - 1, c) +
                                           getbrightness(gx, gy + 1, gz - 1, c) +
                                           getbrightness(gx + 1, gy + 1, gz - 1, c);
                                    col3 = br + getbrightness(gx + 1, gy, gz - 1, c) +
                                           getbrightness(gx, gy - 1, gz - 1, c) +
                                           getbrightness(gx + 1, gy - 1, gz - 1, c);
                                } else col0 = col1 = col2 = col3 = br * 4;
                            }
                            //Get block ID
                            bl = c->GetBlock({x, y, z});
                            tex = Textures::getTextureIndex(bl, face);
                            neighbour = World::GetBlock({(gx + delta[d][0]), (gy + delta[d][1]), (gz + delta[d][2])},
                                                 Blocks::ROCK, c);
                            if (NiceGrass && bl == Blocks::GRASS) {
                                if (d == 0 && World::GetBlock({(gx + 1), (gy - 1), (gz)}, Blocks::ROCK, c) == Blocks::GRASS)
                                    tex = Textures::getTextureIndex(bl, 1);
                                else {
                                    if (d == 1 && World::GetBlock({(gx - 1), (gy - 1), (gz)}, Blocks::ROCK, c) == Blocks::GRASS)
                                        tex = Textures::getTextureIndex(bl, 1);
                                    else if (d == 4 && World::GetBlock({gx, gy - 1, gz + 1}, Blocks::ROCK, c) == Blocks::GRASS)
                                        tex = Textures::getTextureIndex(bl, 1);
                                    else if (d == 5 && World::GetBlock({gx, gy - 1, gz - 1}, Blocks::ROCK, c) == Blocks::GRASS)
                                        tex = Textures::getTextureIndex(bl, 1);
                                }
                            }
                            //Render
                            const auto& info = BlockInfo(bl);
                            if (bl == Blocks::AIR || bl == neighbour && bl != Blocks::LEAF ||
                                BlockInfo(neighbour).isOpaque() ||
                                steps == 0 && info.isTranslucent() ||
                                steps == 1 && (!info.isTranslucent() || !info.isSolid()) ||
                                steps == 2 && (!info.isTranslucent() || info.isSolid())) {
                                //Not valid block
                                if (valid) {
                                    if (BlockInfo(neighbour).isOpaque() && !cur.once) {
                                        if (cur_l_mx < cur.length) cur_l_mx = cur.length;
                                        cur_l_mx++;
                                    } else {
                                        RenderPrimitive(cur);
                                        valid = false;
                                    }
                                }
                                continue;
                            }
                            if (valid) {
                                if (col0 != col1 || col1 != col2 || col2 != col3 || cur.once || tex != cur.tex ||
                                    col0 != cur.col0) {
                                    RenderPrimitive(cur);
                                    cur.x = x;
                                    cur.y = y;
                                    cur.z = z;
                                    cur.length = cur_l_mx = 0;
                                    cur.tex = tex;
                                    cur.col0 = col0;
                                    cur.col1 = col1;
                                    cur.col2 = col2;
                                    cur.col3 = col3;
                                    cur.once = col0 != col1 || col1 != col2 || col2 != col3;
                                } else {
                                    if (cur_l_mx > cur.length) cur.length = cur_l_mx;
                                    cur.length++;
                                }
                            } else {
                                valid = true;
                                cur.x = x;
                                cur.y = y;
                                cur.z = z;
                                cur.length = cur_l_mx = 0;
                                cur.tex = tex;
                                cur.col0 = col0;
                                cur.col1 = col1;
                                cur.col2 = col2;
                                cur.col3 = col3;
                                cur.once = col0 != col1 || col1 != col2 || col2 != col3;
                            }
                        }
                        if (valid) {
                            RenderPrimitive(cur);
                            valid = false;
                        }
                    }
            }
            Renderer::Flush(c->vbuffer[steps], c->vertexes[steps]);
        }
    }

    void RenderDepthModel(World::Chunk *c) {
        const auto cx = c->cx, cy = c->cy, cz = c->cz;
        auto x = 0, y = 0, z = 0;
        QuadPrimitive_Depth cur;
        Block bl, neighbour;
        auto valid = false;
        int cur_l_mx = bl = neighbour = 0;
        //Linear merge for depth model
        Renderer::Init(0, 0);
        for (auto d = 0; d < 6; d++) {
            cur.direction = d;
            for (auto i = 0; i < 16; i++)
                for (auto j = 0; j < 16; j++) {
                    for (auto k = 0; k < 16; k++) {
                        //Get position
                        if (d < 2) x = i, y = j, z = k;
                        else if (d < 4) x = i, y = j, z = k;
                        else x = k, y = i, z = j;
                        //Get block ID
                        bl = c->GetBlock({x, y, z});
                        //Get neighbour ID
                        const auto xx = x + delta[d][0], yy = y + delta[d][1], zz = z + delta[d][2];
                        const auto gx = cx * 16 + xx, gy = cy * 16 + yy, gz = cz * 16 + zz;
                        if (xx < 0 || xx >= 16 || yy < 0 || yy >= 16 || zz < 0 || zz >= 16) {
                            neighbour = World::GetBlock({(gx), (gy), (gz)});
                        }
                        else {
                            neighbour = c->GetBlock({(xx), (yy), (zz)});
                        }
                        //Render
                        if (bl == Blocks::AIR || bl == Blocks::GLASS || bl == neighbour && bl != Blocks::LEAF ||
                            BlockInfo(neighbour).isOpaque() || BlockInfo(bl).isTranslucent()) {
                            //Not valid block
                            if (valid) {
                                if (BlockInfo(neighbour).isOpaque()) {
                                    if (cur_l_mx < cur.length) cur_l_mx = cur.length;
                                    cur_l_mx++;
                                } else {
                                    RenderPrimitive_Depth(cur);
                                    valid = false;
                                }
                            }
                            continue;
                        }
                        if (valid) {
                            if (cur_l_mx > cur.length) cur.length = cur_l_mx;
                            cur.length++;
                        } else {
                            valid = true;
                            cur.x = x;
                            cur.y = y;
                            cur.z = z;
                            cur.length = cur_l_mx = 0;
                        }
                    }
                    if (valid) {
                        RenderPrimitive_Depth(cur);
                        valid = false;
                    }
                }
        }
        Renderer::Flush(c->vbuffer[3], c->vertexes[3]);
    }
}