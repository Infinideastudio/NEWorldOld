//==============================   Initialize   ================================//
//==============================初始化(包括闪屏)================================//

#include "Definitions.h"
#include "Blocks.h"
#include "Textures.h"
#include "Renderer.h"
#include "TextRenderer.h"
#include "Player.h"
#include "WorldGen.h"
#include "World.h"
#include "WorldRenderer.h"
#include "Particles.h"
#include "Hitbox.h"
#include "GUI.h"
#include "Menus.h"
#include "FrustumTest.h"
#include "Network.h"
#include "Items.h"
#include "Globalization.h"
#include "Command.h"
#include "ModLoader.h"
#include "Setup.h"

void registerCommands();
bool loadGame();
void saveGame();
ThreadFunc updateThreadFunc(void*);
void gameUpdate();
void frameLinkedUpdate();

void render();
void readback();
void drawBorder(float x, float y, float z);
void drawBreaking(float level, float x, float y, float z);
void drawGUI();
void drawBagRow(int row, int itemid, int xbase, int ybase, int spac, float alpha);
void drawBag();

void loadOptions();
void saveOptions();

int getMouseScroll() { return mw; }
int getMouseButton() { return mb; }

Mutex_t updateMutex;
Thread_t updateThread;
double updateTimer;
bool updateThreadRun;

double speedupAnimTimer;
double touchdownAnimTimer;
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
BlockID selb;
Brightness selbr;
bool selce;
int selbx, selby, selbz, selcx, selcy, selcz;
float FOVyExt;

std::u32string chatword;
bool chatmode = false;
vector<Command> commands;
vector<string> chatMessages;

#if 0
woca, 这样注释都行？！
(这儿编译不过去的童鞋，你的FB编译器版本貌似和我的不一样，把这几行注释掉吧。。。)
== == == == == == == == == == == == == == == == == == ==
等等不对啊！！！明明都改成c++了。。。还说是FB。。。
正常点的C++编译器都应该不会在这儿报错吧23333333
#endif

bool keyDown[GLFW_KEY_LAST + 1];

bool isKeyDown(int key) {
	return glfwGetKey(MainWindow, key) == GLFW_PRESS;
}

bool isKeyPressed(int key) {
	if (key > GLFW_KEY_LAST || key <= 0) return false;
	bool down = glfwGetKey(MainWindow, key) == GLFW_PRESS;
	bool res = down && !keyDown[key];
	keyDown[key] = down;
	return res;
}

void updateKeyStates() {
	for (int i = 0; i <= GLFW_KEY_LAST; i++) keyDown[i] = (glfwGetKey(MainWindow, i) == GLFW_PRESS);
}

//==============================  Main Program  ================================//
//==============================     主程序     ================================//

int main() {
	// 终于进入main函数了！激动人心的一刻！！！

#ifndef NEWORLD_USE_WINAPI
	setlocale(LC_ALL, "zh_CN.UTF-8");
#endif

	loadOptions();
	Globalization::Load();

	_mkdir("configs");
	_mkdir("worlds");
	_mkdir("screenshots");
	_mkdir("mods");

	WindowWidth = DefaultWindowWidth;
	WindowHeight = DefaultWindowHeight;

	createWindow();
	setupScreen();
	splashScreen();
	loadTextures();
	registerCommands();

	printf("[Console][Game]");
	printf("Loading Mods...\n");
	Mod::ModLoader::loadMods();

	// 菜单游戏循环
	while (!glfwWindowShouldClose(MainWindow)) {
		GameBegin = GameExit = false;
		GUI::clearTransition();
		Menus::mainmenu();

		// 初始化游戏状态
		if (Multiplayer) {
			printf("[Console][Game]");
			printf("Init networking...\n");
			fastSrand((unsigned int)time(NULL));
			Player::name = "";
			Player::onlineID = rand();
			Network::init(ServerIP, ServerPort);
		}

		printf("[Console][Game]");
		printf("Init player...\n");
		if (loadGame()) Player::init(Player::xpos, Player::ypos, Player::zpos);
		else Player::spawn();

		printf("[Console][Game]");
		printf("Init world...\n");
		World::Init();

		// 初始化游戏更新线程
		updateMutex = MutexCreate();
		MutexLock(updateMutex);
		updateThreadRun = true;
		updateThread = ThreadCreate(&updateThreadFunc, NULL);
		updateTimer = timer();

		// 这才是游戏开始!
		printf("[Console][Game]");
		printf("Main loop started\n");
		mxl = mx;
		myl = my;
		shouldShowCursor = false;
		fctime = uctime = timer();

		// 主循环，被简化成这样，惨不忍睹啊！
		while (!glfwWindowShouldClose(MainWindow) && !GameExit) {
			// 等待上一帧完成后渲染下一帧
			MutexUnlock(updateMutex);
			glfwSwapBuffers(MainWindow); // 屏幕刷新，千万别删，后果自负！！！
			MutexLock(updateMutex);
			readback();
			render();
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
			frameLinkedUpdate();

			// 暂停菜单
			if (paused) {
				paused = false;
				readback(); // Ensure creation of thumbnail
				GUI::clearTransition();
				Menus::gamemenu();
				mxl = mx;
				myl = my;
				updateTimer = fctime = uctime = timer();
			}
		};

		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		glClearDepth(1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		TextRenderer::setFontColor(1.0, 1.0, 1.0, 1.0);
		TextRenderer::renderString(0, 0, "Saving world...");
		glfwSwapBuffers(MainWindow);

		// 停止游戏更新线程
		printf("[Console][Game]");
		printf("Terminate threads\n");
		updateThreadRun = false;
		MutexUnlock(updateMutex);
		ThreadWait(updateThread);
		ThreadDestroy(updateThread);
		MutexDestroy(updateMutex);

		// 保存并卸载世界
		saveGame();
		World::destroyAllChunks();
		if (Multiplayer) Network::cleanUp();
	}

	Mod::ModLoader::unloadMods();

	// 结束程序，删了也没关系 ←_←（吐槽FB和glfw中）
	// 不对啊这不是FB！！！这是正宗的C++！！！！！！
	// 楼上的楼上在瞎说！！！别信他的！！！
	glfwTerminate();
	return 0;
	// This is the END of the program!
}

ThreadFunc updateThreadFunc(void*) {
	MutexLock(updateMutex);
	while (updateThreadRun) {
		double currTimer = timer();
		if (currTimer - updateTimer >= 5.0) updateTimer = currTimer;
		while (currTimer - updateTimer >= 1.0 / 30.0 && upsc < 60) {
			updateTimer += 1.0 / 30.0;
			upsc++;
			gameUpdate();
		}
		MutexUnlock(updateMutex);
		Sleep(1);
		MutexLock(updateMutex);
	}
	MutexUnlock(updateMutex);
	return 0;
}

void saveGame() {
	World::saveAllChunks();
	if (!Player::save(World::worldname)) {
#ifdef NEWORLD_CONSOLE_OUTPUT
		DebugWarning("Failed saving player info!");
#endif
	}
}

bool loadGame() {
	if (!Player::load(World::worldname)) {
#ifdef NEWORLD_CONSOLE_OUTPUT
		DebugWarning("Failed loading player info!");
#endif
		return false;
	}
	return true;
}

void registerCommands() {
	commands.push_back(Command("/help", [](const vector<string>& command) {
		if (command.size() != 1) return false;
		chatMessages.push_back("Controls: W/A/S/D/SPACE/SHIFT = move, R/F = fast move (creative mode), E = open inventory,");
		chatMessages.push_back("          left/right mouse button = break/place blocks, mouse wheel = select blocks,");
		chatMessages.push_back("					F1 = switch game mode, F2 = take screenshot, F3 = switch debug panel,");
		chatMessages.push_back("					F4 = switch cross wall (creative mode), F5 = switch HUD,");
		chatMessages.push_back("					F7 = switch full screen mode, F8 = fast forward game time");
		chatMessages.push_back("Commands: /help | /clear | /kit | /give <id> <amount> | /tp <x> <y> <z> | /clearinventory | /suicide");
		chatMessages.push_back("        | /setblock <x> <y> <z> <id> | /tree <x> <y> <z> | /explode <x> <y> <z> <radius> | /time <time>");
		return true;
	}));
	commands.push_back(Command("/clear", [](const vector<string>& command) {
		if (command.size() != 1) return false;
		chatMessages.clear();
		return true;
	}));
	commands.push_back(Command("/kit", [](const vector<string>& command) {
		if (command.size() != 1) return false;
		Player::inventory[0][0] = 1;
		Player::inventoryAmount[0][0] = 255;
		Player::inventory[0][1] = 2;
		Player::inventoryAmount[0][1] = 255;
		Player::inventory[0][2] = 3;
		Player::inventoryAmount[0][2] = 255;
		Player::inventory[0][3] = 4;
		Player::inventoryAmount[0][3] = 255;
		Player::inventory[0][4] = 5;
		Player::inventoryAmount[0][4] = 255;
		Player::inventory[0][5] = 6;
		Player::inventoryAmount[0][5] = 255;
		Player::inventory[0][6] = 7;
		Player::inventoryAmount[0][6] = 255;
		Player::inventory[0][7] = 8;
		Player::inventoryAmount[0][7] = 255;
		Player::inventory[0][8] = 9;
		Player::inventoryAmount[0][8] = 255;
		Player::inventory[0][9] = 10;
		Player::inventoryAmount[0][9] = 255;
		Player::inventory[1][0] = 11;
		Player::inventoryAmount[1][0] = 255;
		Player::inventory[1][1] = 12;
		Player::inventoryAmount[1][1] = 255;
		Player::inventory[1][2] = 13;
		Player::inventoryAmount[1][2] = 255;
		Player::inventory[1][3] = 14;
		Player::inventoryAmount[1][3] = 255;
		Player::inventory[1][4] = 15;
		Player::inventoryAmount[1][4] = 255;
		Player::inventory[1][5] = 16;
		Player::inventoryAmount[1][5] = 255;
		Player::inventory[1][6] = 17;
		Player::inventoryAmount[1][6] = 255;
		Player::inventory[1][7] = 18;
		Player::inventoryAmount[1][7] = 255;
		return true;
	}));
	commands.push_back(Command("/give", [](const vector<string>& command) {
		if (command.size() != 3) return false;
		ItemID itemid;
		conv(command[1], itemid);
		short amount;
		conv(command[2], amount);
		Player::addItem(itemid, amount);
		return true;
	}));
	commands.push_back(Command("/tp", [](const vector<string>& command) {
		if (command.size() != 4) return false;
		double x;
		conv(command[1], x);
		double y;
		conv(command[2], y);
		double z;
		conv(command[3], z);
		Player::xpos = x;
		Player::ypos = y;
		Player::zpos = z;
		return true;
	}));
	commands.push_back(Command("/clearinventory", [](const vector<string>& command) {
		if (command.size() != 1) return false;
		for (int i = 0; i < 4; i++) {
			for (int j = 0; j < 10; j++) {
				Player::inventory[i][j] = 0;
				Player::inventoryAmount[i][j] = 0;
			}
		}
		return true;
	}));
	commands.push_back(Command("/suicide", [](const vector<string>& command) {
		if (command.size() != 1) return false;
		Player::spawn();
		return true;
	}));
	commands.push_back(Command("/setblock", [](const vector<string>& command) {
		if (command.size() != 5) return false;
		int x;
		conv(command[1], x);
		int y;
		conv(command[2], y);
		int z;
		conv(command[3], z);
		BlockID b;
		conv(command[4], b);
		World::setblock(x, y, z, b);
		return true;
	}));
	commands.push_back(Command("/tree", [](const vector<string>& command) {
		if (command.size() != 4) return false;
		int x;
		conv(command[1], x);
		int y;
		conv(command[2], y);
		int z;
		conv(command[3], z);
		World::buildtree(x, y, z);
		return true;
	}));
	commands.push_back(Command("/explode", [](const vector<string>& command) {
		if (command.size() != 5) return false;
		int x;
		conv(command[1], x);
		int y;
		conv(command[2], y);
		int z;
		conv(command[3], z);
		int r;
		conv(command[4], r);
		World::explode(x, y, z, r);
		return true;
	}));
	commands.push_back(Command("/time", [](const vector<string>& command) {
		if (command.size() != 2) return false;
		int time;
		conv(command[1], time);
		if (time < 0) return false;
		GameTime = time;
		return true;
	}));
	commands.push_back(Command("/gamemode", [](const vector<string>& command) {
		if (command.size() != 2) return false;
		int mode;
		conv(command[1], mode);
		Player::changeGameMode(mode);
		return true;
	}));
}

bool doCommand(const vector<string>& command) {
	for (unsigned int i = 0; i != commands.size(); i++) {
		if (command[0] == commands[i].identifier)
			return commands[i].execute(command);
	}
	return false;
}

void gameUpdate() {
	const int SelectPrecision = 32;
	const int SelectDistance = 8;
	const float WalkSpeed = 0.15f;
	const float RunSpeed = 0.3f;
	const int MaxAirJumps = 3 - 1;

	static double wPressTimer;
	static bool wPressedOnce;

	Player::BlockInHand = Player::inventory[3][Player::indexInHand];

	// 生命值相关
	if (Player::health > 0 || Player::gamemode == Player::Creative) {
		if (Player::ypos < -100) Player::health -= ((-100) - Player::ypos) / 100;
		if (Player::health < Player::healthMax) Player::health += Player::healSpeed;
		if (Player::health > Player::healthMax) Player::health = Player::healthMax;
	} else Player::spawn();

	// 时间
	GameTime++;

	World::meshedChunks = 0;
	World::updatedChunks = 0;

	// Move chunk pointer array
	if (World::cpArray.originX != Player::cxt - RenderDistance - 2 || World::cpArray.originY != Player::cyt - RenderDistance - 2 || World::cpArray.originZ != Player::czt - RenderDistance - 2)
		World::cpArray.moveTo(Player::cxt - RenderDistance - 2, Player::cyt - RenderDistance - 2, Player::czt - RenderDistance - 2);

	// Move height map
	if (World::HMap.originX != (Player::cxt - RenderDistance - 2) * 16 || World::HMap.originZ != (Player::czt - RenderDistance - 2) * 16)
		World::HMap.moveTo((Player::cxt - RenderDistance - 2) * 16, (Player::czt - RenderDistance - 2) * 16);

	for (auto const& [_, c] : World::chunks) {
		// 加载动画
		c->updateLoadAnimOffset();

		// 随机状态更新
		if (rnd() < 1.0 / 8.0) {
			int x, y, z, gx, gy, gz;
			int cx = c->x();
			int cy = c->y();
			int cz = c->z();
			x = int(rnd() * 16);
			gx = x + cx * 16;
			y = int(rnd() * 16);
			gy = y + cy * 16;
			z = int(rnd() * 16);
			gz = z + cz * 16;
			if (c->getblock(x, y, z) == Blocks::DIRT &&
				World::getblock(gx, gy + 1, gz, Blocks::NONEMPTY) == Blocks::AIR && (
					World::getblock(gx + 1, gy, gz, Blocks::AIR) == Blocks::GRASS ||
					World::getblock(gx - 1, gy, gz, Blocks::AIR) == Blocks::GRASS ||
					World::getblock(gx, gy, gz + 1, Blocks::AIR) == Blocks::GRASS ||
					World::getblock(gx, gy, gz - 1, Blocks::AIR) == Blocks::GRASS ||
					World::getblock(gx + 1, gy + 1, gz, Blocks::AIR) == Blocks::GRASS ||
					World::getblock(gx - 1, gy + 1, gz, Blocks::AIR) == Blocks::GRASS ||
					World::getblock(gx, gy + 1, gz + 1, Blocks::AIR) == Blocks::GRASS ||
					World::getblock(gx, gy + 1, gz - 1, Blocks::AIR) == Blocks::GRASS ||
					World::getblock(gx + 1, gy - 1, gz, Blocks::AIR) == Blocks::GRASS ||
					World::getblock(gx - 1, gy - 1, gz, Blocks::AIR) == Blocks::GRASS ||
					World::getblock(gx, gy - 1, gz + 1, Blocks::AIR) == Blocks::GRASS ||
					World::getblock(gx, gy - 1, gz - 1, Blocks::AIR) == Blocks::GRASS)) {
				// 长草
				c->setblock(x, y, z, Blocks::GRASS);
				World::updateblock(x + cx * 16, y + cy * 16 + 1, z + cz * 16, true);
			}
			if (c->getblock(x, y, z) == Blocks::GRASS && World::getblock(gx, gy + 1, gz, Blocks::AIR) != Blocks::AIR) {
				// 草被覆盖
				c->setblock(x, y, z, Blocks::DIRT);
				World::updateblock(x + cx * 16, y + cy * 16 + 1, z + cz * 16, true);
			}
		}
	}

	// 判断选中的方块
	double lx, ly, lz, lxl, lyl, lzl;
	lx = Player::xpos;
	ly = Player::ypos + Player::height + Player::heightExt;
	lz = Player::zpos;

	sel = false;
	selx = sely = selz = selbx = selby = selbz = selcx = selcy = selcz = selb = selbr = 0;

	if (chatmode) {
		shouldShowCursor = true;

		if (isKeyPressed(GLFW_KEY_ENTER)) {
			chatmode = false;
			mxl = mx;
			myl = my;
			mwl = mw;
			mbl = mb;
			if (!chatword.empty()) { // 指令的执行，或发出聊天文本
				if (chatword[0] == '/') { // 指令
					auto utf8 = UnicodeUTF8(chatword);
					std::vector<string> command = split(utf8, " ");
					if (!doCommand(command)) { // 执行失败
						DebugWarning("Fail to execute the command: " + utf8);
						chatMessages.push_back("Fail to execute the command: " + utf8);
					}
				}
				else
					chatMessages.push_back(UnicodeUTF8(chatword));
			}
			chatword.clear();
		}

		if (!inputstr.empty()) {
			chatword += inputstr;
			inputstr.clear();
		}
		if (backspace && !chatword.empty()) {
			chatword = chatword.substr(0, chatword.length() - 1);
			backspace = false;
		}

		// 自动补全
		if (isKeyPressed(GLFW_KEY_TAB) && chatmode && !chatword.empty() && chatword[0] == '/') {
			for (unsigned int i = 0; i != commands.size(); i++) {
				if (beginWith(commands[i].identifier, UnicodeUTF8(chatword)))
					chatword = UTF8Unicode(commands[i].identifier);
			}
		}
	}
	else if (bagOpened) {
		shouldShowCursor = true;

		if (isKeyPressed(GLFW_KEY_E)) {
			bagOpened = false;
			bagAnimTimer = timer();
			mxl = mx;
			myl = my;
			mwl = mw;
			mbl = mb;
		}
	}
	else {
		shouldShowCursor = false;
		inputstr.clear();
		backspace = false;

		// 从玩家位置发射一条线段
		for (int i = 0; i < SelectPrecision * SelectDistance; i++) {
			lxl = lx;
			lyl = ly;
			lzl = lz;

			// 线段延伸
			lx += std::sin(Pi / 180 * (Player::heading - 180)) * sin(Pi / 180 * (Player::lookupdown + 90)) / SelectPrecision;
			ly += std::cos(Pi / 180 * (Player::lookupdown + 90)) / SelectPrecision;
			lz += std::cos(Pi / 180 * (Player::heading - 180)) * sin(Pi / 180 * (Player::lookupdown + 90)) / SelectPrecision;

			// 碰到方块
			if (BlockInfo(World::getblock(RoundInt(lx), RoundInt(ly), RoundInt(lz))).isSolid()) {
				int x, y, z, xl, yl, zl;
				x = RoundInt(lx);
				y = RoundInt(ly);
				z = RoundInt(lz);
				xl = RoundInt(lxl);
				yl = RoundInt(lyl);
				zl = RoundInt(lzl);

				selx = x;
				sely = y;
				selz = z;
				sel = true;

				// 找方块所在区块及位置
				selcx = getchunkpos(x);
				selcy = getchunkpos(y);
				selcz = getchunkpos(z);
				selbx = getblockpos(x);
				selby = getblockpos(y);
				selbz = getblockpos(z);

				if (World::chunkOutOfBound(selcx, selcy, selcz) == false) {
					World::Chunk* cp = World::getChunkPtr(selcx, selcy, selcz);
					if (cp == nullptr || cp == World::EmptyChunkPtr) continue;
					selb = cp->getblock(selbx, selby, selbz);
				}
				selbr = World::getbrightness(xl, yl, zl);
				selb = World::getblock(x, y, z);
				if (mb == 1) { // 鼠标左键
					Particles::throwParticle(selb,
											 float(x + rnd() - 0.5f), float(y + rnd() - 0.2f), float(z + rnd() - 0.5f),
											 float(rnd() * 0.2f - 0.1f), float(rnd() * 0.2f - 0.1f), float(rnd() * 0.2f - 0.1f),
											 float(rnd() * 0.01f + 0.02f), int(rnd() * 30) + 30);

					const float MinHardness = 0.2f;
					if (selx != oldselx || sely != oldsely || selz != oldselz) seldes = 0.0f;
					else if (Player::gamemode == Player::Creative) seldes += 1.0f / 30.0f / MinHardness;
					else if (BlockInfo(selb).getHardness() <= MinHardness) seldes += 1.0f / 30.0f / MinHardness;
					else seldes += 1.0f / 30.0f / BlockInfo(selb).getHardness();

					if (seldes >= 1.0f) {
						Player::addItem(selb);
						for (int j = 1; j <= 25; j++) {
							Particles::throwParticle(selb,
													 float(x + rnd() - 0.5f), float(y + rnd() - 0.2f), float(z + rnd() - 0.5f),
													 float(rnd() * 0.2f - 0.1f), float(rnd() * 0.2f - 0.1f), float(rnd() * 0.2f - 0.1f),
													 float(rnd() * 0.02 + 0.03), int(rnd() * 60) + 30);
						}
						World::pickblock(x, y, z);
					}
				}
				if (mb == 2 && mbp == false) { // 鼠标右键
					if ( Player::inventoryAmount[3][Player::indexInHand] > 0 && isBlock(Player::inventory[3][Player::indexInHand])) {
						// 放置方块
						if (Player::putBlock(xl, yl, zl, Player::BlockInHand)) {
							Player::inventoryAmount[3][Player::indexInHand]--;
							if (Player::inventoryAmount[3][Player::indexInHand] == 0) Player::inventory[3][Player::indexInHand] = Blocks::AIR;
						}
					} else {
						// 使用物品

					}
				}
				break;
			}
		}

		if (selx != oldselx || sely != oldsely || selz != oldselz || mb == 0) seldes = 0.0f;
		oldselx = selx;
		oldsely = sely;
		oldselz = selz;

		Player::intxpos = RoundInt(Player::xpos);
		Player::intypos = RoundInt(Player::ypos);
		Player::intzpos = RoundInt(Player::zpos);

		// 更新方向
		Player::heading += Player::xlookspeed;
		Player::lookupdown += Player::ylookspeed;
		Player::xlookspeed = Player::ylookspeed = 0.0;

		// 移动！(生命在于运动)
		if (isKeyDown(GLFW_KEY_W) || Player::glidingNow) {
			if (!wPressedOnce) {
				if (wPressTimer == 0.0) {
					wPressTimer = timer();
				}
				else {
					if (timer() - wPressTimer <= 0.5) { Player::Running = true; wPressTimer = 0.0; }
					else wPressTimer = timer();
				}
			}
			if (wPressTimer != 0.0 && timer() - wPressTimer > 0.5) wPressTimer = 0.0;
			wPressedOnce = true;
			if (!Player::glidingNow) {
				Player::xa += -sin(Player::heading * Pi / 180.0) * Player::speed;
				Player::za += -cos(Player::heading * Pi / 180.0) * Player::speed;
			} else {
				Player::xa = sin(Pi / 180 * (Player::heading - 180)) * sin(Pi / 180 * (Player::lookupdown + 90)) * Player::glidingSpeed * speedCast;
				Player::ya = cos(Pi / 180 * (Player::lookupdown + 90)) * Player::glidingSpeed * speedCast;
				Player::za = cos(Pi / 180 * (Player::heading - 180)) * sin(Pi / 180 * (Player::lookupdown + 90)) * Player::glidingSpeed * speedCast;
				if (Player::ya < 0) Player::ya *= 2;
			}
		} else {
			Player::Running = false;
			wPressedOnce = false;
		}
		if (Player::Running) Player::speed = RunSpeed;
		else Player::speed = WalkSpeed;

		if (isKeyDown(GLFW_KEY_S) && !Player::glidingNow) {
			Player::xa += sin(Player::heading * Pi / 180.0) * Player::speed;
			Player::za += cos(Player::heading * Pi / 180.0) * Player::speed;
			wPressTimer = 0.0;
		}

		if (isKeyDown(GLFW_KEY_A) && !Player::glidingNow) {
			Player::xa += sin((Player::heading - 90) * Pi / 180.0) * Player::speed;
			Player::za += cos((Player::heading - 90) * Pi / 180.0) * Player::speed;
			wPressTimer = 0.0;
		}

		if (isKeyDown(GLFW_KEY_D) && !Player::glidingNow) {
			Player::xa += -sin((Player::heading - 90) * Pi / 180.0) * Player::speed;
			Player::za += -cos((Player::heading - 90) * Pi / 180.0) * Player::speed;
			wPressTimer = 0.0;
		}

		if (!Player::Flying && !Player::CrossWall) {
			double horizontalSpeed = sqrt(Player::xa * Player::xa + Player::za * Player::za);
			if (horizontalSpeed > Player::speed && !Player::glidingNow) {
				Player::xa *= Player::speed / horizontalSpeed;
				Player::za *= Player::speed / horizontalSpeed;
			}
		} else {
			if (isKeyDown(GLFW_KEY_R) && !Player::glidingNow) {
				if (isKeyDown(GLFW_KEY_LEFT_CONTROL)) {
					Player::xa = -sin(Player::heading * Pi / 180.0) * RunSpeed * 10;
					Player::za = -cos(Player::heading * Pi / 180.0) * RunSpeed * 10;
				} else {
					Player::xa = sin(Pi / 180 * (Player::heading - 180)) * sin(Pi / 180 * (Player::lookupdown + 90)) * RunSpeed * 20;
					Player::ya = cos(Pi / 180 * (Player::lookupdown + 90)) * RunSpeed * 20;
					Player::za = cos(Pi / 180 * (Player::heading - 180)) * sin(Pi / 180 * (Player::lookupdown + 90)) * RunSpeed * 20;
				}
			}

			if (isKeyDown(GLFW_KEY_F) && !Player::glidingNow) {
				if (isKeyDown(GLFW_KEY_LEFT_CONTROL)) {
					Player::xa = sin(Player::heading * Pi / 180.0) * RunSpeed * 10;
					Player::za = cos(Player::heading * Pi / 180.0) * RunSpeed * 10;
				} else {
					Player::xa = -sin(Pi / 180 * (Player::heading - 180)) * sin(Pi / 180 * (Player::lookupdown + 90)) * RunSpeed * 20;
					Player::ya = -cos(Pi / 180 * (Player::lookupdown + 90)) * RunSpeed * 20;
					Player::za = -cos(Pi / 180 * (Player::heading - 180)) * sin(Pi / 180 * (Player::lookupdown + 90)) * RunSpeed * 20;
				}
			}
		}

		// 切换方块
		if (isKeyPressed(GLFW_KEY_Z) && Player::indexInHand > 0) Player::indexInHand--;
		if (isKeyPressed(GLFW_KEY_X) && Player::indexInHand < 9) Player::indexInHand++;
		if ((int)Player::indexInHand + (mwl - mw) < 0)Player::indexInHand = 0;
		else if ((int)Player::indexInHand + (mwl - mw) > 9)Player::indexInHand = 9;
		else Player::indexInHand += (char)(mwl - mw);
		mwl = mw;

		// 起跳/上升
		if (isKeyDown(GLFW_KEY_SPACE)) {
			if (!Player::Flying && !Player::CrossWall) {
				if (!Player::inWater) {
					if (isKeyPressed(GLFW_KEY_SPACE) && Player::AirJumps < MaxAirJumps || Player::OnGround) {
						if (Player::OnGround == false) {
							Player::jump = 0.3;
							Player::AirJumps++;
						}
						else {
							Player::jump = 0.3;
							Player::OnGround = false;
						}
					}
				}
				else {
					Player::ya = 0.2;
				}
			}
			else {
				Player::ya += WalkSpeed / 2;
			}
		}

		if ((isKeyDown(GLFW_KEY_LEFT_SHIFT) || isKeyDown(GLFW_KEY_RIGHT_SHIFT)) && !Player::glidingNow) {
			if (Player::CrossWall || Player::Flying) Player::ya -= WalkSpeed / 2;
			wPressTimer = 0.0;
		}

		// Open inventory
		if (isKeyPressed(GLFW_KEY_E) && showHUD) {
			bagOpened = true;
			bagAnimTimer = timer();
			shouldGetThumbnail = true;
		}

		// Some undocumented functions
		if (isKeyDown(GLFW_KEY_K) && Player::Glide && !Player::OnGround && !Player::glidingNow) {
			double h = Player::ypos + Player::height + Player::heightExt;
			Player::glidingEnergy = g * h;
			Player::glidingSpeed = 0;
			Player::glidingNow = true;
		}

		if (isKeyPressed(GLFW_KEY_L)) World::saveAllChunks();

		// 各种设置切换
		if (isKeyPressed(GLFW_KEY_F1)) {
			Player::changeGameMode(Player::gamemode == Player::Creative ?
									  Player::Survival : Player::Creative);
		}
		if (isKeyPressed(GLFW_KEY_F2)) {
			shouldGetScreenshot = true;
			screenshotAnimTimer = timer();
		}
		if (isKeyPressed(GLFW_KEY_F3)) showDebugPanel = !showDebugPanel;
		if (isKeyPressed(GLFW_KEY_H) && isKeyDown(GLFW_KEY_F3)) {
			showHitboxes = !showHitboxes;
			showDebugPanel = true;
		}
		if (Renderer::AdvancedRender) {
			if (isKeyPressed(GLFW_KEY_M) && isKeyDown(GLFW_KEY_F3)) {
				showShadowMap = !showShadowMap;
				showDebugPanel = true;
			}
		} else showShadowMap = false;
		if (isKeyPressed(GLFW_KEY_G) && isKeyDown(GLFW_KEY_F3)) {
			showMeshWireframe = !showMeshWireframe;
			showDebugPanel = true;
		}
		if (isKeyPressed(GLFW_KEY_F4) && Player::gamemode == Player::GameMode::Creative) Player::CrossWall = !Player::CrossWall;
		if (isKeyPressed(GLFW_KEY_F5)) showHUD = !showHUD;
		if (isKeyPressed(GLFW_KEY_F6)) Player::Glide = !Player::Glide;
		if (isKeyPressed(GLFW_KEY_F7)) shouldToggleFullscreen = true;
		if (isKeyDown(GLFW_KEY_F8)) GameTime += 30;
		if (isKeyPressed(GLFW_KEY_ENTER)) chatmode = true;
		if (isKeyPressed(GLFW_KEY_SLASH)) {
			chatmode = true;
			chatword = UTF8Unicode("/");
		}
	}

	// 跳跃
	if (!Player::glidingNow) {
		if (!Player::inWater) {
			if (!Player::Flying && !Player::CrossWall) {
				Player::ya = -0.001;
				if (Player::OnGround) {
					Player::jump = 0.0;
					Player::AirJumps = 0;
				} else {
					Player::jump -= 0.03;
					Player::ya = Player::jump + 0.03 / 2.0;
				}
			} else {
				Player::jump = 0.0;
				Player::AirJumps = 0;
			}
		} else {
			Player::jump = 0.0;
			Player::AirJumps = MaxAirJumps;
			if (Player::ya <= 0.001 && !Player::Flying && !Player::CrossWall) {
				Player::ya = -0.001;
				if (!Player::OnGround) Player::ya -= 0.1;
			}
		}
	}

	// 爬墙
	//if (Player::NearWall && Player::Flying == false && Player::CrossWall == false) {
	//  Player::ya += walkspeed
	//  Player::jump = 0.0
	//}

	if (Player::glidingNow) {
		double& E = Player::glidingEnergy;
		double oldh = Player::ypos + Player::height + Player::heightExt + Player::ya;
		double h = oldh;
		if (E - Player::glidingMinimumSpeed < h * g)  // 小于最小速度
			h = (E - Player::glidingMinimumSpeed) / g;
		Player::glidingSpeed = sqrt(2 * (E - g * h));
		E -= EDrop;
		Player::ya += h - oldh;
	}

	mbp = mb;
	updateKeyStates();
	Particles::updateall();

	Player::intxpos = RoundInt(Player::xpos);
	Player::intypos = RoundInt(Player::ypos);
	Player::intzpos = RoundInt(Player::zpos);
	Player::updatePosition();
	Player::xposold = Player::xpos;
	Player::yposold = Player::ypos;
	Player::zposold = Player::zpos;
	Player::intxposold = RoundInt(Player::xpos);
	Player::intyposold = RoundInt(Player::ypos);
	Player::intzposold = RoundInt(Player::zpos);
}

void frameLinkedUpdate() {
	// Find chunks for unloading & loading & meshing
	World::sortChunkUpdateLists(RoundInt(Player::xpos), RoundInt(Player::ypos), RoundInt(Player::zpos));

	// Load chunks
	for (auto load : World::chunkLoadList) {
		int cx = std::get<1>(load);
		int cy = std::get<2>(load);
		int cz = std::get<3>(load);
		World::Chunk* c = World::AddChunk(cx, cy, cz);
		if (c->empty()) {
			World::DeleteChunk(cx, cy, cz);
			World::cpArray.setChunkPtr(cx, cy, cz, World::EmptyChunkPtr);
		}
	}

	// Unload chunks
	for (auto unload : World::chunkUnloadList) {
		int cx = std::get<1>(unload);
		int cy = std::get<2>(unload);
		int cz = std::get<3>(unload);
		World::DeleteChunk(cx, cy, cz);
	}

	// Mesh updated chunks
	for (auto meshing : World::chunkMeshingList) {
		meshing.second->buildMeshes();
	}

	// 处理计时
	double currTimer = timer();

	// 视野特效
	if (Player::Running) {
		if (FOVyExt < 9.8f) {
			float timeDelta = static_cast<float>(currTimer - speedupAnimTimer);
			FOVyExt = 10.0f - (10.0f - FOVyExt) * std::pow(0.8f, timeDelta * 30.0f);
		}
		else FOVyExt = 10.0f;
	}
	else {
		if (FOVyExt > 0.2f) {
			float timeDelta = static_cast<float>(currTimer - speedupAnimTimer);
			FOVyExt *= std::pow(0.8f, timeDelta * 30.0f);
		}
		else FOVyExt = 0.0f;
	}
	speedupAnimTimer = currTimer;

	// 半蹲特效
	if (Player::OnGround) {
		if (Player::jump < -0.005) {
			if (Player::jump <= -(Player::height - 0.5f))
				Player::heightExt = -(Player::height - 0.5f);
			else
				Player::heightExt = static_cast<float>(Player::jump);
		}
		else {
			if (Player::heightExt <= -0.005) {
				float timeDelta = static_cast<float>(currTimer - touchdownAnimTimer);
				Player::heightExt *= std::pow(0.8f, timeDelta * 30.0f);
			}
		}
	}
	touchdownAnimTimer = currTimer;

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
		if (mx != mxl) Player::xlookspeed -= (mx - mxl) * MouseSpeed;
		if (my != myl) Player::ylookspeed += (my - myl) * MouseSpeed;
		// 限制角度，别把头转掉下来了 ←_←
		if (Player::lookupdown + Player::ylookspeed < -90.0) Player::ylookspeed = -90.0 - Player::lookupdown;
		if (Player::lookupdown + Player::ylookspeed > 90.0) Player::ylookspeed = 90.0 - Player::lookupdown;
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
void render() {
	const float SkyColorR = 0.70f;
	const float SkyColorG = 0.80f;
	const float SkyColorB = 0.86f;

	double currTimer = timer();
	double interp = (currTimer - updateTimer) * 30.0;
	double xpos = Player::xpos - Player::xd + interp * Player::xd;
	double ypos = Player::ypos + Player::height + Player::heightExt - Player::yd + interp * Player::yd;
	double zpos = Player::zpos - Player::zd + interp * Player::zd;

	// Calculate sun position (temporary: horizontal movement only)
	float interpolatedTime = GameTime - 1.0f + static_cast<float>(interp);
	Renderer::sunlightHeading = interpolatedTime / 43200.0f * 360.0f;

	// World rendering starts here
	double plookupdown = Player::lookupdown + Player::ylookspeed;
	double pheading = Player::heading + Player::xlookspeed;

	// Calculate matrices
	Player::ViewFrustum.LoadIdentity();
	Player::ViewFrustum.MultRotate(-static_cast<float>(pheading), 0.0f, 1.0f, 0.0f);
	Player::ViewFrustum.MultRotate(static_cast<float>(plookupdown), 1.0f, 0.0f, 0.0f);
	Player::ViewFrustum.MultPerspective(FOVyNormal + FOVyExt, static_cast<float>(WindowWidth) / WindowHeight, 0.05f, RenderDistance * 16.0f);

	// Clear framebuffers
	if (Renderer::AdvancedRender) Renderer::ClearSGDBuffers();

	glClearColor(SkyColorR, SkyColorG, SkyColorB, 1.0f);
	glClearDepth(1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Bind main texture array
	glBindTexture(GL_TEXTURE_2D_ARRAY, BlockTextureArray);

	if (showMeshWireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	// Build shadow map
	FrustumTest lightFrustum = Renderer::getShadowMapFrustum();
	if (Renderer::AdvancedRender) {
		FrustumTest lightFrustumTest = Renderer::getShadowMapFrustumExperimental(pheading, plookupdown, Player::ViewFrustum);
		WorldRenderer::ListRenderChunks(xpos, ypos, zpos, Renderer::getShadowDistance(), interp, lightFrustumTest);
		Renderer::StartShadowPass(lightFrustum, interpolatedTime);
		WorldRenderer::RenderChunks(xpos, ypos, zpos, 0);
		Renderer::shaders[Renderer::ActiveShader].setUniform("u_translation", Mat4f(1.0f).data);
		Particles::renderall(xpos, ypos, zpos, interp);
		Renderer::EndShadowPass();
	}

	// Draw the opaque parts of the world
	WorldRenderer::ListRenderChunks(xpos, ypos, zpos, RenderDistance, interp, Player::ViewFrustum);
	Renderer::StartOpaquePass(Player::ViewFrustum, interpolatedTime);
	WorldRenderer::RenderChunks(xpos, ypos, zpos, 0);
	Renderer::shaders[Renderer::ActiveShader].setUniform("u_translation", Mat4f(1.0f).data);
	Particles::renderall(xpos, ypos, zpos, interp);
	Renderer::EndOpaquePass();

	// Draw the translucent parts of the world
	Renderer::StartTranslucentPass(Player::ViewFrustum, interpolatedTime);
	glDisable(GL_CULL_FACE);
	if (sel) {
		float x = static_cast<float>(selx - xpos);
		float y = static_cast<float>(sely - ypos);
		float z = static_cast<float>(selz - zpos);
		Renderer::shaders[Renderer::ActiveShader].setUniform("u_translation", Mat4f::translation(Vec3f(x, y, z)).data);
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
	WorldRenderer::RenderChunks(xpos, ypos, zpos, 1);
	glEnable(GL_CULL_FACE);
	Renderer::EndTranslucentPass();

	if (showMeshWireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	// Full screen passes
	if (Renderer::AdvancedRender) {
		Renderer::StartFinalPass(xpos, ypos, zpos, Player::ViewFrustum, lightFrustum, interpolatedTime);
		Renderer::Begin(GL_QUADS, 2, 2, 0);
		Renderer::TexCoord2f(0.0f, 1.0f); Renderer::Vertex2i(0, 0);
		Renderer::TexCoord2f(0.0f, 0.0f); Renderer::Vertex2i(0, WindowHeight);
		Renderer::TexCoord2f(1.0f, 0.0f); Renderer::Vertex2i(WindowWidth, WindowHeight);
		Renderer::TexCoord2f(1.0f, 1.0f); Renderer::Vertex2i(WindowWidth, 0);
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

	if (World::getblock(RoundInt(xpos), RoundInt(ypos), RoundInt(zpos)) == Blocks::WATER) {
		auto& shader = Renderer::shaders[Renderer::UIShader];
		shader.bind();
		glBindTexture(GL_TEXTURE_2D_ARRAY, BlockTextureArray);
		Renderer::Begin(GL_QUADS, 2, 3, 4);
		Renderer::Color4f(1.0f, 1.0f, 1.0f, 1.0f);
		Renderer::TexCoord3f(0.0f, 0.0f, static_cast<float>(Textures::getTextureIndex(Blocks::WATER, 1)));
		Renderer::TexCoord2f(0.0f, 1.0f); Renderer::Vertex2i(0, 0);
		Renderer::TexCoord2f(0.0f, 0.0f); Renderer::Vertex2i(0, WindowHeight);
		Renderer::TexCoord2f(1.0f, 0.0f); Renderer::Vertex2i(WindowWidth, WindowHeight);
		Renderer::TexCoord2f(1.0f, 1.0f); Renderer::Vertex2i(WindowWidth, 0);
		Renderer::End().render();
		shader.unbind();
	}

	if (showHUD) {
		drawGUI();
		drawBag();
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

void saveScreenshot(int x, int y, int w, int h, string filename) {
	Textures::ImageRGB scrBuffer;
	int bufw = w, bufh = h;
	while (bufw % 4 != 0) bufw += 1;
	while (bufh % 4 != 0) bufh += 1;
	scrBuffer.sizeX = bufw;
	scrBuffer.sizeY = bufh;
	scrBuffer.buffer = std::make_unique<uint8_t[]>(bufw * bufh * 3);
	glReadPixels(x, y, bufw, bufh, GL_RGB, GL_UNSIGNED_BYTE, scrBuffer.buffer.get());
	Textures::SaveRGBImage(filename, scrBuffer);
}

void readback() {
	if (shouldGetScreenshot) {
		shouldGetScreenshot = false;
		time_t t = time(0);
		char tmp[64];
		tm* timeinfo;
#ifdef NEWORLD_COMPILE_DISABLE_SECURE
		timeinfo = localtime(&t);
#else
		timeinfo = new tm;
		localtime_s(timeinfo, &t);
#endif
		strftime(tmp, sizeof(tmp), "%Y-%m-%d %H-%M-%S", timeinfo);
		delete timeinfo;
		std::stringstream ss;
		ss << "screenshots/" << tmp << ".bmp";
		saveScreenshot(0, 0, WindowWidth, WindowHeight, ss.str());
	}

	if (shouldGetThumbnail) {
		shouldGetThumbnail = false;
		std::stringstream ss;
		ss << "worlds/" << World::worldname << "/thumbnail.bmp";
		saveScreenshot(0, 0, WindowWidth, WindowHeight, ss.str());
	}
}

const float centers[6][3] = {
	{ +0.5f, 0.0f, 0.0f },
	{ -0.5f, 0.0f, 0.0f },
	{ 0.0f, +0.5f, 0.0f },
	{ 0.0f, -0.5f, 0.0f },
	{ 0.0f, 0.0f, +0.5f },
	{ 0.0f, 0.0f, -0.5f },
};

const float cube[6][4][3] = {
	{ { +0.5f, -0.5f, +0.5f }, { +0.5f, -0.5f, -0.5f }, { +0.5f, +0.5f, -0.5f }, { +0.5f, +0.5f, +0.5f } },
	{ { -0.5f, -0.5f, -0.5f }, { -0.5f, -0.5f, +0.5f }, { -0.5f, +0.5f, +0.5f }, { -0.5f, +0.5f, -0.5f } },
	{ { -0.5f, +0.5f, +0.5f }, { +0.5f, +0.5f, +0.5f }, { +0.5f, +0.5f, -0.5f }, { -0.5f, +0.5f, -0.5f } },
	{ { -0.5f, -0.5f, -0.5f }, { +0.5f, -0.5f, -0.5f }, { +0.5f, -0.5f, +0.5f }, { -0.5f, -0.5f, +0.5f } },
	{ { -0.5f, -0.5f, +0.5f }, { +0.5f, -0.5f, +0.5f }, { +0.5f, +0.5f, +0.5f }, { -0.5f, +0.5f, +0.5f } },
	{ { +0.5f, -0.5f, -0.5f }, { -0.5f, -0.5f, -0.5f }, { -0.5f, +0.5f, -0.5f }, { +0.5f, +0.5f, -0.5f } },
};

const float texcoords[4][2] = {
	{ 0.0f, 0.0f },
	{ 1.0f, 0.0f },
	{ 1.0f, 1.0f },
	{ 0.0f, 1.0f },
};

// Draw the block selection border
void drawBorder(float x, float y, float z) {
	const float eps = 0.005f;
	const float width = 1.0f / 32.0f;

	if (Renderer::AdvancedRender) Renderer::Begin(GL_QUADS, 3, 3, 1, 3, 1);
	else Renderer::Begin(GL_QUADS, 3, 3, 1);
	Renderer::Attrib1f(65535.0f); // For indicator elements
	Renderer::TexCoord3f(0.0f, 0.0f, static_cast<float>(Textures::WHITE));
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
	const float eps = 0.005f;

	if (level <= 0.0f) return;
	int index = int(level * 8);
	if (index < 0) index = 0;
	if (index > 7) index = 7;

	if (Renderer::AdvancedRender) Renderer::Begin(GL_QUADS, 3, 3, 1, 3, 1);
	else Renderer::Begin(GL_QUADS, 3, 3, 1);
	Renderer::Attrib1f(65535.0f); // For indicator elements
	Renderer::TexCoord3f(0.0f, 0.0f, static_cast<float>(Textures::BREAKING_0 + index));
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

void drawGUI() {
	const int linelength = 10;
	const int linedist = 30;
	int disti = (int)(seldes * linedist);

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
		if (selb != Blocks::AIR) {
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

	if (Player::gamemode == Player::Survival) {
		glColor4d(0.8, 0.0, 0.0, 0.3);
		glBegin(GL_QUADS);
		glVertex2d(10, 10);
		glVertex2d(10, 30);
		glVertex2d(200, 30);
		glVertex2d(200, 10);
		glEnd();

		double healthPercent = (double)Player::health / Player::healthMax;
		glColor4d(1.0, 0.0, 0.0, 0.5);
		glBegin(GL_QUADS);
		glVertex2d(20, 15);
		glVertex2d(20, 25);
		glVertex2d(20 + healthPercent * 170, 25);
		glVertex2d(20 + healthPercent * 170, 15);
		glEnd();
	}

	glEnable(GL_TEXTURE_2D);

	if (Renderer::AdvancedRender && showShadowMap) {
		float xi = 1.0f - static_cast<float>(WindowHeight) / WindowWidth;
		float yi = 1.0f;
		float xa = 1.0f;
		float ya = 0.0f;

		Renderer::shadow.bindDepthTexture(0);
		auto& shader = Renderer::shaders[Renderer::DebugShadowShader];
		shader.bind();
		shader.setUniformI("u_shadow_texture", 0);
		Renderer::Begin(GL_QUADS, 2, 2, 0);
		Renderer::TexCoord2f(0.0f, 1.0f); Renderer::Vertex2f(xi, yi);
		Renderer::TexCoord2f(0.0f, 0.0f); Renderer::Vertex2f(xi, ya);
		Renderer::TexCoord2f(1.0f, 0.0f); Renderer::Vertex2f(xa, ya);
		Renderer::TexCoord2f(1.0f, 1.0f); Renderer::Vertex2f(xa, yi);
		Renderer::End().render();
		shader.unbind();

		/*
		auto const& viewFrustum = Player::ViewFrustum;
		auto lightFrustum = Renderer::getShadowMapFrustumExperimental(Player::heading, Player::lookupdown, Player::ViewFrustum);
		float length = Renderer::getShadowDistance() * 16.0f;

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
	auto debugText = [=](string s) mutable {
		TextRenderer::renderString(0, lineHeight * textPos, s);
		textPos++;
	};

	TextRenderer::setFontColor(1.0f, 1.0f, 1.0f, 0.8f);
	if (showDebugPanel && selb != Blocks::AIR) {
		std::stringstream ss;
		ss << BlockInfo(selb).getBlockName() << " (id: " << (int)selb << ")";
		TextRenderer::renderString(WindowWidth / 2 + 50, WindowHeight / 2 + 50 - TextRenderer::getLineHeight(), ss.str());
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
		TextRenderer::renderUnicodeString(0, WindowHeight - 33 - lineHeight, chatword);
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
		std::stringstream ss;
		ss << std::fixed << std::setprecision(4);

		ss << "v" << GameVersion << " [GL " << GLMajorVersion << "." << GLMinorVersion << "." << GLRevisionVersion << "]";
		debugText(ss.str());
		ss.str("");
		ss << fps << " fps, " << ups << " ups";
		debugText(ss.str());
		ss.str("");

		ss << "game mode: " << (Player::gamemode == Player::GameMode::Creative ? "creative" : "survival") << " mode";
		debugText(ss.str());
		ss.str("");
		if (Renderer::AdvancedRender) {
			ss << "shadow view: " << boolstr(showShadowMap);
			debugText(ss.str());
			ss.str("");
		}
		ss << "cross wall: " << boolstr(Player::CrossWall);
		debugText(ss.str());
		ss.str("");
		ss << "gliding: " << boolstr(Player::Glide);
		debugText(ss.str());
		ss.str("");

		ss << "x: " << Player::xpos << ", y: " << Player::ypos << ", z: " << Player::zpos;
		debugText(ss.str());
		ss.str("");
		ss << "heading: " << Player::heading << ", pitch: " << Player::lookupdown;
		debugText(ss.str());
		ss.str("");
		ss << "grounded: " << boolstr(Player::OnGround) << ", jump: " << Player::jump;
		debugText(ss.str());
		ss.str("");
		ss << "near wall: " << boolstr(Player::NearWall) << ", in water: " << boolstr(Player::inWater);
		debugText(ss.str());
		ss.str("");
		int h = (GameTime / 30 / 60) % 24;
		int m = (GameTime / 30) % 60;
		int s = GameTime % 30 * 2;
		ss << "time: "
			<< (h < 10 ? "0" : "") << h << ":"
			<< (m < 10 ? "0" : "") << m << ":"
			<< (s < 10 ? "0" : "") << s
			<< " (" << GameTime << "/" << 30 * 60 * 24 << ")";
		debugText(ss.str());
		ss.str("");
		/*
		ss << "gliding: " << boolstr(Player::glidingNow);
		debugText(ss.str());
		ss.str("");
		ss << "energy: " << Player::glidingEnergy;
		debugText(ss.str());
		ss.str("");
		ss << "speed: " << Player::glidingSpeed;
		debugText(ss.str());
		ss.str("");
		*/

		ss << World::loadedChunks << " chunks loaded";
		debugText(ss.str());
		ss.str("");
		ss << WorldRenderer::RenderChunkList.size() << " chunks rendered";
		debugText(ss.str());
		ss.str("");
		ss << World::unloadedChunks << " chunks unloaded";
		debugText(ss.str());
		ss.str("");
		ss << World::updatedChunks << " chunks updated";
		debugText(ss.str());
		ss.str("");

		if (Multiplayer) {
			MutexLock(Network::mutex);
			ss << Network::getRequestCount() << "/" << NetworkRequestMax << " network requests";
			debugText(ss.str());
			ss.str("");
			MutexUnlock(Network::mutex);
		}

#ifdef NEWORLD_DEBUG_PERFORMANCE_REC
		ss << c_getChunkPtrFromCPA << " chunk pointer array requests";
		debugText(ss.str());
		ss.str("");
		ss << c_getChunkPtrFromSearch << " search requests";
		debugText(ss.str());
		ss.str("");
		ss << c_getHeightFromHMap << " heightmap requests";
		debugText(ss.str());
		ss.str("");
		ss << c_getHeightFromWorldGen << " worldgen requests";
		debugText(ss.str());
		ss.str("");
#endif
	}
	else {
		std::stringstream ss;
		ss << "v" << GameVersion;
		debugText(ss.str());
		ss.str("");
		ss << fps << " fps";
		debugText(ss.str());
		ss.str("");
		ss << (Player::gamemode == Player::GameMode::Creative ? "creative" : "survival") << " mode";
		debugText(ss.str());
		ss.str("");
	}
}

void drawBagRow(int row, int itemid, int xbase, int ybase, int spac, float alpha) {
	// 画出背包的一行
	auto& shader = Renderer::shaders[Renderer::UIShader];
	for (int i = 0; i < 10; i++) {
		glBindTexture(GL_TEXTURE_2D, i == itemid ? SelectedTexture : UnselectedTexture);
		glBegin(GL_QUADS);
		glColor4f(1.0f, 1.0f, 1.0f, alpha);
		glTexCoord2f(0.0f, 1.0f); glVertex2i(xbase + i * (32 + spac), ybase);
		glTexCoord2f(1.0f, 1.0f); glVertex2i(xbase + i * (32 + spac), ybase + 32);
		glTexCoord2f(1.0f, 0.0f); glVertex2i(xbase + i * (32 + spac) + 32, ybase + 32);
		glTexCoord2f(0.0f, 0.0f); glVertex2i(xbase + i * (32 + spac) + 32, ybase);
		glEnd();

		if (Player::inventory[row][i] != Blocks::AIR) {
			shader.bind();
			glBindTexture(GL_TEXTURE_2D_ARRAY, BlockTextureArray);
			Renderer::Begin(GL_QUADS, 2, 3, 4);
			Renderer::Color4f(1.0f, 1.0f, 1.0f, 1.0f);
			Renderer::TexCoord3f(0.0f, 0.0f, static_cast<float>(Textures::getTextureIndex(Player::inventory[row][i], 1)));
			Renderer::TexCoord2f(0.0f, 1.0f); Renderer::Vertex2i(xbase + i * (32 + spac) + 2, ybase + 2);
			Renderer::TexCoord2f(0.0f, 0.0f); Renderer::Vertex2i(xbase + i * (32 + spac) + 2, ybase + 30);
			Renderer::TexCoord2f(1.0f, 0.0f); Renderer::Vertex2i(xbase + i * (32 + spac) + 30, ybase + 30);
			Renderer::TexCoord2f(1.0f, 1.0f); Renderer::Vertex2i(xbase + i * (32 + spac) + 30, ybase + 2);
			Renderer::End().render();
			shader.unbind();

			std::stringstream ss;
			ss << (int)Player::inventoryAmount[row][i];
			TextRenderer::renderString(xbase + i * (32 + spac), ybase, ss.str());
		}
	}
}

void drawBag() {
	// 背包界面与更新
	static int si, sj, sf;
	int csi = -1, csj = -1;
	int leftp = (WindowWidth - 392) / 2;
	int upp = WindowHeight - 152 - 16;
	static int mousew, mouseb, mousebl;
	static BlockID indexselected = Blocks::AIR;
	static short Amountselected = 0;
	double curtime = timer();
	double TimeDelta = curtime - bagAnimTimer;
	float bagAnim = (float)(1.0 - pow(0.9, TimeDelta * 60.0) + pow(0.9, bagAnimDuration * 60.0) / bagAnimDuration * TimeDelta);

	if (bagOpened) {
		mousew = mw;
		mouseb = mb;

		glDisable(GL_TEXTURE_2D);
		glBegin(GL_QUADS);
		if (curtime - bagAnimTimer > bagAnimDuration) glColor4f(0.2f, 0.2f, 0.2f, 0.6f);
		else glColor4f(0.2f, 0.2f, 0.2f, 0.6f * bagAnim);
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
					if (mx >= j * (32 + 8) + leftp && mx <= j * (32 + 8) + 32 + leftp &&
							my >= i * (32 + 8) + upp && my <= i * (32 + 8) + 32 + upp) {
						csi = si = i;
						csj = sj = j;
						sf = 1;
						if (mousebl == 0 && mouseb == 1 && indexselected == Player::inventory[i][j]) {
							if (Player::inventoryAmount[i][j] + Amountselected <= 255) {
								Player::inventoryAmount[i][j] += Amountselected;
								Amountselected = 0;
							} else {
								Amountselected = Player::inventoryAmount[i][j] + Amountselected - 255;
								Player::inventoryAmount[i][j] = 255;
							}
						}
						if (mousebl == 0 && mouseb == 1 && indexselected != Player::inventory[i][j]) {
							std::swap(Amountselected, Player::inventoryAmount[i][j]);
							std::swap(indexselected, Player::inventory[i][j]);
						}
						if (mousebl == 0 && mouseb == 2 && indexselected == Player::inventory[i][j] && Player::inventoryAmount[i][j] < 255) {
							Amountselected--;
							Player::inventoryAmount[i][j]++;
						}
						if (mousebl == 0 && mouseb == 2 && Player::inventory[i][j] == Blocks::AIR) {
							Amountselected--;
							Player::inventoryAmount[i][j] = 1;
							Player::inventory[i][j] = indexselected;
						}

						if (Amountselected == 0) indexselected = Blocks::AIR;
						if (indexselected == Blocks::AIR) Amountselected = 0;
						if (Player::inventoryAmount[i][j] == 0) Player::inventory[i][j] = Blocks::AIR;
						if (Player::inventory[i][j] == Blocks::AIR) Player::inventoryAmount[i][j] = 0;
					}
				}
				drawBagRow(i, (csi == i ? csj : -1), (WindowWidth - 392) / 2, WindowHeight - 152 - 16 + i * 40, 8, 1.0f);
			}
		}

		if (indexselected != Blocks::AIR) {
			auto& shader = Renderer::shaders[Renderer::UIShader];
			shader.bind();
			glBindTexture(GL_TEXTURE_2D_ARRAY, BlockTextureArray);
			Renderer::Begin(GL_QUADS, 2, 3, 4);
			Renderer::Color4f(1.0f, 1.0f, 1.0f, 1.0f);
			Renderer::TexCoord3f(0.0f, 0.0f, static_cast<float>(Textures::getTextureIndex(indexselected, 1)));
			Renderer::TexCoord2f(0.0f, 1.0f); Renderer::Vertex2i(mx - 16, my - 16);
			Renderer::TexCoord2f(0.0f, 0.0f); Renderer::Vertex2i(mx - 16, my + 16);
			Renderer::TexCoord2f(1.0f, 0.0f); Renderer::Vertex2i(mx + 16, my + 16);
			Renderer::TexCoord2f(1.0f, 1.0f); Renderer::Vertex2i(mx + 16, my - 16);
			Renderer::End().render();
			shader.unbind();

			std::stringstream ss;
			ss << Amountselected;
			TextRenderer::renderString((int)mx - 16, (int)my - 16, ss.str());
		}
		if (Player::inventory[si][sj] != 0 && sf == 1) {
			TextRenderer::renderString((int)mx, (int)my - 16, BlockInfo(Player::inventory[si][sj]).getBlockName());
		}

		int xbase = 0, ybase = 0, spac = 0;
		float alpha = 0.5f + 0.5f * bagAnim;
		if (curtime - bagAnimTimer <= bagAnimDuration) {
			xbase = (int)round(((WindowWidth - 392) / 2) * bagAnim);
			ybase = (int)round((WindowHeight - 152 - 16 + 120 - (WindowHeight - 32)) * bagAnim + (WindowHeight - 32));
			spac = (int)round(8 * bagAnim);
			drawBagRow(3, -1, xbase, ybase, spac, alpha);
			xbase = (int)round(((WindowWidth - 392) / 2 - WindowWidth) * bagAnim + WindowWidth);
			ybase = (int)round((WindowHeight - 152 - 16 - (WindowHeight - 32)) * bagAnim + (WindowHeight - 32));
			for (int i = 0; i < 3; i++) {
				drawBagRow(i, -1, xbase, ybase + i * 40, spac, alpha);
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
			xbase = (int)round(((WindowWidth - 392) / 2) - ((WindowWidth - 392) / 2) * bagAnim);
			ybase = (int)round((WindowHeight - 152 - 16 + 120 - (WindowHeight - 32)) - (WindowHeight - 152 - 16 + 120 - (WindowHeight - 32)) * bagAnim + (WindowHeight - 32));
			spac = (int)round(8 - 8 * bagAnim);
			drawBagRow(3, Player::indexInHand, xbase, ybase, spac, alpha);
			xbase = (int)round(((WindowWidth - 392) / 2 - WindowWidth) - ((WindowWidth - 392) / 2 - WindowWidth) * bagAnim + WindowWidth);
			ybase = (int)round((WindowHeight - 152 - 16 - (WindowHeight - 32)) - (WindowHeight - 152 - 16 - (WindowHeight - 32)) * bagAnim + (WindowHeight - 32));
			for (int i = 0; i < 3; i++) {
				drawBagRow(i, -1, xbase, ybase + i * 40, spac, alpha);
			}
		} else drawBagRow(3, Player::indexInHand, 0, WindowHeight - 32, 0, 0.5f);
	}
}

template<typename T>
void loadoption(std::map<string, string> &m, const char* name, T &value) {
	if (m.find(name) == m.end()) return;
	std::stringstream ss;
	ss << m[name];
	ss >> value;
}

void loadOptions() {
	std::map<std::string, std::string> options;
	std::ifstream filein("configs/options.ini", std::ios::in);
	if (!filein.is_open()) return;
	std::string name, value;
	while (!filein.eof()) {
		filein >> name >> value;
		options[name] = value;
	}
	filein.close();
	loadoption(options, "Language", Globalization::Cur_Lang);
	loadoption(options, "FOV", FOVyNormal);
	loadoption(options, "RenderDistance", RenderDistance);
	loadoption(options, "Sensitivity", MouseSpeed);
	loadoption(options, "SmoothLighting", SmoothLighting);
	loadoption(options, "FancyGrass", NiceGrass);
	loadoption(options, "MergeFaceRendering", MergeFace);
	loadoption(options, "MultiSample", Multisample);
	loadoption(options, "AdvancedRender", Renderer::AdvancedRender);
	loadoption(options, "ShadowMapRes", Renderer::ShadowRes);
	loadoption(options, "ShadowDistance", Renderer::MaxShadowDistance);
	loadoption(options, "SoftShadow", Renderer::SoftShadow);
	loadoption(options, "VolumetricClouds", Renderer::VolumetricClouds);
	loadoption(options, "AmbientOcclusion", Renderer::AmbientOcclusion);
	loadoption(options, "VerticalSync", VerticalSync);
	loadoption(options, "UIFontSize", TextRenderer::FontSize);
	loadoption(options, "UIStretch", UIStretch);
	loadoption(options, "UIBackgroundBlur", UIBackgroundBlur);
}

template<typename T>
void saveoption(std::ofstream &out, const char* name, T &value) {
	out << std::string(name) << " " << value << std::endl;
}

void saveOptions() {
	std::map<std::string, std::string> options;
	std::ofstream fileout("configs/options.ini", std::ios::out);
	if (!fileout.is_open()) return;
	saveoption(fileout, "Language", Globalization::Cur_Lang);
	saveoption(fileout, "FOV", FOVyNormal);
	saveoption(fileout, "RenderDistance", RenderDistance);
	saveoption(fileout, "Sensitivity", MouseSpeed);
	saveoption(fileout, "SmoothLighting", SmoothLighting);
	saveoption(fileout, "FancyGrass", NiceGrass);
	saveoption(fileout, "MergeFaceRendering", MergeFace);
	saveoption(fileout, "MultiSample", Multisample);
	saveoption(fileout, "AdvancedRender", Renderer::AdvancedRender);
	saveoption(fileout, "ShadowMapRes", Renderer::ShadowRes);
	saveoption(fileout, "ShadowDistance", Renderer::MaxShadowDistance);
	saveoption(fileout, "SoftShadow", Renderer::SoftShadow);
	saveoption(fileout, "VolumetricClouds", Renderer::VolumetricClouds);
	saveoption(fileout, "AmbientOcclusion", Renderer::AmbientOcclusion);
	saveoption(fileout, "VerticalSync", VerticalSync);
	saveoption(fileout, "UIFontSize", TextRenderer::FontSize);
	saveoption(fileout, "UIStretch", UIStretch);
	saveoption(fileout, "UIBackgroundBlur", UIBackgroundBlur);
	fileout.close();
}
