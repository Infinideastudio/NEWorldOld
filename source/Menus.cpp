#include "Menus.h"
#include "World.h"
#include "WorldGen.h"
#include "Player.h"
#include "Textures.h"
#include "TextRenderer.h"
#include "utils/filesystem.h"

string BoolYesNo(bool b)
{
    return b ? "是" : "否";
}
string BoolEnabled(bool b)
{
    return b ? "开启" : "关闭";
}

extern bool gamebegin;
extern void saveoptions();

template<typename T>
string strWithVar(string str, T var)
{
    std::stringstream ss;
    ss << str << var;
    return ss.str();
}

class MainMenu : public gui::Form
{
private:
    gui::imagebox title;
    gui::button startbtn, optionsbtn, quitbtn;
    void onLoad()
    {
        title = gui::imagebox(0.0f, 1.0f, 0.5f, 1.0f, tex_title, -256, 256, 20, 276, 0.5, 0.5, 0.0, 0.0);
        startbtn = gui::button("开始游戏", -200, 200, 280, 312, 0.5, 0.5, 0.0, 0.0);
        optionsbtn = gui::button(">> 选项...", -200, -3, 318, 352, 0.5, 0.5, 0.0, 0.0);
        quitbtn = gui::button("退出", 3, 200, 318, 352, 0.5, 0.5, 0.0, 0.0);
        registerControls(title, startbtn, optionsbtn, quitbtn);
    }
    void onUpdate()
    {
        if (startbtn.clicked)
        {
            worldmenu();
        }

        if (gamebegin)
        {
            ExitSignal = true;
        }

        if (optionsbtn.clicked)
        {
            options();
        }

        if (quitbtn.clicked)
        {
            exit(0);
        }
    }
};
void mainmenu()
{
    MainMenu Menu;
    Menu.start();
}

class OptionsMenu : public gui::Form
{
private:
    gui::label title;
    gui::trackbar FOVyBar, mmsBar, viewdistBar;
    gui::button rdstbtn, gistbtn, backbtn, savebtn;
    void onLoad()
    {
        title = gui::label("=================<  选 项  >=================", -225, 225, 20, 36, 0.5, 0.5, 0.0, 0.0);
        FOVyBar = gui::trackbar(strWithVar("视野角度：", FOVyNormal), 120, (int)(FOVyNormal - 1), -250, -10, 60, 84, 0.5, 0.5, 0.0, 0.0);
        mmsBar = gui::trackbar(strWithVar("鼠标灵敏度：", mousemove), 120, (int)(mousemove * 40 * 2 - 1), 10, 250, 60, 84, 0.5, 0.5, 0.0, 0.0);
        viewdistBar = gui::trackbar(strWithVar("渲染距离：", viewdistance), 120, (viewdistance - 2) * 4 - 1, -250, -10, 96, 120, 0.5, 0.5, 0.0, 0.0);
        rdstbtn = gui::button(">> 渲染选项...", -250, -10, 204, 228, 0.5, 0.5, 0.0, 0.0);
        gistbtn = gui::button(">> 图形界面选项...", 10, 250, 204, 228, 0.5, 0.5, 0.0, 0.0);
        backbtn = gui::button("<< 返回主菜单", -250, -10, -44, -20, 0.5, 0.5, 1.0, 1.0);
        savebtn = gui::button("保存设置", 10, 250, -44, -20, 0.5, 0.5, 1.0, 1.0);
        registerControls(title, FOVyBar, mmsBar, viewdistBar, rdstbtn, gistbtn, backbtn, savebtn);
    }
    void onUpdate()
    {
        FOVyNormal = (float)(FOVyBar.barpos + 1);
        mousemove = (mmsBar.barpos / 2 + 1) / 40.0f;
        viewdistance = (viewdistBar.barpos + 1) / 4 + 2;

        if (rdstbtn.clicked)
        {
            Renderoptions();
        }

        if (gistbtn.clicked)
        {
            GUIoptions();
        }

        if (backbtn.clicked)
        {
            ExitSignal = true;
        }

        if (savebtn.clicked)
        {
            saveoptions();
        }

        FOVyBar.text = strWithVar("视野角度：", FOVyNormal);
        mmsBar.text = strWithVar("鼠标灵敏度：", mousemove);
        viewdistBar.text = strWithVar("渲染距离：", viewdistance);
    }
};
void options()
{
    OptionsMenu Menu;
    Menu.start();
}

class RenderOptionsMenu : public gui::Form
{
private:
    gui::label title;
    gui::button smoothlightingbtn, fancygrassbtn, mergefacebtn, backbtn;
    void onLoad()
    {
        title = gui::label("==============<  渲 染 选 项  >==============", -225, 225, 20, 36, 0.5, 0.5, 0.0, 0.0);
        smoothlightingbtn = gui::button("平滑光照：", -250, -10, 60, 84, 0.5, 0.5, 0.0, 0.0);
        fancygrassbtn = gui::button("草方块材质连接：", 10, 250, 60, 84, 0.5, 0.5, 0.0, 0.0);
        mergefacebtn = gui::button("合并面渲染：", -250, -10, 96, 120, 0.5, 0.5, 0.0, 0.0);
        backbtn = gui::button("<< 返回选项菜单", -250, 250, -44, -20, 0.5, 0.5, 1.0, 1.0);
        registerControls(title, smoothlightingbtn, fancygrassbtn, mergefacebtn, backbtn);

        if (MergeFace)
        {
            SmoothLighting = smoothlightingbtn.enabled = NiceGrass = fancygrassbtn.enabled = false;
        }
    }
    void onUpdate()
    {
        if (smoothlightingbtn.clicked)
        {
            SmoothLighting = !SmoothLighting;
        }

        if (fancygrassbtn.clicked)
        {
            NiceGrass = !NiceGrass;
        }

        if (mergefacebtn.clicked)
        {
            MergeFace = !MergeFace;

            if (MergeFace)
            {
                SmoothLighting = smoothlightingbtn.enabled = NiceGrass = fancygrassbtn.enabled = false;
            }
            else
            {
                SmoothLighting = smoothlightingbtn.enabled = NiceGrass = fancygrassbtn.enabled = true;
            }
        }

        if (backbtn.clicked)
        {
            ExitSignal = true;
        }

        smoothlightingbtn.text = "平滑光照：" + BoolEnabled(SmoothLighting);
        fancygrassbtn.text = "草方块材质连接：" + BoolEnabled(NiceGrass);
        mergefacebtn.text = "合并面渲染：" + BoolEnabled(MergeFace);
    }
};
void Renderoptions()
{
    RenderOptionsMenu Menu;
    Menu.start();
}

class GUIOptionsMenu : public gui::Form
{
private:
    gui::label title;
    gui::button fontbtn, blurbtn, backbtn;
    void onLoad()
    {
        title = gui::label(locale["neworld.menus.guioptions.caption"], -225, 225, 20, 36, 0.5, 0.5, 0.0, 0.0);
        fontbtn = gui::button(locale["neworld.menus.guioptions.font"], -250, -10, 60, 84, 0.5, 0.5, 0.0, 0.0);
        blurbtn = gui::button(locale["neworld.menus.guioptions.blur"], 10, 250, 60, 84, 0.5, 0.5, 0.0, 0.0);
        backbtn = gui::button(locale["neworld.menus.guioptions.back"], -250, 250, -44, -20, 0.5, 0.5, 1.0, 1.0);
        registerControls(title, fontbtn, blurbtn, backbtn);
    }
    void onUpdate()
    {
        if (fontbtn.clicked)
        {
            TextRenderer::useUnicodeASCIIFont = !TextRenderer::useUnicodeASCIIFont;
        }

        if (blurbtn.clicked)
        {
            GUIScreenBlur = !GUIScreenBlur;
        }

        if (backbtn.clicked)
        {
            ExitSignal = true;
        }

        fontbtn.text = locale["neworld.menus.guioptions.caption"] + BoolYesNo(TextRenderer::useUnicodeASCIIFont);
        blurbtn.text = locale["neworld.menus.guioptions.blur"] + BoolEnabled(GUIScreenBlur);
    }
};
void GUIoptions()
{
    GUIOptionsMenu Menu;
    Menu.start();
}

class WorldMenu : public gui::Form
{
private:
    int leftp = windowwidth / 2 - 200;
    int midp = windowwidth / 2;
    int rightp = windowwidth / 2 + 200;
    int downp = windowheight - 20;
    bool refresh = true;
    int selected = 0, mouseon;
    int worldcount;
    string chosenWorldName;
    vector<string> worldnames;
    vector<TextureID> thumbnails, texSizeX, texSizeY;
    int trs = 0;
    gui::label title;
    gui::vscroll vscroll;
    gui::button enterbtn, deletebtn, backbtn;
    void onLoad()
    {
        title = gui::label(locale["neworld.menus.world.caption"], -225, 225, 20, 36, 0.5, 0.5, 0.0, 0.0);
        vscroll = gui::vscroll(100, 0, 275, 295, 36, -20, 0.5, 0.5, 0.0, 1.0);
        enterbtn = gui::button(locale["neworld.menus.world.enter"], -250, -10, -80, -56, 0.5, 0.5, 1.0, 1.0);
        deletebtn = gui::button(locale["neworld.menus.world.delete"], 10, 250, -80, -56, 0.5, 0.5, 1.0, 1.0);
        backbtn = gui::button(locale["neworld.menus.world.back"], -250, 250, -44, -20, 0.5, 0.5, 1.0, 1.0);
        registerControls(title, vscroll, enterbtn, deletebtn, backbtn);
        world::worldname = "";
        enterbtn.enabled = false;
        deletebtn.enabled = false;
        vscroll.defaultv = true;
    }
    void onUpdate()
    {
        worldcount = (int)worldnames.size();
        leftp = windowwidth / 2 - 250;
        midp = windowwidth / 2;
        rightp = windowwidth / 2 + 250;
        downp = windowheight - 20;

        vscroll.barheight = (downp - 72 - 48) * (downp - 36 - 40) / (64 * worldcount + 64);

        if (vscroll.barheight > downp - 36 - 40)
        {
            vscroll.enabled = false;
            vscroll.barheight = downp - 36 - 40;
        }
        else
        {
            vscroll.enabled = true;
        }

        trs = vscroll.barpos * (64 * worldcount + 64) / (downp - 36 - 40);
        mouseon = -1;

        if (mx >= midp - 250 && mx <= midp + 250 && my >= 48 && my <= downp - 72)
        {
            for (int i = 0; i < worldcount; i++)
            {
                if (my >= 48 + i * 64 - trs && my <= 48 + i * 64 + 60 - trs)
                {
                    if (mb == 1 && mbl == 0)
                    {
                        chosenWorldName = worldnames[i];
                        selected = i;
                    }

                    mouseon = i;
                }
            }

            if (my >= 48 + worldcount * 64 - trs && my <= 48 + worldcount * 64 + 60 - trs)
            {
                if (mb == 0 && mbl == 1)
                {
                    createworldmenu();
                    refresh = true;
                }

                mouseon = worldcount;
            }
        }

        if (enterbtn.clicked)
        {
            gamebegin = true;
            world::worldname = chosenWorldName;
			player::load(world::worldname);
			printf("[xxx]:WorldSeed:%d\n", WorldGen::seed);
        }

        if (deletebtn.clicked)
        {
            //删除世界文件
            system((string("rd /s/q Worlds\\") + chosenWorldName).c_str());
            deletebtn.clicked = false;
            world::worldname = "";
            enterbtn.enabled = false;
            deletebtn.enabled = false;
            refresh = true;
        }

        if (refresh)
        {
            worldnames.clear();
            thumbnails.clear();
            texSizeX.clear();
            texSizeY.clear();
            worldcount = 0;
            selected = -1;
            mouseon = -1;
            vscroll.barpos = 0;
            chosenWorldName = "";
            //查找所有世界存档
            FileSystem::forInDirectory("./Worlds", [this](bool isDir, std::string path)
            {
                if (isDir && path != "." && path != "..")
                {
                    Textures::TEXTURE_RGB tmb;
                    worldnames.push_back(path);
                    std::fstream file;
                    file.open((std::string("./Worlds") + path + std::string("/Thumbnail.bmp")).c_str(), std::ios::in);
                    thumbnails.push_back(0);
                    texSizeX.push_back(0);
                    texSizeY.push_back(0);

                    if (file.is_open())
                    {
                        Textures::LoadRGBImage(tmb, std::string("./Worlds") + path + std::string("/Thumbnail.bmp"));
                        glGenTextures(1, &thumbnails[thumbnails.size() - 1]);
                        glBindTexture(GL_TEXTURE_2D, thumbnails[thumbnails.size() - 1]);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, tmb.sizeX, tmb.sizeY, 0, GL_RGB, GL_UNSIGNED_BYTE,
                                     tmb.buffer.get());
                        texSizeX[texSizeX.size() - 1] = tmb.sizeX;
                        texSizeY[texSizeY.size() - 1] = tmb.sizeY;
                    }

                    file.close();
                }
            });

            refresh = false;
        }

        enterbtn.enabled = chosenWorldName != "";
        deletebtn.enabled = chosenWorldName != "";

        if (backbtn.clicked)
        {
            ExitSignal = true;
        }

        if (gamebegin)
        {
            ExitSignal = true;
        }
    }
    void onRender()
    {
        glEnable(GL_SCISSOR_TEST);
        glScissor(0, windowheight - (downp - 72), windowwidth, downp - 72 - 48 + 1);
        glTranslatef(0.0f, (float) - trs, 0.0f);

        for (int i = 0; i < worldcount; i++)
        {
            int xmin, xmax, ymin, ymax;
            xmin = midp - 250, xmax = midp + 250;
            ymin = 48 + i * 64, ymax = 48 + i * 64 + 60;

            if (thumbnails[i] == -1)
            {
                glDisable(GL_TEXTURE_2D);

                if (mouseon == i)
                {
                    glColor4f(0.5, 0.5, 0.5, gui::FgA);
                }
                else
                {
                    glColor4f(gui::FgR, gui::FgG, gui::FgB, gui::FgA);
                }

                glBegin(GL_QUADS);
                glVertex2i(midp - 250, 48 + i * 64);
                glVertex2i(midp + 250, 48 + i * 64);
                glVertex2i(midp + 250, 48 + i * 64 + 60);
                glVertex2i(midp - 250, 48 + i * 64 + 60);
                glEnd();
            }
            else
            {
                bool marginOnSides;
                float w, h;

                //计算材质坐标，保持高宽比（按钮大小为500x60），有小学数学基础的人仔细想一想应该能懂QAQ
                if (texSizeX[i] * 60 / 500 < texSizeY[i])
                {
                    marginOnSides = true;
                    w = 1.0f, h = texSizeX[i] * 60 / 500.0f / texSizeY[i];
                }
                else
                {
                    marginOnSides = false;
                    w = texSizeY[i] * 500 / 60.0f / texSizeX[i];
                    h = 1.0f;
                }

                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, thumbnails[i]);

                if (mouseon == (int)i)
                {
                    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
                }
                else
                {
                    glColor4f(0.8f, 0.8f, 0.8f, 0.9f);
                }

                glBegin(GL_QUADS);
                glTexCoord2f(0.5f - w / 2, 0.5f + h / 2), glVertex2i(midp - 250, 48 + i * 64);
                glTexCoord2f(0.5f + w / 2, 0.5f + h / 2), glVertex2i(midp + 250, 48 + i * 64);
                glTexCoord2f(0.5f + w / 2, 0.5f - h / 2), glVertex2i(midp + 250, 48 + i * 64 + 60);
                glTexCoord2f(0.5f - w / 2, 0.5f - h / 2), glVertex2i(midp - 250, 48 + i * 64 + 60);
                glEnd();
            }

            glColor4f(gui::FgR * 0.9f, gui::FgG * 0.9f, gui::FgB * 0.9f, 0.9f);
            glDisable(GL_TEXTURE_2D);
            glLineWidth(1.0);
            glBegin(GL_LINE_LOOP);
            glVertex2i(xmin, ymin);
            glVertex2i(xmin, ymax);
            glVertex2i(xmax, ymax);
            glVertex2i(xmax, ymin);
            glEnd();

            if (selected == (int)i)
            {
                glLineWidth(2.0);
                glColor4f(0.0, 0.0, 0.0, 1.0);
                glBegin(GL_LINE_LOOP);
                glVertex2i(midp - 250, 48 + i * 64);
                glVertex2i(midp + 250, 48 + i * 64);
                glVertex2i(midp + 250, 48 + i * 64 + 60);
                glVertex2i(midp - 250, 48 + i * 64 + 60);
                glEnd();
            }

            TextRenderer::renderString((windowwidth - TextRenderer::getStrWidth(worldnames[i])) / 2, (140 + i * 128) / 2, worldnames[i]);
        }

        int i = worldcount;
        glDisable(GL_TEXTURE_2D);

        if (mouseon == i)
        {
            glColor4f(0.5f, 0.5f, 0.5f, gui::FgA);
        }
        else
        {
            glColor4f(gui::FgR, gui::FgG, gui::FgB, gui::FgA);
        }

        glBegin(GL_QUADS);
        glVertex2i(midp - 250, 48 + i * 64);
        glVertex2i(midp + 250, 48 + i * 64);
        glVertex2i(midp + 250, 48 + i * 64 + 60);
        glVertex2i(midp - 250, 48 + i * 64 + 60);
        glEnd();
        glColor4f(gui::FgR * 0.9f, gui::FgG * 0.9f, gui::FgB * 0.9f, 0.9f);
        glDisable(GL_TEXTURE_2D);
        glLineWidth(1.0);
        glBegin(GL_LINE_LOOP);
        glVertex2i(midp - 250, 48 + i * 64);
        glVertex2i(midp + 250, 48 + i * 64);
        glVertex2i(midp + 250, 48 + i * 64 + 60);
        glVertex2i(midp - 250, 48 + i * 64 + 60);
        glEnd();
        TextRenderer::renderString((windowwidth - TextRenderer::getStrWidth(locale["neworld.menus.world.new"])) / 2, (140 + i * 128) / 2, ">>创建新的世界");
        glDisable(GL_SCISSOR_TEST);
    }
};
void worldmenu()
{
    WorldMenu Menu;
    Menu.start();
}

class CreateWorldMenu : public gui::Form
{
private:
	bool worldnametbChanged, worldseedtbChanged;
    gui::label title;
	gui::textbox worldnametb, worldseedtb;
    gui::button okbtn, backbtn;
    void onLoad()
    {
        title = gui::label(locale["neworld.menus.newworld.new"], -225, 225, 20, 36, 0.5, 0.5, 0.0, 0.0);
        worldnametb = gui::textbox(locale["neworld.menus.newworld.name"], -250, 250, 48, 72, 0.5, 0.5, 0.0, 0.0);
        okbtn = gui::button(locale["neworld.menus.newworld.ok"], -250, 250, 84, 120, 0.5, 0.5, 0.0, 0.0);
        backbtn = gui::button(locale["neworld.menus.newworld.back"], -250, 250, -44, -20, 0.5, 0.5, 1.0, 1.0);
        registerControls(title, worldnametb, okbtn, backbtn);
        inputstr = "";
        okbtn.enabled = false;
        worldnametbChanged = false;
		worldseedtbChanged = false;
    }
    void onUpdate()
    {
        if (worldnametb.pressed && !worldnametbChanged)
        {
            worldnametb.text = "";
            worldnametbChanged = true;
        }
		if (worldseedtb.pressed&&!worldseedtbChanged) {
			worldseedtb.text = "";
			worldseedtbChanged = true;
		}
        if (worldnametb.text == "" || !worldnametbChanged || worldnametb.text.find(" ") != -1)
        {
            okbtn.enabled = false;
        }
        else
        {
            okbtn.enabled = true;
        }

        if (okbtn.clicked)
        {
            if (worldnametb.text != "")
            {
                world::worldname = worldnametb.text;
				if (worldseedtb.text != "")
					WorldGen::seed = atoi(worldseedtb.text.c_str());
				else
					WorldGen::seed = 3404;
                gamebegin = true;
            }

            ExitSignal = true;
        }

        if (backbtn.clicked)
        {
            ExitSignal = true;
        }

        inputstr = "";
    }
};
void createworldmenu()
{
    CreateWorldMenu Menu;
    Menu.start();
}

class GameMenu : public gui::Form
{
private:
    gui::label title;
    gui::button resumebtn, exitbtn;
    void onLoad()
    {
        title = gui::label(locale["neworld.menus.game.caption"], -225, 225, 0, 16, 0.5, 0.5, 0.25, 0.25);
        resumebtn = gui::button(locale["neworld.menus.game.resume"], -200, 200, -35, -3, 0.5, 0.5, 0.5, 0.5);
        exitbtn = gui::button(locale["neworld.menus.game.back"], -200, 200, 3, 35, 0.5, 0.5, 0.5, 0.5);
        registerControls(title, resumebtn, exitbtn);
    }
    void onUpdate()
    {
        Mutex.unlock();
        //Make update thread realize that it should pause
        Mutex.lock();

        if (resumebtn.clicked)
        {
            ExitSignal = true;
        }

        if (exitbtn.clicked)
        {
			player::save(world::worldname);
            gameexit = ExitSignal = true;
        }
    }
};
void gamemenu()
{
    GameMenu Menu;
    Menu.start();
}
