module;

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>
#undef assert

export module neworld;
import std;
import types;
import commands;
import globals;
import frustum_tests;
import globalization;
import gui;
import menus;
import particles;
import rendering;
import setup;
import text_rendering;
import textures;
import worlds;
import vec3;
import mat4;
import menus;
import items;

//==============================   Initialize   ================================//
//==============================初始化(包括闪屏)================================//

void registerCommands();
void updateThreadFunc(worlds::World& world);
void gameUpdate(worlds::World& world);
void frameLinkedUpdate(worlds::World& world);

void render(worlds::World& world);
void readback(worlds::World& world);
void drawBorder(float x, float y, float z);
void drawBreaking(float level, float x, float y, float z);
void drawGUI(worlds::World& world);
void drawBagRow(player::Player& world, int row, int itemid, int xbase, int ybase, int spac, float alpha);
void drawBag(player::Player& world);

std::mutex updateMutex;
double updateTimer;
bool updateThreadRun;

double speedupAnimTimer;
double screenshotAnimTimer;
double bagAnimTimer;
double bagAnimDuration = 0.5;
bool shouldShowCursor = true;
bool shouldGetScreenshot;
bool shouldGetThumbnail;
bool shouldToggleFullscreen;

int fps, fpsc, ups, upsc;
double fctime, uctime;

bool paused = false;
bool bagOpened = false;
bool showHUD = true;
bool showDebugPanel = false;
bool showHitboxes = false;
bool showShadowMap = false;
bool showMeshWireframe = false;

int selx, sely, selz, oldselx, oldsely, oldselz, selface;
bool sel;
float selt, seldes;
blocks::Id selb;
bool selce;
float FOVyExt;

std::u32string chatword;
bool chatmode = false;
std::vector<Command> commands;
std::vector<std::string> chatMessages;

// Error: "extended character '。' is not valid in an identifier"
/*
#if 0
woca, 这样注释都行？！
(这儿编译不过去的童鞋，你的FB编译器版本貌似和我的不一样，把这几行注释掉吧。。。)
======================================
等等不对啊！！！明明都改成C++23了。。。还说是FB。。。
#endif
*/

bool keyDown[GLFW_KEY_LAST + 1];

bool isKeyDown(int key) {
    return glfwGetKey(MainWindow, key) == GLFW_PRESS;
}

bool isKeyPressed(int key) {
    if (key > GLFW_KEY_LAST || key <= 0)
        return false;
    bool down = glfwGetKey(MainWindow, key) == GLFW_PRESS;
    bool res = down && !keyDown[key];
    keyDown[key] = down;
    return res;
}

void updateKeyStates() {
    for (int i = 0; i <= GLFW_KEY_LAST; i++)
        keyDown[i] = (glfwGetKey(MainWindow, i) == GLFW_PRESS);
}

//==============================  Main Program  ================================//
//==============================     主程序     ================================//

export int main() {
    // 终于进入main函数了！激动人心的一刻！！！
    loadOptions();
    Globalization::Load();

    std::filesystem::create_directories("configs");
    std::filesystem::create_directories("worlds");
    std::filesystem::create_directories("screenshots");

    WindowWidth = DefaultWindowWidth;
    WindowHeight = DefaultWindowHeight;

    createWindow();
    splashScreen();
    loadTextures();
    register_base_blocks();
    registerCommands();

    // 菜单游戏循环
    while (!glfwWindowShouldClose(MainWindow)) {
        GameBegin = GameExit = false;
        GUI::clearTransition();
        Menus::mainmenu();

        // 初始化游戏状态
        spdlog::debug("Init world...");
        auto world = worlds::World(Cur_WorldName);

        // 初始化游戏更新线程
        updateMutex.lock();
        updateThreadRun = true;
        updateTimer = Timer();
        auto updateThread = std::thread(updateThreadFunc, std::ref(world));

        // 这才是游戏开始!
        spdlog::info("Main loop started");
        mxl = mx;
        myl = my;
        shouldShowCursor = false;
        shouldGetScreenshot = false;
        shouldGetThumbnail = false;
        shouldToggleFullscreen = false;
        fctime = uctime = Timer();

        // 主循环，被简化成这样，惨不忍睹啊！
        while (!glfwWindowShouldClose(MainWindow) && !GameExit) {
            // 等待上一帧完成后渲染下一帧
            updateMutex.unlock();
            glfwSwapBuffers(MainWindow); // 屏幕刷新，千万别删，后果自负！！！
            updateMutex.lock();
            readback(world);
            render(world);
            fpsc++;

            // 检测帧速率
            if (Timer() - fctime >= 1.0) {
                fps = fpsc;
                fpsc = 0;
                fctime = Timer();
            }

            // 检测更新速率
            if (Timer() - uctime >= 1.0) {
                uctime = Timer();
                ups = upsc;
                upsc = 0;
            }

            // 检测所有窗口事件
            glfwPollEvents();

            // 须每帧处理的输入和游戏更新
            frameLinkedUpdate(world);

            // 暂停菜单
            if (paused) {
                shouldGetThumbnail = true;
                readback(world);
                GUI::clearTransition();
                Menus::gamemenu();
                paused = false;
                mxl = mx;
                myl = my;
                updateTimer = fctime = uctime = Timer();
            }
        };

        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClearDepth(1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        TextRenderer::setFontColor(1.0, 1.0, 1.0, 1.0);
        TextRenderer::renderString(1, 1, "Saving world...");
        glfwSwapBuffers(MainWindow);

        // 停止游戏更新线程
        spdlog::info("Terminating threads");
        updateThreadRun = false;
        updateMutex.unlock();
        updateThread.join();

        // 保存并卸载世界
        world.save_to_files();
    }

    // 结束程序，删了也没关系 ←_←（吐槽FB和glfw中）
    // 不对啊这不是FB！！！这是正宗的C++23！！！！！！
    // 楼上的楼上在瞎说！！！别信他的！！！
    glfwTerminate();
    return 0;
    // This is the END of the program!
}

void updateThreadFunc(worlds::World& world) {
    updateMutex.lock();
    while (updateThreadRun) {
        double currTimer = Timer();
        while (currTimer - updateTimer >= 1.0 / 30.0) {
            if (upsc >= 60)
                updateTimer = currTimer;
            updateTimer += 1.0 / 30.0;
            upsc++;
            gameUpdate(world);
        }
        updateMutex.unlock();
        Sleep(1);
        updateMutex.lock();
    }
    updateMutex.unlock();
}

void registerCommands() {
    commands.emplace_back("/help", [](std::vector<std::string> const& command, worlds::World&) {
        if (command.size() != 1)
            return false;
        chatMessages.emplace_back(
            "Controls: W/A/S/D/SPACE/SHIFT = move, R/F = fast move (creative mode), E = open inventory,"
        );
        chatMessages.emplace_back(
            "          left/right mouse button = break/place blocks, mouse wheel = select blocks,"
        );
        chatMessages.emplace_back("          F1 = switch game mode, F2 = take screenshot, F3 = switch debug panel,");
        chatMessages.emplace_back("          F4 = switch cross wall (creative mode), F5 = switch HUD,");
        chatMessages.emplace_back("          F7 = switch full screen mode, F8 = fast forward game time");
        chatMessages.emplace_back(
            "Commands: /help | /clear | /kit | /give <id> <amount> | /tp <x> <y> <z> | /clearinventory | /suicide |"
        );
        chatMessages.emplace_back(
            "          /setblock <x> <y> <z> <id> | /tree <x> <y> <z> | /explode <x> <y> <z> <radius> | /time <time>"
        );
        return true;
    });
    commands.emplace_back("/clear", [](std::vector<std::string> const& command, worlds::World&) {
        if (command.size() != 1)
            return false;
        chatMessages.clear();
        return true;
    });
    commands.emplace_back("/kit", [](std::vector<std::string> const& command, worlds::World&) {
        if (command.size() != 1)
            return false;
        // player::inventory[0][0] = blocks::Id(1);
        // player::inventoryAmount[0][0] = 255;
        // player::inventory[0][1] = blocks::Id(2);
        // player::inventoryAmount[0][1] = 255;
        // player::inventory[0][2] = blocks::Id(3);
        // player::inventoryAmount[0][2] = 255;
        // player::inventory[0][3] = blocks::Id(4);
        // player::inventoryAmount[0][3] = 255;
        // player::inventory[0][4] = blocks::Id(5);
        // player::inventoryAmount[0][4] = 255;
        // player::inventory[0][5] = blocks::Id(6);
        // player::inventoryAmount[0][5] = 255;
        // player::inventory[0][6] = blocks::Id(7);
        // player::inventoryAmount[0][6] = 255;
        // player::inventory[0][7] = blocks::Id(8);
        // player::inventoryAmount[0][7] = 255;
        // player::inventory[0][8] = blocks::Id(9);
        // player::inventoryAmount[0][8] = 255;
        // player::inventory[0][9] = blocks::Id(10);
        // player::inventoryAmount[0][9] = 255;
        // player::inventory[1][0] = blocks::Id(11);
        // player::inventoryAmount[1][0] = 255;
        // player::inventory[1][1] = blocks::Id(12);
        // player::inventoryAmount[1][1] = 255;
        // player::inventory[1][2] = blocks::Id(13);
        // player::inventoryAmount[1][2] = 255;
        // player::inventory[1][3] = blocks::Id(14);
        // player::inventoryAmount[1][3] = 255;
        // player::inventory[1][4] = blocks::Id(15);
        // player::inventoryAmount[1][4] = 255;
        // player::inventory[1][5] = blocks::Id(16);
        // player::inventoryAmount[1][5] = 255;
        // player::inventory[1][6] = blocks::Id(17);
        // player::inventoryAmount[1][6] = 255;
        // player::inventory[1][7] = blocks::Id(18);
        // player::inventoryAmount[1][7] = 255;
        return true;
    });
    commands.emplace_back("/give", [](std::vector<std::string> const& command, worlds::World& w) {
        if (command.size() != 3)
            return false;
        auto itemid = blocks::Id(std::stoi(command[1]));
        auto amount = static_cast<size_t>(std::stoi(command[2]));
        w.player().addItem({itemid, amount});
        return true;
    });
    commands.emplace_back("/tp", [](std::vector<std::string> const& command, worlds::World& w) {
        if (command.size() != 4)
            return false;
        double x = std::stod(command[1]);
        double y = std::stod(command[2]);
        double z = std::stod(command[3]);
        w.player().setPosition({x, y, z});
        return true;
    });
    commands.emplace_back("/clearinventory", [](std::vector<std::string> const& command, worlds::World& w) {
        if (command.size() != 1)
            return false;
        w.player().clearInventory();
        return true;
    });
    commands.emplace_back("/suicide", [](std::vector<std::string> const& command, worlds::World& w) {
        if (command.size() != 1)
            return false;
        w.player().spawn();
        return true;
    });
    commands.emplace_back("/setblock", [](std::vector<std::string> const& command, worlds::World& world) {
        if (command.size() != 5)
            return false;
        int x = std::stoi(command[1]);
        int y = std::stoi(command[2]);
        int z = std::stoi(command[3]);
        auto b = blocks::Id(std::stoi(command[4]));
        world.put_block(Vec3i(x, y, z), b);
        return true;
    });
    commands.emplace_back("/tree", [](std::vector<std::string> const& command, worlds::World& world) {
        if (command.size() != 4)
            return false;
        int x = std::stoi(command[1]);
        int y = std::stoi(command[2]);
        int z = std::stoi(command[3]);
        world.build_tree(Vec3i(x, y, z));
        return true;
    });
    commands.emplace_back("/explode", [](std::vector<std::string> const& command, worlds::World& world) {
        if (command.size() != 5)
            return false;
        int x = std::stoi(command[1]);
        int y = std::stoi(command[2]);
        int z = std::stoi(command[3]);
        int r = std::stoi(command[4]);
        world.explode(Vec3i(x, y, z), r);
        return true;
    });
    commands.emplace_back("/time", [](std::vector<std::string> const& command, worlds::World&) {
        if (command.size() != 2)
            return false;
        int time = std::stoi(command[1]);
        if (time < 0)
            return false;
        GameTime = time;
        return true;
    });
    commands.emplace_back("/gamemode", [](std::vector<std::string> const& command, worlds::World& w) {
        if (command.size() != 2)
            return false;
        auto mode = static_cast<player::Player::GameMode>(std::stoi(command[1]));
        w.player().changeGameMode(mode);
        return true;
    });
}

bool doCommand(std::vector<std::string> const& command, worlds::World& world) {
    for (unsigned int i = 0; i != commands.size(); i++) {
        if (command[0] == commands[i].identifier)
            return commands[i].execute(command, world);
    }
    return false;
}

void gameUpdate(worlds::World& world) {
    int const SelectPrecision = 32;
    int const SelectDistance = 8;

    static double wPressTimer;
    static bool wPressedOnce;

    auto& player = world.player();
    player.update();

    // 时间
    GameTime++;

    // Move chunk pointer array and height map
    world.set_center(player.chunkCoord());

    for (auto const& [_, c]: world.chunks()) {
        // 加载动画
        c->update_load_anim();

        // 随机状态更新
        if (rnd() < 1.0 / 8.0) {
            int x, y, z, gx, gy, gz;
            int cx = c->coord().x;
            int cy = c->coord().y;
            int cz = c->coord().z;
            x = int(rnd() * chunks::Chunk::SIZE);
            gx = x + cx * chunks::Chunk::SIZE;
            y = int(rnd() * chunks::Chunk::SIZE);
            gy = y + cy * chunks::Chunk::SIZE;
            z = int(rnd() * chunks::Chunk::SIZE);
            gz = z + cz * chunks::Chunk::SIZE;
            if (c->block(Vec3u(x, y, z)).id == base_blocks().dirt
                && world.block_or_air(Vec3i(gx, gy + 1, gz)).id == base_blocks().air
                && (world.block_or_air(Vec3i(gx + 1, gy, gz)).id == base_blocks().grass
                    || world.block_or_air(Vec3i(gx - 1, gy, gz)).id == base_blocks().grass
                    || world.block_or_air(Vec3i(gx, gy, gz + 1)).id == base_blocks().grass
                    || world.block_or_air(Vec3i(gx, gy, gz - 1)).id == base_blocks().grass
                    || world.block_or_air(Vec3i(gx + 1, gy + 1, gz)).id == base_blocks().grass
                    || world.block_or_air(Vec3i(gx - 1, gy + 1, gz)).id == base_blocks().grass
                    || world.block_or_air(Vec3i(gx, gy + 1, gz + 1)).id == base_blocks().grass
                    || world.block_or_air(Vec3i(gx, gy + 1, gz - 1)).id == base_blocks().grass
                    || world.block_or_air(Vec3i(gx + 1, gy - 1, gz)).id == base_blocks().grass
                    || world.block_or_air(Vec3i(gx - 1, gy - 1, gz)).id == base_blocks().grass
                    || world.block_or_air(Vec3i(gx, gy - 1, gz + 1)).id == base_blocks().grass
                    || world.block_or_air(Vec3i(gx, gy - 1, gz - 1)).id == base_blocks().grass)) {
                // 长草
                c->block_ref(Vec3u(x, y, z)).id = base_blocks().grass;
                world.update_block(Vec3i(gx, gy, gz), true);
            }
            if (c->block(Vec3u(x, y, z)).id == base_blocks().grass
                && world.block_or_air(Vec3i(gx, gy + 1, gz)).id != base_blocks().air) {
                // 草被覆盖
                c->block_ref(Vec3u(x, y, z)).id = base_blocks().dirt;
                world.update_block(Vec3i(gx, gy, gz), true);
            }
        }
    }

    // 更新世界方块
    updated_blocks = 0;
    world.process_block_updates();

    // 判断选中的方块
    sel = false;
    selx = sely = selz = 0;
    selb = base_blocks().air;

    if (chatmode) {
        shouldShowCursor = true;

        if (isKeyPressed(GLFW_KEY_ENTER)) {
            chatmode = false;
            mxl = mx;
            myl = my;
            mwl = mw;
            mbl = mb;
            if (!chatword.empty()) {      // 指令的执行，或发出聊天文本
                if (chatword[0] == '/') { // 指令
                    auto utf8 = UnicodeUTF8(chatword);
                    auto command = split(utf8, " ");
                    if (!doCommand(command, world)) { // 执行失败
                        spdlog::warn("Fail to execute the command: {}", utf8);
                        chatMessages.push_back("Fail to execute the command: " + utf8);
                    }
                } else
                    chatMessages.push_back(UnicodeUTF8(chatword));
            }
            chatword.clear();
        }

        if (!inputstr.empty())
            chatword += inputstr;
        if (backspace && !chatword.empty())
            chatword = chatword.substr(0, chatword.length() - 1);

        // 自动补全
        if (isKeyPressed(GLFW_KEY_TAB) && chatmode && !chatword.empty() && chatword[0] == '/') {
            for (auto& command: commands) {
                if (command.identifier.starts_with(UnicodeUTF8(chatword)))
                    chatword = UTF8Unicode(command.identifier);
            }
        }
    } else if (bagOpened) {
        shouldShowCursor = true;

        if (isKeyPressed(GLFW_KEY_E)) {
            bagOpened = false;
            bagAnimTimer = Timer();
            mxl = mx;
            myl = my;
            mwl = mw;
            mbl = mb;
        }
    } else {
        shouldShowCursor = false;

        // 从玩家位置发射一条线段
        Vec3d lPos = player.lookCoord();
        Vec3d lookPosOld = lPos;
        for (int i = 0; i < SelectPrecision * SelectDistance; i++) {
            lookPosOld = lPos;

            // 线段延伸
            lPos.x += std::sin(Pi / 180 * (player.getHeading() - 180))
                    * std::sin(Pi / 180 * (player.getLookUpDown() + 90)) / SelectPrecision;
            lPos.y += std::cos(Pi / 180 * (player.getLookUpDown() + 90)) / SelectPrecision;
            lPos.z += std::cos(Pi / 180 * (player.getHeading() - 180))
                    * std::sin(Pi / 180 * (player.getLookUpDown() + 90)) / SelectPrecision;

            // 碰到方块
            auto coord = Vec3i(lPos.map<int>([](double x) { return std::lround(x); }));
            auto coordl = Vec3i(lookPosOld.map<int>([](double x) { return std::lround(x); }));
            if (block_info(world.block_or_air(coord).id).solid) {
                selx = coord.x;
                sely = coord.y;
                selz = coord.z;
                sel = true;

                selb = world.block_or_air(coord).id;
                if (mb == 1) { // 鼠标左键
                    particles::throw_particle(
                        selb,
                        float(coord.x + rnd() - 0.5f),
                        float(coord.y + rnd() - 0.2f),
                        float(coord.z + rnd() - 0.5f),
                        float(rnd() * 0.2f - 0.1f),
                        float(rnd() * 0.2f - 0.1f),
                        float(rnd() * 0.2f - 0.1f),
                        float(rnd() * 0.01f + 0.02f),
                        int(rnd() * 30) + 30
                    );

                    float const MinHardness = 0.2f;
                    if (selx != oldselx || sely != oldsely || selz != oldselz)
                        seldes = 0.0f;
                    else if (player.getGameMode() == player::Player::GameMode::Creative)
                        seldes += 1.0f / 30.0f / MinHardness;
                    else if (block_info(selb).hardness <= MinHardness)
                        seldes += 1.0f / 30.0f / MinHardness;
                    else
                        seldes += 1.0f / 30.0f / block_info(selb).hardness;

                    if (seldes >= 1.0f) {
                        player.addItem({selb, 1});
                        for (int j = 1; j <= 25; j++) {
                            particles::throw_particle(
                                selb,
                                float(coord.x + rnd() - 0.5f),
                                float(coord.y + rnd() - 0.2f),
                                float(coord.z + rnd() - 0.5f),
                                float(rnd() * 0.2f - 0.1f),
                                float(rnd() * 0.2f - 0.1f),
                                float(rnd() * 0.2f - 0.1f),
                                float(rnd() * 0.02 + 0.03),
                                int(rnd() * 60) + 30
                            );
                        }
                        world.put_block(coord, base_blocks().air);
                    }
                }
                if (mb == 2 && mbp == false) { // 鼠标右键
                    auto& holdingItem = player.holdingItemStack();
                    if (!holdingItem.empty()) {
                        // 放置方块
                        if (player.putBlock(world, coordl, holdingItem.id)) {
                            if (--holdingItem.count == 0)
                                holdingItem = {};
                        }
                    } else {
                        // 使用物品
                    }
                }
                break;
            }
        }

        if (selx != oldselx || sely != oldsely || selz != oldselz || mb == 0)
            seldes = 0.0f;
        oldselx = selx;
        oldsely = sely;
        oldselz = selz;

        // 更新方向
        player.updateHeadingAndLookUpDown();

        // 移动！(生命在于运动)
        if (isKeyDown(GLFW_KEY_W)) {
            if (!wPressedOnce) {
                if (wPressTimer == 0.0) {
                    wPressTimer = Timer();
                } else {
                    if (Timer() - wPressTimer <= 0.5) {
                        player.setRunning(true);
                        wPressTimer = 0.0;
                    } else
                        wPressTimer = Timer();
                }
            }
            if (wPressTimer != 0.0 && Timer() - wPressTimer > 0.5)
                wPressTimer = 0.0;
            wPressedOnce = true;
            auto velocity = player.getVelocity();
            player.setVelocity({
                velocity.x - std::sin(player.getHeading() * Pi / 180.0) * player.getSpeed(),
                velocity.y,
                velocity.z - std::cos(player.getHeading() * Pi / 180.0) * player.getSpeed(),
            });
        } else {
            player.setRunning(false);
            wPressedOnce = false;
        }

        if (isKeyDown(GLFW_KEY_S)) {
            auto velocity = player.getVelocity();
            player.setVelocity({
                velocity.x + std::sin(player.getHeading() * Pi / 180.0) * player.getSpeed(),
                velocity.y,
                velocity.z + std::cos(player.getHeading() * Pi / 180.0) * player.getSpeed(),
            });
            wPressTimer = 0.0;
        }

        if (isKeyDown(GLFW_KEY_A)) {
            auto velocity = player.getVelocity();
            player.setVelocity({
                velocity.x + std::sin((player.getHeading() - 90) * Pi / 180.0) * player.getSpeed(),
                velocity.y,
                velocity.z + std::cos((player.getHeading() - 90) * Pi / 180.0) * player.getSpeed(),
            });
            wPressTimer = 0.0;
        }

        if (isKeyDown(GLFW_KEY_D)) {
            auto velocity = player.getVelocity();
            player.setVelocity({
                velocity.x - std::sin((player.getHeading() - 90) * Pi / 180.0) * player.getSpeed(),
                velocity.y,
                velocity.z - std::cos((player.getHeading() - 90) * Pi / 180.0) * player.getSpeed(),
            });
            wPressTimer = 0.0;
        }

        if (!player.isFlying() && !player.canCrossWall()) {
            Vec3d velocity = player.getVelocity();
            double horizontalSpeed = std::sqrt(velocity.x * velocity.x + velocity.z * velocity.z);
            if (horizontalSpeed > player.getSpeed()) {
                player.setVelocity(
                    {velocity.x * player.getSpeed() / horizontalSpeed,
                     velocity.y,
                     velocity.z * player.getSpeed() / horizontalSpeed}
                );
            }
        } else {
            if (isKeyDown(GLFW_KEY_R)) {
                Vec3d velocity = player.getVelocity();
                if (isKeyDown(GLFW_KEY_LEFT_CONTROL)) {
                    player.setVelocity(
                        {velocity.x - std::sin(player.getHeading() * Pi / 180.0) * player.getSpeed() * 10,
                         velocity.y,
                         velocity.z - std::cos(player.getHeading() * Pi / 180.0) * player.getSpeed() * 10}
                    );
                } else {
                    player.setVelocity(
                        {std::sin(Pi / 180 * (player.getHeading() - 180))
                             * std::sin(Pi / 180 * (player.getLookUpDown() + 90)) * player.getSpeed() * 20,
                         std::cos(Pi / 180 * (player.getLookUpDown() + 90)) * player.getSpeed() * 20,
                         std::cos(Pi / 180 * (player.getHeading() - 180))
                             * std::sin(Pi / 180 * (player.getLookUpDown() + 90)) * player.getSpeed() * 20}
                    );
                }
            }
        }

        // 切换方块
        if (isKeyPressed(GLFW_KEY_Z))
            player.setIndexInHand(player.getIndexInHand() - 1);
        if (isKeyPressed(GLFW_KEY_X))
            player.setIndexInHand(player.getIndexInHand() + 1);
        player.setIndexInHand(player.getIndexInHand() + (mwl - mw));
        mwl = mw;

        // 起跳/上升
        if (isKeyDown(GLFW_KEY_SPACE)) {
            player.processJump(isKeyPressed(GLFW_KEY_SPACE));
        }

        if ((isKeyDown(GLFW_KEY_LEFT_SHIFT) || isKeyDown(GLFW_KEY_RIGHT_SHIFT))) {
            if (player.canCrossWall() || player.isFlying())
                player.setVelocity(player.getVelocity() + Vec3d{0, -player.getSpeed() / 2, 0});
            wPressTimer = 0.0;
        }

        // Open inventory
        if (isKeyPressed(GLFW_KEY_E) && showHUD) {
            bagOpened = true;
            bagAnimTimer = Timer();
            shouldGetThumbnail = true;
        }

        // Save world
        if (isKeyPressed(GLFW_KEY_L))
            world.save_to_files();

        // 各种设置切换
        if (isKeyPressed(GLFW_KEY_F1)) {
            player.changeGameMode(
                player.getGameMode() == player::Player::GameMode::Creative ? player::Player::GameMode::Survival
                                                                           : player::Player::GameMode::Creative
            );
        }
        if (isKeyPressed(GLFW_KEY_F2)) {
            shouldGetScreenshot = true;
            screenshotAnimTimer = Timer();
        }
        if (isKeyPressed(GLFW_KEY_F3))
            showDebugPanel = !showDebugPanel;
        if (isKeyPressed(GLFW_KEY_H) && isKeyDown(GLFW_KEY_F3)) {
            showHitboxes = !showHitboxes;
            showDebugPanel = true;
        }
        if (AdvancedRender) {
            if (isKeyPressed(GLFW_KEY_M) && isKeyDown(GLFW_KEY_F3)) {
                showShadowMap = !showShadowMap;
                showDebugPanel = true;
            }
        } else
            showShadowMap = false;
        if (isKeyPressed(GLFW_KEY_G) && isKeyDown(GLFW_KEY_F3)) {
            showMeshWireframe = !showMeshWireframe;
            showDebugPanel = true;
        }
        if (isKeyPressed(GLFW_KEY_F4) && player.getGameMode() == player::Player::GameMode::Creative)
            player.setCanCrossWall(!player.canCrossWall());
        if (isKeyPressed(GLFW_KEY_F5))
            showHUD = !showHUD;
        if (isKeyPressed(GLFW_KEY_F7))
            shouldToggleFullscreen = true;
        if (isKeyDown(GLFW_KEY_F8))
            GameTime += 30;
        if (isKeyPressed(GLFW_KEY_ENTER))
            chatmode = true;
        if (isKeyPressed(GLFW_KEY_SLASH)) {
            chatmode = true;
            chatword = UTF8Unicode("/");
        }
    }

    // 爬墙
    // if (Player::NearWall && Player::Flying == false && Player::CrossWall == false) {
    //  Player::ya += walkspeed
    //  Player::jump = 0.0
    //}

    mbp = mb;
    updateKeyStates();
    inputstr.clear();
    backspace = false;

    particles::update_all(world);

    static double timer = Timer();
    double const currentTime = Timer();
    player.updatePosition(world, currentTime - timer);
    timer = currentTime;
}

void frameLinkedUpdate(worlds::World& world) {
    // Find chunks for unloading & loading & meshing
    auto& player = world.player();
    auto center = Vec3i(player.getPosition());
    world.update_chunk_lists(center);

    // Load chunks
    world.process_chunk_loads();

    // Unload chunks
    world.process_chunk_unloads();

    // Mesh updated chunks
    world.process_chunk_meshings();

    // 处理计时
    double currTimer = Timer();

    // 视野特效
    if (player.isRunning()) {
        if (FOVyExt < 9.8f) {
            float timeDelta = static_cast<float>(currTimer - speedupAnimTimer);
            FOVyExt = 10.0f - (10.0f - FOVyExt) * std::pow(0.8f, timeDelta * 30.0f);
        } else
            FOVyExt = 10.0f;
    } else {
        if (FOVyExt > 0.2f) {
            float timeDelta = static_cast<float>(currTimer - speedupAnimTimer);
            FOVyExt *= std::pow(0.8f, timeDelta * 30.0f);
        } else
            FOVyExt = 0.0f;
    }
    speedupAnimTimer = currTimer;

    // 鼠标状态和位置
    double mxd, myd;
    glfwSetInputMode(MainWindow, GLFW_CURSOR, shouldShowCursor ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
    glfwGetCursorPos(MainWindow, &mxd, &myd);
    mxl = mx;
    myl = my;
    mx = static_cast<int>(mxd);
    my = static_cast<int>(myd);

    // 转头！你治好了我多年的颈椎病！
    if (!shouldShowCursor) {
        if (mx != mxl)
            player.setXLookSpeed(player.getXLookSpeed() - (mx - mxl) * MouseSpeed);
        if (my != myl)
            player.setYLookSpeed(player.getYLookSpeed() + (my - myl) * MouseSpeed);
    }

    // 切换全屏
    if (shouldToggleFullscreen) {
        shouldToggleFullscreen = false;
        toggleFullScreen();
    }

    // 暂停按键
    if (isKeyDown(GLFW_KEY_ESCAPE)) {
        shouldGetThumbnail = true;
        paused = true;
    }
}

// Render the whole scene and HUD
void render(worlds::World& world) {
    float const SkyColorR = 0.70f;
    float const SkyColorG = 0.80f;
    float const SkyColorB = 0.86f;

    auto& player = world.player();
    double currTimer = Timer();
    double interp = (currTimer - updateTimer) * 30.0;
    Vec3d view_coord = player.lookCoord() - (player.getPosition() - player.getPositionOld()) * (1 - interp);

    // Calculate sun position (temporary: horizontal movement only)
    float interpolatedTime = GameTime - 1.0f + static_cast<float>(interp);
    Renderer::sunlightHeading = interpolatedTime / 43200.0f * 360.0f;

    // World rendering starts here
    double plookupdown = player.getLookUpDown() + player.getYLookSpeed();
    double pheading = player.getHeading() + player.getXLookSpeed();

    // Calculate matrices
    auto viewMatrix = Mat4f(1.0f);
    viewMatrix = Mat4f::rotate(-static_cast<float>(pheading * Pi / 180.0), Vec3f(0.0f, 1.0f, 0.0f)) * viewMatrix;
    viewMatrix = Mat4f::rotate(static_cast<float>(plookupdown * Pi / 180.0), Vec3f(1.0f, 0.0f, 0.0f)) * viewMatrix;
    viewMatrix = Mat4f::perspective(
                     static_cast<float>((FOVyNormal + FOVyExt) * Pi / 180.0),
                     static_cast<float>(WindowWidth) / WindowHeight,
                     0.05f,
                     static_cast<float>(RenderDistance * chunks::Chunk::SIZE)
                 )
               * viewMatrix;

    // Clear framebuffers
    if (AdvancedRender)
        Renderer::ClearSGDBuffers();

    glClearColor(SkyColorR, SkyColorG, SkyColorB, 1.0f);
    glClearDepth(1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Bind main texture array
    glBindTexture(GL_TEXTURE_2D_ARRAY, BlockTextureArray);

    if (showMeshWireframe)
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    // Build shadow map
    auto shadowMatrix = Renderer::getShadowMatrix();
    if (AdvancedRender) {
        auto shadowMatrixExp = Renderer::getShadowMatrixExperimental(
            FOVyNormal + FOVyExt,
            static_cast<float>(WindowWidth) / WindowHeight,
            pheading,
            plookupdown
        );
        auto shadowMatrixTest = FrustumTest(shadowMatrixExp);
        auto list = world.list_render_chunks(view_coord, Renderer::getShadowDistance(), interp, shadowMatrixTest);
        Renderer::StartShadowPass(shadowMatrix, interpolatedTime);
        world.render_chunks(view_coord, list, 0);
        Renderer::shaders[Renderer::ActiveShader].setUniform("u_translation", Vec3f(0.0f));
        particles::render_all(world, view_coord, interp);
        Renderer::EndShadowPass();
    }

    // Draw the opaque parts of the world
    auto list = world.list_render_chunks(view_coord, RenderDistance, interp, FrustumTest(viewMatrix));
    rendered_chunks = static_cast<int>(list.size());
    Renderer::StartOpaquePass(viewMatrix, interpolatedTime);
    world.render_chunks(view_coord, list, 0);
    Renderer::shaders[Renderer::ActiveShader].setUniform("u_translation", Vec3f(0.0f));
    particles::render_all(world, view_coord, interp);
    Renderer::EndOpaquePass();

    // Draw the translucent parts of the world
    Renderer::StartTranslucentPass(viewMatrix, interpolatedTime);
    glDisable(GL_CULL_FACE);
    if (sel) {
        float x = static_cast<float>(selx - view_coord.x);
        float y = static_cast<float>(sely - view_coord.y);
        float z = static_cast<float>(selz - view_coord.z);
        Renderer::shaders[Renderer::ActiveShader].setUniform("u_translation", Vec3f(x, y, z));
        if (showHUD) {
            // Temporary solution pre GL 4.0 (glBlendFuncSeparatei)
            glColorMaski(0, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
            drawBorder(0, 0, 0);
            glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            glColorMaski(1, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
            glColorMaski(2, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
            glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ZERO);
            drawBorder(0, 0, 0);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            glColorMaski(1, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            glColorMaski(2, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        }
        drawBreaking(seldes, 0, 0, 0);
    }
    world.render_chunks(view_coord, list, 1);
    glEnable(GL_CULL_FACE);
    Renderer::EndTranslucentPass();

    if (showMeshWireframe)
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    // Full screen passes
    if (AdvancedRender) {
        Renderer::StartFinalPass(view_coord.x, view_coord.y, view_coord.z, viewMatrix, shadowMatrix, interpolatedTime);
        Renderer::Begin(GL_QUADS, 2, 2, 0);
        Renderer::TexCoord2f(0.0f, 1.0f);
        Renderer::Vertex2i(0, 0);
        Renderer::TexCoord2f(0.0f, 0.0f);
        Renderer::Vertex2i(0, WindowHeight);
        Renderer::TexCoord2f(1.0f, 0.0f);
        Renderer::Vertex2i(WindowWidth, WindowHeight);
        Renderer::TexCoord2f(1.0f, 1.0f);
        Renderer::Vertex2i(WindowWidth, 0);
        Renderer::End().render();
        Renderer::EndFinalPass();
    }

    /*
    if (DebugHitbox) {
        glLoadIdentity();
        glRotated(plookupdown, 1, 0, 0);
        glRotated(360.0 - pheading, 0, 1, 0);
        glTranslated(-xpos, -ypos, -zpos);

        glDisable(GL_CULL_FACE);
        glDisable(GL_TEXTURE_2D);

        for (unsigned int i = 0; i < Player::Hitboxes.size(); i++)
            Hitbox::renderAABB(Player::Hitboxes[i], GUI::FgR, GUI::FgG, GUI::FgB, 3, 0.002);

        glLoadIdentity();
        glRotated(plookupdown, 1, 0, 0);
        glRotated(360.0 - pheading, 0, 1, 0);
        glTranslated(-Player::xpos, -Player::ypos - Player::height - Player::heightExt, -Player::zpos);

        Hitbox::renderAABB(Player::playerbox, 1.0f, 1.0f, 1.0f, 1);
        Hitbox::renderAABB(Hitbox::Expand(Player::playerbox, Player::xd, Player::yd, Player::zd), 1.0f, 1.0f, 1.0f, 1);

        glEnable(GL_CULL_FACE);
        glEnable(GL_TEXTURE_2D);
    }
    */

    // HUD rendering starts here
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, WindowWidth, WindowHeight, 0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glClearDepth(1.0f);
    glClear(GL_DEPTH_BUFFER_BIT);

    if (world.block_or_air(Vec3i(view_coord)).id == base_blocks().water) {
        auto& shader = Renderer::shaders[Renderer::UIShader];
        shader.bind();
        glBindTexture(GL_TEXTURE_2D_ARRAY, BlockTextureArray);
        Renderer::Begin(GL_QUADS, 2, 3, 4);
        Renderer::Color4f(1.0f, 1.0f, 1.0f, 1.0f);
        Renderer::TexCoord3f(0.0f, 0.0f, static_cast<float>(Textures::getTextureIndex(base_blocks().water, 0)));
        Renderer::TexCoord2f(0.0f, 1.0f);
        Renderer::Vertex2i(0, 0);
        Renderer::TexCoord2f(0.0f, 0.0f);
        Renderer::Vertex2i(0, WindowHeight);
        Renderer::TexCoord2f(1.0f, 0.0f);
        Renderer::Vertex2i(WindowWidth, WindowHeight);
        Renderer::TexCoord2f(1.0f, 1.0f);
        Renderer::Vertex2i(WindowWidth, 0);
        Renderer::End().render();
        shader.unbind();
    }

    if (showHUD) {
        drawGUI(world);
        drawBag(world.player());
    }

    if (currTimer - screenshotAnimTimer <= 1.0 && !shouldGetScreenshot) {
        float col = 1.0f - static_cast<float>(currTimer - screenshotAnimTimer);
        glDisable(GL_TEXTURE_2D);
        glBegin(GL_QUADS);
        glColor4f(1.0f, 1.0f, 1.0f, col);
        glVertex2i(0, 0);
        glVertex2i(0, WindowHeight);
        glVertex2i(WindowWidth, WindowHeight);
        glVertex2i(WindowWidth, 0);
        glEnd();
        glEnable(GL_TEXTURE_2D);
    }
}

void saveScreenshot(int x, int y, int w, int h, std::string filename) {
    Textures::ImageRGB scrBuffer;
    int bufw = w, bufh = h;
    while (bufw % 4 != 0)
        bufw += 1;
    while (bufh % 4 != 0)
        bufh += 1;
    scrBuffer.sizeX = bufw;
    scrBuffer.sizeY = bufh;
    scrBuffer.buffer = std::make_unique<uint8_t[]>(bufw * bufh * 3);
    glReadBuffer(GL_FRONT);
    glReadPixels(x, y, bufw, bufh, GL_RGB, GL_UNSIGNED_BYTE, scrBuffer.buffer.get());
    Textures::SaveRGBImage(filename, scrBuffer);
}

void readback(worlds::World& world) {
    if (shouldGetScreenshot) {
        shouldGetScreenshot = false;
        auto now = std::chrono::system_clock::now();
        auto nowTime = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << "screenshots/" << std::put_time(std::localtime(&nowTime), "%Y-%m-%d %H:%M:%S") << ".bmp";
        saveScreenshot(0, 0, WindowWidth, WindowHeight, ss.str());
    }

    if (shouldGetThumbnail) {
        shouldGetThumbnail = false;
        std::stringstream ss;
        ss << "worlds/" << world.name() << "/thumbnail.bmp";
        saveScreenshot(0, 0, WindowWidth, WindowHeight, ss.str());
    }
}

float const centers[6][3] = {
    {+0.5f,  0.0f,  0.0f},
    {-0.5f,  0.0f,  0.0f},
    { 0.0f, +0.5f,  0.0f},
    { 0.0f, -0.5f,  0.0f},
    { 0.0f,  0.0f, +0.5f},
    { 0.0f,  0.0f, -0.5f},
};

float const cube[6][4][3] = {
    {{+0.5f, -0.5f, +0.5f}, {+0.5f, -0.5f, -0.5f}, {+0.5f, +0.5f, -0.5f}, {+0.5f, +0.5f, +0.5f}},
    {{-0.5f, -0.5f, -0.5f}, {-0.5f, -0.5f, +0.5f}, {-0.5f, +0.5f, +0.5f}, {-0.5f, +0.5f, -0.5f}},
    {{-0.5f, +0.5f, +0.5f}, {+0.5f, +0.5f, +0.5f}, {+0.5f, +0.5f, -0.5f}, {-0.5f, +0.5f, -0.5f}},
    {{-0.5f, -0.5f, -0.5f}, {+0.5f, -0.5f, -0.5f}, {+0.5f, -0.5f, +0.5f}, {-0.5f, -0.5f, +0.5f}},
    {{-0.5f, -0.5f, +0.5f}, {+0.5f, -0.5f, +0.5f}, {+0.5f, +0.5f, +0.5f}, {-0.5f, +0.5f, +0.5f}},
    {{+0.5f, -0.5f, -0.5f}, {-0.5f, -0.5f, -0.5f}, {-0.5f, +0.5f, -0.5f}, {+0.5f, +0.5f, -0.5f}},
};

float const texcoords[4][2] = {
    {0.0f, 0.0f},
    {1.0f, 0.0f},
    {1.0f, 1.0f},
    {0.0f, 1.0f},
};

// Draw the block selection border
void drawBorder(float x, float y, float z) {
    float const eps = 0.005f;
    float const width = 1.0f / 32.0f;

    if (AdvancedRender)
        Renderer::Begin(GL_QUADS, 3, 3, 1, 3, 1);
    else
        Renderer::Begin(GL_QUADS, 3, 3, 1);
    Renderer::Attrib1f(65535.0f); // For indicator elements
    Renderer::TexCoord3f(0.0f, 0.0f, static_cast<float>(TextureIndex::WHITE));
    Renderer::Color3f(1.0f, 1.0f, 1.0f);
    for (int i = 0; i < 6; i++) {
        float const* center = centers[i];
        float xc = center[0], yc = center[1], zc = center[2];
        Renderer::Normal3f(xc * 2.0f, yc * 2.0f, zc * 2.0f);
        for (int j = 0; j < 4; j++) {
            float const* first = cube[i][j];
            float const* second = cube[i][(j + 1) % 4];
            float x0 = first[0], y0 = first[1], z0 = first[2];
            float x1 = second[0], y1 = second[1], z1 = second[2];
            float xd0 = (x0 - xc) * 2.0f, yd0 = (y0 - yc) * 2.0f, zd0 = (z0 - zc) * 2.0f;
            float xd1 = (x1 - xc) * 2.0f, yd1 = (y1 - yc) * 2.0f, zd1 = (z1 - zc) * 2.0f;
            x0 += (x0 > 0.0f ? eps : -eps), y0 += (y0 > 0.0f ? eps : -eps), z0 += (z0 > 0.0f ? eps : -eps);
            x1 += (x1 > 0.0f ? eps : -eps), y1 += (y1 > 0.0f ? eps : -eps), z1 += (z1 > 0.0f ? eps : -eps);
            Renderer::Vertex3f(x + x0, y + y0, z + z0);
            Renderer::Vertex3f(x + x1, y + y1, z + z1);
            Renderer::Vertex3f(x + x1 - xd1 * width, y + y1 - yd1 * width, z + z1 - zd1 * width);
            Renderer::Vertex3f(x + x0 - xd0 * width, y + y0 - yd0 * width, z + z0 - zd0 * width);
        }
    }
    Renderer::End().render();
}

void drawBreaking(float level, float x, float y, float z) {
    float const eps = 0.005f;

    if (level <= 0.0f)
        return;
    int index = int(level * 8);
    if (index < 0)
        index = 0;
    if (index > 7)
        index = 7;

    if (AdvancedRender)
        Renderer::Begin(GL_QUADS, 3, 3, 1, 3, 1);
    else
        Renderer::Begin(GL_QUADS, 3, 3, 1);
    Renderer::Attrib1f(65535.0f); // For indicator elements
    Renderer::TexCoord3f(0.0f, 0.0f, static_cast<float>(TextureIndex::BREAKING_0) + static_cast<float>(index));
    Renderer::Color3f(1.0f, 1.0f, 1.0f);
    for (int i = 0; i < 6; i++) {
        float const* center = centers[i];
        float xc = center[0], yc = center[1], zc = center[2];
        Renderer::Normal3f(xc * 2.0f, yc * 2.0f, zc * 2.0f);
        for (int j = 0; j < 4; j++) {
            float const* texcoord = texcoords[j];
            float const* point = cube[i][j];
            float u0 = texcoord[0], v0 = texcoord[1];
            float x0 = point[0], y0 = point[1], z0 = point[2];
            x0 += (x0 > 0.0f ? eps : -eps), y0 += (y0 > 0.0f ? eps : -eps), z0 += (z0 > 0.0f ? eps : -eps);
            Renderer::TexCoord2f(u0, v0);
            Renderer::Vertex3f(x + x0, y + y0, z + z0);
        }
    }
    Renderer::End().render();
}

void drawGUI(worlds::World& world) {
    int const linelength = 10;
    int const linedist = 30;
    int disti = (int) (seldes * linedist);
    auto& player = world.player();

    glDisable(GL_TEXTURE_2D);
    glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ZERO);

    if (showDebugPanel) {
        glBegin(GL_LINES);
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        glVertex2i(WindowWidth / 2 - linedist + disti, WindowHeight / 2 - linedist + disti);
        glVertex2i(WindowWidth / 2 - linedist + disti, WindowHeight / 2 - linedist + linelength + disti);
        glVertex2i(WindowWidth / 2 - linedist + disti, WindowHeight / 2 - linedist + disti);
        glVertex2i(WindowWidth / 2 - linedist + linelength + disti, WindowHeight / 2 - linedist + disti);
        glVertex2i(WindowWidth / 2 + linedist - disti, WindowHeight / 2 - linedist + disti);
        glVertex2i(WindowWidth / 2 + linedist - disti, WindowHeight / 2 - linedist + linelength + disti);
        glVertex2i(WindowWidth / 2 + linedist - disti, WindowHeight / 2 - linedist + disti);
        glVertex2i(WindowWidth / 2 + linedist - linelength - disti, WindowHeight / 2 - linedist + disti);
        glVertex2i(WindowWidth / 2 - linedist + disti, WindowHeight / 2 + linedist - disti);
        glVertex2i(WindowWidth / 2 - linedist + disti, WindowHeight / 2 + linedist - linelength - disti);
        glVertex2i(WindowWidth / 2 - linedist + disti, WindowHeight / 2 + linedist - disti);
        glVertex2i(WindowWidth / 2 - linedist + linelength + disti, WindowHeight / 2 + linedist - disti);
        glVertex2i(WindowWidth / 2 + linedist - disti, WindowHeight / 2 + linedist - disti);
        glVertex2i(WindowWidth / 2 + linedist - disti, WindowHeight / 2 + linedist - linelength - disti);
        glVertex2i(WindowWidth / 2 + linedist - disti, WindowHeight / 2 + linedist - disti);
        glVertex2i(WindowWidth / 2 + linedist - linelength - disti, WindowHeight / 2 + linedist - disti);
        if (selb != base_blocks().air) {
            glVertex2i(WindowWidth / 2, WindowHeight / 2);
            glVertex2i(WindowWidth / 2 + 50, WindowHeight / 2 + 50);
            glVertex2i(WindowWidth / 2 + 50, WindowHeight / 2 + 50);
            glVertex2i(WindowWidth / 2 + 250, WindowHeight / 2 + 50);
        }
        glEnd();
    }

    glBegin(GL_QUADS);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glVertex2i(WindowWidth / 2 - 10, WindowHeight / 2 - 1);
    glVertex2i(WindowWidth / 2 - 10, WindowHeight / 2 + 1);
    glVertex2i(WindowWidth / 2 + 10, WindowHeight / 2 + 1);
    glVertex2i(WindowWidth / 2 + 10, WindowHeight / 2 - 1);
    glVertex2i(WindowWidth / 2 - 1, WindowHeight / 2 - 10);
    glVertex2i(WindowWidth / 2 - 1, WindowHeight / 2 + 10);
    glVertex2i(WindowWidth / 2 + 1, WindowHeight / 2 + 10);
    glVertex2i(WindowWidth / 2 + 1, WindowHeight / 2 - 10);
    glEnd();

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (player.getGameMode() == player::Player::GameMode::Survival) {
        glColor4d(0.8, 0.0, 0.0, 0.3);
        glBegin(GL_QUADS);
        glVertex2d(10, 10);
        glVertex2d(10, 30);
        glVertex2d(200, 30);
        glVertex2d(200, 10);
        glEnd();

        double healthPercent = (double) player.getHealth() / player.getMaxHealth();
        glColor4d(1.0, 0.0, 0.0, 0.5);
        glBegin(GL_QUADS);
        glVertex2d(20, 15);
        glVertex2d(20, 25);
        glVertex2d(20 + healthPercent * 170, 25);
        glVertex2d(20 + healthPercent * 170, 15);
        glEnd();
    }

    glEnable(GL_TEXTURE_2D);

    if (AdvancedRender && showShadowMap) {
        float xi = 1.0f - static_cast<float>(WindowHeight) / WindowWidth;
        float yi = 1.0f;
        float xa = 1.0f;
        float ya = 0.0f;

        Renderer::shadow.bindDepthTexture(0);
        auto& shader = Renderer::shaders[Renderer::DebugShadowShader];
        shader.bind();
        shader.setUniformI("u_shadow_texture", 0);
        Renderer::Begin(GL_QUADS, 2, 2, 0);
        Renderer::TexCoord2f(0.0f, 1.0f);
        Renderer::Vertex2f(xi, yi);
        Renderer::TexCoord2f(0.0f, 0.0f);
        Renderer::Vertex2f(xi, ya);
        Renderer::TexCoord2f(1.0f, 0.0f);
        Renderer::Vertex2f(xa, ya);
        Renderer::TexCoord2f(1.0f, 1.0f);
        Renderer::Vertex2f(xa, yi);
        Renderer::End().render();
        shader.unbind();

        /*
        auto const& viewFrustum = Player::ViewFrustum;
        auto lightFrustum = Renderer::getShadowMapFrustumExperimental(Player::heading, Player::lookupdown,
        Player::ViewFrustum); float length = Renderer::getShadowDistance() * 16.0f;

        auto viewRotate = Mat4f(1.0f);
        viewRotate *= Mat4f::rotation(static_cast<float>(Player::heading), Vec3f(0.0f, 1.0f, 0.0f));
        viewRotate *= Mat4f::rotation(-static_cast<float>(Player::lookupdown), Vec3f(1.0f, 0.0f, 0.0f));

        float halfHeight = std::tan(viewFrustum.getFov() * (std::numbers::pi_v<float> / 180.0f) / 2.0f);
        float halfWidth = halfHeight * viewFrustum.getAspect();
        float zNear = 10.0f, zFar = length;
        float xNear = halfWidth * zNear, yNear = halfHeight * zNear;
        float xFar = halfWidth * zFar, yFar = halfHeight * zFar;
        auto vertices = std::array<Vec3f, 8>{
            Vec3f(-xNear, -yNear, -zNear),
            Vec3f(xNear, -yNear, -zNear),
            Vec3f(xNear, yNear, -zNear),
            Vec3f(-xNear, yNear, -zNear),
            Vec3f(-xFar, -yFar, -zFar),
            Vec3f(xFar, -yFar, -zFar),
            Vec3f(xFar, yFar, -zFar),
            Vec3f(-xFar, yFar, -zFar),
        };
        auto indices = std::array<std::pair<int, int>, 12>{
            std::pair{ 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },
            std::pair{ 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
            std::pair{ 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 },
        };
        auto toLightSpace = Mat4f(lightFrustum.getModlMatrix()) * viewRotate;
        for (size_t i = 0; i < vertices.size(); i++) vertices[i] = toLightSpace.transformVec3(vertices[i]);
        float x0 = static_cast<float>(WindowWidth - WindowHeight / 4);
        float y0 = static_cast<float>(WindowHeight / 4);
        float xd = static_cast<float>(WindowHeight / 4);
        float yd = static_cast<float>(-WindowHeight / 4);

        glDisable(GL_TEXTURE_2D);
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        glBegin(GL_LINES);
        for (auto [i, j] : indices) {
            auto first = vertices[i];
            auto second = vertices[j];
            glVertex2f(x0 + first.x * xd, y0 + first.y * yd);
            glVertex2f(x0 + second.x * xd, y0 + second.y * yd);
        }
        glEnd();
        glEnable(GL_TEXTURE_2D);
        */
    }

    int lineHeight = TextRenderer::getLineHeight();
    int textPos = 0;
    auto debugText = [=](std::string const& s) mutable {
        TextRenderer::renderString(0, lineHeight * textPos, s);
        textPos++;
    };

    TextRenderer::setFontColor(1.0f, 1.0f, 1.0f, 0.8f);
    if (showDebugPanel && selb != base_blocks().air) {
        std::stringstream ss;
        ss << block_info(selb).name << " (id: " << static_cast<int>(selb.get()) << ")";
        TextRenderer::renderString(
            WindowWidth / 2 + 50,
            WindowHeight / 2 + 50 - TextRenderer::getLineHeight(),
            ss.str()
        );
    }
    if (chatmode) {
        glDisable(GL_TEXTURE_2D);
        glBegin(GL_QUADS);
        glColor4f(GUI::FgR, GUI::FgG, GUI::FgB, GUI::FgA);
        glVertex2i(1, WindowHeight - 33 - lineHeight);
        glVertex2i(1, WindowHeight - 33);
        glVertex2i(WindowWidth - 1, WindowHeight - 33);
        glVertex2i(WindowWidth - 1, WindowHeight - 33 - lineHeight);
        glEnd();
        glEnable(GL_TEXTURE_2D);
        TextRenderer::renderString(0, WindowHeight - 33 - lineHeight, chatword);
    }
    int count = 0;
    for (size_t i = chatMessages.size(); i-- > 0 && count < 10; count++) {
        glDisable(GL_TEXTURE_2D);
        glBegin(GL_QUADS);
        glColor4f(GUI::BgR, GUI::BgG, GUI::BgB, count + 1 == 10 ? 0.0f : GUI::BgA);
        glVertex2i(1, WindowHeight - 34 - lineHeight * (count + 2));
        glColor4f(GUI::BgR, GUI::BgG, GUI::BgB, GUI::BgA);
        glVertex2i(1, WindowHeight - 34 - lineHeight * (count + 1));
        glColor4f(GUI::BgR, GUI::BgG, GUI::BgB, GUI::BgA);
        glVertex2i(WindowWidth - 1, WindowHeight - 34 - lineHeight * (count + 1));
        glColor4f(GUI::BgR, GUI::BgG, GUI::BgB, count + 1 == 10 ? 0.0f : GUI::BgA);
        glVertex2i(WindowWidth - 1, WindowHeight - 34 - lineHeight * (count + 2));
        glEnd();
        glEnable(GL_TEXTURE_2D);
        TextRenderer::renderString(0, WindowHeight - 34 - lineHeight * (count + 2), chatMessages[i]);
    }

    TextRenderer::setFontColor(1.0f, 1.0f, 1.0f, 0.9f);
    if (showDebugPanel) {
        auto boolstr = [](bool b) {
            return b ? "true" : "false";
        };

        std::stringstream ss;
        ss << std::fixed << std::setprecision(4);

        ss << "v" << GameVersion << " [GL " << GLMajorVersion << "." << GLMinorVersion << "]";
        debugText(ss.str());
        ss.str("");
        ss << fps << " fps, " << ups << " ups";
        debugText(ss.str());
        ss.str("");

        ss << "game mode: " << (player.getGameMode() == player::Player::GameMode::Creative ? "creative" : "survival")
           << " mode";
        debugText(ss.str());
        ss.str("");
        if (AdvancedRender) {
            ss << "shadow view: " << boolstr(showShadowMap);
            debugText(ss.str());
            ss.str("");
        }
        ss << "cross wall: " << boolstr(player.canCrossWall());
        debugText(ss.str());
        ss.str("");

        ss << "x: " << player.getPosition().x << ", y: " << player.getPosition().y << ", z: " << player.getPosition().z;
        debugText(ss.str());
        ss.str("");
        ss << "heading: " << player.getHeading() << ", pitch: " << player.getLookUpDown();
        debugText(ss.str());
        ss.str("");
        ss << "grounded: " << boolstr(player.isOnGround());
        debugText(ss.str());
        ss.str("");
        ss << "near wall: " << boolstr(player.isNearWall()) << ", in water: " << boolstr(player.isInWater());
        debugText(ss.str());
        ss.str("");
        int h = (GameTime / 30 / 60) % 24;
        int m = (GameTime / 30) % 60;
        int s = GameTime % 30 * 2;
        ss << "time: " << (h < 10 ? "0" : "") << h << ":" << (m < 10 ? "0" : "") << m << ":" << (s < 10 ? "0" : "") << s
           << " (" << GameTime << "/" << 30 * 60 * 24 << ")";
        debugText(ss.str());
        ss.str("");

        ss << world.chunks().size() << " chunks loaded";
        debugText(ss.str());
        ss.str("");
        ss << rendered_chunks << " chunks rendered";
        debugText(ss.str());
        ss.str("");
        ss << unloaded_chunks << " chunks unloaded";
        debugText(ss.str());
        ss.str("");
        ss << meshed_chunks << " chunks meshed";
        debugText(ss.str());
        ss.str("");
        ss << updated_blocks << "/" << world.block_update_queue().size() << " blocks updated";
        debugText(ss.str());
        ss.str("");

    } else {
        std::stringstream ss;
        ss << "v" << GameVersion;
        debugText(ss.str());
        ss.str("");
        ss << fps << " fps";
        debugText(ss.str());
        ss.str("");
        ss << (player.getGameMode() == player::Player::GameMode::Creative ? "creative" : "survival") << " mode";
        debugText(ss.str());
        ss.str("");
    }
}

void drawBagRow(player::Player& player, int row, int itemid, int xbase, int ybase, int spac, float alpha) {
    // 画出背包的一行
    auto& shader = Renderer::shaders[Renderer::UIShader];
    for (int i = 0; i < 10; i++) {
        glBindTexture(GL_TEXTURE_2D, i == itemid ? SelectedTexture : UnselectedTexture);
        glBegin(GL_QUADS);
        glColor4f(1.0f, 1.0f, 1.0f, alpha);
        glTexCoord2f(0.0f, 1.0f);
        glVertex2i(xbase + i * (32 + spac), ybase);
        glTexCoord2f(1.0f, 1.0f);
        glVertex2i(xbase + i * (32 + spac), ybase + 32);
        glTexCoord2f(1.0f, 0.0f);
        glVertex2i(xbase + i * (32 + spac) + 32, ybase + 32);
        glTexCoord2f(0.0f, 0.0f);
        glVertex2i(xbase + i * (32 + spac) + 32, ybase);
        glEnd();

        auto& item = player.getInventory(row, i);
        if (!item.empty()) {
            shader.bind();
            glBindTexture(GL_TEXTURE_2D_ARRAY, BlockTextureArray);
            Renderer::Begin(GL_QUADS, 2, 3, 4);
            Renderer::Color4f(1.0f, 1.0f, 1.0f, 1.0f);
            Renderer::TexCoord3f(0.0f, 0.0f, static_cast<float>(Textures::getTextureIndex(item.id, 0)));
            Renderer::TexCoord2f(0.0f, 1.0f);
            Renderer::Vertex2i(xbase + i * (32 + spac) + 2, ybase + 2);
            Renderer::TexCoord2f(0.0f, 0.0f);
            Renderer::Vertex2i(xbase + i * (32 + spac) + 2, ybase + 30);
            Renderer::TexCoord2f(1.0f, 0.0f);
            Renderer::Vertex2i(xbase + i * (32 + spac) + 30, ybase + 30);
            Renderer::TexCoord2f(1.0f, 1.0f);
            Renderer::Vertex2i(xbase + i * (32 + spac) + 30, ybase + 2);
            Renderer::End().render();
            shader.unbind();

            TextRenderer::renderString(xbase + i * (32 + spac), ybase, std::to_string(item.count));
        }
    }
}

void drawBag(player::Player& player) {
    // 背包界面与更新
    static int si, sj, sf;
    int csi = -1, csj = -1;
    int leftp = (WindowWidth - 392) / 2;
    int upp = WindowHeight - 152 - 16;
    static int mousew, mouseb, mousebl;
    static items::ItemStack itemSelected;
    double curtime = Timer();
    double TimeDelta = curtime - bagAnimTimer;
    float bagAnim = (float) (1.0 - std::pow(0.9, TimeDelta * 60.0)
                             + std::pow(0.9, bagAnimDuration * 60.0) / bagAnimDuration * TimeDelta);

    if (bagOpened) {
        mousew = mw;
        mouseb = mb;

        glDisable(GL_TEXTURE_2D);
        glBegin(GL_QUADS);
        if (curtime - bagAnimTimer > bagAnimDuration)
            glColor4f(0.2f, 0.2f, 0.2f, 0.6f);
        else
            glColor4f(0.2f, 0.2f, 0.2f, 0.6f * bagAnim);
        glVertex2i(0, 0);
        glVertex2i(0, WindowHeight);
        glVertex2i(WindowWidth, WindowHeight);
        glVertex2i(WindowWidth, 0);
        glEnd();
        glEnable(GL_TEXTURE_2D);

        sf = 0;
        if (curtime - bagAnimTimer > bagAnimDuration) {
            for (int i = 0; i < 4; i++) {
                for (int j = 0; j < 10; j++) {
                    auto& item = player.getInventory(i, j);
                    if (mx >= j * (32 + 8) + leftp && mx <= j * (32 + 8) + 32 + leftp && my >= i * (32 + 8) + upp
                        && my <= i * (32 + 8) + 32 + upp) {
                        csi = si = i;
                        csj = sj = j;
                        sf = 1;
                        if (mousebl == 0 && mouseb == 1 && itemSelected.id == item.id) {
                            if (item.count + itemSelected.count <= 255) {
                                item.count += itemSelected.count;
                                itemSelected.count = 0;
                            } else {
                                itemSelected.count = item.count + itemSelected.count - 255;
                                item.count = 255;
                            }
                        }
                        if (mousebl == 0 && mouseb == 1 && itemSelected.id != item.id) {
                            std::swap(itemSelected, item);
                        }
                        if (mousebl == 0 && mouseb == 2 && itemSelected.id == item.id && item.count < 255) {
                            item.count++;
                            itemSelected.count--;
                        }
                        if (mousebl == 0 && mouseb == 2 && item.empty()) {
                            itemSelected.count--;
                            item.id = itemSelected.id;
                            item.count = 1;
                        }

                        if (itemSelected.count == 0)
                            itemSelected = {};
                        if (item.count == 0)
                            item = {};
                    }
                }
                drawBagRow(
                    player,
                    i,
                    (csi == i ? csj : -1),
                    (WindowWidth - 392) / 2,
                    WindowHeight - 152 - 16 + i * 40,
                    8,
                    1.0f
                );
            }
        }

        if (itemSelected.id != base_blocks().air) {
            auto& shader = Renderer::shaders[Renderer::UIShader];
            shader.bind();
            glBindTexture(GL_TEXTURE_2D_ARRAY, BlockTextureArray);
            Renderer::Begin(GL_QUADS, 2, 3, 4);
            Renderer::Color4f(1.0f, 1.0f, 1.0f, 1.0f);
            Renderer::TexCoord3f(0.0f, 0.0f, static_cast<float>(Textures::getTextureIndex(itemSelected.id, 0)));
            Renderer::TexCoord2f(0.0f, 1.0f);
            Renderer::Vertex2i(mx - 16, my - 16);
            Renderer::TexCoord2f(0.0f, 0.0f);
            Renderer::Vertex2i(mx - 16, my + 16);
            Renderer::TexCoord2f(1.0f, 0.0f);
            Renderer::Vertex2i(mx + 16, my + 16);
            Renderer::TexCoord2f(1.0f, 1.0f);
            Renderer::Vertex2i(mx + 16, my - 16);
            Renderer::End().render();
            shader.unbind();

            TextRenderer::renderString((int) mx - 16, (int) my - 16, std::to_string(itemSelected.count));
        }
        auto item = player.getInventory(si, sj);
        if (!item.empty() && sf == 1) {
            TextRenderer::renderString((int) mx, (int) my - 16, block_info(item.id).name);
        }

        int xbase = 0, ybase = 0, spac = 0;
        float alpha = 0.5f + 0.5f * bagAnim;
        if (curtime - bagAnimTimer <= bagAnimDuration) {
            xbase = std::lround(((WindowWidth - 392) / 2) * bagAnim);
            ybase = std::lround((WindowHeight - 152 - 16 + 120 - (WindowHeight - 32)) * bagAnim + (WindowHeight - 32));
            spac = std::lround(8 * bagAnim);
            drawBagRow(player, 3, -1, xbase, ybase, spac, alpha);
            xbase = std::lround(((WindowWidth - 392) / 2 - WindowWidth) * bagAnim + WindowWidth);
            ybase = std::lround((WindowHeight - 152 - 16 - (WindowHeight - 32)) * bagAnim + (WindowHeight - 32));
            for (int i = 0; i < 3; i++) {
                drawBagRow(player, i, -1, xbase, ybase + i * 40, spac, alpha);
            }
        }

        mousebl = mouseb;

    } else {

        if (curtime - bagAnimTimer <= bagAnimDuration) {
            glDisable(GL_TEXTURE_2D);
            glColor4f(0.2f, 0.2f, 0.2f, 0.6f - 0.6f * bagAnim);
            glBegin(GL_QUADS);
            glVertex2i(0, 0);
            glVertex2i(0, WindowHeight);
            glVertex2i(WindowWidth, WindowHeight);
            glVertex2i(WindowWidth, 0);
            glEnd();
            glEnable(GL_TEXTURE_2D);

            int xbase = 0, ybase = 0, spac = 0;
            float alpha = 1.0f - 0.5f * bagAnim;
            xbase = std::lround(((WindowWidth - 392) / 2) - ((WindowWidth - 392) / 2) * bagAnim);
            ybase = std::lround(
                (WindowHeight - 152 - 16 + 120 - (WindowHeight - 32))
                - (WindowHeight - 152 - 16 + 120 - (WindowHeight - 32)) * bagAnim + (WindowHeight - 32)
            );
            spac = std::lround(8 - 8 * bagAnim);
            drawBagRow(player, 3, player.getIndexInHand(), xbase, ybase, spac, alpha);
            xbase = std::lround(
                ((WindowWidth - 392) / 2 - WindowWidth) - ((WindowWidth - 392) / 2 - WindowWidth) * bagAnim
                + WindowWidth
            );
            ybase = std::lround(
                (WindowHeight - 152 - 16 - (WindowHeight - 32))
                - (WindowHeight - 152 - 16 - (WindowHeight - 32)) * bagAnim + (WindowHeight - 32)
            );
            for (int i = 0; i < 3; i++) {
                drawBagRow(player, i, -1, xbase, ybase + i * 40, spac, alpha);
            }
        } else
            drawBagRow(player, 3, player.getIndexInHand(), 0, WindowHeight - 32, 0, 0.5f);
    }
}
