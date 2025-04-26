//==============================   Initialize   ================================//
//==============================初始化(包括闪屏)================================//

#include "Definitions.h"
#include "Blocks.h"
#include "Textures.h"
#include "GLProc.h"
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
void updategame();

void render();
void present();
void readback();
void drawBorder(int x, int y, int z);
void drawBreaking(float level, int x, int y, int z);
void drawGUI();
void drawBagRow(int row, int itemid, int xbase, int ybase, int spac, float alpha);
void drawBag();
void drawShadowMap();

void saveScreenshot(int x, int y, int w, int h, string filename);
void createThumbnail();
void loadOptions();
void saveOptions();

int getMouseScroll() { return mw; }
int getMouseButton() { return mb; }

Mutex_t Mutex;
Thread_t updateThread;
double lastupdate, updateTimer;
double lastframe;
bool updateThreadRun, updateThreadPaused;

double SpeedupAnimTimer;
double TouchdownAnimTimer;
double screenshotAnimTimer;
double bagAnimTimer;
double bagAnimDuration = 0.5;
bool shouldShowCursor = true;
bool shouldGetScreenshot;
bool shouldGetThumbnail;
bool shouldToggleFullscreen;

int fps, fpsc, ups, upsc;
double fctime, uctime;

bool GUIrenderswitch = true;
bool DebugMode = false;
bool DebugHitbox = false;
bool DebugShadow = false;
bool DebugMergeFace = false;

int selx, sely, selz, oldselx, oldsely, oldselz, selface;
bool sel;
float selt, seldes;
BlockID selb;
Brightness selbr;
bool selce;
int selbx, selby, selbz, selcx, selcy, selcz;

string chatword;
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

	windowwidth = defaultwindowwidth;
	windowheight = defaultwindowheight;
	cout << "[Console][Event]Initialize GLFW" << (glfwInit() == 1 ? "" : " - Failed!") << endl;
	createWindow();
	setupScreen();
	splashScreen();
	loadTextures();
	Mod::ModLoader::loadMods();
main_menu:
	gamebegin = gameexit = false;
	GUI::clearTransition();
	Menus::mainmenu();
	glEnable(GL_TEXTURE_2D);

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClearDepth(1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glfwSwapBuffers(MainWindow);
	glfwPollEvents();

	Mutex = MutexCreate();
	MutexLock(Mutex);
	updateThreadRun = true;
	updateThreadPaused = true;
	updateThread = ThreadCreate(&updateThreadFunc, NULL);
	if (multiplayer) {
		fastSrand((unsigned int)time(NULL));
		Player::name = "";
		Player::onlineID = rand();
		Network::init(serverip, port);
	}
	//初始化游戏状态
	printf("[Console][Game]");
	printf("Init player...\n");
	if (loadGame()) Player::init(Player::xpos, Player::ypos, Player::zpos);
	else Player::spawn();
	printf("[Console][Game]");
	printf("Init world...\n");
	World::Init();
	registerCommands();
	printf("[Console][Game]");
	printf("Loading Mods...\n");

	//这才是游戏开始!
	mxl = mx;
	myl = my;
	printf("[Console][Game]");
	printf("Main loop started\n");
	shouldShowCursor = false;
	updateThreadPaused = false;
	fctime = uctime = timer();

	// 主循环，被简化成这样，惨不忍睹啊！
	do {
		glfwSetInputMode(MainWindow, GLFW_CURSOR, shouldShowCursor ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);

		// 等待上一帧完成后渲染下一帧
		MutexUnlock(Mutex);
		present();
		MutexLock(Mutex);
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

		if (shouldToggleFullscreen) {
			shouldToggleFullscreen = false;
			toggleFullScreen();
		}

		if (isKeyDown(GLFW_KEY_ESCAPE)) {
			updateThreadPaused = true;
			createThumbnail();
			GUI::clearTransition();
			Menus::gamemenu();
			if (!gameexit) {
				mxl = mx;
				myl = my;
			}
			updateThreadPaused = false;
		}

		if (gameexit) {
			glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
			glClearDepth(1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			TextRenderer::setFontColor(1.0, 1.0, 1.0, 1.0);
			TextRenderer::renderString(0, 0, "Saving world...");
			glfwSwapBuffers(MainWindow);
			glfwPollEvents();

			printf("[Console][Game]");
			printf("Terminate threads\n");
			updateThreadRun = false;
			MutexUnlock(Mutex);
			ThreadWait(updateThread);
			ThreadDestroy(updateThread);
			MutexDestroy(Mutex);
			saveGame();
			World::destroyAllChunks();
			if (multiplayer) Network::cleanUp();
			goto main_menu;
		}

	} while (!glfwWindowShouldClose(MainWindow));

	saveGame();
	Mod::ModLoader::unloadMods();

	updateThreadRun = false;
	MutexUnlock(Mutex);
	ThreadWait(updateThread);
	ThreadDestroy(updateThread);
	MutexDestroy(Mutex);

	// 结束程序，删了也没关系 ←_←（吐槽FB和glfw中）
	// 不对啊这不是FB！！！这是正宗的C++！！！！！！
	// 楼上的楼上在瞎说！！！别信他的！！！
	glfwTerminate();
	return 0;
	// This is the END of the program!
}

ThreadFunc updateThreadFunc(void*) {
	MutexLock(Mutex);
	lastupdate = timer();

	while (true) {
wait:
		MutexUnlock(Mutex);
		Sleep(1);
		MutexLock(Mutex);
		if (!updateThreadRun) break;
		if (updateThreadPaused) {
			lastupdate = timer();
			goto wait;
		}

		updateTimer = timer();
		if (updateTimer - lastupdate >= 5.0) lastupdate = updateTimer;

		while ((updateTimer - lastupdate) >= 1.0 / 30.0 && upsc < 60) {
			lastupdate += 1.0 / 30.0;
			upsc++;
			updategame();
		}
	}

	MutexUnlock(Mutex);
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
		gametime = time;
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

void updategame() {
	static double wPressTimer;
	static bool wPressedOnce;

	Player::BlockInHand = Player::inventory[3][Player::indexInHand];
	//生命值相关
	if (Player::health > 0 || Player::gamemode == Player::Creative) {
		if (Player::ypos < -100) Player::health -= ((-100) - Player::ypos) / 100;
		if (Player::health < Player::healthMax) Player::health += Player::healSpeed;
		if (Player::health > Player::healthMax) Player::health = Player::healthMax;
	} else Player::spawn();

	//时间
	gametime++;

	World::meshedChunks = 0;
	World::updatedChunks = 0;

	// Move chunk pointer array
	if (World::cpArray.originX != Player::cxt - viewdistance - 2 || World::cpArray.originY != Player::cyt - viewdistance - 2 || World::cpArray.originZ != Player::czt - viewdistance - 2)
		World::cpArray.moveTo(Player::cxt - viewdistance - 2, Player::cyt - viewdistance - 2, Player::czt - viewdistance - 2);

	// Move height map
	if (World::HMap.originX != (Player::cxt - viewdistance - 2) * 16 || World::HMap.originZ != (Player::czt - viewdistance - 2) * 16)
		World::HMap.moveTo((Player::cxt - viewdistance - 2) * 16, (Player::czt - viewdistance - 2) * 16);

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
			if (chatword != "") { // 指令的执行，或发出聊天文本
				if (chatword.substr(0, 1) == "/") { // 指令
					vector<string> command = split(chatword, " ");
					if (!doCommand(command)) { // 执行失败
						DebugWarning("Fail to execute the command: " + chatword);
						chatMessages.push_back("Fail to execute the command: " + chatword);
					}
				}
				else
					chatMessages.push_back(chatword);
			}
			chatword = "";
		}

		if (isKeyPressed(GLFW_KEY_BACKSPACE) && chatword.length() > 0) {
			int n = chatword[chatword.length() - 1];
			if (n > 0 && n <= 127)
				chatword = chatword.substr(0, chatword.length() - 1);
			else
				chatword = chatword.substr(0, chatword.length() - 2);
		}
		else {
			chatword += inputstr;
			inputstr = "";
		}

		// 自动补全
		if (isKeyPressed(GLFW_KEY_TAB) && chatmode && chatword.size() > 0 && chatword.substr(0, 1) == "/") {
			for (unsigned int i = 0; i != commands.size(); i++) {
				if (beginWith(commands[i].identifier, chatword))
					chatword = commands[i].identifier;
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
		inputstr = "";

		// 从玩家位置发射一条线段
		for (int i = 0; i < selectPrecision * selectDistance; i++) {
			lxl = lx;
			lyl = ly;
			lzl = lz;

			// 线段延伸
			lx += sin(M_PI / 180 * (Player::heading - 180)) * sin(M_PI / 180 * (Player::lookupdown + 90)) / (double)selectPrecision;
			ly += cos(M_PI / 180 * (Player::lookupdown + 90)) / (double)selectPrecision;
			lz += cos(M_PI / 180 * (Player::heading - 180)) * sin(M_PI / 180 * (Player::lookupdown + 90)) / (double)selectPrecision;

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
				Player::xa += -sin(Player::heading * M_PI / 180.0) * Player::speed;
				Player::za += -cos(Player::heading * M_PI / 180.0) * Player::speed;
			} else {
				Player::xa = sin(M_PI / 180 * (Player::heading - 180)) * sin(M_PI / 180 * (Player::lookupdown + 90)) * Player::glidingSpeed * speedCast;
				Player::ya = cos(M_PI / 180 * (Player::lookupdown + 90)) * Player::glidingSpeed * speedCast;
				Player::za = cos(M_PI / 180 * (Player::heading - 180)) * sin(M_PI / 180 * (Player::lookupdown + 90)) * Player::glidingSpeed * speedCast;
				if (Player::ya < 0) Player::ya *= 2;
			}
		} else {
			Player::Running = false;
			wPressedOnce = false;
		}
		if (Player::Running)Player::speed = runspeed;
		else Player::speed = walkspeed;

		if (isKeyDown(GLFW_KEY_S) && !Player::glidingNow) {
			Player::xa += sin(Player::heading * M_PI / 180.0) * Player::speed;
			Player::za += cos(Player::heading * M_PI / 180.0) * Player::speed;
			wPressTimer = 0.0;
		}

		if (isKeyDown(GLFW_KEY_A) && !Player::glidingNow) {
			Player::xa += sin((Player::heading - 90) * M_PI / 180.0) * Player::speed;
			Player::za += cos((Player::heading - 90) * M_PI / 180.0) * Player::speed;
			wPressTimer = 0.0;
		}

		if (isKeyDown(GLFW_KEY_D) && !Player::glidingNow) {
			Player::xa += -sin((Player::heading - 90) * M_PI / 180.0) * Player::speed;
			Player::za += -cos((Player::heading - 90) * M_PI / 180.0) * Player::speed;
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
					Player::xa = -sin(Player::heading * M_PI / 180.0) * runspeed * 10;
					Player::za = -cos(Player::heading * M_PI / 180.0) * runspeed * 10;
				} else {
					Player::xa = sin(M_PI / 180 * (Player::heading - 180)) * sin(M_PI / 180 * (Player::lookupdown + 90)) * runspeed * 20;
					Player::ya = cos(M_PI / 180 * (Player::lookupdown + 90)) * runspeed * 20;
					Player::za = cos(M_PI / 180 * (Player::heading - 180)) * sin(M_PI / 180 * (Player::lookupdown + 90)) * runspeed * 20;
				}
			}

			if (isKeyDown(GLFW_KEY_F) && !Player::glidingNow) {
				if (isKeyDown(GLFW_KEY_LEFT_CONTROL)) {
					Player::xa = sin(Player::heading * M_PI / 180.0) * runspeed * 10;
					Player::za = cos(Player::heading * M_PI / 180.0) * runspeed * 10;
				} else {
					Player::xa = -sin(M_PI / 180 * (Player::heading - 180)) * sin(M_PI / 180 * (Player::lookupdown + 90)) * runspeed * 20;
					Player::ya = -cos(M_PI / 180 * (Player::lookupdown + 90)) * runspeed * 20;
					Player::za = -cos(M_PI / 180 * (Player::heading - 180)) * sin(M_PI / 180 * (Player::lookupdown + 90)) * runspeed * 20;
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
				Player::ya += walkspeed / 2;
			}
		}

		if ((isKeyDown(GLFW_KEY_LEFT_SHIFT) || isKeyDown(GLFW_KEY_RIGHT_SHIFT)) && !Player::glidingNow) {
			if (Player::CrossWall || Player::Flying) Player::ya -= walkspeed / 2;
			wPressTimer = 0.0;
		}

		// Open inventory
		if (isKeyPressed(GLFW_KEY_E) && GUIrenderswitch) {
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
		if (isKeyPressed(GLFW_KEY_F3)) DebugMode = !DebugMode;
		if (isKeyPressed(GLFW_KEY_H) && isKeyDown(GLFW_KEY_F3)) {
			DebugHitbox = !DebugHitbox;
			DebugMode = true;
		}
		if (Renderer::AdvancedRender) {
			if (isKeyPressed(GLFW_KEY_M) && isKeyDown(GLFW_KEY_F3)) {
				DebugShadow = !DebugShadow;
				DebugMode = true;
			}
		} else DebugShadow = false;
		if (isKeyPressed(GLFW_KEY_G) && isKeyDown(GLFW_KEY_F3)) {
			DebugMergeFace = !DebugMergeFace;
			DebugMode = true;
		}
		if (isKeyPressed(GLFW_KEY_F4) && Player::gamemode == Player::GameMode::Creative) Player::CrossWall = !Player::CrossWall;
		if (isKeyPressed(GLFW_KEY_F5)) GUIrenderswitch = !GUIrenderswitch;
		if (isKeyPressed(GLFW_KEY_F6)) Player::Glide = !Player::Glide;
		if (isKeyPressed(GLFW_KEY_F7)) shouldToggleFullscreen = true;
		if (isKeyDown(GLFW_KEY_F8)) gametime += 30;
		if (isKeyPressed(GLFW_KEY_ENTER)) chatmode = true;
		if (isKeyPressed(GLFW_KEY_SLASH)) {
			chatmode = true;
			chatword = "/";
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

// Render the whole scene and HUD
void render() {
	double curtime = timer();
	double TimeDelta;
	double xpos, ypos, zpos;
	double mxd, myd;
	glfwGetCursorPos(MainWindow, &mxd, &myd);
	mxl = mx;
	myl = my;
	mx = static_cast<int>(mxd);
	my = static_cast<int>(myd);

	if (Player::Running) {
		if (FOVyExt < 9.8) {
			TimeDelta = curtime - SpeedupAnimTimer;
			FOVyExt = 10.0f - (10.0f - FOVyExt) * (float)pow(0.8, TimeDelta * 30);
			SpeedupAnimTimer = curtime;
		} else FOVyExt = 10.0;
	} else {
		if (FOVyExt > 0.2) {
			TimeDelta = curtime - SpeedupAnimTimer;
			FOVyExt *= (float)pow(0.8, TimeDelta * 30);
			SpeedupAnimTimer = curtime;
		} else FOVyExt = 0.0;
	}
	SpeedupAnimTimer = curtime;

	if (Player::OnGround) {
		// 半蹲特效
		if (Player::jump < -0.005) {
			if (Player::jump <= -(Player::height - 0.5f))
				Player::heightExt = -(Player::height - 0.5f);
			else
				Player::heightExt = (float)Player::jump;
			TouchdownAnimTimer = curtime;
		} else {
			if (Player::heightExt <= -0.005) {
				Player::heightExt *= (float)pow(0.8, (curtime - TouchdownAnimTimer) * 30);
				TouchdownAnimTimer = curtime;
			}
		}
	}

	double interp = (curtime - lastupdate) * 30.0;
	xpos = Player::xpos - Player::xd + interp * Player::xd;
	ypos = Player::ypos + Player::height + Player::heightExt - Player::yd + interp * Player::yd;
	zpos = Player::zpos - Player::zd + interp * Player::zd;

	if (!shouldShowCursor) {
		// 转头！你治好了我多年的颈椎病！
		if (mx != mxl) Player::xlookspeed -= (mx - mxl) * mousemove;
		if (my != myl) Player::ylookspeed += (my - myl) * mousemove;
		/*
		if (isKeyDown(GLFW_KEY_RIGHT) == 1) Player::xlookspeed -= mousemove * 16 * (curtime - lastframe) * 30.0;
		if (isKeyDown(GLFW_KEY_LEFT) == 1) Player::xlookspeed += mousemove * 16 * (curtime - lastframe) * 30.0;
		if (isKeyDown(GLFW_KEY_UP) == 1) Player::ylookspeed -= mousemove * 16 * (curtime - lastframe) * 30.0;
		if (isKeyDown(GLFW_KEY_DOWN) == 1) Player::ylookspeed += mousemove * 16 * (curtime - lastframe) * 30.0;
		*/
		// 限制角度，别把头转掉下来了 ←_←
		if (Player::lookupdown + Player::ylookspeed < -90.0) Player::ylookspeed = -90.0 - Player::lookupdown;
		if (Player::lookupdown + Player::ylookspeed > 90.0) Player::ylookspeed = 90.0 - Player::lookupdown;
	}

	// Calculate sun position
	float interpolatedTime = gametime - 1.0f + static_cast<float>(interp);
	// daylight = clamp((1.0 - cos((double)gametime / 43200.0f * 2.0 * M_PI) * 2.0) / 2.0, 0.05, 1.0);
	// Renderer::sunlightXrot = 90 * daylight;
	Renderer::sunlightYrot = interpolatedTime / 43200.0f * 360.0f;

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

	double plookupdown = Player::lookupdown + Player::ylookspeed;
	double pheading = Player::heading + Player::xlookspeed;

	Player::ViewFrustum.LoadIdentity();
	Player::ViewFrustum.SetPerspective(FOVyNormal + FOVyExt, (float)windowwidth / windowheight, 0.05f, viewdistance * 16.0f);
	Player::ViewFrustum.MultRotate((float)plookupdown, 1, 0, 0);
	Player::ViewFrustum.MultRotate(360.0f - (float)pheading, 0, 1, 0);
	Player::ViewFrustum.update();

	glClearColor(skycolorR, skycolorG, skycolorB, 1.0f);
	glClearDepth(1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glBindTexture(GL_TEXTURE_2D_ARRAY, BlockTextureArray);

	if (DebugMergeFace) glPolygonMode(GL_FRONT, GL_LINE);

	if (Renderer::AdvancedRender) {
		int shadowdist = min(Renderer::MaxShadowDist, viewdistance);
		FrustumTest lightFrustum = Renderer::getShadowMapFrustum(pheading, plookupdown, shadowdist, Player::ViewFrustum);

		// Clear shadow buffer, G-buffers and D-buffer
		Renderer::ClearSGDBuffers();

		// Build shadow map
		WorldRenderer::ListRenderChunks(xpos, ypos, zpos, shadowdist + 2, interp, {});
		Renderer::StartShadowPass(lightFrustum, interpolatedTime);
		WorldRenderer::RenderChunks(xpos, ypos, zpos, 0);
		Renderer::shaders[Renderer::ActiveShader].setUniform("u_translation", Mat4f(1.0f).data);
		Particles::renderall(xpos, ypos, zpos, interp);
		Renderer::EndShadowPass();
	}

	WorldRenderer::ListRenderChunks(xpos, ypos, zpos, viewdistance, interp, Player::ViewFrustum);
	Renderer::StartOpaquePass(Player::ViewFrustum, interpolatedTime);
	WorldRenderer::RenderChunks(xpos, ypos, zpos, 0);
	Renderer::shaders[Renderer::ActiveShader].setUniform("u_translation", Mat4f(1.0f).data);
	Particles::renderall(xpos, ypos, zpos, interp);
	Renderer::EndOpaquePass();

	Renderer::StartTranslucentPass(Player::ViewFrustum, interpolatedTime);
	WorldRenderer::RenderChunks(xpos, ypos, zpos, 1);
	Renderer::EndTranslucentPass();

	if (DebugMergeFace) glPolygonMode(GL_FRONT, GL_FILL);

	if (Renderer::AdvancedRender) {
		Renderer::StartFinalPass(xpos, ypos, zpos, pheading, plookupdown, Player::ViewFrustum, interpolatedTime);
		Renderer::Begin(GL_QUADS, 2, 2, 0);
		Renderer::TexCoord2f(0.0f, 1.0f); Renderer::Vertex2i(0, 0);
		Renderer::TexCoord2f(0.0f, 0.0f); Renderer::Vertex2i(0, windowheight);
		Renderer::TexCoord2f(1.0f, 0.0f); Renderer::Vertex2i(windowwidth, windowheight);
		Renderer::TexCoord2f(1.0f, 1.0f); Renderer::Vertex2i(windowwidth, 0);
		Renderer::End().render();
		Renderer::EndFinalPass();
	}

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glMultMatrixf(Player::ViewFrustum.getProjMatrix());
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glMultMatrixf(Player::ViewFrustum.getModlMatrix());

	if (seldes > 0.0f) {
		glTranslated(selx - xpos, sely - ypos, selz - zpos);
		drawBreaking(seldes, 0, 0, 0);
		glTranslated(-selx + xpos, -sely + ypos, -selz + zpos);
	}

	if (GUIrenderswitch && sel) {
		glTranslated(selx - xpos, sely - ypos, selz - zpos);
		drawBorder(0, 0, 0);
		glTranslated(-selx + xpos, -sely + ypos, -selz + zpos);
	}

	glLoadIdentity();
	glRotated(plookupdown, 1, 0, 0);
	glRotated(360.0 - pheading, 0, 1, 0);
	glTranslated(-xpos, -ypos, -zpos);

	if (DebugHitbox) {
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

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, windowwidth, windowheight, 0, -1.0, 1.0);
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
		Renderer::TexCoord2f(0.0f, 0.0f); Renderer::Vertex2i(0, windowheight);
		Renderer::TexCoord2f(1.0f, 0.0f); Renderer::Vertex2i(windowwidth, windowheight);
		Renderer::TexCoord2f(1.0f, 1.0f); Renderer::Vertex2i(windowwidth, 0);
		Renderer::End().render();
		shader.unbind();
	}

	if (GUIrenderswitch) {
		drawGUI();
		if (Renderer::AdvancedRender && DebugShadow) drawShadowMap();
		drawBag();
	}

	if (curtime - screenshotAnimTimer <= 1.0 && !shouldGetScreenshot) {
		float col = 1.0f - (float)(curtime - screenshotAnimTimer);
		glDisable(GL_TEXTURE_2D);
		glBegin(GL_QUADS);
		glColor4f(1.0f, 1.0f, 1.0f, col);
		glVertex2i(0, 0);
		glVertex2i(0, windowheight);
		glVertex2i(windowwidth, windowheight);
		glVertex2i(windowwidth, 0);
		glEnd();
		glEnable(GL_TEXTURE_2D);
	}

	lastframe = curtime;
}

void present() {
	// 屏幕刷新，千万别删，后果自负！！！
	glfwSwapBuffers(MainWindow);
	glfwPollEvents();
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
		saveScreenshot(0, 0, windowwidth, windowheight, ss.str());
	}

	if (shouldGetThumbnail) {
		shouldGetThumbnail = false;
		createThumbnail();
	}
}

// Draw the block selection border
void drawBorder(int x, int y, int z) {
	const float eps = 0.005f;
	glDisable(GL_TEXTURE_2D);

	glBegin(GL_LINES);
	glColor4f(0.2f, 0.2f, 0.2f, 1.0f);
	glVertex3f(-(0.5f + eps) + x, (0.5f + eps) + y, -(0.5f + eps) + z);
	glVertex3f(-(0.5f + eps) + x, (0.5f + eps) + y, (0.5f + eps) + z);
	glVertex3f(-(0.5f + eps) + x, (0.5f + eps) + y, (0.5f + eps) + z);
	glVertex3f((0.5f + eps) + x, (0.5f + eps) + y, (0.5f + eps) + z);
	glVertex3f((0.5f + eps) + x, (0.5f + eps) + y, (0.5f + eps) + z);
	glVertex3f((0.5f + eps) + x, (0.5f + eps) + y, -(0.5f + eps) + z);
	glVertex3f(-(0.5f + eps) + x, (0.5f + eps) + y, -(0.5f + eps) + z);
	glVertex3f((0.5f + eps) + x, (0.5f + eps) + y, -(0.5f + eps) + z);
	glEnd();

	glBegin(GL_LINES);
	glColor4f(0.2f, 0.2f, 0.2f, 1.0f);
	glVertex3f(-(0.5f + eps) + x, -(0.5f + eps) + y, -(0.5f + eps) + z);
	glVertex3f((0.5f + eps) + x, -(0.5f + eps) + y, -(0.5f + eps) + z);
	glVertex3f((0.5f + eps) + x, -(0.5f + eps) + y, (0.5f + eps) + z);
	glVertex3f((0.5f + eps) + x, -(0.5f + eps) + y, -(0.5f + eps) + z);
	glVertex3f((0.5f + eps) + x, -(0.5f + eps) + y, (0.5f + eps) + z);
	glVertex3f(-(0.5f + eps) + x, -(0.5f + eps) + y, (0.5f + eps) + z);
	glVertex3f(-(0.5f + eps) + x, -(0.5f + eps) + y, -(0.5f + eps) + z);
	glVertex3f(-(0.5f + eps) + x, -(0.5f + eps) + y, (0.5f + eps) + z);
	glEnd();

	glBegin(GL_LINES);
	glColor4f(0.2f, 0.2f, 0.2f, 1.0f);
	glVertex3f((0.5f + eps) + x, -(0.5f + eps) + y, -(0.5f + eps) + z);
	glVertex3f((0.5f + eps) + x, (0.5f + eps) + y, -(0.5f + eps) + z);
	glVertex3f((0.5f + eps) + x, (0.5f + eps) + y, -(0.5f + eps) + z);
	glVertex3f((0.5f + eps) + x, (0.5f + eps) + y, (0.5f + eps) + z);
	glVertex3f((0.5f + eps) + x, (0.5f + eps) + y, (0.5f + eps) + z);
	glVertex3f((0.5f + eps) + x, -(0.5f + eps) + y, (0.5f + eps) + z);
	glVertex3f((0.5f + eps) + x, -(0.5f + eps) + y, -(0.5f + eps) + z);
	glVertex3f((0.5f + eps) + x, -(0.5f + eps) + y, (0.5f + eps) + z);
	glEnd();

	glBegin(GL_LINES);
	glColor4f(0.2f, 0.2f, 0.2f, 1.0f);
	glVertex3f(-(0.5f + eps) + x, -(0.5f + eps) + y, -(0.5f + eps) + z);
	glVertex3f(-(0.5f + eps) + x, -(0.5f + eps) + y, (0.5f + eps) + z);
	glVertex3f(-(0.5f + eps) + x, -(0.5f + eps) + y, (0.5f + eps) + z);
	glVertex3f(-(0.5f + eps) + x, (0.5f + eps) + y, (0.5f + eps) + z);
	glVertex3f(-(0.5f + eps) + x, (0.5f + eps) + y, (0.5f + eps) + z);
	glVertex3f(-(0.5f + eps) + x, (0.5f + eps) + y, -(0.5f + eps) + z);
	glVertex3f(-(0.5f + eps) + x, -(0.5f + eps) + y, -(0.5f + eps) + z);
	glVertex3f(-(0.5f + eps) + x, (0.5f + eps) + y, -(0.5f + eps) + z);
	glEnd();

	glBegin(GL_LINES);
	glColor4f(0.2f, 0.2f, 0.2f, 1.0f);
	glVertex3f(-(0.5f + eps) + x, -(0.5f + eps) + y, (0.5f + eps) + z);
	glVertex3f((0.5f + eps) + x, -(0.5f + eps) + y, (0.5f + eps) + z);
	glVertex3f((0.5f + eps) + x, -(0.5f + eps) + y, (0.5f + eps) + z);
	glVertex3f((0.5f + eps) + x, (0.5f + eps) + y, (0.5f + eps) + z);
	glVertex3f((0.5f + eps) + x, (0.5f + eps) + y, (0.5f + eps) + z);
	glVertex3f(-(0.5f + eps) + x, (0.5f + eps) + y, (0.5f + eps) + z);
	glVertex3f(-(0.5f + eps) + x, -(0.5f + eps) + y, (0.5f + eps) + z);
	glVertex3f(-(0.5f + eps) + x, (0.5f + eps) + y, (0.5f + eps) + z);
	glEnd();

	glBegin(GL_LINES);
	glColor4f(0.2f, 0.2f, 0.2f, 1.0f);
	glVertex3f(-(0.5f + eps) + x, -(0.5f + eps) + y, -(0.5f + eps) + z);
	glVertex3f(-(0.5f + eps) + x, (0.5f + eps) + y, -(0.5f + eps) + z);
	glVertex3f(-(0.5f + eps) + x, (0.5f + eps) + y, -(0.5f + eps) + z);
	glVertex3f((0.5f + eps) + x, (0.5f + eps) + y, -(0.5f + eps) + z);
	glVertex3f((0.5f + eps) + x, (0.5f + eps) + y, -(0.5f + eps) + z);
	glVertex3f((0.5f + eps) + x, -(0.5f + eps) + y, -(0.5f + eps) + z);
	glVertex3f(-(0.5f + eps) + x, -(0.5f + eps) + y, -(0.5f + eps) + z);
	glVertex3f((0.5f + eps) + x, -(0.5f + eps) + y, -(0.5f + eps) + z);
	glEnd();

	glEnable(GL_TEXTURE_2D);
}

void drawBreaking(float level, int x, int y, int z) {
	const float eps = 0.005f;

	int index = int(level * 8);
	if (index < 0) index = 0;
	if (index > 7) index = 7;

	glBindTexture(GL_TEXTURE_2D, DestroyImage[index]);

	glBegin(GL_QUADS);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glTexCoord2f(0.0f, 0.0f); glVertex3f(-(0.5f + eps) + x, -(0.5f + eps) + y, (0.5f + eps) + z);
	glTexCoord2f(1.0f, 0.0f); glVertex3f((0.5f + eps) + x, -(0.5f + eps) + y, (0.5f + eps) + z);
	glTexCoord2f(1.0f, 1.0f); glVertex3f((0.5f + eps) + x, (0.5f + eps) + y, (0.5f + eps) + z);
	glTexCoord2f(0.0f, 1.0f); glVertex3f(-(0.5f + eps) + x, (0.5f + eps) + y, (0.5f + eps) + z);
	glEnd();

	glBegin(GL_QUADS);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glTexCoord2f(1.0f, 0.0f); glVertex3f(-(0.5f + eps) + x, -(0.5f + eps) + y, -(0.5f + eps) + z);
	glTexCoord2f(1.0f, 1.0f); glVertex3f(-(0.5f + eps) + x, (0.5f + eps) + y, -(0.5f + eps) + z);
	glTexCoord2f(0.0f, 1.0f); glVertex3f((0.5f + eps) + x, (0.5f + eps) + y, -(0.5f + eps) + z);
	glTexCoord2f(0.0f, 0.0f); glVertex3f((0.5f + eps) + x, -(0.5f + eps) + y, -(0.5f + eps) + z);
	glEnd();

	glBegin(GL_QUADS);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glTexCoord2f(1.0f, 0.0f); glVertex3f((0.5f + eps) + x, -(0.5f + eps) + y, -(0.5f + eps) + z);
	glTexCoord2f(1.0f, 1.0f); glVertex3f((0.5f + eps) + x, (0.5f + eps) + y, -(0.5f + eps) + z);
	glTexCoord2f(0.0f, 1.0f); glVertex3f((0.5f + eps) + x, (0.5f + eps) + y, (0.5f + eps) + z);
	glTexCoord2f(0.0f, 0.0f); glVertex3f((0.5f + eps) + x, -(0.5f + eps) + y, (0.5f + eps) + z);
	glEnd();

	glBegin(GL_QUADS);
	glTexCoord2f(0.0f, 0.0f); glVertex3f(-(0.5f + eps) + x, -(0.5f + eps) + y, -(0.5f + eps) + z);
	glTexCoord2f(1.0f, 0.0f); glVertex3f(-(0.5f + eps) + x, -(0.5f + eps) + y, (0.5f + eps) + z);
	glTexCoord2f(1.0f, 1.0f); glVertex3f(-(0.5f + eps) + x, (0.5f + eps) + y, (0.5f + eps) + z);
	glTexCoord2f(0.0f, 1.0f); glVertex3f(-(0.5f + eps) + x, (0.5f + eps) + y, -(0.5f + eps) + z);
	glEnd();

	glBegin(GL_QUADS);
	glTexCoord2f(0.0f, 1.0f); glVertex3f(-(0.5f + eps) + x, (0.5f + eps) + y, -(0.5f + eps) + z);
	glTexCoord2f(0.0f, 0.0f); glVertex3f(-(0.5f + eps) + x, (0.5f + eps) + y, (0.5f + eps) + z);
	glTexCoord2f(1.0f, 0.0f); glVertex3f((0.5f + eps) + x, (0.5f + eps) + y, (0.5f + eps) + z);
	glTexCoord2f(1.0f, 1.0f); glVertex3f((0.5f + eps) + x, (0.5f + eps) + y, -(0.5f + eps) + z);
	glEnd();

	glBegin(GL_QUADS);
	glTexCoord2f(1.0f, 1.0f); glVertex3f(-(0.5f + eps) + x, -(0.5f + eps) + y, -(0.5f + eps) + z);
	glTexCoord2f(0.0f, 1.0f); glVertex3f((0.5f + eps) + x, -(0.5f + eps) + y, -(0.5f + eps) + z);
	glTexCoord2f(0.0f, 0.0f); glVertex3f((0.5f + eps) + x, -(0.5f + eps) + y, (0.5f + eps) + z);
	glTexCoord2f(1.0f, 0.0f); glVertex3f(-(0.5f + eps) + x, -(0.5f + eps) + y, (0.5f + eps) + z);
	glEnd();
}

void drawGUI() {
	int disti = (int)(seldes * linedist);

	if (DebugMode) {
		if (selb != Blocks::AIR) {
			glBegin(GL_LINES);
			glColor4f(GUI::FgR, GUI::FgG, GUI::FgB, 0.8f);
			glVertex2i(windowwidth / 2, windowheight / 2);
			glVertex2i(windowwidth / 2 + 50, windowheight / 2 + 50);
			glVertex2i(windowwidth / 2 + 50, windowheight / 2 + 50);
			glVertex2i(windowwidth / 2 + 250, windowheight / 2 + 50);
			glEnd();
			TextRenderer::setFontColor(1.0f, 1.0f, 1.0f, 0.8f);
			std::stringstream ss;
			ss << BlockInfo(selb).getBlockName() << " (id: " << (int)selb << ")";
			TextRenderer::renderASCIIString(windowwidth / 2 + 50, windowheight / 2 + 50 - 16, ss.str());
		}

		glDisable(GL_TEXTURE_2D);

		glBegin(GL_LINES);
		glColor4f(0.0f, 0.0f, 0.0f, 0.9f);

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

		glEnable(GL_TEXTURE_2D);
	}

	glDisable(GL_TEXTURE_2D);

	glBegin(GL_QUADS);
	glColor4f(0.0f, 0.0f, 0.0f, 0.9f);
	glVertex2i(windowwidth / 2 - 16, windowheight / 2 - 2);
	glVertex2i(windowwidth / 2 - 16, windowheight / 2 + 2);
	glVertex2i(windowwidth / 2 + 16, windowheight / 2 + 2);
	glVertex2i(windowwidth / 2 + 16, windowheight / 2 - 2);
	glVertex2i(windowwidth / 2 - 2, windowheight / 2 - 16);
	glVertex2i(windowwidth / 2 - 2, windowheight / 2 + 16);
	glVertex2i(windowwidth / 2 + 2, windowheight / 2 + 16);
	glVertex2i(windowwidth / 2 + 2, windowheight / 2 - 16);
	glEnd();

	glBegin(GL_QUADS);
	glColor4f(1.0f, 1.0f, 1.0f, 0.9f);
	glVertex2i(windowwidth / 2 - 15, windowheight / 2 - 1);
	glVertex2i(windowwidth / 2 - 15, windowheight / 2 + 1);
	glVertex2i(windowwidth / 2 + 15, windowheight / 2 + 1);
	glVertex2i(windowwidth / 2 + 15, windowheight / 2 - 1);
	glVertex2i(windowwidth / 2 - 1, windowheight / 2 - 15);
	glVertex2i(windowwidth / 2 - 1, windowheight / 2 + 15);
	glVertex2i(windowwidth / 2 + 1, windowheight / 2 + 15);
	glVertex2i(windowwidth / 2 + 1, windowheight / 2 - 15);
	glEnd();

	/*
	if (seldes > 0.0f) {
		glBegin(GL_QUADS);
		glColor4f(0.5, 0.5, 0.5, 1.0);
		glVertex2i(windowwidth / 2 - 15, windowheight / 2 - 1);
		glVertex2i(windowwidth / 2 - 15, windowheight / 2 + 1);
		glVertex2i(windowwidth / 2 - 15 + (int)(seldes * 15), windowheight / 2 + 1);
		glVertex2i(windowwidth / 2 - 15 + (int)(seldes * 15), windowheight / 2 - 1);
		glVertex2i(windowwidth / 2 + 15 - (int)(seldes * 15), windowheight / 2 - 1);
		glVertex2i(windowwidth / 2 + 15 - (int)(seldes * 15), windowheight / 2 + 1);
		glVertex2i(windowwidth / 2 + 15, windowheight / 2 + 1);
		glVertex2i(windowwidth / 2 + 15, windowheight / 2 - 1);
		glVertex2i(windowwidth / 2 - 1, windowheight / 2 - 15);
		glVertex2i(windowwidth / 2 - 1, windowheight / 2 - 15 + (int)(seldes * 15));
		glVertex2i(windowwidth / 2 + 1, windowheight / 2 - 15 + (int)(seldes * 15));
		glVertex2i(windowwidth / 2 + 1, windowheight / 2 - 15);
		glVertex2i(windowwidth / 2 - 1, windowheight / 2 + 15 - (int)(seldes * 15));
		glVertex2i(windowwidth / 2 - 1, windowheight / 2 + 15);
		glVertex2i(windowwidth / 2 + 1, windowheight / 2 + 15);
		glVertex2i(windowwidth / 2 + 1, windowheight / 2 + 15 - (int)(seldes * 15));
		glEnd();
	}
	*/

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

	TextRenderer::setFontColor(1.0f, 1.0f, 1.0f, 0.9f);
	if (chatmode) {
		glColor4f(GUI::FgR, GUI::FgG, GUI::FgB, GUI::FgA);
		glDisable(GL_TEXTURE_2D);
		glBegin(GL_QUADS);
		glVertex2i(1, windowheight - 51);
		glVertex2i(1, windowheight - 33);
		glVertex2i(windowwidth - 1, windowheight - 33);
		glVertex2i(windowwidth - 1, windowheight - 51);
		glEnd();
		glEnable(GL_TEXTURE_2D);
		TextRenderer::renderString(0, windowheight - 50, chatword);
	}
	int posy = 0;
	for (size_t i = chatMessages.size(); i-- > 0; ) {
		TextRenderer::renderString(0, windowheight - 80 - 18 * posy++, chatMessages[i]);
	}

	if (DebugMode) {
		int textPos = 0;
		auto debugText = [textPos](string s) mutable {
			TextRenderer::renderASCIIString(0, 16 * textPos, s);
			textPos++;
		};

		std::stringstream ss;
		ss << std::fixed << std::setprecision(4);

		ss << "v" << VERSION << " [GL " << GLVersionMajor << "." << GLVersionMinor << "." << GLVersionRev << "]";
		debugText(ss.str());
		ss.str("");
		ss << fps << " fps, " << ups << " ups";
		debugText(ss.str());
		ss.str("");

		ss << "game mode: " << (Player::gamemode == Player::GameMode::Creative ? "creative" : "survival") << " mode";
		debugText(ss.str());
		ss.str("");
		if (Renderer::AdvancedRender) {
			ss << "shadow view: " << boolstr(DebugShadow);
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
		int h = (gametime / 30 / 60) % 24;
		int m = (gametime / 30) % 60;
		int s = gametime % 30 * 2;
		ss << "time: "
			<< (h < 10 ? "0" : "") << h << ":"
			<< (m < 10 ? "0" : "") << m << ":"
			<< (s < 10 ? "0" : "") << s
			<< " (" << gametime << "/" << 30 * 60 * 24 << ")";
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

		if (multiplayer) {
			MutexLock(Network::mutex);
			ss << Network::getRequestCount() << "/" << networkRequestMax << " network requests";
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
		TextRenderer::setFontColor(1.0f, 1.0f, 1.0f, 0.9f);
		std::stringstream ss;
		ss << "v" << VERSION;
		TextRenderer::renderASCIIString(0, 0, ss.str());
		ss.str("");
		ss << fps << " fps";
		TextRenderer::renderASCIIString(0, 16, ss.str());
		ss.str("");
		ss << (Player::gamemode == Player::GameMode::Creative ? "creative" : "survival") << " mode";
		TextRenderer::renderASCIIString(0, 32, ss.str());
		ss.str("");
	}
}

void drawBagRow(int row, int itemid, int xbase, int ybase, int spac, float alpha) {
	// 画出背包的一行
	auto& shader = Renderer::shaders[Renderer::UIShader];
	for (int i = 0; i < 10; i++) {
		glBindTexture(GL_TEXTURE_2D, i == itemid ? tex_select : tex_unselect);
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
	int leftp = (windowwidth - 392) / 2;
	int upp = windowheight - 152 - 16;
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
		glVertex2i(0, windowheight);
		glVertex2i(windowwidth, windowheight);
		glVertex2i(windowwidth, 0);
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
				drawBagRow(i, (csi == i ? csj : -1), (windowwidth - 392) / 2, windowheight - 152 - 16 + i * 40, 8, 1.0f);
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
			xbase = (int)round(((windowwidth - 392) / 2) * bagAnim);
			ybase = (int)round((windowheight - 152 - 16 + 120 - (windowheight - 32)) * bagAnim + (windowheight - 32));
			spac = (int)round(8 * bagAnim);
			drawBagRow(3, -1, xbase, ybase, spac, alpha);
			xbase = (int)round(((windowwidth - 392) / 2 - windowwidth) * bagAnim + windowwidth);
			ybase = (int)round((windowheight - 152 - 16 - (windowheight - 32)) * bagAnim + (windowheight - 32));
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
			glVertex2i(0, windowheight);
			glVertex2i(windowwidth, windowheight);
			glVertex2i(windowwidth, 0);
			glEnd();
			glEnable(GL_TEXTURE_2D);

			int xbase = 0, ybase = 0, spac = 0;
			float alpha = 1.0f - 0.5f * bagAnim;
			xbase = (int)round(((windowwidth - 392) / 2) - ((windowwidth - 392) / 2) * bagAnim);
			ybase = (int)round((windowheight - 152 - 16 + 120 - (windowheight - 32)) - (windowheight - 152 - 16 + 120 - (windowheight - 32)) * bagAnim + (windowheight - 32));
			spac = (int)round(8 - 8 * bagAnim);
			drawBagRow(3, Player::indexInHand, xbase, ybase, spac, alpha);
			xbase = (int)round(((windowwidth - 392) / 2 - windowwidth) - ((windowwidth - 392) / 2 - windowwidth) * bagAnim + windowwidth);
			ybase = (int)round((windowheight - 152 - 16 - (windowheight - 32)) - (windowheight - 152 - 16 - (windowheight - 32)) * bagAnim + (windowheight - 32));
			for (int i = 0; i < 3; i++) {
				drawBagRow(i, -1, xbase, ybase + i * 40, spac, alpha);
			}
		} else drawBagRow(3, Player::indexInHand, 0, windowheight - 32, 0, 0.5f);
	}
}

void drawShadowMap() {
	float xi = 1.0f - static_cast<float>(windowheight) / windowwidth;
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
}

void saveScreenshot(int x, int y, int w, int h, string filename) {
	Textures::ImageRGB scrBuffer;
	int bufw = w, bufh = h;
	while (bufw % 4 != 0)  bufw += 1;
	while (bufh % 4 != 0)  bufh += 1;
	scrBuffer.sizeX = bufw;
	scrBuffer.sizeY = bufh;
	scrBuffer.buffer = unique_ptr<ubyte[]>(new ubyte[bufw * bufh * 3]);
	glReadPixels(x, y, bufw, bufh, GL_RGB, GL_UNSIGNED_BYTE, scrBuffer.buffer.get());
	Textures::SaveRGBImage(filename, scrBuffer);
}

void createThumbnail() {
	std::stringstream ss;
	ss << "Worlds/" << World::worldname << "/Thumbnail.bmp";
	saveScreenshot(0, 0, windowwidth, windowheight, ss.str());
}

template<typename T>
void loadoption(std::map<string, string> &m, const char* name, T &value) {
	if (m.find(name) == m.end()) return;
	std::stringstream ss;
	ss << m[name];
	ss >> value;
}

void loadOptions() {
	std::map<string, string> options;
	std::ifstream filein("configs/options.ini", std::ios::in);
	if (!filein.is_open()) return;
	string name, value;
	while (!filein.eof()) {
		filein >> name >> value;
		options[name] = value;
	}
	filein.close();
	loadoption(options, "Language", Globalization::Cur_Lang);
	loadoption(options, "FOV", FOVyNormal);
	loadoption(options, "RenderDistance", viewdistance);
	loadoption(options, "Sensitivity", mousemove);
	loadoption(options, "CloudWidth", cloudwidth);
	loadoption(options, "SmoothLighting", SmoothLighting);
	loadoption(options, "FancyGrass", NiceGrass);
	loadoption(options, "MergeFaceRendering", MergeFace);
	loadoption(options, "MultiSample", Multisample);
	loadoption(options, "AdvancedRender", Renderer::AdvancedRender);
	loadoption(options, "ShadowMapRes", Renderer::ShadowRes);
	loadoption(options, "ShadowDistance", Renderer::MaxShadowDist);
	loadoption(options, "SoftShadow", Renderer::SoftShadow);
	loadoption(options, "VolumetricClouds", Renderer::VolumetricClouds);
	loadoption(options, "AmbientOcclusion", Renderer::AmbientOcclusion);
	loadoption(options, "VerticalSync", vsync);
	loadoption(options, "GUIBackgroundBlur", GUIScreenBlur);
	loadoption(options, "ForceUnicodeFont", TextRenderer::useUnicodeASCIIFont);
}

template<typename T>
void saveoption(std::ofstream &out, const char* name, T &value) {
	out << string(name) << " " << value << endl;
}

void saveOptions() {
	std::map<string, string> options;
	std::ofstream fileout("configs/options.ini", std::ios::out);
	if (!fileout.is_open()) return;
	saveoption(fileout, "Language", Globalization::Cur_Lang);
	saveoption(fileout, "FOV", FOVyNormal);
	saveoption(fileout, "RenderDistance", viewdistance);
	saveoption(fileout, "Sensitivity", mousemove);
	saveoption(fileout, "CloudWidth", cloudwidth);
	saveoption(fileout, "SmoothLighting", SmoothLighting);
	saveoption(fileout, "FancyGrass", NiceGrass);
	saveoption(fileout, "MergeFaceRendering", MergeFace);
	saveoption(fileout, "MultiSample", Multisample);
	saveoption(fileout, "AdvancedRender", Renderer::AdvancedRender);
	saveoption(fileout, "ShadowMapRes", Renderer::ShadowRes);
	saveoption(fileout, "ShadowDistance", Renderer::MaxShadowDist);
	saveoption(fileout, "SoftShadow", Renderer::SoftShadow);
	saveoption(fileout, "VolumetricClouds", Renderer::VolumetricClouds);
	saveoption(fileout, "AmbientOcclusion", Renderer::AmbientOcclusion);
	saveoption(fileout, "VerticalSync", vsync);
	saveoption(fileout, "GUIBackgroundBlur", GUIScreenBlur);
	saveoption(fileout, "ForceUnicodeFont", TextRenderer::useUnicodeASCIIFont);
	fileout.close();
}