module;

#include <glad/gl.h>
#undef assert

module menus;
import std;
import types;
import gui;
import globals;
import globalization;
import rendering;
import render;
import textures;

namespace Menus {
using Globalization::GetStrbyKey;

class WorldMenu: public GUI::Form {
private:
    int leftp = 0;
    int midp = 0;
    int rightp = 0;
    int downp = 0;
    bool refresh = true;
    int selected = 0, mouseon = false;
    std::string chosenWorldName;
    std::vector<std::string> worldnames;
    std::vector<render::Texture> thumbnails;
    std::vector<int> texSizeX, texSizeY;
    int trs = 0;
    GUI::Label title = GUI::Label("", -225, 225, 20, 36, 0.5, 0.5, 0.0, 0.0);
    GUI::VScroll vscroll = GUI::VScroll(100, 0, 275, 295, 36, -20, 0.5, 0.5, 0.0, 1.0);
    GUI::Button enterbtn = GUI::Button("", -250, -10, -80, -56, 0.5, 0.5, 1.0, 1.0);
    GUI::Button deletebtn = GUI::Button("", 10, 250, -80, -56, 0.5, 0.5, 1.0, 1.0);
    GUI::Button backbtn = GUI::Button("", -250, 250, -44, -20, 0.5, 0.5, 1.0, 1.0);
    static constexpr int top = 48;
    static constexpr int itemHeight = 70;
    static constexpr int borderWidth = 5;

    void onLoad() override {
        title.centered = true;
        vscroll.defaultv = true;
        enterbtn.enabled = false;
        deletebtn.enabled = false;
        registerControls({&title, &vscroll, &enterbtn, &deletebtn, &backbtn});
        Cur_WorldName = "";
    }

    void onUpdate() override {
        title.text = GetStrbyKey("NEWorld.worlds.caption");
        enterbtn.text = GetStrbyKey("NEWorld.worlds.enter");
        deletebtn.text = GetStrbyKey("NEWorld.worlds.delete");
        backbtn.text = GetStrbyKey("NEWorld.worlds.back");

        int worldcount = (int) worldnames.size();
        leftp = static_cast<int>(WindowWidth / 2.0 / Stretch - 250);
        midp = static_cast<int>(WindowWidth / 2.0 / Stretch);
        rightp = static_cast<int>(WindowWidth / 2.0 / Stretch + 250);
        downp = static_cast<int>(WindowHeight / Stretch - 20);

        vscroll.barheight = (downp - 72 - top) * (downp - 36 - 40) / (itemHeight * (worldcount + 1));
        if (vscroll.barheight > downp - 36 - 40) {
            vscroll.enabled = false;
            vscroll.barheight = downp - 36 - 40;
        } else
            vscroll.enabled = true;

        trs = vscroll.barpos * (itemHeight * (worldcount + 1)) / (downp - 36 - 40);
        mouseon = -1;
        if (mx >= midp - 250 && mx <= midp + 250 && my >= top && my <= downp - 72) {
            for (int i = 0; i < worldcount; i++) {
                if (my >= top + i * itemHeight - trs && my <= top + (i + 1) * itemHeight - trs) {
                    if (mb == 1 && mbl == 0) {
                        chosenWorldName = worldnames[i];
                        selected = i;
                    }
                    mouseon = i;
                }
            }
            if (my >= top + worldcount * itemHeight - trs && my <= top + (worldcount + 1) * itemHeight - trs) {
                if (mb == 0 && mbl == 1) {
                    createworldmenu();
                    refresh = true;
                }
                mouseon = worldcount;
            }
        }
        if (enterbtn.clicked) {
            Cur_WorldName = chosenWorldName;
            GameBegin = true;
        }
        if (deletebtn.clicked && !chosenWorldName.empty()) {
            std::filesystem::remove_all(std::filesystem::path("worlds") / chosenWorldName);
            deletebtn.clicked = false;
            Cur_WorldName = "";
            enterbtn.enabled = false;
            deletebtn.enabled = false;
            refresh = true;
        }
        if (refresh) {
            worldnames.clear();
            thumbnails.clear();
            texSizeX.clear();
            texSizeY.clear();
            worldcount = 0;
            selected = -1;
            mouseon = -1;
            vscroll.barpos = 0;
            chosenWorldName = "";
            for (auto const& entry: std::filesystem::directory_iterator("worlds")) {
                if (entry.is_directory()) {
                    worldnames.emplace_back(entry.path().filename().string());
                    thumbnails.emplace_back();
                    texSizeX.emplace_back();
                    texSizeY.emplace_back();
                    if (std::filesystem::exists(entry.path() / "thumbnail.png")) {
                        GLint width = 0, height = 0;
                        thumbnails.back() = Textures::LoadTexture(entry.path() / "thumbnail.png", true);
                        thumbnails.back().bind(0);
                        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
                        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);
                        texSizeX.back() = width;
                        texSizeY.back() = height;
                    }
                }
            }

            refresh = false;
        }

        enterbtn.enabled = chosenWorldName != "";
        deletebtn.enabled = chosenWorldName != "";
        if (backbtn.clicked)
            exit = true;
        if (GameBegin)
            exit = true;
    }

    void onRender() override {
        int scissorTop = static_cast<int>(top * Stretch);
        int scissorBottom = static_cast<int>((downp - 72) * Stretch);
        glEnable(GL_SCISSOR_TEST);
        glScissor(0, WindowHeight - scissorBottom, WindowWidth, std::max(0, scissorBottom - scissorTop));
        // glTranslatef(0.0f, (float) -trs, 0.0f);
        for (int i = 0; i < (int) thumbnails.size(); i++) {
            int xmin = 0, xmax = 0, ymin = 0, ymax = 0;
            xmin = midp - 250, xmax = midp + 250;
            ymin = top + i * itemHeight, ymax = top + (i + 1) * itemHeight;
            if (selected == (int) i) {
                /*
                glDisable(GL_TEXTURE_2D);
                glBegin(GL_QUADS);
                glColor4f(0.2f, 0.2f, 0.2f, 0.6f);
                GUI::UIVertex(midp - 255, top + i * itemHeight);
                GUI::UIVertex(midp - 255, top + (i + 1) * itemHeight);
                GUI::UIVertex(midp + 255, top + (i + 1) * itemHeight);
                GUI::UIVertex(midp + 255, top + i * itemHeight);
                glEnd();
                glEnable(GL_TEXTURE_2D);
                */
            }
            if (!thumbnails[i]) {
                /*
                glDisable(GL_TEXTURE_2D);
                glBegin(GL_QUADS);
                if (mouseon == i)
                    glColor4f(0.5, 0.5, 0.5, GUI::FgA);
                else
                    glColor4f(GUI::FgR, GUI::FgG, GUI::FgB, GUI::FgA);
                GUI::UIVertex(midp - 250, top + i * itemHeight + borderWidth);
                GUI::UIVertex(midp - 250, top + (i + 1) * itemHeight - borderWidth);
                GUI::UIVertex(midp + 250, top + (i + 1) * itemHeight - borderWidth);
                GUI::UIVertex(midp + 250, top + i * itemHeight + borderWidth);
                glEnd();
                glEnable(GL_TEXTURE_2D);
                */
            } else {
                bool marginOnSides = false;
                float w = 0.0f, h = 0.0f;
                if (texSizeX[i] * 60 / 500 < texSizeY[i]) {
                    marginOnSides = true;
                    w = 1.0f, h = texSizeX[i] * 60 / 500.0f / texSizeY[i];
                } else {
                    marginOnSides = false;
                    w = texSizeY[i] * 500 / 60.0f / texSizeX[i];
                    h = 1.0f;
                }
                thumbnails[i].bind(0);
                /*
                if (mouseon == i)
                    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
                else
                    glColor4f(0.8f, 0.8f, 0.8f, 0.9f);
                glBegin(GL_QUADS);
                glTexCoord2f(0.5f - w / 2, 0.5f + h / 2), GUI::UIVertex(midp - 250, top + i * itemHeight + borderWidth);
                glTexCoord2f(0.5f - w / 2, 0.5f - h / 2),
                    GUI::UIVertex(midp - 250, top + (i + 1) * itemHeight - borderWidth);
                glTexCoord2f(0.5f + w / 2, 0.5f - h / 2),
                    GUI::UIVertex(midp + 250, top + (i + 1) * itemHeight - borderWidth);
                glTexCoord2f(0.5f + w / 2, 0.5f + h / 2), GUI::UIVertex(midp + 250, top + i * itemHeight + borderWidth);
                glEnd();
                */
            }
            GUI::UIRenderString(
                midp - 250,
                midp + 250,
                top + i * itemHeight,
                top + (i + 1) * itemHeight,
                worldnames[i],
                true
            );
        }
        int i = (int) thumbnails.size();
        /*
        glDisable(GL_TEXTURE_2D);
        glBegin(GL_QUADS);
        if (mouseon == i)
            glColor4f(0.5f, 0.5f, 0.5f, GUI::FgA);
        else
            glColor4f(GUI::FgR, GUI::FgG, GUI::FgB, GUI::FgA);
        GUI::UIVertex(midp - 250, top + i * itemHeight + borderWidth);
        GUI::UIVertex(midp - 250, top + (i + 1) * itemHeight - borderWidth);
        GUI::UIVertex(midp + 250, top + (i + 1) * itemHeight - borderWidth);
        GUI::UIVertex(midp + 250, top + i * itemHeight + borderWidth);
        glEnd();
        glBegin(GL_LINE_LOOP);
        glColor4f(GUI::FgR * 0.9f, GUI::FgG * 0.9f, GUI::FgB * 0.9f, 0.9f);
        GUI::UIVertex(midp - 250, top + i * itemHeight + borderWidth);
        GUI::UIVertex(midp - 250, top + (i + 1) * itemHeight - borderWidth);
        GUI::UIVertex(midp + 250, top + (i + 1) * itemHeight - borderWidth);
        GUI::UIVertex(midp + 250, top + i * itemHeight + borderWidth);
        glEnd();
        glEnable(GL_TEXTURE_2D);
        */
        GUI::UIRenderString(
            midp - 250,
            midp + 250,
            top + i * itemHeight,
            top + (i + 1) * itemHeight,
            GetStrbyKey("NEWorld.worlds.new"),
            true
        );
        glDisable(GL_SCISSOR_TEST);
    }
};

void worldmenu() {
    WorldMenu Menu;
    Menu.start();
}
}
