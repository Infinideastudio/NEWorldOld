module;

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>
#undef assert

export module neworld;
import std;
import types;
import math;
import commands;
import globals;
import globalization;
import gui;
import menus;
import particles;
import rendering;
import render;
import setup;
import text_rendering;
import textures;
import worlds;
import menus;
import items;
using namespace std::literals::chrono_literals;

//==============================   Initialize   ================================//
//==============================初始化(包括闪屏)================================//

void update_thread_func(std::stop_token stop_token, worlds::World& world);
void game_update(worlds::World& world);
void frame_linked_update(worlds::World& world);

void render_scene(worlds::World& world);
void readback(worlds::World& world);
void draw_block_selection_border(float x, float y, float z);
void draw_block_breaking_texture(float level, float x, float y, float z);
void draw_hud(worlds::World& world);
void draw_inventory_row(player::Player& world, int row, int itemid, int xbase, int ybase, int spac, float alpha);
void draw_inventory(player::Player& world);

std::mutex update_mutex;
double update_timer;

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
CommandRegistry command_registry;
std::vector<std::string> chat_messages;

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
    load_options();
    Globalization::Load();

    std::filesystem::create_directories("configs");
    std::filesystem::create_directories("worlds");
    std::filesystem::create_directories("screenshots");

    WindowWidth = DefaultWindowWidth;
    WindowHeight = DefaultWindowHeight;

    create_window();
    splash_screen();
    load_textures();
    register_base_blocks();
    register_base_commands(command_registry);

    // 菜单游戏循环
    while (!glfwWindowShouldClose(MainWindow)) {
        GameBegin = GameExit = false;
        GUI::clearTransition();
        Menus::mainmenu();

        // 初始化游戏状态
        spdlog::debug("Init world...");
        auto world = worlds::World(Cur_WorldName);

        // 初始化游戏更新线程
        update_mutex.lock();
        update_timer = timer();
        auto update_thread = std::jthread(update_thread_func, std::ref(world));

        // 这才是游戏开始!
        spdlog::info("Main loop started");
        mxl = mx;
        myl = my;
        shouldShowCursor = false;
        shouldGetScreenshot = false;
        shouldGetThumbnail = false;
        shouldToggleFullscreen = false;
        fctime = uctime = timer();

        // 主循环，被简化成这样，惨不忍睹啊！
        while (!glfwWindowShouldClose(MainWindow) && !GameExit) {
            // 等待上一帧完成后渲染下一帧
            update_mutex.unlock();
            glfwSwapBuffers(MainWindow); // 屏幕刷新，千万别删，后果自负！！！
            update_mutex.lock();
            readback(world);
            render_scene(world);
            fpsc++;

            // 检测帧速率
            if (timer() - fctime >= 1.0) {
                fps = fpsc;
                fpsc = 0;
                fctime = timer();
            }

            // 检测更新速率
            if (timer() - uctime >= 1.0) {
                uctime = timer();
                ups = upsc;
                upsc = 0;
            }

            // 检测所有窗口事件
            glfwPollEvents();

            // 须每帧处理的输入和游戏更新
            frame_linked_update(world);

            // 暂停菜单
            if (paused) {
                shouldGetThumbnail = true;
                readback(world);
                GUI::clearTransition();
                Menus::gamemenu();
                paused = false;
                mxl = mx;
                myl = my;
                update_timer = fctime = uctime = timer();
            }
        };

        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClearDepth(1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        TextRenderer::set_font_color(1.0, 1.0, 1.0, 1.0);
        TextRenderer::render_string(1, 1, "Saving world...");
        glfwSwapBuffers(MainWindow);

        // 停止游戏更新线程
        spdlog::info("Terminating threads");
        update_mutex.unlock();
        update_thread.request_stop();
        update_thread.join();

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

void update_thread_func(std::stop_token stop_token, worlds::World& world) {
    while (!stop_token.stop_requested()) {
        update_mutex.lock();
        auto curr_timer = timer();
        while (curr_timer - update_timer >= 1.0 / 30.0) {
            if (upsc >= 60) {
                update_timer = curr_timer;
            }
            update_timer += 1.0 / 30.0;
            upsc++;
            game_update(world);
        }
        update_mutex.unlock();
        std::this_thread::sleep_for(1ms);
    }
}

void game_update(worlds::World& world) {
    int const SelectPrecision = 32;
    int const SelectDistance = 8;

    static double wPressTimer;
    static bool wPressedOnce;

    auto& player = world.player();

    // 时间
    GameTime++;

    // Move chunk pointer array and height map
    auto center = Vec3i(player.coord());
    world.set_center(center);

    // 加载动画
    world.update_load_anim();

    for (auto const& [_, c]: world.chunks()) {
        // 随机状态更新
        if (rnd() < 1.0 / 8.0) {
            int cx = c->coord().x();
            int cy = c->coord().y();
            int cz = c->coord().z();
            auto x = static_cast<uint32_t>(rnd() * chunks::Chunk::SIZE);
            auto gx = static_cast<int32_t>(x) + cx * chunks::Chunk::SIZE;
            auto y = static_cast<uint32_t>(rnd() * chunks::Chunk::SIZE);
            auto gy = static_cast<int32_t>(y) + cy * chunks::Chunk::SIZE;
            auto z = static_cast<uint32_t>(rnd() * chunks::Chunk::SIZE);
            auto gz = static_cast<int32_t>(z) + cz * chunks::Chunk::SIZE;
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
            if (!chatword.empty()) { // 指令的执行，或发出聊天文本
                auto utf8 = unicode_utf8(chatword);
                if (chatword[0] == '/') { // 指令
                    command_registry.execute_on(utf8, world, chat_messages);
                } else {
                    chat_messages.push_back(utf8);
                }
            }
            chatword.clear();
        }

        if (!inputstr.empty())
            chatword += inputstr;
        if (backspace && !chatword.empty())
            chatword = chatword.substr(0, chatword.length() - 1);

        // 自动补全
        if (isKeyPressed(GLFW_KEY_TAB) && chatmode && !chatword.empty() && chatword[0] == '/') {
            if (auto res = command_registry.try_auto_complete(unicode_utf8(chatword))) {
                chatword = utf8_unicode(*res);
            }
        }
    } else if (bagOpened) {
        shouldShowCursor = true;

        if (isKeyPressed(GLFW_KEY_E)) {
            bagOpened = false;
            bagAnimTimer = timer();
            mxl = mx;
            myl = my;
            mwl = mw;
            mbl = mb;
        }
    } else {
        shouldShowCursor = false;

        // 从玩家位置发射一条线段
        auto coord = player.look_coord();
        auto coord_last = coord;
        auto direction = player.orientation().direction();
        for (auto i = 0; i < SelectPrecision * SelectDistance; i++) {
            // 线段延伸
            coord_last = coord;
            coord += direction / SelectPrecision;
            auto int_coord_last = coord_last.floor<int32_t>();
            auto int_coord = coord.floor<int32_t>();
            // 碰到方块
            if (block_info(world.block_or_air(int_coord).id).solid) {
                selx = int_coord.x();
                sely = int_coord.y();
                selz = int_coord.z();
                sel = true;
                selb = world.block_or_air(int_coord).id;
                if (mb == 1) { // 鼠标左键
                    particles::throw_particle(
                        selb,
                        float(int_coord.x() + rnd()),
                        float(int_coord.y() + rnd() + 0.3),
                        float(int_coord.z() + rnd()),
                        float(rnd() * 0.2f - 0.1f),
                        float(rnd() * 0.2f - 0.1f),
                        float(rnd() * 0.2f - 0.1f),
                        float(rnd() * 0.01f + 0.02f),
                        int(rnd() * 30) + 30
                    );

                    constexpr auto MIN_HARDNESS = 0.2f;
                    if (selx != oldselx || sely != oldsely || selz != oldselz)
                        seldes = 0.0f;
                    else if (player.game_mode() == player::Player::GameMode::CREATIVE)
                        seldes += 1.0f / 30.0f / MIN_HARDNESS;
                    else if (block_info(selb).hardness <= MIN_HARDNESS)
                        seldes += 1.0f / 30.0f / MIN_HARDNESS;
                    else
                        seldes += 1.0f / 30.0f / block_info(selb).hardness;

                    if (seldes >= 1.0f) {
                        player.add_item({selb, 1});
                        for (int j = 1; j <= 25; j++) {
                            particles::throw_particle(
                                selb,
                                float(int_coord.x() + rnd()),
                                float(int_coord.y() + rnd() + 0.3),
                                float(int_coord.z() + rnd()),
                                float(rnd() * 0.2f - 0.1f),
                                float(rnd() * 0.2f - 0.1f),
                                float(rnd() * 0.2f - 0.1f),
                                float(rnd() * 0.02 + 0.03),
                                int(rnd() * 60) + 30
                            );
                        }
                        world.put_block(int_coord, base_blocks().air);
                    }
                }
                if (mb == 2 && mbp == false) { // 鼠标右键
                    auto& holdingItem = player.held_item_stack();
                    if (!holdingItem.empty()) {
                        if (player.put_block(world, int_coord_last, holdingItem.id)) {
                            if (--holdingItem.count == 0)
                                holdingItem = {};
                        }
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
        auto angles = player.orientation();
        auto angles_front = Eulerd(angles.heading(), 0.0, 0.0);
        auto angles_back = Eulerd(angles.heading() - Pi, 0.0, 0.0);
        auto angles_left = Eulerd(angles.heading() + Pi / 2, 0.0, 0.0);
        auto angles_right = Eulerd(angles.heading() - Pi / 2, 0.0, 0.0);

        // 移动！(生命在于运动)
        if (isKeyDown(GLFW_KEY_W)) {
            if (!wPressedOnce) {
                if (wPressTimer == 0.0) {
                    wPressTimer = timer();
                } else {
                    if (timer() - wPressTimer <= 0.5) {
                        player.set_running(true);
                        wPressTimer = 0.0;
                    } else
                        wPressTimer = timer();
                }
            }
            if (wPressTimer != 0.0 && timer() - wPressTimer > 0.5)
                wPressTimer = 0.0;
            wPressedOnce = true;
            auto velocity = player.velocity();
            player.set_velocity(velocity + angles_front.direction() * player.speed());
        } else {
            player.set_running(false);
            wPressedOnce = false;
        }

        if (isKeyDown(GLFW_KEY_S)) {
            auto velocity = player.velocity();
            player.set_velocity(velocity + angles_back.direction() * player.speed());
            wPressTimer = 0.0;
        }

        if (isKeyDown(GLFW_KEY_A)) {
            auto velocity = player.velocity();
            player.set_velocity(velocity + angles_left.direction() * player.speed());
            wPressTimer = 0.0;
        }

        if (isKeyDown(GLFW_KEY_D)) {
            auto velocity = player.velocity();
            player.set_velocity(velocity + angles_right.direction() * player.speed());
            wPressTimer = 0.0;
        }

        if (!player.flying() && !player.cross_wall()) {
            auto velocity = player.velocity();
            auto xz = Vec3d(velocity.x(), 0.0, velocity.z());
            if (xz.length() > player.speed()) {
                xz = xz.normalize() * player.speed();
                player.set_velocity(Vec3d(xz.x(), velocity.y(), xz.z()));
            }
        } else {
            if (isKeyDown(GLFW_KEY_R)) {
                auto velocity = player.velocity();
                player.set_velocity(
                    velocity
                    + (isKeyDown(GLFW_KEY_LEFT_CONTROL) ? angles_front : angles).direction() * player.speed() * 10.0
                );
            }
            if (isKeyDown(GLFW_KEY_F)) {
                auto velocity = player.velocity();
                player.set_velocity(
                    velocity
                    - (isKeyDown(GLFW_KEY_LEFT_CONTROL) ? angles_front : angles).direction() * player.speed() * 10.0
                );
            }
        }

        // 切换方块
        if (isKeyPressed(GLFW_KEY_Z)) {
            player.set_held_item_stack_index((player.held_item_stack_index() + 10 - 1) % 10);
        }
        if (isKeyPressed(GLFW_KEY_X)) {
            player.set_held_item_stack_index((player.held_item_stack_index() + 1) % 10);
        }
        player.set_held_item_stack_index((player.held_item_stack_index() + 10 + (mw - mwl) % 10) % 10);
        mwl = mw;

        // 起跳/上升
        if (isKeyDown(GLFW_KEY_SPACE)) {
            player.on_jump(isKeyPressed(GLFW_KEY_SPACE));
        }

        // 下蹲/下降
        if (isKeyDown(GLFW_KEY_LEFT_SHIFT) || isKeyDown(GLFW_KEY_RIGHT_SHIFT)) {
            player.on_crouch();
        }

        // Open inventory
        if (isKeyPressed(GLFW_KEY_E) && showHUD) {
            bagOpened = true;
            bagAnimTimer = timer();
            shouldGetThumbnail = true;
        }

        // Save world
        if (isKeyPressed(GLFW_KEY_L))
            world.save_to_files();

        // 各种设置切换
        if (isKeyPressed(GLFW_KEY_F1)) {
            player.set_game_mode(
                player.game_mode() == player::Player::GameMode::CREATIVE ? player::Player::GameMode::SURVIVAL
                                                                         : player::Player::GameMode::CREATIVE
            );
        }
        if (isKeyPressed(GLFW_KEY_F2)) {
            shouldGetScreenshot = true;
            screenshotAnimTimer = timer();
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
        if (isKeyPressed(GLFW_KEY_F4))
            player.set_cross_wall(!player.cross_wall());
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
            chatword = utf8_unicode("/");
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
    player.update(world);
}

void frame_linked_update(worlds::World& world) {
    // Find chunks for unloading & loading & meshing
    auto& player = world.player();
    auto center = Vec3i(player.coord());
    world.update_chunk_lists(center);

    // Load chunks
    world.process_chunk_loads();

    // Unload chunks
    world.process_chunk_unloads();

    // Mesh updated chunks
    world.process_chunk_meshings();

    // 处理计时
    double currTimer = timer();

    // 视野特效
    if (player.running()) {
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
        auto angles = player.orientation();
        angles.heading() -= (mx - mxl) * MouseSpeed * Pi / 180.0;
        angles.pitch() -= (my - myl) * MouseSpeed * Pi / 180.0;
        player.set_orientation(angles);
    }

    // 切换全屏
    if (shouldToggleFullscreen) {
        shouldToggleFullscreen = false;
        toggle_full_screen();
    }

    // 暂停按键
    if (isKeyDown(GLFW_KEY_ESCAPE)) {
        shouldGetThumbnail = true;
        paused = true;
    }
}

// Render the whole scene and HUD
void render_scene(worlds::World& world) {
    float const SkyColorR = 0.70f;
    float const SkyColorG = 0.80f;
    float const SkyColorB = 0.86f;

    auto& player = world.player();
    double currTimer = timer();
    double interp = (currTimer - update_timer) * 30.0;
    Vec3d view_coord = player.look_coord() - player.velocity() * (1.0 - interp);

    // Calculate sun position (temporary: horizontal movement only)
    float interpolatedTime = GameTime - 1.0f + static_cast<float>(interp);
    Renderer::sunlightHeading = interpolatedTime / 43200.0f * 360.0f;

    // Calculate matrices
    auto view_angles = static_cast<Eulerf>(player.orientation());
    auto view_matrix = view_angles.view_matrix();
    view_matrix = Mat4f::perspective(
                      static_cast<float>((FOVyNormal + FOVyExt) * Pi / 180.0),
                      static_cast<float>(WindowWidth) / WindowHeight,
                      0.05f,
                      static_cast<float>(RenderDistance * chunks::Chunk::SIZE)
                  )
                * view_matrix;

    auto shadow_matrix = Renderer::getShadowMatrix();
    Renderer::SetUniforms(view_coord, view_matrix, shadow_matrix, interpolatedTime);

    // Clear framebuffers
    if (AdvancedRender)
        Renderer::ClearSGDBuffers();

    glClearColor(SkyColorR, SkyColorG, SkyColorB, 1.0f);
    glClearDepth(1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Bind main texture array
    BlockTextureArray.bind(0);

    if (showMeshWireframe)
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    // Build shadow map
    if (AdvancedRender) {
        auto shadowMatrixExp = Renderer::getShadowMatrixExperimental(
            FOVyNormal + FOVyExt,
            static_cast<float>(WindowWidth) / WindowHeight,
            static_cast<Eulerf>(view_angles)
        );
        auto shadowFrustum = Frustum(shadowMatrixExp);
        auto list = world.list_render_chunks(view_coord, Renderer::getShadowDistance(), interp, shadowFrustum);
        Renderer::StartShadowPass();
        world.render_chunks(view_coord, list, 0);
        particles::render_all(world, view_coord, interp);
        Renderer::EndShadowPass();
    }

    // Draw the opaque parts of the world
    auto list = world.list_render_chunks(view_coord, RenderDistance, interp, Frustum(view_matrix));
    rendered_chunks = static_cast<int>(list.size());
    Renderer::StartOpaquePass();
    world.render_chunks(view_coord, list, 0);
    particles::render_all(world, view_coord, interp);
    Renderer::EndOpaquePass();

    // Draw the translucent parts of the world
    Renderer::StartTranslucentPass();
    glDisable(GL_CULL_FACE);
    if (sel) {
        auto x = static_cast<float>(selx - view_coord.x());
        auto y = static_cast<float>(sely - view_coord.y());
        auto z = static_cast<float>(selz - view_coord.z());
        if (showHUD) {
            // Temporary solution pre GL 4.0 (glBlendFuncSeparatei)
            glColorMaski(0, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
            draw_block_selection_border(x, y, z);
            glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            glColorMaski(1, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
            glColorMaski(2, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
            glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ZERO);
            draw_block_selection_border(x, y, z);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            glColorMaski(1, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            glColorMaski(2, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        }
        draw_block_breaking_texture(seldes, x, y, z);
    }
    world.render_chunks(view_coord, list, 1);
    glEnable(GL_CULL_FACE);
    Renderer::EndTranslucentPass();

    if (showMeshWireframe)
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    // Full screen passes
    if (AdvancedRender) {
        namespace spec = render::attrib_layout::spec;
        auto v = render::AttribIndexBuilder<spec::Coord<spec::Vec2f>, spec::TexCoord<spec::Vec2f>>();
        v.tex_coord({0.0f, 1.0f});
        v.coord({0, 0});
        v.tex_coord({0.0f, 0.0f});
        v.coord({0, WindowHeight});
        v.tex_coord({1.0f, 0.0f});
        v.coord({WindowWidth, WindowHeight});
        v.tex_coord({1.0f, 1.0f});
        v.coord({WindowWidth, 0});
        v.end_primitive();
        auto va = render::VertexArray::create(v, render::VertexArray::Primitive::TRIANGLE_FAN);

        Renderer::StartFinalPass();
        va.first.render();
        Renderer::EndFinalPass();
    }

    // HUD rendering starts here
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, WindowWidth, WindowHeight, 0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glClearDepth(1.0f);
    glClear(GL_DEPTH_BUFFER_BIT);

    auto& shader = Renderer::shaders[Renderer::UIShader];
    shader.bind();

    auto int_view_coord = view_coord.floor<int32_t>();
    if (world.block_or_air(int_view_coord).id == base_blocks().water) {
        namespace spec = render::attrib_layout::spec;
        auto v = render::
            AttribIndexBuilder<spec::Coord<spec::Vec2f>, spec::TexCoord<spec::Vec3f>, spec::Color<spec::Vec4u8>>();
        auto tex = static_cast<float>(Textures::getTextureIndex(base_blocks().water, 0));
        v.color({255, 255, 255, 255});
        v.tex_coord({0.0f, 1.0f, tex});
        v.coord({0, 0});
        v.tex_coord({0.0f, 0.0f, tex});
        v.coord({0, WindowHeight});
        v.tex_coord({1.0f, 0.0f, tex});
        v.coord({WindowWidth, WindowHeight});
        v.tex_coord({1.0f, 1.0f, tex});
        v.coord({WindowWidth, 0});
        v.end_primitive();
        auto va = render::VertexArray::create(v, render::VertexArray::Primitive::TRIANGLE_FAN);
        BlockTextureArray.bind(0);
        va.first.render();
    }

    if (showHUD) {
        draw_hud(world);
        draw_inventory(world.player());
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
    auto buffer = Textures::ImageRGBA(1, h, w);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_FRONT);
    glReadPixels(x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, buffer.data());
    Textures::SaveImage(filename, buffer);
}

void readback(worlds::World& world) {
    if (shouldGetScreenshot) {
        shouldGetScreenshot = false;
        auto now = std::chrono::system_clock::now();
        saveScreenshot(0, 0, WindowWidth, WindowHeight, std::format("screenshots/{}.png", now));
    }

    if (shouldGetThumbnail) {
        shouldGetThumbnail = false;
        saveScreenshot(0, 0, WindowWidth, WindowHeight, std::format("worlds/{}/thumbnail.png", world.name()));
    }
}

constexpr auto centers = std::array<Vec3f, 6>({
    {1.0f, 0.5f, 0.5f},
    {0.0f, 0.5f, 0.5f},
    {0.5f, 1.0f, 0.5f},
    {0.5f, 0.0f, 0.5f},
    {0.5f, 0.5f, 1.0f},
    {0.5f, 0.5f, 0.0f},
});

constexpr auto cube = std::array<std::array<Vec3f, 4>, 6>({
    {{{1.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}}},
    {{{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}}},
    {{{0.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}}},
    {{{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}}},
    {{{0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 1.0f}}},
    {{{1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}}},
});

constexpr auto tex_coords = std::array<Vec2f, 4>({
    {0.0f, 0.0f},
    {1.0f, 0.0f},
    {1.0f, 1.0f},
    {0.0f, 1.0f},
});

// Draw the block selection border
void draw_block_selection_border(float x, float y, float z) {
    namespace spec = render::attrib_layout::spec;
    using render::VertexArray;
    using AttribIndexBuilder = render::AttribIndexBuilder<
        spec::Coord<spec::Vec3f>,
        spec::TexCoord<spec::Vec3f>,
        spec::Color<spec::Vec3u8>,
        spec::Normal<spec::Vec3i8>,
        spec::Material<spec::UInt16>>;

    constexpr auto EPS = 0.005f;
    constexpr auto WIDTH = 1.0f / 32.0f;

    auto v = AttribIndexBuilder();
    v.material(65535); // For indicator elements
    v.tex_coord({0.0f, 0.0f, static_cast<float>(TextureIndex::WHITE)});
    v.color({255, 255, 255});

    for (auto i = 0uz; i < cube.size(); i++) {
        auto const& center = centers[i];
        auto xc = center[0], yc = center[1], zc = center[2];
        v.normal({xc * 2.0f, yc * 2.0f, zc * 2.0f});
        for (auto j = 0uz; j < cube[i].size(); j++) {
            auto const& first = cube[i][j];
            auto const& second = cube[i][(j + 1) % 4];
            auto x0 = first[0], y0 = first[1], z0 = first[2];
            auto x1 = second[0], y1 = second[1], z1 = second[2];
            auto xd0 = (x0 - xc) * 2.0f, yd0 = (y0 - yc) * 2.0f, zd0 = (z0 - zc) * 2.0f;
            auto xd1 = (x1 - xc) * 2.0f, yd1 = (y1 - yc) * 2.0f, zd1 = (z1 - zc) * 2.0f;
            x0 += (x0 > 0.0f ? EPS : -EPS), y0 += (y0 > 0.0f ? EPS : -EPS), z0 += (z0 > 0.0f ? EPS : -EPS);
            x1 += (x1 > 0.0f ? EPS : -EPS), y1 += (y1 > 0.0f ? EPS : -EPS), z1 += (z1 > 0.0f ? EPS : -EPS);
            v.coord({x + x0, y + y0, z + z0});
            v.coord({x + x1, y + y1, z + z1});
            v.coord({x + x1 - xd1 * WIDTH, y + y1 - yd1 * WIDTH, z + z1 - zd1 * WIDTH});
            v.coord({x + x0 - xd0 * WIDTH, y + y0 - yd0 * WIDTH, z + z0 - zd0 * WIDTH});
            v.end_primitive();
        }
    }
    VertexArray::create(v, VertexArray::Primitive::TRIANGLE_FAN).first.render();
}

void draw_block_breaking_texture(float level, float x, float y, float z) {
    namespace spec = render::attrib_layout::spec;
    using render::VertexArray;
    using AttribIndexBuilder = render::AttribIndexBuilder<
        spec::Coord<spec::Vec3f>,
        spec::TexCoord<spec::Vec3f>,
        spec::Color<spec::Vec3u8>,
        spec::Normal<spec::Vec3i8>,
        spec::Material<spec::UInt16>>;

    constexpr auto EPS = 0.005f;
    if (level <= 0.0f) {
        return;
    }
    auto index = std::clamp(int(level * 8), 0, 7);
    auto tex = static_cast<float>(TextureIndex::BREAKING_0) + static_cast<float>(index);

    auto v = AttribIndexBuilder();
    v.material(65535); // For indicator elements
    v.color({255, 255, 255});

    for (auto i = 0uz; i < cube.size(); i++) {
        auto const& center = centers[i];
        auto xc = center[0], yc = center[1], zc = center[2];
        v.normal({xc * 2.0f, yc * 2.0f, zc * 2.0f});
        for (auto j = 0uz; j < cube[i].size(); j++) {
            auto const& tex_coord = tex_coords[j];
            auto const& point = cube[i][j];
            auto u0 = tex_coord[0], v0 = tex_coord[1];
            auto x0 = point[0], y0 = point[1], z0 = point[2];
            x0 += (x0 > 0.0f ? EPS : -EPS), y0 += (y0 > 0.0f ? EPS : -EPS), z0 += (z0 > 0.0f ? EPS : -EPS);
            v.tex_coord({u0, v0, tex});
            v.coord({x + x0, y + y0, z + z0});
        }
        v.end_primitive();
    }
    VertexArray::create(v, VertexArray::Primitive::TRIANGLE_FAN).first.render();
}

void draw_hud(worlds::World& world) {
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

    if (player.game_mode() == player::Player::GameMode::SURVIVAL) {
        glColor4d(0.8, 0.0, 0.0, 0.3);
        glBegin(GL_QUADS);
        glVertex2d(10, 10);
        glVertex2d(10, 30);
        glVertex2d(200, 30);
        glVertex2d(200, 10);
        glEnd();

        double healthPercent = (double) player.health() / player.max_health();
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

        namespace spec = render::attrib_layout::spec;
        auto v = render::AttribIndexBuilder<spec::Coord<spec::Vec2f>, spec::TexCoord<spec::Vec2f>>();
        v.tex_coord({0.0f, 1.0f});
        v.coord({xi, yi});
        v.tex_coord({0.0f, 0.0f});
        v.coord({xi, ya});
        v.tex_coord({1.0f, 0.0f});
        v.coord({xa, ya});
        v.tex_coord({1.0f, 1.0f});
        v.coord({xa, yi});
        v.end_primitive();
        auto va = render::VertexArray::create(v, render::VertexArray::Primitive::TRIANGLE_FAN);

        Renderer::shadow.bindDepthTexture(0);
        Renderer::shaders[Renderer::DebugShadowShader].bind();
        va.first.render();
    }

    int lineHeight = TextRenderer::line_height();
    int textPos = 0;
    auto debugText = [=]<typename... Args>(std::format_string<Args...> fmt, Args&&... args) mutable {
        auto s = std::format(fmt, std::forward<Args>(args)...);
        TextRenderer::render_string(0, lineHeight * textPos, s);
        textPos++;
    };

    TextRenderer::set_font_color(1.0f, 1.0f, 1.0f, 0.8f);
    if (showDebugPanel && selb != base_blocks().air) {
        TextRenderer::render_string(
            WindowWidth / 2 + 50,
            WindowHeight / 2 + 50 - TextRenderer::line_height(),
            std::format("{} (id: {})", block_info(selb).name, static_cast<int>(selb.get()))
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
        TextRenderer::render_string(0, WindowHeight - 33 - lineHeight, chatword);
    }
    int count = 0;
    for (size_t i = chat_messages.size(); i-- > 0 && count < 10; count++) {
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
        TextRenderer::render_string(0, WindowHeight - 34 - lineHeight * (count + 2), chat_messages[i]);
    }

    TextRenderer::set_font_color(1.0f, 1.0f, 1.0f, 0.9f);
    if (showDebugPanel) {
        auto boolstr = [](bool b) {
            return b ? "true" : "false";
        };

        debugText("v{} [GL {}.{}]", GameVersion, GLMajorVersion, GLMinorVersion);
        debugText("{} fps, {} ups", fps, ups);
        debugText(
            "Game mode: {} mode",
            player.game_mode() == player::Player::GameMode::CREATIVE ? "creative" : "survival"
        );
        if (AdvancedRender) {
            debugText("shadow view: {}", boolstr(showShadowMap));
        }
        debugText("cross wall: {}", boolstr(player.cross_wall()));
        debugText("x: {:.3f} y: {:.3f} z: {:.3f}", player.coord().x(), player.coord().y(), player.coord().z());
        debugText(
            "heading: {:.3f}, pitch: {:.3f}",
            player.orientation().heading() / Pi * 180.0,
            player.orientation().pitch() / Pi * 180.0
        );
        debugText(
            "grounded: {}, near wall: {}, in water: {}",
            boolstr(player.grounded()),
            boolstr(player.near_wall()),
            boolstr(player.in_water())
        );
        int h = (GameTime / 30 / 60) % 24;
        int m = (GameTime / 30) % 60;
        int s = GameTime % 30 * 2;
        debugText("time: {:02}:{:02}:{:02} ({})", h, m, s, GameTime);
        debugText(
            "chunks {} loaded, {} rendered, {} unloaded, {} meshed",
            world.chunks().size(),
            rendered_chunks,
            unloaded_chunks,
            meshed_chunks
        );
        debugText("{}/{} blocks updated", updated_blocks, world.block_update_queue().size());
    } else {
        debugText("v{}", GameVersion);
        debugText("{} fps", fps);
        debugText("{} mode", player.game_mode() == player::Player::GameMode::CREATIVE ? "creative" : "survival");
    }
}

void draw_inventory_row(player::Player& player, int row, int itemid, int xbase, int ybase, int spac, float alpha) {
    // 画出背包的一行
    for (int i = 0; i < 10; i++) {
        (i == itemid ? SelectedTexture : UnselectedTexture).bind(0);
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

        auto& item = player.inventory_item_stack(row, i);
        if (!item.empty()) {
            namespace spec = render::attrib_layout::spec;
            auto v = render::
                AttribIndexBuilder<spec::Coord<spec::Vec2f>, spec::TexCoord<spec::Vec3f>, spec::Color<spec::Vec4u8>>();
            auto tex = static_cast<float>(Textures::getTextureIndex(item.id, 0));
            v.color({255, 255, 255, 255});
            v.tex_coord({0.0f, 1.0f, tex});
            v.coord({xbase + i * (32 + spac) + 2, ybase + 2});
            v.tex_coord({0.0f, 0.0f, tex});
            v.coord({xbase + i * (32 + spac) + 2, ybase + 30});
            v.tex_coord({1.0f, 0.0f, tex});
            v.coord({xbase + i * (32 + spac) + 30, ybase + 30});
            v.tex_coord({1.0f, 1.0f, tex});
            v.coord({xbase + i * (32 + spac) + 30, ybase + 2});
            v.end_primitive();
            auto va = render::VertexArray::create(v, render::VertexArray::Primitive::TRIANGLE_FAN);
            BlockTextureArray.bind(0);
            va.first.render();

            TextRenderer::render_string(xbase + i * (32 + spac), ybase, std::to_string(item.count));
        }
    }
}

void draw_inventory(player::Player& player) {
    // 背包界面与更新
    static int si, sj, sf;
    int csi = -1, csj = -1;
    int leftp = (WindowWidth - 392) / 2;
    int upp = WindowHeight - 152 - 16;
    static int mousew, mouseb, mousebl;
    static items::ItemStack itemSelected;
    double curtime = timer();
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
                    auto& item = player.inventory_item_stack(i, j);
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
                draw_inventory_row(
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
            namespace spec = render::attrib_layout::spec;
            auto v = render::
                AttribIndexBuilder<spec::Coord<spec::Vec2f>, spec::TexCoord<spec::Vec3f>, spec::Color<spec::Vec4u8>>();
            auto tex = static_cast<float>(Textures::getTextureIndex(itemSelected.id, 0));
            v.color({255, 255, 255, 255});
            v.tex_coord({0.0f, 1.0f, tex});
            v.coord({mx - 16, my - 16});
            v.tex_coord({0.0f, 0.0f, tex});
            v.coord({mx - 16, my + 16});
            v.tex_coord({1.0f, 0.0f, tex});
            v.coord({mx + 16, my + 16});
            v.tex_coord({1.0f, 1.0f, tex});
            v.coord({mx + 16, my - 16});
            v.end_primitive();
            auto va = render::VertexArray::create(v, render::VertexArray::Primitive::TRIANGLE_FAN);
            BlockTextureArray.bind(0);
            va.first.render();

            TextRenderer::render_string((int) mx - 16, (int) my - 16, std::to_string(itemSelected.count));
        }
        auto item = player.inventory_item_stack(si, sj);
        if (!item.empty() && sf == 1) {
            TextRenderer::render_string((int) mx, (int) my - 16, block_info(item.id).name);
        }

        int xbase = 0, ybase = 0, spac = 0;
        float alpha = 0.5f + 0.5f * bagAnim;
        if (curtime - bagAnimTimer <= bagAnimDuration) {
            xbase = std::lround(((WindowWidth - 392) / 2) * bagAnim);
            ybase = std::lround((WindowHeight - 152 - 16 + 120 - (WindowHeight - 32)) * bagAnim + (WindowHeight - 32));
            spac = std::lround(8 * bagAnim);
            draw_inventory_row(player, 3, -1, xbase, ybase, spac, alpha);
            xbase = std::lround(((WindowWidth - 392) / 2 - WindowWidth) * bagAnim + WindowWidth);
            ybase = std::lround((WindowHeight - 152 - 16 - (WindowHeight - 32)) * bagAnim + (WindowHeight - 32));
            for (int i = 0; i < 3; i++) {
                draw_inventory_row(player, i, -1, xbase, ybase + i * 40, spac, alpha);
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
            draw_inventory_row(player, 3, player.held_item_stack_index(), xbase, ybase, spac, alpha);
            xbase = std::lround(
                ((WindowWidth - 392) / 2 - WindowWidth) - ((WindowWidth - 392) / 2 - WindowWidth) * bagAnim
                + WindowWidth
            );
            ybase = std::lround(
                (WindowHeight - 152 - 16 - (WindowHeight - 32))
                - (WindowHeight - 152 - 16 - (WindowHeight - 32)) * bagAnim + (WindowHeight - 32)
            );
            for (int i = 0; i < 3; i++) {
                draw_inventory_row(player, i, -1, xbase, ybase + i * 40, spac, alpha);
            }
        } else
            draw_inventory_row(player, 3, player.held_item_stack_index(), 0, WindowHeight - 32, 0, 0.5f);
    }
}
