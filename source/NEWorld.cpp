//==============================   Initialize   ================================//
//==============================初始化(包括闪屏)================================//

#include "Definitions.h"
#include "utils/jsonhelper.h"
void WindowSizeFunc(GLFWwindow *win, int width, int height);
void MouseButtonFunc(GLFWwindow *, int button, int action, int);
void CharInputFunc(GLFWwindow *, unsigned int c);
void MouseScrollFunc(GLFWwindow *, double, double yoffset);
void splashscreen();
void setupscreen();
void InitGL();
//void glPrintInfoLog(GLhandleARB obj);
void setupNormalFog();
void LoadTextures();
void loadGame();
void saveGame();
void updateThreadFunc();
void drawCloud(double px, double pz);
void updategame();
void debugText(string s, bool init = false);
void Render();
void drawBorder(int x, int y, int z);
void renderDestroy(float level, int x, int y, int z);
void drawGUI();
void drawBagRow(int row, int itemid, int xbase, int ybase, int spac, float alpha);
void drawBag();
void saveScreenshot(int x, int y, int w, int h, string filename);
void createThumbnail();
void loadoptions();
void saveoptions();
int getMouseScroll()
{
    return mw;
}
int getMouseButton()
{
    return mb;
}

#include "Blocks.h"
#include "Textures.h"
#include "Renderer.h"
#include "TextRenderer.h"
#include "Player.h"
#include "WorldGen.h"
#include "World.h"
#include "Particles.h"
#include "Hitbox.h"
#include "GUI.h"
#include "Menus.h"
#include "Frustum.h"
#include "utils/filesystem.h"
#include "utils/jsonhelper.h"

struct RenderChunk
{
    RenderChunk(world::chunk *c, double TimeDelta)
    {
        cx = c->cx;
        cy = c->cy;
        cz = c->cz;
        memcpy(vbuffers, c->vbuffer, 3 * sizeof(VBOID));
        memcpy(vtxs, c->vertexes, 3 * sizeof(vtxCount));
        loadAnim = c->loadAnim * pow(0.6, TimeDelta);
    }
    int cx, cy, cz;
    vtxCount vtxs[3];
    VBOID vbuffers[3];
    double loadAnim;
};

int fps, fpsc, ups, upsc;
double fctime, uctime;
vector<RenderChunk> displayChunks;

bool GUIrenderswitch;
bool DebugMode;
bool DebugHitbox;
bool DebugChunk;
bool DebugPerformance;

int selx, sely, selz, oldselx, oldsely, oldselz, selface;
float selt, seldes;
block selb;
brightness selbr;
bool selce;
int selbx, selby, selbz, selcx, selcy, selcz;

bool updateThreadRun = false, updateThreadPaused;
std::thread updateThread;
#if 0
woca, 这样注释都行？！
(这儿编译不过去的童鞋，你的FB编译器版本貌似和我的不一样，把这几行注释掉吧。。。)
== == == == == == == == == == == == == == == == == == == =
    等等不对啊！！！明明都改成c++了。。。还说是FB。。。
    正常点的C++编译器都应该不会在这儿报错吧23333333
#endif

    //==============================  Main Program  ================================//
    //==============================     主程序     ================================//
int main()
{
    //终于进入main函数了！激动人心的一刻！！！

    locale.setActiveLang("zh_Hans");
    setlocale(LC_ALL, "zh_CN.UTF-8");

    loadoptions();
    
    FileSystem::createDirectory("./Configs");
    FileSystem::createDirectory("./Worlds");
    FileSystem::createDirectory("./Screenshots");

    windowwidth = defaultwindowwidth;
    windowheight = defaultwindowheight;
    cout << "[Console][Event]Initialize GLFW" << (glfwInit() == 1 ? "" : " - Failed!") << endl;
    std::stringstream title;
    title << "NEWorld " << MAJOR_VERSION << MINOR_VERSION << EXT_VERSION;
    MainWindow = glfwCreateWindow(windowwidth, windowheight, title.str().c_str(), nullptr, nullptr);
    MouseCursor = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
    glfwMakeContextCurrent(MainWindow);
    InitGL();
    glfwSetCursor(MainWindow, MouseCursor);
    glfwSetInputMode(MainWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    glfwSetWindowSizeCallback(MainWindow, &WindowSizeFunc);
    glfwSetMouseButtonCallback(MainWindow, &MouseButtonFunc);
    glfwSetScrollCallback(MainWindow, &MouseScrollFunc);
    glfwSetCharCallback(MainWindow, &CharInputFunc);
    setupscreen();
    glDisable(GL_CULL_FACE);
    splashscreen();
    LoadTextures();
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

main_menu:
    gamebegin = gameexit = false;
    glDisable(GL_LINE_SMOOTH);
    gui::clearTransition();
    mainmenu();
    glEnable(GL_LINE_SMOOTH);
    glEnable(GL_TEXTURE_2D);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glfwSwapBuffers(MainWindow);
    glfwPollEvents();

    Mutex.lock();
    updateThreadRun = false;
    updateThread = std::move(std::thread(updateThreadFunc));

    //初始化游戏状态
    printf("[Console][Game]");
    printf("Init player...\n");
    player::InitHitbox();
    player::xpos = 0.0;
	player::ypos = (WorldGen::getHeight(player::xpos, player::zpos) + 2);
    player::zpos = 0.0;
    memset(player::inventorybox, 0, sizeof(player::inventorybox));
    memset(player::inventorypcs, 0, sizeof(player::inventorypcs));
    player::inventorybox[0][0] = 1;
    player::inventorypcs[0][0] = 255;
    player::inventorybox[0][1] = 2;
    player::inventorypcs[0][1] = 255;
    player::inventorybox[0][2] = 3;
    player::inventorypcs[0][2] = 255;
    player::inventorybox[0][3] = 4;
    player::inventorypcs[0][3] = 255;
    player::inventorybox[0][4] = 5;
    player::inventorypcs[0][4] = 255;
    player::inventorybox[0][5] = 6;
    player::inventorypcs[0][5] = 255;
    player::inventorybox[0][6] = 7;
    player::inventorypcs[0][6] = 255;
    player::inventorybox[0][7] = 8;
    player::inventorypcs[0][7] = 255;
    player::inventorybox[0][8] = 9;
    player::inventorypcs[0][8] = 255;
    player::inventorybox[0][9] = 10;
    player::inventorypcs[0][9] = 255;
    player::inventorybox[1][0] = 11;
    player::inventorypcs[1][0] = 255;
    player::inventorybox[1][1] = 12;
    player::inventorypcs[1][1] = 255;
    player::inventorybox[1][2] = 13;
    player::inventorypcs[1][2] = 255;
    player::inventorybox[1][3] = 14;
    player::inventorypcs[1][3] = 255;
    player::inventorybox[1][4] = 15;
    player::inventorypcs[1][4] = 255;
    player::inventorybox[1][5] = 16;
    player::inventorypcs[1][5] = 255;
    player::inventorybox[1][6] = 17;
    player::inventorypcs[1][6] = 255;
    loadGame();
    player::MoveHitboxToPosition();
    player::InitPosition();
    printf("[Console][Game]");
    printf("Init world...\n");
    world::Init();
    GUIrenderswitch = true;
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE);
    setupNormalFog();

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glfwSwapBuffers(MainWindow);
    glfwPollEvents();
    printf("[Console][Game]");
    printf("Game start!\n");

    //这才是游戏开始!
    glfwSetInputMode(MainWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    mxl = mx;
    myl = my;
    printf("[Console][Game]");
    printf("Main loop started\n");
    updateThreadRun = true;
    updateThreadPaused = false;
    fctime = uctime = lastupdate = timer();

    do
    {
        //主循环，被简化成这样，惨不忍睹啊！

        Mutex.unlock();
        Mutex.lock();

        Render();

        if (glfwGetKey(MainWindow, GLFW_KEY_ESCAPE) == 1)
        {
            updateThreadPaused = true;
            createThumbnail();
            gui::clearTransition();
            gamemenu();

            if (!gameexit)
            {
                glfwSetInputMode(MainWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                glDepthFunc(GL_LEQUAL);
                glEnable(GL_CULL_FACE);
                setupNormalFog();
                glfwGetCursorPos(MainWindow, &mx, &my);
                mxl = mx;
                myl = my;
            }

            updateThreadPaused = false;
        }

        if (gameexit)
        {
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            TextRenderer::setFontColor(1.0, 1.0, 1.0, 1.0);
            TextRenderer::renderString(0, 0, "Saving world...");
            glfwSwapBuffers(MainWindow);
            glfwPollEvents();
            printf("[Console][Game]");
            printf("Terminate threads\n");
            updateThreadRun = false;
            Mutex.unlock();
            updateThread.join();
            saveGame();
            world::destroyAllChunks();
            printf("[Console][Game]");
            printf("Threads terminated\n");
            printf("[Console][Game]");
            printf("Returned to main menu\n");
            goto main_menu;
        }

    }
    while (!glfwWindowShouldClose(MainWindow));

    saveGame();

    updateThreadRun = false;
    Mutex.unlock();
    updateThread.join();

    //结束程序，删了也没关系 ←_←（吐槽FB和glfw中）
    //不对啊这不是FB！！！这是正宗的VC++！！！！！！
    //楼上的楼上在瞎说！！！别信他的！！！
    glfwTerminate();
    saveoptions();
    return 0;
    //This is the END of the program!
}
void updateThreadFunc()
{

    //Wait until start...
    Mutex.lock();

    while (!updateThreadRun)
    {
        Mutex.unlock();
        std::this_thread::yield();
        Mutex.lock();
    }

    Mutex.unlock();

    //Thread start
    Mutex.lock();
    lastupdate = timer();

    while (updateThreadRun)
    {

        Mutex.unlock();
        std::this_thread::yield(); //Don't make it always busy
        Mutex.lock();

        while (updateThreadPaused)
        {
            Mutex.unlock();
            std::this_thread::yield(); //Same as before
            Mutex.lock();
            lastupdate = updateTimer = timer();
        }

        FirstUpdateThisFrame = true;
        updateTimer = timer();

        if (updateTimer - lastupdate >= 5.0)
        {
            lastupdate = updateTimer;
        }

        while ((updateTimer - lastupdate) >= 1.0 / 30.0 && upsc < 60)
        {
            lastupdate += 1.0 / 30.0;
            upsc++;
            updategame();
            FirstUpdateThisFrame = false;
        }

        if ((timer() - uctime) >= 1.0)
        {
            uctime = timer();
            ups = upsc;
            upsc = 0;
        }

    }

    Mutex.unlock();
}
/*void updateThreadFunc()
{
    using namespace std::chrono;
    using namespace std::chrono_literals;
    steady_clock::time_point lastSecond = steady_clock::now(), next;

    while (!updateThreadRun)
    {
        std::this_thread::yield(); //Same as before
    }

    while (updateThreadRun)
    {
        Mutex.lock();

        while (updateThreadPaused)
        {
            Mutex.unlock();
            std::this_thread::yield(); //Same as before
            Mutex.lock();
            lastupdate = updateTimer = timer();
        }

        FirstUpdateThisFrame = true;
        updateTimer = timer();

        next = steady_clock::now() + 33ms;
        updategame();
        Mutex.unlock();

        if (steady_clock::now() - lastSecond >= 1s)
        {
            ups = upsc;
            upsc = 0;
        }
        else
        {
            upsc++;
        }
        std::this_thread::sleep_until(next);
    }

}*/

void WindowSizeFunc(GLFWwindow *win, int width, int height)
{
    width = std::max(width, 640);
    height = std::max(height, 400);
    windowwidth = width;
    windowheight = height > 0 ? height : 1;
    glfwSetWindowSize(win, width, height);
    setupscreen();
}

void MouseButtonFunc(GLFWwindow *, int button, int action, int)
{
    mb = 0;

    if (action == GLFW_PRESS)
    {
        if (button == GLFW_MOUSE_BUTTON_LEFT)
        {
            mb += 1;
        }

        if (button == GLFW_MOUSE_BUTTON_RIGHT)
        {
            mb += 2;
        }

        if (button == GLFW_MOUSE_BUTTON_MIDDLE)
        {
            mb += 4;
        }
    }
    else
    {
        mb = 0;
    }
}

void CharInputFunc(GLFWwindow *, unsigned int c)
{
    inputstr += locale.w2cUtf8(std::wstring(1, static_cast<wchar_t>(c)));
}

void MouseScrollFunc(GLFWwindow *, double, double yoffset)
{
    mw += (int)yoffset;
}

void splashscreen()
{
    TextureID splTex = Textures::LoadRGBTexture("Textures/GUI/splashscreen.bmp");
    glEnable(GL_TEXTURE_2D);

    for (int i = 0; i < 256; i += 2)
    {
        glfwSwapBuffers(MainWindow);
        glfwPollEvents();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glBindTexture(GL_TEXTURE_2D, splTex);
        glColor4f((float)i / 256, (float)i / 256, (float)i / 256, 1.0);
        glBegin(GL_QUADS);
        glTexCoord2f(0.0, 1.0);
        glVertex2i(-1, 1);
        glTexCoord2f(850.0f / 1024.0f, 1.0);
        glVertex2i(1, 1);
        glTexCoord2f(850.0f / 1024.0f, 1.0 - 480.0f / 1024.0f);
        glVertex2i(1, -1);
        glTexCoord2f(0.0, 1.0 - 480.0f / 1024.0f);
        glVertex2i(-1, -1);
        glEnd();
        std::this_thread::sleep_for(std::chrono::microseconds(10));
    }

    glfwSwapBuffers(MainWindow);
    glfwPollEvents();
}

void setupscreen()
{

    //OpenGL参数设置
    glViewport(0, 0, windowwidth, windowheight);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glShadeModel(GL_SMOOTH);
    glDisable(GL_DITHER);
    glEnable(GL_CULL_FACE);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_ALPHA_TEST);
    glEnable(GL_BLEND);
    glEnable(GL_LINE_SMOOTH);
    glDepthFunc(GL_LEQUAL);
    glAlphaFunc(GL_GREATER, 0.0); //<--这家伙在卖萌？(往后面看看，卖萌的多着呢)
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
    glHint(GL_FOG_HINT, GL_FASTEST);
    glHint(GL_LINE_SMOOTH_HINT, GL_FASTEST);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glColor4f(0.0, 0.0, 0.0, 1.0);
    TextRenderer::BuildFont(windowwidth, windowheight);
    TextRenderer::setFontColor(1.0, 1.0, 1.0, 1.0);
    glClearColor(0.0, 0.0, 0.0, 1.0);
    glClearDepth(1.0);
}

void InitGL()
{
    //获取OpenGL版本
    GLVersionMajor = glfwGetWindowAttrib(MainWindow, GLFW_CONTEXT_VERSION_MAJOR);
    GLVersionMinor = glfwGetWindowAttrib(MainWindow, GLFW_CONTEXT_VERSION_MINOR);
    GLVersionRev = glfwGetWindowAttrib(MainWindow, GLFW_CONTEXT_REVISION);
    glewInit();
}

void setupNormalFog()
{
    float fogColor[4] = { skycolorR, skycolorG, skycolorB, 1.0f };
    glEnable(GL_FOG);
    glFogi(GL_FOG_MODE, GL_LINEAR);
    glFogfv(GL_FOG_COLOR, fogColor);
    glFogf(GL_FOG_START, viewdistance * 16.0f - 32.0f);
    glFogf(GL_FOG_END, viewdistance * 16.0f);
}

void LoadTextures()
{
    //载入纹理
    Textures::Init();

    tex_select = Textures::LoadRGBATexture("Textures/GUI/select.bmp", "");
    tex_unselect = Textures::LoadRGBATexture("Textures/GUI/unselect.bmp", "");
    tex_title = Textures::LoadRGBATexture("Textures/GUI/title.bmp", "Textures/GUI/titlemask.bmp");

    for (int i = 0; i < 6; i++)
    {
        std::stringstream ss;
        ss << "Textures/GUI/mainmenu" << i << ".bmp";
        tex_mainmenu[i] = Textures::LoadRGBTexture(ss.str());
    }

    DefaultSkin = Textures::LoadRGBATexture("Textures/Player/skin_xiaoqiao.bmp", "Textures/Player/skinmask_xiaoqiao.bmp");

    for (int gloop = 1; gloop <= 10; gloop++)
    {
        auto path = StringUtils::FormatString("./Textures/blocks/destroy_%d.bmp", gloop);
        DestroyImage[gloop] = Textures::LoadRGBATexture(path, path);
    }

    BlockTextures = Textures::LoadRGBATexture("Textures/blocks/Terrain.bmp", "Textures/blocks/Terrainmask.bmp");
    BlockTextures3D = Textures::LoadBlock3DTexture("Textures/blocks/Terrain3D.bmp", "Textures/blocks/Terrain3Dmask.bmp");

}

void saveGame()
{
    world::saveAllChunks();
}

void loadGame()
{
    player::load(world::worldname);
}

bool isPressed(int key, bool setFalse = false)
{
    static bool keyPressed[GLFW_KEY_LAST + 1];

    if (setFalse)
    {
        keyPressed[key] = false;
        return true;
    }

    if (key > GLFW_KEY_LAST || key <= 0)
    {
        return false;
    }

    if (!glfwGetKey(MainWindow, key))
    {
        keyPressed[key] = false;
    }

    if (!keyPressed[key] && glfwGetKey(MainWindow, key))
    {
        keyPressed[key] = true;
        return true;
    }

    return false;
}

void updategame()
{
    static double Wprstm;
    static bool WP;
    player::BlockInHand = player::inventorybox[3][player::itemInHand];

    //HeightMap move
    if (world::HMap.originX != (player::cxt - viewdistance - 2) * 16 || world::HMap.originZ != (player::czt - viewdistance - 2) * 16)
    {
        world::HMap.moveTo((player::cxt - viewdistance - 2) * 16, (player::czt - viewdistance - 2) * 16);
    }

    world::mWorld.tryLoadUnloadChunks(Vec3i(RoundInt(player::xpos), RoundInt(player::ypos), RoundInt(player::zpos)));

    //加载动画
    for (auto&& chk : world::mWorld)
    {
        if (chk.second.loadAnim <= 0.3f)
            chk.second.loadAnim = 0.0f;
        else
            chk.second.loadAnim *= 0.6f;
    }

    //随机状态更新
    for (auto&& chk : world::mWorld)
    {
        int x, y, z, gx, gy, gz;
        int cx = chk.second.cx;
        int cy = chk.second.cy;
        int cz = chk.second.cz;
        x = int(rnd() * 16);
        gx = x + cx * 16;
        y = int(rnd() * 16);
        gy = y + cy * 16;
        z = int(rnd() * 16);
        gz = z + cz * 16;

        if (chk.second.getblock(x, y, z) == blocks::DIRT &&
            world::getblock(gx, gy + 1, gz, blocks::NONEMPTY) == blocks::AIR && (
                world::getblock(gx + 1, gy, gz, blocks::AIR) == blocks::GRASS ||
                world::getblock(gx - 1, gy, gz, blocks::AIR) == blocks::GRASS ||
                world::getblock(gx, gy, gz + 1, blocks::AIR) == blocks::GRASS ||
                world::getblock(gx, gy, gz - 1, blocks::AIR) == blocks::GRASS ||
                world::getblock(gx + 1, gy + 1, gz, blocks::AIR) == blocks::GRASS ||
                world::getblock(gx - 1, gy + 1, gz, blocks::AIR) == blocks::GRASS ||
                world::getblock(gx, gy + 1, gz + 1, blocks::AIR) == blocks::GRASS ||
                world::getblock(gx, gy + 1, gz - 1, blocks::AIR) == blocks::GRASS ||
                world::getblock(gx + 1, gy - 1, gz, blocks::AIR) == blocks::GRASS ||
                world::getblock(gx - 1, gy - 1, gz, blocks::AIR) == blocks::GRASS ||
                world::getblock(gx, gy - 1, gz + 1, blocks::AIR) == blocks::GRASS ||
                world::getblock(gx, gy - 1, gz - 1, blocks::AIR) == blocks::GRASS))
        {
            //长草
            chk.second.setblock(x, y, z, blocks::GRASS);
            world::updateblock(x + cx * 16, y + cy * 16 + 1, z + cz * 16, true);
            world::setChunkUpdated(cx, cy, cz, true);
        }

        if (chk.second.getblock(x, y, z) == blocks::GRASS && world::getblock(gx, gy + 1, gz, blocks::AIR) != blocks::AIR)
        {
            //草被覆盖
            chk.second.setblock(x, y, z, blocks::DIRT);
            world::updateblock(x + cx * 16, y + cy * 16 + 1, z + cz * 16, true);
        }
    }

    //判断选中的方块
    double lx, ly, lz, sidedist[7];
    int sidedistmin;
    lx = player::xpos;
    ly = player::ypos + player::height + player::heightExt;
    lz = player::zpos;

    selx = sely = selz = selbx = selby = selbz = selcx = selcy = selcz = selb = selbr = 0;
    bool put = false;    //标准的chinglish吧。。。主要是put已经被FB作为关键字了。。。   --等等不对啊！这已经是c++了！！！   --所以我就改回来了   --23333所以我要声明一下原来是puted

    if (!bagOpened)
    {
        //从玩家位置发射一条线段
        for (int i = 0; i < selectPrecision * selectDistance; i++)
        {
            //线段延伸
            lx += sin(M_PI / 180 * (player::heading - 180)) * sin(M_PI / 180 * (player::lookupdown + 90)) / (double)selectPrecision;
            ly += cos(M_PI / 180 * (player::lookupdown + 90)) / (double)selectPrecision;
            lz += cos(M_PI / 180 * (player::heading - 180)) * sin(M_PI / 180 * (player::lookupdown + 90)) / (double)selectPrecision;

            //碰到方块
            if (BlockInfo(world::getblock(RoundInt(lx), RoundInt(ly), RoundInt(lz))).isSolid())
            {
                int x, y, z;
                x = RoundInt(lx);
                y = RoundInt(ly);
                z = RoundInt(lz);

                selx = x;
                sely = y;
                selz = z;

                //找方块所在区块及位置
                selcx = getchunkpos(x);
                selbx = getblockpos(x);
                selcy = getchunkpos(y);
                selby = getblockpos(y);
                selcz = getchunkpos(z);
                selbz = getblockpos(z);

                sidedist[1] = abs(y + 0.5 - ly);          //顶面
                sidedist[2] = abs(y - 0.5 - ly);          //底面
                sidedist[3] = abs(x + 0.5 - lx);          //左面
                sidedist[4] = abs(x - 0.5 - lx);          //右面
                sidedist[5] = abs(z + 0.5 - lz);          //前面
                sidedist[6] = abs(z - 0.5 - lz);          //后面
                sidedistmin = 1;                          //离哪个面最近

                for (int j = 2; j <= 6; j++)
                {
                    if (sidedist[j] < sidedist[sidedistmin])
                    {
                        sidedistmin = j;
                    }
                }

                {
                    world::chunk *cp = world::getChunkPtr(selcx, selcy, selcz);
                    if (cp != nullptr)
                        selb = cp->getblock(selbx, selby, selbz);
                }

                switch (sidedistmin)
                {
                case 1:
                    selbr = world::getbrightness(selx, sely + 1, selz);
                    break;
                case 2:
                    selbr = world::getbrightness(selx, sely - 1, selz);
                    break;
                case 3:
                    selbr = world::getbrightness(selx + 1, sely, selz);
                    break;
                case 4:
                    selbr = world::getbrightness(selx - 1, sely, selz);
                    break;
                case 5:
                    selbr = world::getbrightness(selx, sely, selz + 1);
                    break;
                case 6:
                    selbr = world::getbrightness(selx, sely, selz - 1);
                    break;
                }

                if (mb == 1 || glfwGetKey(MainWindow, GLFW_KEY_ENTER) == GLFW_PRESS)
                {
                    particles::throwParticle(world::getblock(x, y, z),
                                             float(x + rnd() - 0.5f), float(y + rnd() - 0.2f), float(z + rnd() - 0.5f),
                                             float(rnd() * 0.2f - 0.1f), float(rnd() * 0.2f - 0.1f), float(rnd() * 0.2f - 0.1f),
                                             float(rnd() * 0.01f + 0.02f), int(rnd() * 30) + 30);

                    if (selx != oldselx || sely != oldsely || selz != oldselz)
                    {
                        seldes = 0.0;
                    }
                    else
                    {
                        seldes += 5.0;
                    }

                    if (seldes >= 100.0)
                    {
                        player::additem(world::getblock(x, y, z));

                        for (int j = 1; j <= 25; j++)
                        {
                            particles::throwParticle(world::getblock(x, y, z),
                                                     float(x + rnd() - 0.5f), float(y + rnd() - 0.2f), float(z + rnd() - 0.5f),
                                                     float(rnd() * 0.2f - 0.1f), float(rnd() * 0.2f - 0.1f), float(rnd() * 0.2f - 0.1f),
                                                     float(rnd() * 0.02 + 0.03), int(rnd() * 60) + 30);
                        }

                        world::setblock(x, y, z, blocks::AIR);
                    }
                }

                //放置方块
                if (((mb == 2 && mbp == false) || isPressed(GLFW_KEY_TAB)) && player::inventorypcs[3][player::itemInHand] > 0)
                {
                    put = true;

                    switch (sidedistmin)
                    {
                        case 1:
                            if (player::putblock(x, y + 1, z, player::BlockInHand) == false)
                            {
                                put = false;
                            }

                            break;

                        case 2:
                            if (player::putblock(x, y - 1, z, player::BlockInHand) == false)
                            {
                                put = false;
                            }

                            break;

                        case 3:
                            if (player::putblock(x + 1, y, z, player::BlockInHand) == false)
                            {
                                put = false;
                            }

                            break;

                        case 4:
                            if (player::putblock(x - 1, y, z, player::BlockInHand) == false)
                            {
                                put = false;
                            }

                            break;

                        case 5:
                            if (player::putblock(x, y, z + 1, player::BlockInHand) == false)
                            {
                                put = false;
                            }

                            break;

                        case 6:
                            if (player::putblock(x, y, z - 1, player::BlockInHand) == false)
                            {
                                put = false;
                            }

                            break;
                    }

                    if (put)
                    {
                        player::inventorypcs[3][player::itemInHand]--;

                        if (player::inventorypcs[3][player::itemInHand] == 0)
                        {
                            player::inventorybox[3][player::itemInHand] = blocks::AIR;
                        }
                    }
                }

                break;
            }
        }

        if (selx != oldselx || sely != oldsely || selz != oldselz || (mb == 0 && glfwGetKey(MainWindow, GLFW_KEY_ENTER) != GLFW_PRESS))
        {
            seldes = 0.0;
        }

        oldselx = selx;
        oldsely = sely;
        oldselz = selz;

        player::intxpos = RoundInt(player::xpos);
        player::intypos = RoundInt(player::ypos);
        player::intzpos = RoundInt(player::zpos);

        //更新方向
        player::heading += player::xlookspeed;
        player::lookupdown += player::ylookspeed;
        player::xlookspeed = player::ylookspeed = 0.0;

        //移动！(生命在于运动)
        if (glfwGetKey(MainWindow, GLFW_KEY_W) || player::gliding())
        {
            if (!WP)
            {
                if (Wprstm == 0.0)
                {
                    Wprstm = timer();
                }
                else
                {
                    if (timer() - Wprstm <= 0.5)
                    {
                        player::Running = true;
                        Wprstm = 0.0;
                    }
                    else
                    {
                        Wprstm = timer();
                    }
                }
            }

            if (Wprstm != 0.0 && timer() - Wprstm > 0.5)
            {
                Wprstm = 0.0;
            }

            WP = true;

            if (!player::gliding())
            {
                player::xa = -sin(player::heading * M_PI / 180.0) * player::speed;
                player::za = -cos(player::heading * M_PI / 180.0) * player::speed;
            }
            else
            {
                player::xa = sin(M_PI / 180 * (player::heading - 180)) * sin(M_PI / 180 * (player::lookupdown + 90)) * player::glidingSpeed * speedCast;
                player::ya = cos(M_PI / 180 * (player::lookupdown + 90)) * player::glidingSpeed * speedCast;
                player::za = cos(M_PI / 180 * (player::heading - 180)) * sin(M_PI / 180 * (player::lookupdown + 90)) * player::glidingSpeed * speedCast;

                if (player::ya < 0)
                {
                    player::ya *= 2;
                }
            }
        }
        else
        {
            player::Running = false;
            WP = false;
        }

        if (player::Running)
        {
            player::speed = runspeed;
        }
        else
        {
            player::speed = walkspeed;
        }

        if (glfwGetKey(MainWindow, GLFW_KEY_S) == GLFW_PRESS && !player::gliding())
        {
            player::xa = sin(player::heading * M_PI / 180.0) * player::speed;
            player::za = cos(player::heading * M_PI / 180.0) * player::speed;
            Wprstm = 0.0;
        }

        if (glfwGetKey(MainWindow, GLFW_KEY_A) == GLFW_PRESS && !player::gliding())
        {
            player::xa = sin((player::heading - 90) * M_PI / 180.0) * player::speed;
            player::za = cos((player::heading - 90) * M_PI / 180.0) * player::speed;
            Wprstm = 0.0;
        }

        if (glfwGetKey(MainWindow, GLFW_KEY_D) == GLFW_PRESS && !player::gliding())
        {
            player::xa = -sin((player::heading - 90) * M_PI / 180.0) * player::speed;
            player::za = -cos((player::heading - 90) * M_PI / 180.0) * player::speed;
            Wprstm = 0.0;
        }

        if (glfwGetKey(MainWindow, GLFW_KEY_R) == GLFW_PRESS && !player::gliding())
        {
            if (glfwGetKey(MainWindow, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
            {
                player::xa = -sin(player::heading * M_PI / 180.0) * runspeed * 10;
                player::za = -cos(player::heading * M_PI / 180.0) * runspeed * 10;
            }
            else
            {
                player::xa = sin(M_PI / 180 * (player::heading - 180)) * sin(M_PI / 180 * (player::lookupdown + 90)) * runspeed * 20;
                player::ya = cos(M_PI / 180 * (player::lookupdown + 90)) * runspeed * 20;
                player::za = cos(M_PI / 180 * (player::heading - 180)) * sin(M_PI / 180 * (player::lookupdown + 90)) * runspeed * 20;
            }
        }

        if (glfwGetKey(MainWindow, GLFW_KEY_F) == GLFW_PRESS && !player::gliding())
        {
            if (glfwGetKey(MainWindow, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
            {
                player::xa = sin(player::heading * M_PI / 180.0) * runspeed * 10;
                player::za = cos(player::heading * M_PI / 180.0) * runspeed * 10;
            }
            else
            {
                player::xa = -sin(M_PI / 180 * (player::heading - 180)) * sin(M_PI / 180 * (player::lookupdown + 90)) * runspeed * 20;
                player::ya = -cos(M_PI / 180 * (player::lookupdown + 90)) * runspeed * 20;
                player::za = -cos(M_PI / 180 * (player::heading - 180)) * sin(M_PI / 180 * (player::lookupdown + 90)) * runspeed * 20;
            }
        }

        //切换方块
        if (isPressed(GLFW_KEY_Z) && player::itemInHand > 0)
        {
            player::itemInHand--;
        }

        if (isPressed(GLFW_KEY_X) && player::itemInHand < 9)
        {
            player::itemInHand++;
        }

        if ((int)player::itemInHand + (mwl - mw) < 0)
        {
            player::itemInHand = 0;
        }
        else if ((int)player::itemInHand + (mwl - mw) > 9)
        {
            player::itemInHand = 9;
        }
        else
        {
            player::itemInHand += static_cast<unsigned char>(mwl - mw);
        }

        mwl = mw;

        //起跳！
        if (isPressed(GLFW_KEY_SPACE))
        {
            if (!player::inWater)
            {
                if ((player::OnGround || player::AirJumps < MaxAirJumps) && FLY == false && CROSS == false)
                {
                    if (player::OnGround == false)
                    {
                        player::jump = 0.3;
                        player::AirJumps++;
                    }
                    else
                    {
                        player::jump = 0.25;
                        player::OnGround = false;
                    }
                }

                if (FLY || CROSS)
                {
                    player::ya += walkspeed / 2;
                    isPressed(GLFW_KEY_SPACE, true);
                }

                Wprstm = 0.0;
            }
            else
            {
                player::ya = walkspeed;
                isPressed(GLFW_KEY_SPACE, true);
            }
        }

        if ((glfwGetKey(MainWindow, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(MainWindow, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS) && !player::gliding())
        {
            if (CROSS || FLY)
            {
                player::ya -= walkspeed / 2;
            }

            Wprstm = 0.0;
        }

        if (glfwGetKey(MainWindow, GLFW_KEY_K) && canGliding && !player::OnGround && !player::gliding())
        {
            double h = player::ypos + player::height + player::heightExt;
            player::glidingEnergy = g * h;
            player::glidingSpeed = 0;
            player::glidingNow = true;
        }

        //各种设置切换
        if (isPressed(GLFW_KEY_F1))
        {
            FLY = !FLY;
            player::jump = 0.0;
        }

        if (isPressed(GLFW_KEY_F2))
        {
            shouldGetScreenshot = true;
        }

        if (isPressed(GLFW_KEY_F3))
        {
            DebugMode = !DebugMode;
        }

        if (isPressed(GLFW_KEY_F3) && glfwGetKey(MainWindow, GLFW_KEY_H) == GLFW_PRESS)
        {
            DebugHitbox = !DebugHitbox;
            DebugMode = true;
        }

        if (isPressed(GLFW_KEY_F4) == GLFW_PRESS)
        {
            CROSS = !CROSS;
        }

        if (isPressed(GLFW_KEY_F5) == GLFW_PRESS)
        {
            GUIrenderswitch = !GUIrenderswitch;
        }

        if (isPressed(GLFW_KEY_F6) == GLFW_PRESS)
        {
            player::xpos = 2147483600;
        }
    }

    if (isPressed(GLFW_KEY_E) && GUIrenderswitch)
    {
        bagOpened = !bagOpened;
        bagAnimTimer = timer();

        if (!bagOpened)
        {
            glfwSetInputMode(MainWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            glfwGetCursorPos(MainWindow, &mx, &my);
            mxl = mx;
            myl = my;
            mwl = mw;
            mbl = mb;
        }
        else
        {
            shouldGetThumbnail = true;
            player::xlookspeed = player::ylookspeed = 0.0;
        }
    }

    if (!bagOpened)
    {
        if (isPressed(GLFW_KEY_L))
        {
            world::saveAllChunks();
        }
    }

    //跳跃
    if (!player::gliding())
    {
        if (!player::inWater)
        {
            if (!FLY && !CROSS && !glfwGetKey(MainWindow, GLFW_KEY_R) && !glfwGetKey(MainWindow, GLFW_KEY_F))
            {
                player::ya = -0.001;

                if (player::OnGround)
                {
                    player::jump = 0.0;
                    player::AirJumps = 0;
                    isPressed(GLFW_KEY_SPACE, true);
                }
                else
                {
                    //自由落体计算
                    player::jump -= 0.025;
                    player::ya = player::jump + 0.5 * 0.6 * 1 / 900;
                }
            }
            else
            {
                player::jump = 0.0;
                player::AirJumps = 0;
            }
        }
        else
        {
            player::jump = 0.0;
            player::AirJumps = MaxAirJumps;
            isPressed(GLFW_KEY_SPACE, true);

            if (player::ya <= 0.001 && !FLY && !CROSS)
            {
                player::ya = - 0.001;

                if (!player::OnGround)
                {
                    player::ya -= 0.1;
                }
            }
        }
    }

    //爬墙
    //if (player::NearWall && FLY == false && CROSS == false){
    //  player::ya += walkspeed
    //  player::jump = 0.0
    //}

    if (player::gliding())
    {
        double &E = player::glidingEnergy;
        double oldh = player::ypos + player::height + player::heightExt + player::ya;
        double h = oldh;

        if (E - glidingMinimumSpeed < h * g)  //小于最小速度
        {
            h = (E - glidingMinimumSpeed) / g;
        }

        player::glidingSpeed = sqrt(2 * (E - g * h));
        E -= EDrop;
        player::ya += h - oldh;
    }

    mbp = mb;
    FirstFrameThisUpdate = true;

    player::intxpos = RoundInt(player::xpos);
    player::intypos = RoundInt(player::ypos);
    player::intzpos = RoundInt(player::zpos);
    player::Move();
    player::xposold = player::xpos;
    player::yposold = player::ypos;
    player::zposold = player::zpos;
    player::intxposold = RoundInt(player::xpos);
    player::intyposold = RoundInt(player::ypos);
    player::intzposold = RoundInt(player::zpos);
    particles::updateall();

    //  Time_updategame += timer() - Time_updategame;

}

void debugText(string s, bool init)
{
    static int pos = 0;

    if (init)
    {
        pos = 0;
        return;
    }

    TextRenderer::renderString(0, 16 * pos, s);
    pos++;
}


double curtime;
double TimeDelta;
double xpos, ypos, zpos;
int renderedChunk;
int TexcoordCount;

void renderWorld()
{
    if (MergeFace)
    {
        glDisable(GL_TEXTURE_2D);
        glEnable(GL_TEXTURE_3D);
        glBindTexture(GL_TEXTURE_3D, BlockTextures3D);
    }
    else
    {
        glBindTexture(GL_TEXTURE_2D, BlockTextures);
    }

    glDisable(GL_BLEND);
    glEnableClientState(GL_COLOR_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_VERTEX_ARRAY);

    for (int i = 0; i < renderedChunk; i++)
    {
        RenderChunk cr = displayChunks[i];

        if (cr.vtxs[0] == 0)
        {
            continue;
        }

        glPushMatrix();
        glTranslated(cr.cx * 16.0 - xpos, cr.cy * 16.0 - cr.loadAnim - ypos, cr.cz * 16.0 - zpos);
        renderer::renderbuffer(cr.vbuffers[0], cr.vtxs[0], TexcoordCount, 3);
        glPopMatrix();
    }

    glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);

    if (MergeFace)
    {
        glDisable(GL_TEXTURE_3D);
        glEnable(GL_TEXTURE_2D);
    }

    glEnable(GL_BLEND);
}

void sortDisplayChunks()
{
    //更新区块显示列表
    world::mWorld.tryUpdateRenderers(Vec3i(RoundInt(player::xpos), RoundInt(player::ypos), RoundInt(player::zpos)));


    //删除已卸载区块的VBO
    if (world::vbuffersShouldDelete.size() > 0)
    {
        glDeleteBuffersARB(world::vbuffersShouldDelete.size(), world::vbuffersShouldDelete.data());
        world::vbuffersShouldDelete.clear();
    }

    double plookupdown = player::lookupdown + player::ylookspeed;
    double pheading = player::heading + player::xlookspeed;

    glLoadIdentity();
    glRotated(plookupdown, 1, 0, 0);
    glRotated(360.0 - pheading, 0, 1, 0);
    Frustum::LoadIdentity();
    Frustum::setPerspective(FOVyNormal + FOVyExt, (float)windowwidth / windowheight, 0.05f, viewdistance * 16.0f);
    Frustum::multRotate((float)plookupdown, 1, 0, 0);
    Frustum::multRotate(360.0f - (float)pheading, 0, 1, 0);
    Frustum::calc();

    displayChunks.clear();

    for (auto&& chk : world::mWorld)
    {
        if (!chk.second.renderBuilt || chk.second.Empty)
        {
            continue;
        }

        if (world::chunkInRange(chk.second.cx, chk.second.cy, chk.second.cz,
                                player::cxt, player::cyt, player::czt, viewdistance))
        {
            if (Frustum::AABBInFrustum(chk.second.getRelativeAABB(xpos, ypos, zpos)))
            {
                displayChunks.push_back(RenderChunk(&chk.second, (curtime - lastupdate) * 30.0));
            }
        }
    }
}

void renderWorldTransparent()
{
    double plookupdown = player::lookupdown + player::ylookspeed;
    double pheading = player::heading + player::xlookspeed;
    glLoadIdentity();
    glRotated(plookupdown, 1, 0, 0);
    glRotated(360.0 - pheading, 0, 1, 0);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_CULL_FACE);

    glEnableClientState(GL_COLOR_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_VERTEX_ARRAY);

    if (MergeFace)
    {
        glDisable(GL_TEXTURE_2D);
        glEnable(GL_TEXTURE_3D);
        glBindTexture(GL_TEXTURE_3D, BlockTextures3D);
    }
    else
    {
        glBindTexture(GL_TEXTURE_2D, BlockTextures);
    }

    for (int i = 0; i < renderedChunk; i++)
    {
        RenderChunk cr = displayChunks[i];

        if (cr.vtxs[1] == 0)
        {
            continue;
        }

        glPushMatrix();
        glTranslated(cr.cx * 16.0 - xpos, cr.cy * 16.0 - cr.loadAnim - ypos, cr.cz * 16.0 - zpos);
        renderer::renderbuffer(cr.vbuffers[1], cr.vtxs[1], TexcoordCount, 3);
        glPopMatrix();
    }

    glDisable(GL_CULL_FACE);

    for (int i = 0; i < renderedChunk; i++)
    {
        RenderChunk cr = displayChunks[i];

        if (cr.vtxs[2] == 0)
        {
            continue;
        }

        glPushMatrix();
        glTranslated(cr.cx * 16.0 - xpos, cr.cy * 16.0 - cr.loadAnim - ypos, cr.cz * 16.0 - zpos);
        renderer::renderbuffer(cr.vbuffers[2], cr.vtxs[2], TexcoordCount, 3);
        glPopMatrix();
    }

    glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);

    if (MergeFace)
    {
        glDisable(GL_TEXTURE_3D);
        glEnable(GL_TEXTURE_2D);
    }

    glLoadIdentity();
    glRotated(plookupdown, 1, 0, 0);
    glRotated(360.0 - pheading, 0, 1, 0);
    glTranslated(-xpos, -ypos, -zpos);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_CULL_FACE);
}

void updatePlayer()
{
    if (player::Running)
    {
        if (FOVyExt < 9.8)
        {
            TimeDelta = curtime - SpeedupAnimTimer;
            FOVyExt = 10.0f - (10.0f - FOVyExt) * (float)pow(0.8, TimeDelta * 30);
            SpeedupAnimTimer = curtime;
        }
        else
        {
            FOVyExt = 10.0;
        }
    }
    else
    {
        if (FOVyExt > 0.2)
        {
            TimeDelta = curtime - SpeedupAnimTimer;
            FOVyExt *= (float)pow(0.8, TimeDelta * 30);
            SpeedupAnimTimer = curtime;
        }
        else
        {
            FOVyExt = 0.0;
        }
    }

    SpeedupAnimTimer = curtime;

    if (player::OnGround)
    {
        //半蹲特效
        if (player::jump < -0.005)
        {
            if (player::jump <= -(player::height - 0.5f))
            {
                player::heightExt = -(player::height - 0.5f);
            }
            else
            {
                player::heightExt = (float)player::jump;
            }

            TouchdownAnimTimer = curtime;
        }
        else
        {
            if (player::heightExt <= -0.005)
            {
                player::heightExt *= (float)pow(0.8, (curtime - TouchdownAnimTimer) * 30);
                TouchdownAnimTimer = curtime;
            }
        }
    }

    xpos = player::xpos - player::xd + (curtime - lastupdate) * 30.0 * player::xd;
    ypos = player::ypos + player::height + player::heightExt - player::yd + (curtime - lastupdate) * 30.0 * player::yd;
    zpos = player::zpos - player::zd + (curtime - lastupdate) * 30.0 * player::zd;

    if (!bagOpened)
    {
        //转头！你治好了我多年的颈椎病！
        if (mx != mxl)
        {
            player::xlookspeed -= (mx - mxl) * mousemove;
        }

        if (my != myl)
        {
            player::ylookspeed += (my - myl) * mousemove;
        }

        if (glfwGetKey(MainWindow, GLFW_KEY_RIGHT) == 1)
        {
            player::xlookspeed -= mousemove * 16 * (curtime - lastframe) * 30.0;
        }

        if (glfwGetKey(MainWindow, GLFW_KEY_LEFT) == 1)
        {
            player::xlookspeed += mousemove * 16 * (curtime - lastframe) * 30.0;
        }

        if (glfwGetKey(MainWindow, GLFW_KEY_UP) == 1)
        {
            player::ylookspeed -= mousemove * 16 * (curtime - lastframe) * 30.0;
        }

        if (glfwGetKey(MainWindow, GLFW_KEY_DOWN) == 1)
        {
            player::ylookspeed += mousemove * 16 * (curtime - lastframe) * 30.0;
        }

        //限制角度，别把头转掉下来了 ←_←
        if (player::lookupdown + player::ylookspeed < -90.0)
        {
            player::ylookspeed = -90.0 - player::lookupdown;
        }

        if (player::lookupdown + player::ylookspeed > 90.0)
        {
            player::ylookspeed = 90.0 - player::lookupdown;
        }
    }
}

void Render()
{
    //画场景
    curtime = timer();
    renderedChunk = 0;
    TexcoordCount = MergeFace ? 3 : 2;

    mxl = mx;
    myl = my;
    glfwGetCursorPos(MainWindow, &mx, &my);

    updatePlayer();

    glClearColor(skycolorR, skycolorG, skycolorB, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(FOVyNormal + FOVyExt, windowwidth / (double)windowheight, 0.05, viewdistance * 16.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glColor4f(1.0, 1.0, 1.0, 1.0);

    player::cxt = getchunkpos((int)player::xpos);
    player::cyt = getchunkpos((int)player::ypos);
    player::czt = getchunkpos((int)player::zpos);

    sortDisplayChunks();
    Mutex.unlock();
    renderedChunk = displayChunks.size();

    renderWorld();

    Mutex.lock();

    if (seldes > 0.0)
    {
        glTranslated(selx - xpos, sely - ypos, selz - zpos);
        renderDestroy(seldes, 0, 0, 0);
        glTranslated(-selx + xpos, -sely + ypos, -selz + zpos);
    }

    glBindTexture(GL_TEXTURE_2D, BlockTextures);
    particles::renderall(xpos, ypos, zpos);

    glDisable(GL_TEXTURE_2D);

    if (GUIrenderswitch)
    {
        glTranslated(selx - xpos, sely - ypos, selz - zpos);
        drawBorder(0, 0, 0);
        glTranslated(-selx + xpos, -sely + ypos, -selz + zpos);
    }

    Mutex.unlock();
    renderWorldTransparent();
    Mutex.lock();

    //Time_renderscene = timer() - Time_renderscene;
    //Time_renderGUI_ = timer();

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, windowwidth, windowheight, 0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    if (world::getblock(RoundInt(xpos), RoundInt(ypos), RoundInt(zpos)) == blocks::WATER)
    {
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        glBindTexture(GL_TEXTURE_2D, BlockTextures);
        double tcX = Textures::getTexcoordX(blocks::WATER, 1);
        double tcY = Textures::getTexcoordY(blocks::WATER, 1);
        glBegin(GL_QUADS);
        glTexCoord2d(tcX, tcY + 1 / 8.0);
        glVertex2i(0, 0);
        glTexCoord2d(tcX, tcY);
        glVertex2i(0, windowheight);
        glTexCoord2d(tcX + 1 / 8.0, tcY);
        glVertex2i(windowwidth, windowheight);
        glTexCoord2d(tcX + 1 / 8.0, tcY + 1 / 8.0);
        glVertex2i(windowwidth, 0);
        glEnd();
    }

    if (GUIrenderswitch)
    {
        drawGUI();
        drawBag();
    }

    glDisable(GL_TEXTURE_2D);

    if (curtime - screenshotAnimTimer <= 1.0 && !shouldGetScreenshot)
    {
        float col = 1.0f - (float)(curtime - screenshotAnimTimer);
        glColor4f(1.0f, 1.0f, 1.0f, col);
        glBegin(GL_QUADS);
        glVertex2i(0, 0);
        glVertex2i(0, windowheight);
        glVertex2i(windowwidth, windowheight);
        glVertex2i(windowwidth, 0);
        glEnd();
    }

    glEnable(GL_TEXTURE_2D);

    if (shouldGetScreenshot)
    {
        shouldGetScreenshot = false;
        screenshotAnimTimer = curtime;
        time_t t = time(0);
        char tmp[64];
        tm *timeinfo = new tm;
#ifdef NEWORLD_COMPILE_DISABLE_SECURE
        timeinfo = localtime(&t);
#else
        localtime_s(timeinfo, &t);
#endif
        strftime(tmp, sizeof(tmp), "%Y年%m月%d日%H时%M分%S秒", timeinfo);
        saveScreenshot(0, 0, windowwidth, windowheight, StringUtils::FormatString("Screenshots/%s.bmp", tmp));
    }

    if (shouldGetThumbnail)
    {
        shouldGetThumbnail = false;
        createThumbnail();
    }

    //屏幕刷新，千万别删，后果自负！！！
    //====refresh====//
    Mutex.unlock();
    glfwSwapBuffers(MainWindow);
    glfwPollEvents();
    Mutex.lock();
    //==refresh end==//

    lastframe = curtime;
    //Time_screensync = timer() - Time_screensync;

}

void drawBorder(int x, int y, int z)
{
    constexpr float extrize = 0.002f; //实际上这个边框应该比方块大一些，否则很难看
    float geomentry[] =
    {
        -(0.5f + extrize) + x, (0.5f + extrize) + y, -(0.5f + extrize) + z,
        -(0.5f + extrize) + x, (0.5f + extrize) + y, (0.5f + extrize) + z,
        (0.5f + extrize) + x, (0.5f + extrize) + y, (0.5f + extrize) + z,
        (0.5f + extrize) + x, (0.5f + extrize) + y, -(0.5f + extrize) + z,
        -(0.5f + extrize) + x, -(0.5f + extrize) + y, -(0.5f + extrize) + z,
        (0.5f + extrize) + x, -(0.5f + extrize) + y, -(0.5f + extrize) + z,
        -(0.5f + extrize) + x, -(0.5f + extrize) + y, (0.5f + extrize) + z,
        (0.5f + extrize) + x, -(0.5f + extrize) + y, (0.5f + extrize) + z,
    };
    unsigned char stride[] =
    {
        // Top Face
        0, 1, 1, 2, 2, 3, 0, 3,
        // Bottom Face
        4, 5, 7, 5, 7, 6, 4, 6,
        // Left Face
        5, 3, 3, 2, 2, 7, 5, 7,
        // Right Face
        4, 6, 6, 1, 1, 0, 4, 0,
        // Front Face
        6, 7, 7, 2, 2, 1, 6, 1,
        // Back Face
        4, 0, 0, 3, 3, 5, 4, 5
    };
    glEnable(GL_LINE_SMOOTH);
    glColor3f(0.2f, 0.2f, 0.2f);
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, geomentry);
    glDrawElements(GL_LINES, 48, GL_UNSIGNED_BYTE, stride);
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisable(GL_LINE_SMOOTH);
}

void drawGUI()
{

    glDepthFunc(GL_ALWAYS);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_LINE_SMOOTH);
    float seldes_100 = seldes / 100.0f;
    int disti = (int)(seldes_100 * linedist);

    if (DebugMode)
    {

        if (selb != blocks::AIR)
        {
            glLineWidth(1);
            glBegin(GL_LINES);
            glColor4f(gui::FgR, gui::FgG, gui::FgB, 0.8f);
            glVertex2i(windowwidth / 2, windowheight / 2);
            glVertex2i(windowwidth / 2 + 50, windowheight / 2 + 50);
            glVertex2i(windowwidth / 2 + 50, windowheight / 2 + 50);
            glVertex2i(windowwidth / 2 + 250, windowheight / 2 + 50);
            glEnd();
            TextRenderer::setFontColor(1.0f, 1.0f, 1.0f, 0.8f);
            glEnable(GL_TEXTURE_2D);
            glDisable(GL_CULL_FACE);
            TextRenderer::renderString(windowwidth / 2 + 50, windowheight / 2 + 50 - 16,
                                       StringUtils::FormatString("%s (ID:%d)", BlockInfo(selb).getBlockName(), (int)selb));
            glDisable(GL_TEXTURE_2D);
            glEnable(GL_CULL_FACE);
            glColor4f(0.0f, 0.0f, 0.0f, 0.9f);
        }
        else
        {
            glColor4f(0.0f, 0.0f, 0.0f, 0.6f);
        }

        glLineWidth(2);

        glBegin(GL_LINES);

        glVertex2i(windowwidth / 2 - linedist + disti, windowheight / 2 - linedist + disti);
        glVertex2i(windowwidth / 2 - linedist + disti, windowheight / 2 - linedist + linelength + disti);
        glVertex2i(windowwidth / 2 - linedist + disti, windowheight / 2 - linedist + disti);
        glVertex2i(windowwidth / 2 - linedist + linelength + disti, windowheight / 2 - linedist + disti);

        glVertex2i(windowwidth / 2 + linedist - disti, windowheight / 2 - linedist + disti);
        glVertex2i(windowwidth / 2 + linedist - disti, windowheight / 2 - linedist + linelength + disti);
        glVertex2i(windowwidth / 2 + linedist - disti, windowheight / 2 - linedist + disti);
        glVertex2i(windowwidth / 2 + linedist - linelength - disti, windowheight / 2 - linedist + disti);

        glVertex2i(windowwidth / 2 - linedist + disti, windowheight / 2 + linedist - disti);
        glVertex2i(windowwidth / 2 - linedist + disti, windowheight / 2 + linedist - linelength - disti);
        glVertex2i(windowwidth / 2 - linedist + disti, windowheight / 2 + linedist - disti);
        glVertex2i(windowwidth / 2 - linedist + linelength + disti, windowheight / 2 + linedist - disti);

        glVertex2i(windowwidth / 2 + linedist - disti, windowheight / 2 + linedist - disti);
        glVertex2i(windowwidth / 2 + linedist - disti, windowheight / 2 + linedist - linelength - disti);
        glVertex2i(windowwidth / 2 + linedist - disti, windowheight / 2 + linedist - disti);
        glVertex2i(windowwidth / 2 + linedist - linelength - disti, windowheight / 2 + linedist - disti);

        glEnd();

    }

    glLineWidth(4);
    glBegin(GL_LINES);
    glColor4f(0.0, 0.0, 0.0, 1.0);
    glVertex2i(windowwidth / 2 - 16, windowheight / 2);
    glVertex2i(windowwidth / 2 + 16, windowheight / 2);
    glVertex2i(windowwidth / 2, windowheight / 2 - 16);
    glVertex2i(windowwidth / 2, windowheight / 2 + 16);
    glEnd();
    glLineWidth(2);
    glBegin(GL_LINES);
    glColor4f(1.0, 1.0, 1.0, 1.0);
    glVertex2i(windowwidth / 2 - 15, windowheight / 2);
    glVertex2i(windowwidth / 2 + 15, windowheight / 2);
    glVertex2i(windowwidth / 2, windowheight / 2 - 15);
    glVertex2i(windowwidth / 2, windowheight / 2 + 15);
    glEnd();

    if (seldes > 0.0)
    {

        glBegin(GL_LINES);
        glColor4f(0.5, 0.5, 0.5, 1.0);
        glVertex2i(windowwidth / 2 - 15, windowheight / 2);
        glVertex2i(windowwidth / 2 - 15 + (int)(seldes_100 * 15), windowheight / 2);
        glVertex2i(windowwidth / 2 + 15, windowheight / 2);
        glVertex2i(windowwidth / 2 + 15 - (int)(seldes_100 * 15), windowheight / 2);
        glVertex2i(windowwidth / 2, windowheight / 2 - 15);
        glVertex2i(windowwidth / 2, windowheight / 2 - 15 + (int)(seldes_100 * 15));
        glVertex2i(windowwidth / 2, windowheight / 2 + 15);
        glVertex2i(windowwidth / 2, windowheight / 2 + 15 - (int)(seldes_100 * 15));
        glEnd();

    }

    glEnable(GL_TEXTURE_2D);
    glDisable(GL_CULL_FACE);

    if (DebugMode)
    {
        using namespace StringUtils;
        TextRenderer::setFontColor(1.0f, 1.0f, 1.0f, 0.9f);
        debugText(FormatString("NEWorld v%d [OpenGL %d.%d|%d]", VERSION, GLVersionMajor, GLVersionMinor, GLVersionRev));
        debugText("Flying:" + boolstr(FLY));
        debugText("Can Gliding:" + boolstr(canGliding));
        debugText("Gliding:" + boolstr(player::gliding()));
        debugText(FormatString("Energy: %f", player::glidingEnergy));
        debugText(FormatString("Speed: %f", player::glidingSpeed));
        debugText("Debug Mode:" + boolstr(DebugMode));
        debugText("Crosswall:" + boolstr(CROSS));
        debugText(FormatString("Block: %s (ID:%d)", BlockInfo(player::BlockInHand).getBlockName(), (int)player::BlockInHand));
        debugText(FormatString("Fps:%d Ups(Tps):%d", fps, ups));

        debugText(FormatString("X: %f Y: %f Z: %f", player::xpos, player::ypos, player::zpos));
        debugText(FormatString("Direction: %f", player::heading));
        debugText(FormatString("Head: %f", player::lookupdown));
        debugText("On ground:" + boolstr(player::OnGround));
        debugText(FormatString("Jump speed: %f", player::jump));
        debugText("Near wall:" + boolstr(player::NearWall));
        debugText("In water:" + boolstr(player::inWater));

        debugText("", true);
    }
    else
    {
        using namespace StringUtils;
        TextRenderer::setFontColor(1.0f, 1.0f, 1.0f, 0.9f);
        TextRenderer::renderString(0, 0, FormatString("v%d", VERSION));
        TextRenderer::renderString(0, 16, FormatString("Fps:%d", fps));
    }

    //检测帧速率
    if (timer() - fctime >= 1.0)
    {
        fps = fpsc;
        fpsc = 0;
        fctime = timer();
    }

    fpsc++;
}

/*void drawCloud(double px, double pz)
{
    //glFogf(GL_FOG_START, 100.0);
    //glFogf(GL_FOG_END, 300.0);
    static double ltimer;
    static bool generated;
    static unsigned int cloudvb[128];
    static int vtxs[128];
    static float f;
    static int l;

    if (ltimer == 0.0)
    {
        ltimer = timer();
    }

    f += (float)(timer() - ltimer) * 0.25f;
    ltimer = timer();

    if (f >= 1.0)
    {
        l += int(f);
        f -= int(f);
        l %= 128;
    }

    if (!generated)
    {
        generated = true;

        for (int i = 0; i != 128; i++)
        {
            for (int j = 0; j != 128; j++)
            {
                world::cloud[i][j] = int(rnd() * 2);
            }
        }

        glGenBuffersARB(128, cloudvb);

        for (int i = 0; i != 128; i++)
        {
            renderer::Init(0, 0);

            for (int j = 0; j != 128; j++)
            {
                if (world::cloud[i][j] != 0)
                {
                    renderer::Vertex3d(j * cloudwidth, 128.0, 0.0);
                    renderer::Vertex3d(j * cloudwidth, 128.0, cloudwidth);
                    renderer::Vertex3d((j + 1)*cloudwidth, 128.0, cloudwidth);
                    renderer::Vertex3d((j + 1)*cloudwidth, 128.0, 0.0);
                }
            }

            renderer::Flush(cloudvb[i], vtxs[i]);
        }
    }

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_CULL_FACE);
    glColor4f(1.0, 1.0, 1.0, 0.5);

    for (int i = 0; i < 128; i++)
    {
        glPushMatrix();
        glTranslated(-64.0 * cloudwidth - px, 0.0, cloudwidth * ((l + i) % 128 + f) - 64.0 * cloudwidth - pz);
        renderer::renderbuffer(cloudvb[i], vtxs[i], 0, 0);
        glPopMatrix();
    }

    //setupNormalFog();
}*/

void renderDestroy(float level, int x, int y, int z)
{
    constexpr float ES = 0.002f;

    float geomentry[] =
    {
        0.0f, 0.0f, -(0.5f + ES) + x, -(0.5f + ES) + y, (0.5f + ES) + z,
        1.0f, 0.0f, (0.5f + ES) + x, -(0.5f + ES) + y, (0.5f + ES) + z,
        1.0f, 1.0f, (0.5f + ES) + x, (0.5f + ES) + y, (0.5f + ES) + z,
        0.0f, 1.0f, -(0.5f + ES) + x, (0.5f + ES) + y, (0.5f + ES) + z,

        1.0f, 0.0f, -(0.5f + ES) + x, -(0.5f + ES) + y, -(0.5f + ES) + z,
        1.0f, 1.0f, -(0.5f + ES) + x, (0.5f + ES) + y, -(0.5f + ES) + z,
        0.0f, 1.0f, (0.5f + ES) + x, (0.5f + ES) + y, -(0.5f + ES) + z,
        0.0f, 0.0f, (0.5f + ES) + x, -(0.5f + ES) + y, -(0.5f + ES) + z,

        1.0f, 0.0f, (0.5f + ES) + x, -(0.5f + ES) + y, -(0.5f + ES) + z,
        1.0f, 1.0f, (0.5f + ES) + x, (0.5f + ES) + y, -(0.5f + ES) + z,
        0.0f, 1.0f, (0.5f + ES) + x, (0.5f + ES) + y, (0.5f + ES) + z,
        0.0f, 0.0f, (0.5f + ES) + x, -(0.5f + ES) + y, (0.5f + ES) + z,

        0.0f, 0.0f, -(0.5f + ES) + x, -(0.5f + ES) + y, -(0.5f + ES) + z,
        1.0f, 0.0f, -(0.5f + ES) + x, -(0.5f + ES) + y, (0.5f + ES) + z,
        1.0f, 1.0f, -(0.5f + ES) + x, (0.5f + ES) + y, (0.5f + ES) + z,
        0.0f, 1.0f, -(0.5f + ES) + x, (0.5f + ES) + y, -(0.5f + ES) + z,

        0.0f, 1.0f, -(0.5f + ES) + x, (0.5f + ES) + y, -(0.5f + ES) + z,
        0.0f, 0.0f, -(0.5f + ES) + x, (0.5f + ES) + y, (0.5f + ES) + z,
        1.0f, 0.0f, (0.5f + ES) + x, (0.5f + ES) + y, (0.5f + ES) + z,
        1.0f, 1.0f, (0.5f + ES) + x, (0.5f + ES) + y, -(0.5f + ES) + z,

        1.0f, 1.0f, -(0.5f + ES) + x, -(0.5f + ES) + y, -(0.5f + ES) + z,
        0.0f, 1.0f, (0.5f + ES) + x, -(0.5f + ES) + y, -(0.5f + ES) + z,
        0.0f, 0.0f, (0.5f + ES) + x, -(0.5f + ES) + y, (0.5f + ES) + z,
        1.0f, 0.0f, -(0.5f + ES) + x, -(0.5f + ES) + y, (0.5f + ES) + z
    };
    glColor4f(1.0, 1.0, 1.0, 1.0);
    glBindTexture(GL_TEXTURE_2D, DestroyImage[(level < 100.0) ? int(level / 10) + 1 : 10]);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glTexCoordPointer(2, GL_FLOAT, sizeof(float) * 5, geomentry);
    glVertexPointer(3, GL_FLOAT, sizeof(float) * 5, geomentry + 2);
    glDrawArrays(GL_QUADS, 0, 24);
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}

void drawBagRow(int row, int itemid, int xbase, int ybase, int spac, float alpha)
{
    //画出背包的一行
    for (int i = 0; i < 10; i++)
    {
        if (i == itemid)
        {
            glBindTexture(GL_TEXTURE_2D, tex_select);
        }
        else
        {
            glBindTexture(GL_TEXTURE_2D, tex_unselect);
        }

        glColor4f(1.0f, 1.0f, 1.0f, alpha);
        glBegin(GL_QUADS);
        glTexCoord2f(0.0, 1.0);
        glVertex2d(xbase + i * (32 + spac), ybase);
        glTexCoord2f(0.0, 0.0);
        glVertex2d(xbase + i * (32 + spac) + 32, ybase);
        glTexCoord2f(1.0, 0.0);
        glVertex2d(xbase + i * (32 + spac) + 32, ybase + 32);
        glTexCoord2f(1.0, 1.0);
        glVertex2d(xbase + i * (32 + spac), ybase + 32);
        glEnd();
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

        if (player::inventorybox[row][i] != blocks::AIR)
        {
            glBindTexture(GL_TEXTURE_2D, BlockTextures);
            double tcX = Textures::getTexcoordX(player::inventorybox[row][i], 1);
            double tcY = Textures::getTexcoordY(player::inventorybox[row][i], 1);
            glBegin(GL_QUADS);
            glTexCoord2d(tcX, tcY + 1 / 8.0);
            glVertex2d(xbase + i * (32 + spac) + 2, ybase + 2);
            glTexCoord2d(tcX + 1 / 8.0, tcY + 1 / 8.0);
            glVertex2d(xbase + i * (32 + spac) + 30, ybase + 2);
            glTexCoord2d(tcX + 1 / 8.0, tcY);
            glVertex2d(xbase + i * (32 + spac) + 30, ybase + 30);
            glTexCoord2d(tcX, tcY);
            glVertex2d(xbase + i * (32 + spac) + 2, ybase + 30);
            glEnd();
            std::stringstream ss;
            ss << (int)player::inventorypcs[row][i];
            TextRenderer::renderString(xbase + i * (32 + spac), ybase, ss.str());
        }
    }
}

void drawBag()
{
    //背包界面与更新
    static int si, sj, sf;
    int csi = -1, csj = -1;
    int leftp = (windowwidth - 392) / 2;
    int upp = windowheight - 152 - 16;
    static int mousew, mouseb, mousebl;
    static block itemselected = blocks::AIR;
    static block pcsselected = 0;
    double curtime = timer();
    double TimeDelta = curtime - bagAnimTimer;
    float bagAnim = (float)(1.0 - pow(0.9, TimeDelta * 60.0) + pow(0.9, bagAnimDuration * 60.0) / bagAnimDuration * TimeDelta);

    if (bagOpened)
    {

        glfwSetInputMode(MainWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        mousew = mw;
        mouseb = mb;
        glDepthFunc(GL_ALWAYS);
        glDisable(GL_CULL_FACE);
        glDisable(GL_TEXTURE_2D);

        if (curtime - bagAnimTimer > bagAnimDuration)
        {
            glColor4f(0.2f, 0.2f, 0.2f, 0.6f);
        }
        else
        {
            glColor4f(0.2f, 0.2f, 0.2f, 0.6f * bagAnim);
        }

        glBegin(GL_QUADS);
        glVertex2i(0, 0);
        glVertex2i(windowwidth, 0);
        glVertex2i(windowwidth, windowheight);
        glVertex2i(0, windowheight);
        glEnd();

        glEnable(GL_TEXTURE_2D);
        glDisable(GL_CULL_FACE);
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        sf = 0;

        if (curtime - bagAnimTimer > bagAnimDuration)
        {
            for (int i = 0; i < 4; i++)
            {
                for (int j = 0; j < 10; j++)
                {
                    if (mx >= j * (32 + 8) + leftp && mx <= j * (32 + 8) + 32 + leftp &&
                        my >= i * (32 + 8) + upp && my <= i * (32 + 8) + 32 + upp)
                    {
                        csi = si = i;
                        csj = sj = j;
                        sf = 1;

                        if (mousebl == 0 && mouseb == 1 && itemselected == player::inventorybox[i][j])
                        {
                            if (player::inventorypcs[i][j] + pcsselected <= 255)
                            {
                                player::inventorypcs[i][j] += pcsselected;
                                pcsselected = 0;
                            }
                            else
                            {
                                pcsselected = player::inventorypcs[i][j] + pcsselected - 255;
                                player::inventorypcs[i][j] = 255;
                            }
                        }

                        if (mousebl == 0 && mouseb == 1 && itemselected != player::inventorybox[i][j])
                        {
                            std::swap(pcsselected, player::inventorypcs[i][j]);
                            std::swap(itemselected, player::inventorybox[i][j]);
                        }

                        if (mousebl == 0 && mouseb == 2 && itemselected == player::inventorybox[i][j] && player::inventorypcs[i][j] < 255)
                        {
                            pcsselected--;
                            player::inventorypcs[i][j]++;
                        }

                        if (mousebl == 0 && mouseb == 2 && player::inventorybox[i][j] == blocks::AIR)
                        {
                            pcsselected--;
                            player::inventorypcs[i][j] = 1;
                            player::inventorybox[i][j] = itemselected;
                        }

                        if (pcsselected == 0)
                        {
                            itemselected = blocks::AIR;
                        }

                        if (itemselected == blocks::AIR)
                        {
                            pcsselected = 0;
                        }

                        if (player::inventorypcs[i][j] == 0)
                        {
                            player::inventorybox[i][j] = blocks::AIR;
                        }

                        if (player::inventorybox[i][j] == blocks::AIR)
                        {
                            player::inventorypcs[i][j] = 0;
                        }
                    }
                }

                drawBagRow(i, (csi == i ? csj : -1), (windowwidth - 392) / 2, windowheight - 152 - 16 + i * 40, 8, 1.0f);
            }
        }

        if (itemselected != blocks::AIR)
        {
            glBindTexture(GL_TEXTURE_2D, BlockTextures);
            double tcX = Textures::getTexcoordX(itemselected, 1);
            double tcY = Textures::getTexcoordY(itemselected, 1);
            glBegin(GL_QUADS);
            glTexCoord2d(tcX, tcY + 1 / 8.0);
            glVertex2d(mx - 16, my - 16);
            glTexCoord2d(tcX + 1 / 8.0, tcY + 1 / 8.0);
            glVertex2d(mx + 16, my - 16);
            glTexCoord2d(tcX + 1 / 8.0, tcY);
            glVertex2d(mx + 16, my + 16);
            glTexCoord2d(tcX, tcY);
            glVertex2d(mx - 16, my + 16);
            glEnd();
            std::stringstream ss;
            ss << pcsselected;
            TextRenderer::renderString((int)mx - 16, (int)my - 16, ss.str());
        }

        if (player::inventorybox[si][sj] != 0 && sf == 1)
        {
            glColor4f(1.0, 1.0, 0.0, 1.0);
            TextRenderer::renderString((int)mx, (int)my - 16, BlockInfo(player::inventorybox[si][sj]).getBlockName());
        }

        int xbase = 0, ybase = 0, spac = 0;
        float alpha = 0.5f + 0.5f * bagAnim;

        if (curtime - bagAnimTimer <= bagAnimDuration)
        {
            xbase = (int)round(((windowwidth - 392) / 2) * bagAnim);
            ybase = (int)round((windowheight - 152 - 16 + 120 - (windowheight - 32)) * bagAnim + (windowheight - 32));
            spac = (int)round(8 * bagAnim);
            drawBagRow(3, -1, xbase, ybase, spac, alpha);
            xbase = (int)round(((windowwidth - 392) / 2 - windowwidth) * bagAnim + windowwidth);
            ybase = (int)round((windowheight - 152 - 16 - (windowheight - 32)) * bagAnim + (windowheight - 32));

            for (int i = 0; i < 3; i++)
            {
                glColor4f(1.0f, 1.0f, 1.0f, bagAnim);
                drawBagRow(i, -1, xbase, ybase + i * 40, spac, alpha);
            }
        }

        glEnable(GL_TEXTURE_2D);
        glDisable(GL_CULL_FACE);
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

        mousebl = mouseb;
    }
    else
    {

        glEnable(GL_TEXTURE_2D);
        glDisable(GL_CULL_FACE);

        if (curtime - bagAnimTimer <= bagAnimDuration)
        {
            glDisable(GL_TEXTURE_2D);
            glColor4f(0.2f, 0.2f, 0.2f, 0.6f - 0.6f * bagAnim);
            glBegin(GL_QUADS);
            glVertex2i(0, 0);
            glVertex2i(windowwidth, 0);
            glVertex2i(windowwidth, windowheight);
            glVertex2i(0, windowheight);
            glEnd();
            glEnable(GL_TEXTURE_2D);
            int xbase = 0, ybase = 0, spac = 0;
            float alpha = 1.0f - 0.5f * bagAnim;
            xbase = (int)round(((windowwidth - 392) / 2) - ((windowwidth - 392) / 2) * bagAnim);
            ybase = (int)round((windowheight - 152 - 16 + 120 - (windowheight - 32)) - (windowheight - 152 - 16 + 120 - (windowheight - 32)) * bagAnim + (windowheight - 32));
            spac = (int)round(8 - 8 * bagAnim);
            glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
            drawBagRow(3, player::itemInHand, xbase, ybase, spac, alpha);
            xbase = (int)round(((windowwidth - 392) / 2 - windowwidth) - ((windowwidth - 392) / 2 - windowwidth) * bagAnim + windowwidth);
            ybase = (int)round((windowheight - 152 - 16 - (windowheight - 32)) - (windowheight - 152 - 16 - (windowheight - 32)) * bagAnim + (windowheight - 32));

            for (int i = 0; i < 3; i++)
            {
                glColor4f(1.0f, 1.0f, 1.0f, 1.0f - bagAnim);
                drawBagRow(i, -1, xbase, ybase + i * 40, spac, alpha);
            }
        }
        else
        {
            drawBagRow(3, player::itemInHand, 0, windowheight - 32, 0, 0.5f);
        }
    }
}

void saveScreenshot(int x, int y, int w, int h, string filename)
{
    Textures::TEXTURE_RGB scrBuffer;
    w = static_cast<int>(ceil(static_cast<float>(w) / 4.0f)) * 4;
    h = static_cast<int>(ceil(static_cast<float>(h) / 4.0f)) * 4;
    scrBuffer.sizeX = w;
    scrBuffer.sizeY = h;
    scrBuffer.buffer = unique_ptr<ubyte[]>(new ubyte[w * h * 3]);
    glReadPixels(x, y, w, h, GL_RGB, GL_UNSIGNED_BYTE, scrBuffer.buffer.get());
    Textures::SaveRGBImage(filename, scrBuffer);
}

void createThumbnail()
{
    std::stringstream ss;
    ss << "Worlds/" << world::worldname << "/Thumbnail.bmp";
    saveScreenshot(0, 0, windowwidth, windowheight, ss.str());
}

void loadoptions()
{
    Json file = readJsonFromFile("./Configs/options.json");
    FOVyNormal = getJsonValue<float>(file["FOV"], 60.0f);
    viewdistance = getJsonValue<int>(file["RenderDistance"], 8);
    mousemove = getJsonValue<float>(file["Sensitivity"], 0.2f);
    cloudwidth = getJsonValue<int>(file["CloudWidth"], 10);
    SmoothLighting = getJsonValue<bool>(file["SmoothLighting"], true);
    NiceGrass = getJsonValue<bool>(file["FancyGrass"], true);
    MergeFace = getJsonValue<bool>(file["MergeFaceRendering"], false);
    GUIScreenBlur = getJsonValue<bool>(file["GUIBackgroundBlur"], true);
    TextRenderer::useUnicodeASCIIFont = getJsonValue<bool>(file["ForceUnicodeFont"], false);
}

void saveoptions()
{
    Json file;
    file["FOV"] = FOVyNormal;
    file["RenderDistance"] = viewdistance;
    file["Sensitivity"] = mousemove;
    file["CloudWidth"] = cloudwidth;
    file["SmoothLighting"] = SmoothLighting;
    file["FancyGrass"] = NiceGrass;
    file["MergeFaceRendering"] = MergeFace;
    file["GUIBackgroundBlur"] = GUIScreenBlur;
    file["ForceUnicodeFont"] = TextRenderer::useUnicodeASCIIFont;
    writeJsonToFile("./Configs/options.json", file);
}