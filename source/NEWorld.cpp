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
#include "Particles.h"
#include "Hitbox.h"
#include "GUI.h"
#include "Menus.h"
#include "Frustum.h"
#include "Network.h"
#include "Effect.h"
#include "Items.h"
#include "International.h"
#include "Command.h"
void WindowSizeFunc(GLFWwindow* win, int width, int height);
void MouseButtonFunc(GLFWwindow*, int button, int action, int);
void CharInputFunc(GLFWwindow*, unsigned int c);
void MouseScrollFunc(GLFWwindow*, double, double yoffset);
void splashscreen();
void setupscreen();
void InitGL();
//void glPrintInfoLog(GLhandleARB obj);
void setupNormalFog();
void LoadTextures();
bool loadGame();
void saveGame();
ThreadFunc updateThreadFunc(void*);
void drawCloud(double px, double pz);
void updategame();
void debugText(string s, bool init = false);
void Render();
void drawBorder(int x,int y,int z);
void renderDestroy(float level,int x,int y,int z);
void drawGUI();
void drawBagRow(int row, int itemid, int xbase, int ybase, int spac, float alpha);
void drawBag();
void saveScreenshot(int x, int y, int w, int h, string filename);
void createThumbnail();
void loadoptions();
void saveoptions();
int getMouseScroll() { return mw; }
int getMouseButton() { return mb; }
void registerCommands();

struct RenderChunk{
	RenderChunk(World::chunk* c, double TimeDelta){
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
bool sel;
float selt, seldes;
block selb;
brightness selbr;
bool selce;
int selbx, selby, selbz, selcx, selcy, selcz;

string chatword;
bool chatmode = false;
vector<Command> commands;

#if 0
	woca, 这样注释都行？！
	(这儿编译不过去的童鞋，你的FB编译器版本貌似和我的不一样，把这几行注释掉吧。。。)
	=======================================
	等等不对啊！！！明明都改成c++了。。。还说是FB。。。
	正常点的C++编译器都应该不会在这儿报错吧23333333
#endif

//==============================  Main Program  ================================//
//==============================     主程序     ================================//

int main(){
	//终于进入main函数了！激动人心的一刻！！！
	
#ifndef NEWORLD_USE_WINAPI
	setlocale(LC_ALL, "zh_CN.UTF-8");
#endif

	loadoptions();
	NEInternational::LoadLang("CHS");

	system("md Configs");
	system("md Worlds");
	system("md Screenshots");
	
	windowwidth = defaultwindowwidth;
	windowheight = defaultwindowheight;
	cout << "[Console][Event]Initialize GLFW" << (glfwInit() == 1 ? "" : " - Failed!") << endl;
	std::stringstream title;
	title << "NEWorld " << MAJOR_VERSION << MINOR_VERSION << EXT_VERSION;
	MainWindow = glfwCreateWindow(windowwidth, windowheight, title.str().c_str(), NULL, NULL);
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
	GUI::clearTransition();
	Menus::mainmenu();
	glEnable(GL_LINE_SMOOTH);
	glEnable(GL_TEXTURE_2D);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glfwSwapBuffers(MainWindow);
	glfwPollEvents();
	
	Mutex = MutexCreate();
	MutexLock(Mutex);
	updateThread = ThreadCreate(&updateThreadFunc, NULL);
	if (multiplayer) {
		srand((unsigned int)time(NULL));
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
	mxl = mx; myl = my;
	printf("[Console][Game]");
	printf("Main loop started\n");
	updateThreadRun = true;
	fctime = uctime = lastupdate = timer();

	do{
		//主循环，被简化成这样，惨不忍睹啊！

		MutexUnlock(Mutex);
		MutexLock(Mutex);

		if ((timer() - uctime) >= 1.0){
			uctime = timer();
			ups = upsc;
			upsc = 0;
		}

		Render();
		
		if (glfwGetKey(MainWindow, GLFW_KEY_ESCAPE) == 1) {
			updateThreadPaused = true;
			createThumbnail();
			GUI::clearTransition();
			Menus::gamemenu();
			if (!gameexit) {
				glfwSetInputMode(MainWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
				glDepthFunc(GL_LEQUAL);
				glEnable(GL_CULL_FACE);
				setupNormalFog();
				glfwGetCursorPos(MainWindow, &mx, &my);
				mxl = mx; myl = my;
			}
			updateThreadPaused = false;
		}
		if (gameexit){
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
			printf("[Console][Game]");
			printf("Threads terminated\n");
			printf("[Console][Game]");
			printf("Returned to main menu\n");
			if (multiplayer) Network::cleanUp();
			goto main_menu;
		}
		
	} while (!glfwWindowShouldClose(MainWindow));
	saveGame();

	updateThreadRun = false;
	MutexUnlock(Mutex);
	ThreadWait(updateThread);
	ThreadDestroy(updateThread);
	MutexDestroy(Mutex);

	//结束程序，删了也没关系 ←_←（吐槽FB和glfw中）
	//不对啊这不是FB！！！这是正宗的VC++！！！！！！
	//楼上的楼上在瞎说！！！别信他的！！！
	glfwTerminate();
	return 0;
	//This is the END of the program!
}

ThreadFunc updateThreadFunc(void*){

	//Wait until start...
	MutexLock(Mutex);
	while (!updateThreadRun){
		MutexUnlock(Mutex);
		Sleep(1);
		MutexLock(Mutex);
	}
	MutexUnlock(Mutex);

	//Thread start
	MutexLock(Mutex);
	lastupdate = timer();

	while (updateThreadRun){

		MutexUnlock(Mutex);
		Sleep(1); //Don't make it always busy
		MutexLock(Mutex);

		while (updateThreadPaused){
			MutexUnlock(Mutex);
			Sleep(1); //Same as before
			MutexLock(Mutex);
			lastupdate = updateTimer = timer();
		}
		
		FirstUpdateThisFrame = true;
		updateTimer = timer();
		if (updateTimer - lastupdate >= 5.0) lastupdate = updateTimer;

		while ((updateTimer - lastupdate) >= 1.0 / 30.0 && upsc < 60){
			lastupdate += 1.0 / 30.0;
			upsc++;
			updategame();
			FirstUpdateThisFrame = false;
		}

		if ((timer() - uctime) >= 1.0) {
			uctime = timer();
			ups = upsc;
			upsc = 0;
		}

	}
	MutexUnlock(Mutex);
	
	return 0;
}

void WindowSizeFunc(GLFWwindow* win, int width, int height) {
	if (width<650) width = 650;
	if (height<400) height = 400;
	windowwidth = width;
	windowheight = height > 0 ? height : 1;
	glfwSetWindowSize(win, width, height);
	setupscreen();
}

void MouseButtonFunc(GLFWwindow*, int button, int action, int){
	mb = 0;
	if (action == GLFW_PRESS){
		if (button == GLFW_MOUSE_BUTTON_LEFT)mb += 1;
		if (button == GLFW_MOUSE_BUTTON_RIGHT)mb += 2;
		if (button == GLFW_MOUSE_BUTTON_MIDDLE)mb += 4;
	}
	else mb = 0;
}

void CharInputFunc(GLFWwindow*, unsigned int c) {
	if (c >= 128) {
		wchar_t* pwszUnicode = new wchar_t[2];
		pwszUnicode[0] = (wchar_t)c;
		pwszUnicode[1] = '\0';
		char* pszMultiByte;
		pszMultiByte = (char*)malloc((unsigned int)4);
		pszMultiByte = (char*)realloc(pszMultiByte, WCharToMByte(pszMultiByte, pwszUnicode, 4));
		inputstr += pszMultiByte;
		free(pszMultiByte);
		delete[] pwszUnicode;
	}
	else inputstr += (char)c;
}

void MouseScrollFunc(GLFWwindow*, double, double yoffset) {
	mw += (int)yoffset;
}

void splashscreen(){
	TextureID splTex = Textures::LoadRGBTexture("Textures/GUI/splashscreen.bmp");
	glEnable(GL_TEXTURE_2D);
	for (int i = 0; i < 256; i += 2) {
		glfwSwapBuffers(MainWindow);
		glfwPollEvents();
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glBindTexture(GL_TEXTURE_2D, splTex);
		glColor4f((float)i / 256, (float)i / 256, (float)i / 256, 1.0);
		glBegin(GL_QUADS);
			glTexCoord2f(0.0, 1.0); glVertex2i(-1, 1);
			glTexCoord2f(850.0f / 1024.0f, 1.0); glVertex2i(1, 1);
			glTexCoord2f(850.0f / 1024.0f, 1.0 - 480.0f / 1024.0f); glVertex2i(1, -1);
			glTexCoord2f(0.0, 1.0 - 480.0f / 1024.0f); glVertex2i(-1, -1);
		glEnd();
		Sleep(10);
	}
	glfwSwapBuffers(MainWindow);
	glfwPollEvents();
}

void setupscreen() {

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
	glGenBuffersARB(1, &World::EmptyBuffer);

}

void InitGL() {
	//获取OpenGL版本
	GLVersionMajor = glfwGetWindowAttrib(MainWindow, GLFW_CONTEXT_VERSION_MAJOR);
	GLVersionMinor = glfwGetWindowAttrib(MainWindow, GLFW_CONTEXT_VERSION_MINOR);
	GLVersionRev = glfwGetWindowAttrib(MainWindow, GLFW_CONTEXT_REVISION);
	glGenBuffersARB = (PFNGLGENBUFFERSARBPROC)glfwGetProcAddress("glGenBuffersARB");
	glBindBufferARB = (PFNGLBINDBUFFERARBPROC)glfwGetProcAddress("glBindBufferARB");
	glBufferDataARB = (PFNGLBUFFERDATAARBPROC)glfwGetProcAddress("glBufferDataARB");
	glDeleteBuffersARB = (PFNGLDELETEBUFFERSARBPROC)glfwGetProcAddress("glDeleteBuffersARB");
	glCreateShaderObjectARB = (PFNGLCREATESHADEROBJECTARBPROC)glfwGetProcAddress("glCreateShaderObjectARB");
	glShaderSourceARB = (PFNGLSHADERSOURCEARBPROC)glfwGetProcAddress("glShaderSourceARB");
	glCompileShaderARB = (PFNGLCOMPILESHADERARBPROC)glfwGetProcAddress("glCompileShaderARB");
	glCreateProgramObjectARB = (PFNGLCREATEPROGRAMOBJECTARBPROC)glfwGetProcAddress("glCreateProgramObjectARB");
	glAttachObjectARB = (PFNGLATTACHOBJECTARBPROC)glfwGetProcAddress("glAttachObjectARB");
	glLinkProgramARB = (PFNGLLINKPROGRAMARBPROC)glfwGetProcAddress("glLinkProgramARB");
	glUseProgramObjectARB = (PFNGLUSEPROGRAMOBJECTARBPROC)glfwGetProcAddress("glUseProgramObjectARB");
	glGetObjectParameterivARB = (PFNGLGETOBJECTPARAMETERIVARBPROC)glfwGetProcAddress("glGetObjectParameterivARB");
	glGetInfoLogARB = (PFNGLGETINFOLOGARBPROC)glfwGetProcAddress("glGetInfoLogARB");
	glDetachObjectARB = (PFNGLDETACHOBJECTARBPROC)glfwGetProcAddress("glDetachObjectARB");
	glDeleteObjectARB = (PFNGLDELETEOBJECTARBPROC)glfwGetProcAddress("glDeleteObjectARB");
	glTexImage3D = (PFNGLTEXIMAGE3DPROC)glfwGetProcAddress("glTexImage3D");
}

void setupNormalFog() {
	float fogColor[4] = { skycolorR, skycolorG, skycolorB, 1.0f };
	glEnable(GL_FOG);
	glFogi(GL_FOG_MODE, GL_LINEAR);
	glFogfv(GL_FOG_COLOR, fogColor);
	glFogf(GL_FOG_START, viewdistance * 16.0f - 32.0f);
	glFogf(GL_FOG_END, viewdistance * 16.0f);
}

void LoadTextures(){
	//载入纹理
	Textures::Init();
	
	tex_select = Textures::LoadRGBATexture("Textures/GUI/select.bmp", "");
	tex_unselect = Textures::LoadRGBATexture("Textures/GUI/unselect.bmp", "");
	tex_title = Textures::LoadRGBATexture("Textures/GUI/title.bmp", "Textures/GUI/titlemask.bmp");
	for (int i = 0; i < 6; i++) {
		std::stringstream ss;
		ss << "Textures/GUI/mainmenu" << i << ".bmp";
		tex_mainmenu[i] = Textures::LoadRGBTexture(ss.str());
	}
	
	DefaultSkin = Textures::LoadRGBATexture("Textures/Player/skin_xiaoqiao.bmp", "Textures/Player/skinmask_xiaoqiao.bmp");

	for (int gloop = 1; gloop <= 10; gloop++){
		string path = "Textures/blocks/destroy_" + itos(gloop) + ".bmp";
		DestroyImage[gloop] = Textures::LoadRGBATexture(path, path);
	}

	BlockTextures = Textures::LoadRGBATexture("Textures/blocks/Terrain.bmp", "Textures/blocks/Terrainmask.bmp");
	BlockTextures3D = Textures::LoadBlock3DTexture("Textures/blocks/Terrain3D.bmp", "Textures/blocks/Terrain3Dmask.bmp");
	loadItemsTextures();
}

void saveGame(){
	World::saveAllChunks();
	if (!Player::save(World::worldname)) {
#ifdef NEWORLD_CONSOLE_OUTPUT
		DebugWarning("Failed saving player info!");
#endif
	}
}

bool loadGame(){
	if (!Player::load(World::worldname)) {
#ifdef NEWORLD_CONSOLE_OUTPUT
		DebugWarning("Failed loading player info!");
#endif
		return false;
	}
	return true;
}

bool isPressed(int key, bool setFalse = false) {
	static bool keyPressed[GLFW_KEY_LAST + 1];
	if (setFalse) { keyPressed[key] = false; return true; }
	if (key > GLFW_KEY_LAST || key <= 0) return false;
	if (!glfwGetKey(MainWindow, key)) keyPressed[key] = false;
	if (!keyPressed[key] && glfwGetKey(MainWindow, key)) {
		keyPressed[key] = true;
		return true;
	}
	return false;
}

void registerCommands() {
	commands.push_back(Command("/give", [](const vector<string>& command) {
		if (command.size() != 3) return false;
		item itemid; conv(command[1], itemid);
		short amount; conv(command[2], amount);
		Player::addItem(itemid, amount);
		return true;
	}));
	commands.push_back(Command("/tp", [](const vector<string>& command) {
		if (command.size() != 4) return false;
		double x; conv(command[1], x);
		double y; conv(command[2], y);
		double z; conv(command[3], z);
		Player::xpos = x;
		Player::ypos = y;
		Player::zpos = z;
		return true;
	}));
	commands.push_back(Command("/suicide", [](const vector<string>& command) {
		Player::spawn();
		return true;
	}));
	commands.push_back(Command("/setblock", [](const vector<string>& command) {
		if (command.size() != 5) return false;
		int x; conv(command[1], x);
		int y; conv(command[2], y);
		int z; conv(command[3], z);
		block b; conv(command[4], b);
		World::setblock(x, y, z, b);
		return true;
	}));
	commands.push_back(Command("/tree", [](const vector<string>& command) {
		if (command.size() != 4) return false;
		int x; conv(command[1], x);
		int y; conv(command[2], y);
		int z; conv(command[3], z);
		World::buildtree(x, y, z);
		return true;
	}));
	commands.push_back(Command("/explode", [](const vector<string>& command) {
		if (command.size() != 5) return false;
		int x; conv(command[1], x);
		int y; conv(command[2], y);
		int z; conv(command[3], z);
		int r; conv(command[4], r);
		World::explode(x, y, z, r);
		return true;
	}));
}

bool doCommand(const vector<string>& command) {
	for (int i = 0; i != commands.size(); i++) {
		if (command[0] == commands[i].identifier) {
			return commands[i].execute(command);
		}
	}
}

void updategame(){
	//Time_updategame_ = timer();
	static double Wprstm;
	static bool WP;
	//static double mxl, myl;
	//glfwGetCursorPos(MainWindow, &mx, &my);
	Player::BlockInHand = Player::inventory[3][Player::indexInHand];
	//生命值相关
	if (Player::health > 0) {
		if (Player::health < Player::healthMax) Player::health += Player::healSpeed;
		if (Player::health > Player::healthMax) Player::health = Player::healthMax;
	}
	else {
		Player::health = 1;
	}

	//World::unloadedChunks=0
	World::rebuiltChunks = 0;
	World::updatedChunks = 0;

	//ciArray move
	if (World::cpArray.originX != Player::cxt - viewdistance - 2 || World::cpArray.originY != Player::cyt - viewdistance - 2 || World::cpArray.originZ != Player::czt - viewdistance - 2){
		World::cpArray.moveTo(Player::cxt - viewdistance - 2, Player::cyt - viewdistance - 2, Player::czt - viewdistance - 2);
	}
	//HeightMap move
	if (World::HMap.originX != (Player::cxt - viewdistance - 2) * 16 || World::HMap.originZ != (Player::czt - viewdistance - 2) * 16){
		World::HMap.moveTo((Player::cxt - viewdistance - 2) * 16, (Player::czt - viewdistance - 2) * 16);
	}

	if (FirstUpdateThisFrame){
		World::sortChunkLoadUnloadList(RoundInt(Player::xpos), RoundInt(Player::ypos), RoundInt(Player::zpos));

		//卸载区块(Unload chunks)
		int sumUnload;
		sumUnload = World::chunkUnloads > World::MaxChunkUnloads ? World::MaxChunkUnloads : World::chunkUnloads;
		for (int i = 0; i < sumUnload; i++) {
			World::chunk* cp = World::chunkUnloadList[i].first;
#ifdef NEWORLD_DEBUG
			if (cp == nullptr)DebugError("Unload error!");
#endif
			int cx = cp->cx, cy = cp->cy, cz = cp->cz;
			cp->Unload();
			World::DeleteChunk(cx, cy, cz);
		}

		//加载区块(Load chunks)
		int sumLoad;
		sumLoad = World::chunkLoads > World::MaxChunkLoads ? World::MaxChunkLoads : World::chunkLoads;
		for (int i = 0; i < sumLoad; i++){
			int cx = World::chunkLoadList[i][1];
			int cy = World::chunkLoadList[i][2];
			int cz = World::chunkLoadList[i][3];
			World::chunk* c = World::AddChunk(cx, cy, cz);
			c->Load();
			if (c->Empty) {
				c->Unload(); World::DeleteChunk(cx, cy, cz);
				World::cpArray.setChunkPtr(cx, cy, cz, World::EmptyChunkPtr);
			}
		}
		
	}
	
	//加载动画
	for (int i = 0; i < World::loadedChunks; i++){
		World::chunk* cp = World::chunks[i];
		if (cp->loadAnim <= 0.3f)
			cp->loadAnim = 0.0f;
		else
			cp->loadAnim *= 0.6f;
	}

	//随机状态更新
	for (int i = 0; i < World::loadedChunks; i++){
		int x, y, z, gx, gy, gz;
		int cx = World::chunks[i]->cx;
		int cy = World::chunks[i]->cy;
		int cz = World::chunks[i]->cz;
		x = int(rnd() * 16); gx = x + cx * 16;
		y = int(rnd() * 16); gy = y + cy * 16;
		z = int(rnd() * 16); gz = z + cz * 16;
		if (World::chunks[i]->getblock(x, y, z) == Blocks::DIRT &&
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
			World::getblock(gx, gy - 1, gz - 1, Blocks::AIR) == Blocks::GRASS)){
			//长草
			World::chunks[i]->setblock(x, y, z, Blocks::GRASS);
			World::updateblock(x + cx * 16, y + cy * 16 + 1, z + cz * 16, true);
			World::setChunkUpdated(cx, cy, cz, true);
		}
		if (World::chunks[i]->getblock(x, y, z) == Blocks::GRASS && World::getblock(gx, gy + 1, gz, Blocks::AIR) != Blocks::AIR) {
			//草被覆盖
			World::chunks[i]->setblock(x, y, z, Blocks::DIRT);
			World::updateblock(x + cx * 16, y + cy * 16 + 1, z + cz * 16, true);
		}
	}
	
	//判断选中的方块
	double lx, ly, lz, sidedist[7];
	int sidedistmin;
	lx = Player::xpos; ly = Player::ypos + Player::height + Player::heightExt; lz = Player::zpos;
	
	sel = false;
	selx = sely = selz = selbx = selby = selbz = selcx = selcy = selcz = selb = selbr = 0;
	bool put = false;
	
	if (!bagOpened) {

		//从玩家位置发射一条线段
		for (int i = 0; i < selectPrecision*selectDistance; i++) {
			//线段延伸
			lx += sin(M_PI / 180 * (Player::heading - 180))*sin(M_PI / 180 * (Player::lookupdown + 90)) / (double)selectPrecision;
			ly += cos(M_PI / 180 * (Player::lookupdown + 90)) / (double)selectPrecision;
			lz += cos(M_PI / 180 * (Player::heading - 180))*sin(M_PI / 180 * (Player::lookupdown + 90)) / (double)selectPrecision;

			//碰到方块
			if (BlockInfo(World::getblock(RoundInt(lx), RoundInt(ly), RoundInt(lz))).isSolid()) {
				int x, y, z;
				x = RoundInt(lx);
				y = RoundInt(ly);
				z = RoundInt(lz);

				selx = x; sely = y; selz = z;
				sel = true;

				//找方块所在区块及位置
				selcx = getchunkpos(x);
				selbx = getblockpos(x);
				selcy = getchunkpos(y);
				selby = getblockpos(y);
				selcz = getchunkpos(z);
				selbz = getblockpos(z);
				
				sidedist[1] = abs(y + 0.5 - ly);          //顶面
				sidedist[2] = abs(y - 0.5 - ly);		  //底面
				sidedist[3] = abs(x + 0.5 - lx);		  //左面
				sidedist[4] = abs(x - 0.5 - lx);	   	  //右面
				sidedist[5] = abs(z + 0.5 - lz);		  //前面
				sidedist[6] = abs(z - 0.5 - lz);		  //后面
				sidedistmin = 1;						  //离哪个面最近
				for (int j = 2; j <= 6; j++) {
					if (sidedist[j] < sidedist[sidedistmin]) sidedistmin = j;
				}

				if (World::chunkOutOfBound(selcx, selcy, selcz) == false) {
					World::chunk* cp = World::getChunkPtr(selcx, selcy, selcz);
					if (cp == nullptr || cp == World::EmptyChunkPtr) continue;
					selb = cp->getblock(selbx, selby, selbz);
				}

				switch (sidedistmin) {
				case 1:
					if (World::chunkOutOfBound(selcx, selcy, selcz) == false)
						selbr = World::getbrightness(selx, sely + 1, selz);
					break;
				case 2:
					if (World::chunkOutOfBound(selcx, selcy, selcz) == false)
						selbr = World::getbrightness(selx, sely - 1, selz);
					break;
				case 3:
					if (World::chunkOutOfBound(selcx, selcy, selcz) == false)
						selbr = World::getbrightness(selx + 1, sely, selz);
					break;
				case 4:
					if (World::chunkOutOfBound(selcx, selcy, selcz) == false)
						selbr = World::getbrightness(selx - 1, sely, selz);
					break;
				case 5:
					if (World::chunkOutOfBound(selcx, selcy, selcz) == false)
						selbr = World::getbrightness(selx, sely, selz + 1);
					break;
				case 6:
					if (World::chunkOutOfBound(selcx, selcy, selcz) == false)
						selbr = World::getbrightness(selx, sely, selz - 1);
					break;
				}

				if (mb == 1 || glfwGetKey(MainWindow, GLFW_KEY_ENTER) == GLFW_PRESS) {
					Particles::throwParticle(World::getblock(x, y, z),
						float(x + rnd() - 0.5f), float(y + rnd() - 0.2f), float(z + rnd() - 0.5f),
						float(rnd()*0.2f - 0.1f), float(rnd()*0.2f - 0.1f), float(rnd()*0.2f - 0.1f),
						float(rnd()*0.01f + 0.02f), int(rnd() * 30) + 30);

					if (selx != oldselx || sely != oldsely || selz != oldselz)
						seldes = 0.0;
					else
						seldes += 5.0;
					if (seldes >= 100.0) {
						Player::addItem(World::getblock(x, y, z));
						for (int j = 1; j <= 25; j++) {
							Particles::throwParticle(World::getblock(x, y, z),
								float(x + rnd() - 0.5f), float(y + rnd() - 0.2f), float(z + rnd() - 0.5f),
								float(rnd()*0.2f - 0.1f), float(rnd()*0.2f - 0.1f), float(rnd()*0.2f - 0.1f),
								float(rnd()*0.02 + 0.03), int(rnd() * 60) + 30);
						}
						World::pickblock(x, y, z);
					}
				}
				if (((mb == 2 && mbp == false) || isPressed(GLFW_KEY_TAB))) { //鼠标右键
					if (Player::inventoryAmount[3][Player::indexInHand] > 0 && isBlock(Player::inventory[3][Player::indexInHand])) {
						//放置方块
						put = true;
						switch (sidedistmin) {
						case 1:
							if (Player::putBlock(x, y + 1, z, Player::BlockInHand) == false) put = false;
							break;
						case 2:
							if (Player::putBlock(x, y - 1, z, Player::BlockInHand) == false) put = false;
							break;
						case 3:
							if (Player::putBlock(x + 1, y, z, Player::BlockInHand) == false) put = false;
							break;
						case 4:
							if (Player::putBlock(x - 1, y, z, Player::BlockInHand) == false) put = false;
							break;
						case 5:
							if (Player::putBlock(x, y, z + 1, Player::BlockInHand) == false) put = false;
							break;
						case 6:
							if (Player::putBlock(x, y, z - 1, Player::BlockInHand) == false) put = false;
							break;
						}
						if (put) {
							Player::inventoryAmount[3][Player::indexInHand]--;
							if (Player::inventoryAmount[3][Player::indexInHand] == 0) Player::inventory[3][Player::indexInHand] = Blocks::AIR;
						}
					}
					else {
						//使用物品

					}
				}
				break;
			}
		}

		if (selx != oldselx || sely != oldsely || selz != oldselz || (mb == 0 && glfwGetKey(MainWindow, GLFW_KEY_ENTER) != GLFW_PRESS)) seldes = 0.0;
		oldselx = selx;
		oldsely = sely;
		oldselz = selz;
		
		Player::intxpos = RoundInt(Player::xpos);
		Player::intypos = RoundInt(Player::ypos);
		Player::intzpos = RoundInt(Player::zpos);

		//更新方向
		Player::heading += Player::xlookspeed;
		Player::lookupdown += Player::ylookspeed;
		Player::xlookspeed = Player::ylookspeed = 0.0;
	
		if (!chatmode) {
			//移动！(生命在于运动)
			if (glfwGetKey(MainWindow, GLFW_KEY_W) || Player::glidingNow) {
				if (!WP) {
					if (Wprstm == 0.0) {
						Wprstm = timer();
					}
					else {
						if (timer() - Wprstm <= 0.5) { Player::Running = true; Wprstm = 0.0; }
						else Wprstm = timer();
					}
				}
				if (Wprstm != 0.0 && timer() - Wprstm > 0.5) Wprstm = 0.0;
				WP = true;
				if (!Player::glidingNow) {
					Player::xa += -sin(Player::heading*M_PI / 180.0) * Player::speed;
					Player::za += -cos(Player::heading*M_PI / 180.0) * Player::speed;
				}
				else {
					Player::xa = sin(M_PI / 180 * (Player::heading - 180))*sin(M_PI / 180 * (Player::lookupdown + 90)) * Player::glidingSpeed * speedCast;
					Player::ya = cos(M_PI / 180 * (Player::lookupdown + 90)) * Player::glidingSpeed * speedCast;
					Player::za = cos(M_PI / 180 * (Player::heading - 180))*sin(M_PI / 180 * (Player::lookupdown + 90)) * Player::glidingSpeed * speedCast;
					if (Player::ya < 0) Player::ya *= 2;
				}
			}
			else {
				Player::Running = false;
				WP = false;
			}
			if (Player::Running)Player::speed = runspeed;
			else Player::speed = walkspeed;

			if (glfwGetKey(MainWindow, GLFW_KEY_S) == GLFW_PRESS&&!Player::glidingNow) {
				Player::xa += sin(Player::heading*M_PI / 180.0) * Player::speed;
				Player::za += cos(Player::heading*M_PI / 180.0) * Player::speed;
				Wprstm = 0.0;
			}

			if (glfwGetKey(MainWindow, GLFW_KEY_A) == GLFW_PRESS&&!Player::glidingNow) {
				Player::xa += sin((Player::heading - 90)*M_PI / 180.0) * Player::speed;
				Player::za += cos((Player::heading - 90)*M_PI / 180.0) * Player::speed;
				Wprstm = 0.0;
			}

			if (glfwGetKey(MainWindow, GLFW_KEY_D) == GLFW_PRESS&&!Player::glidingNow) {
				Player::xa += -sin((Player::heading - 90)*M_PI / 180.0) * Player::speed;
				Player::za += -cos((Player::heading - 90)*M_PI / 180.0) * Player::speed;
				Wprstm = 0.0;
			}

			if (!Player::Flying) {
				double horizontalSpeed = sqrt(Player::xa*Player::xa + Player::za*Player::za);
				if (horizontalSpeed > Player::speed && !Player::glidingNow) {
					Player::xa *= Player::speed / horizontalSpeed;
					Player::za *= Player::speed / horizontalSpeed;
				}
			}
			else {
				if (glfwGetKey(MainWindow, GLFW_KEY_R) == GLFW_PRESS && !Player::glidingNow) {
					if (glfwGetKey(MainWindow, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
						Player::xa = -sin(Player::heading*M_PI / 180.0) * runspeed * 10;
						Player::za = -cos(Player::heading*M_PI / 180.0) * runspeed * 10;
					}
					else {
						Player::xa = sin(M_PI / 180 * (Player::heading - 180))*sin(M_PI / 180 * (Player::lookupdown + 90)) * runspeed * 20;
						Player::ya = cos(M_PI / 180 * (Player::lookupdown + 90)) * runspeed * 20;
						Player::za = cos(M_PI / 180 * (Player::heading - 180))*sin(M_PI / 180 * (Player::lookupdown + 90)) * runspeed * 20;
					}
				}

				if (glfwGetKey(MainWindow, GLFW_KEY_F) == GLFW_PRESS && !Player::glidingNow) {
					if (glfwGetKey(MainWindow, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
						Player::xa = sin(Player::heading*M_PI / 180.0) * runspeed * 10;
						Player::za = cos(Player::heading*M_PI / 180.0) * runspeed * 10;
					}
					else {
						Player::xa = -sin(M_PI / 180 * (Player::heading - 180))*sin(M_PI / 180 * (Player::lookupdown + 90)) * runspeed * 20;
						Player::ya = -cos(M_PI / 180 * (Player::lookupdown + 90)) * runspeed * 20;
						Player::za = -cos(M_PI / 180 * (Player::heading - 180))*sin(M_PI / 180 * (Player::lookupdown + 90)) * runspeed * 20;
					}
				}
			}

			//切换方块
			if (isPressed(GLFW_KEY_Z) && Player::indexInHand > 0) Player::indexInHand--;
			if (isPressed(GLFW_KEY_X) && Player::indexInHand < 9) Player::indexInHand++;
			if ((int)Player::indexInHand + (mwl - mw) < 0)Player::indexInHand = 0;
			else if ((int)Player::indexInHand + (mwl - mw) > 9)Player::indexInHand = 9;
			else Player::indexInHand += (byte)(mwl - mw);
			mwl = mw;

			//起跳！
			if (isPressed(GLFW_KEY_SPACE)) {
				if (!Player::inWater) {
					if ((Player::OnGround || Player::AirJumps < MaxAirJumps) && Player::Flying == false && Player::CrossWall == false) {
						if (Player::OnGround == false) {
							Player::jump = 0.3;
							Player::AirJumps++;
						}
						else {
							Player::jump = 0.25;
							Player::OnGround = false;
						}
					}
					if (Player::Flying || Player::CrossWall) {
						Player::ya += walkspeed / 2;
						isPressed(GLFW_KEY_SPACE, true);
					}
					Wprstm = 0.0;
				}
				else {
					Player::ya = walkspeed;
					isPressed(GLFW_KEY_SPACE, true);
				}
			}

			if ((glfwGetKey(MainWindow, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(MainWindow, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS) && !Player::glidingNow) {
				if (Player::CrossWall || Player::Flying) Player::ya -= walkspeed / 2;
				Wprstm = 0.0;
			}

			if (glfwGetKey(MainWindow, GLFW_KEY_K) && Player::Glide && !Player::OnGround && !Player::glidingNow) {
				double h = Player::ypos + Player::height + Player::heightExt;
				Player::glidingEnergy = g*h;
				Player::glidingSpeed = 0;
				Player::glidingNow = true;
			}

			//各种设置切换
			if (isPressed(GLFW_KEY_F1)) {
				Player::Flying = !Player::Flying;
				Player::jump = 0.0;
			}
			if (isPressed(GLFW_KEY_F2)) shouldGetScreenshot = true;
			if (isPressed(GLFW_KEY_F3)) DebugMode = !DebugMode;
			if (isPressed(GLFW_KEY_F3) && glfwGetKey(MainWindow, GLFW_KEY_H) == GLFW_PRESS) {
				DebugHitbox = !DebugHitbox;
				DebugMode = true;
			}
			if (isPressed(GLFW_KEY_F4)) Player::CrossWall = !Player::CrossWall;
			if (isPressed(GLFW_KEY_F5)) GUIrenderswitch = !GUIrenderswitch;
			if (isPressed(GLFW_KEY_F6)) Player::Glide = !Player::Glide;
			if (isPressed(GLFW_KEY_F7)) Player::spawn();
			if (isPressed(GLFW_KEY_SLASH)) chatmode = true; //斜杠将会在下面的if(chatmode)里添加
		}
		
		if (isPressed(GLFW_KEY_ENTER) == GLFW_PRESS) {
			chatmode = !chatmode;
			if (chatword != "") { //指令的执行，或发出聊天文本
				if (chatword.substr(0, 1) == "/") { //指令
					vector<string> command = split(chatword, " ");
					if (!doCommand(command)) { //执行失败
						DebugWarning("Fail to execute the command: " + chatword);
					}
				}
				else {

				}
			}
			chatword = "";
		}
		if (chatmode) {
			if (isPressed(GLFW_KEY_BACKSPACE) && chatword.length()>0) {
				int n = chatword[chatword.length() - 1];
				if (n > 0 && n <= 127)
					chatword = chatword.substr(0, chatword.length() - 1);
				else
					chatword = chatword.substr(0, chatword.length() - 2);
			}
			else {
				chatword += inputstr;
			}
			//自动补全
			if (isPressed(GLFW_KEY_TAB) && chatmode && chatword.size()>0 && chatword.substr(0, 1) == "/") {
				for (int i = 0; i != commands.size(); i++) {
					if (beginWith(commands[i].identifier, chatword)) {
						chatword = commands[i].identifier;
					}
				}
			}
		}
	}

	inputstr = "";

	if (isPressed(GLFW_KEY_E) && GUIrenderswitch && !chatmode){
		bagOpened = !bagOpened;
		bagAnimTimer = timer();
		if (!bagOpened) {
			glfwSetInputMode(MainWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			glfwGetCursorPos(MainWindow, &mx, &my);
			mxl = mx; myl = my; mwl = mw; mbl = mb;
		}
		else {
			shouldGetThumbnail = true;
			Player::xlookspeed = Player::ylookspeed = 0.0;
		}
	}
	
	if (!bagOpened && !chatmode){
		if (isPressed(GLFW_KEY_L))World::saveAllChunks();
	}

	//跳跃
	if (!Player::glidingNow) {
		if (!Player::inWater) {
			if (!Player::Flying && !Player::CrossWall && !glfwGetKey(MainWindow, GLFW_KEY_R) && !glfwGetKey(MainWindow, GLFW_KEY_F)) {
				Player::ya = -0.001;
				if (Player::OnGround) {
					Player::jump = 0.0;
					Player::AirJumps = 0;
					isPressed(GLFW_KEY_SPACE, true);
				}
				else {
					//自由落体计算
					Player::jump -= 0.025;
					Player::ya = Player::jump + 0.5 * 0.6 * 1 / 900;
				}
			}
			else {
				Player::jump = 0.0;
				Player::AirJumps = 0;
			}
		}
		else {
			Player::jump = 0.0;
			Player::AirJumps = MaxAirJumps;
			isPressed(GLFW_KEY_SPACE, true);
			if (Player::ya <= 0.001 && !Player::Flying && !Player::CrossWall) {
				Player::ya =- 0.001;
				if (!Player::OnGround) Player::ya -= 0.1;
			}
		}
	}

	//爬墙
	//if (Player::NearWall && Player::Flying == false && Player::CrossWall == false){
	//	Player::ya += walkspeed
	//	Player::jump = 0.0
	//}
	
	if (Player::glidingNow) {
		double& E = Player::glidingEnergy;
		double oldh = Player::ypos + Player::height + Player::heightExt + Player::ya;
		double h = oldh;
		if (E - Player::glidingMinimumSpeed < h*g) {  //小于最小速度
			h = (E - Player::glidingMinimumSpeed) / g;
		}
		Player::glidingSpeed = sqrt(2 * (E - g*h));
		E -= EDrop;
		Player::ya += h - oldh;
	}

	mbp = mb;
	FirstFrameThisUpdate = true;
	
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
	Particles::updateall();

	//	Time_updategame += timer() - Time_updategame;

}

void debugText(string s, bool init) {
	static int pos = 0;
	if (init) {
		pos = 0;
		return;
	}
	TextRenderer::renderString(0, 16 * pos, s);
	pos++;
}

void Render() {
	//画场景
	double curtime = timer();
	double TimeDelta;
	double xpos, ypos, zpos;
	int renderedChunk = 0;
	int TexcoordCount = MergeFace ? 3 : 2;
	/*
	static vector<GLint> multiDrawArrays[3];
	static vector<GLsizei> multiDrawCounts[3];
	for (int i = 0; i < 3; i++) {
		multiDrawArrays[i].clear();
		multiDrawCounts[i].clear();
	}
	*/

	mxl = mx; myl = my;
	glfwGetCursorPos(MainWindow, &mx, &my);
	
	if (Player::Running) {
		if (FOVyExt < 9.8) {
			TimeDelta = curtime - SpeedupAnimTimer;
			FOVyExt = 10.0f - (10.0f - FOVyExt)*(float)pow(0.8, TimeDelta * 30);
			SpeedupAnimTimer = curtime;
		}
		else FOVyExt = 10.0;
	}
	else {
		if (FOVyExt > 0.2) {
			TimeDelta = curtime - SpeedupAnimTimer;
			FOVyExt *= (float)pow(0.8, TimeDelta * 30);
			SpeedupAnimTimer = curtime;
		}
		else FOVyExt = 0.0;
	}
	SpeedupAnimTimer = curtime;

	if (Player::OnGround) {
		//半蹲特效
		if (Player::jump < -0.005) {
			if (Player::jump <= -(Player::height - 0.5f))
				Player::heightExt = -(Player::height - 0.5f);
			else
				Player::heightExt = (float)Player::jump;
			TouchdownAnimTimer = curtime;
		}
		else {
			if (Player::heightExt <= -0.005) {
				Player::heightExt *= (float)pow(0.8, (curtime - TouchdownAnimTimer) * 30);
				TouchdownAnimTimer = curtime;
			}
		}
	}

	xpos = Player::xpos - Player::xd + (curtime - lastupdate) * 30.0 * Player::xd;
	ypos = Player::ypos + Player::height + Player::heightExt - Player::yd + (curtime - lastupdate) * 30.0 * Player::yd;
	zpos = Player::zpos - Player::zd + (curtime - lastupdate) * 30.0 * Player::zd;
	
	if (!bagOpened) {
		//转头！你治好了我多年的颈椎病！
		if (mx != mxl) Player::xlookspeed -= (mx - mxl)*mousemove;
		if (my != myl) Player::ylookspeed += (my - myl)*mousemove;
		if (glfwGetKey(MainWindow, GLFW_KEY_RIGHT) == 1) Player::xlookspeed -= mousemove * 16 * (curtime - lastframe) * 30.0;
		if (glfwGetKey(MainWindow, GLFW_KEY_LEFT) == 1) Player::xlookspeed += mousemove * 16 * (curtime - lastframe) * 30.0;
		if (glfwGetKey(MainWindow, GLFW_KEY_UP) == 1) Player::ylookspeed -= mousemove * 16 * (curtime - lastframe) * 30.0;
		if (glfwGetKey(MainWindow, GLFW_KEY_DOWN) == 1) Player::ylookspeed += mousemove * 16 * (curtime - lastframe) * 30.0;
		//限制角度，别把头转掉下来了 ←_←
		if (Player::lookupdown + Player::ylookspeed < -90.0) Player::ylookspeed = -90.0 - Player::lookupdown;
		if (Player::lookupdown + Player::ylookspeed > 90.0) Player::ylookspeed = 90.0 - Player::lookupdown;
	}

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

	Player::cxt = getchunkpos((int)Player::xpos);
	Player::cyt = getchunkpos((int)Player::ypos);
	Player::czt = getchunkpos((int)Player::zpos);

	//更新区块显示列表
	World::sortChunkBuildRenderList(RoundInt(Player::xpos), RoundInt(Player::ypos), RoundInt(Player::zpos));
	int brl = World::chunkBuildRenders > World::MaxChunkRenders ? World::MaxChunkRenders : World::chunkBuildRenders;
	for (int i = 0; i < brl; i++) {
		int ci = World::chunkBuildRenderList[i][1];
		World::chunks[ci]->buildRender();
	}

	//删除已卸载区块的VBO
	if (World::vbuffersShouldDelete.size() > 0) {
		glDeleteBuffersARB(World::vbuffersShouldDelete.size(), World::vbuffersShouldDelete.data());
		World::vbuffersShouldDelete.clear();
	}

	double plookupdown = Player::lookupdown + Player::ylookspeed;
	double pheading = Player::heading + Player::xlookspeed;

	glLoadIdentity();
	glRotated(plookupdown, 1, 0, 0);
	glRotated(360.0 - pheading, 0, 1, 0);
	Frustum::LoadIdentity();
	Frustum::SetPerspective(FOVyNormal + FOVyExt, (float)windowwidth / windowheight, 0.05f, viewdistance * 16.0f);
	Frustum::MultRotate((float)plookupdown, 1, 0, 0);
	Frustum::MultRotate(360.0f - (float)pheading, 0, 1, 0);
	Frustum::update();
	World::calcVisible(xpos, ypos, zpos);

	displayChunks.clear();
	renderedChunk = 0;
	for (int i = 0; i < World::loadedChunks; i++) {
		if (!World::chunks[i]->renderBuilt || World::chunks[i]->Empty) continue;
		if (World::chunkInRange(World::chunks[i]->cx, World::chunks[i]->cy, World::chunks[i]->cz,
			Player::cxt, Player::cyt, Player::czt, viewdistance)) {
			if (World::chunks[i]->visible) {
				renderedChunk++;
				displayChunks.push_back(RenderChunk(World::chunks[i], (curtime - lastupdate) * 30.0));
			}
		}
	}

	MutexUnlock(Mutex);
	
	if (MergeFace) {
		glDisable(GL_TEXTURE_2D);
		glEnable(GL_TEXTURE_3D);
		glBindTexture(GL_TEXTURE_3D, BlockTextures3D);
	}
	else glBindTexture(GL_TEXTURE_2D, BlockTextures);

	glDisable(GL_BLEND);
	glEnableClientState(GL_COLOR_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnableClientState(GL_VERTEX_ARRAY);

	for (int i = 0; i < renderedChunk; i++) {
		RenderChunk cr = displayChunks[i];
		if (cr.vtxs[0] == 0) continue;
		glPushMatrix();
		glTranslated(cr.cx * 16.0 - xpos, cr.cy * 16.0 - cr.loadAnim - ypos, cr.cz * 16.0 - zpos);
		Renderer::renderbuffer(cr.vbuffers[0], cr.vtxs[0], TexcoordCount, 3);
		glPopMatrix();
	}

	glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
	glDisableClientState(GL_COLOR_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_VERTEX_ARRAY);
	if (MergeFace) {
		glDisable(GL_TEXTURE_3D);
		glEnable(GL_TEXTURE_2D);
	}
	glEnable(GL_BLEND);

	MutexLock(Mutex);

	if (seldes > 0.0) {
		glTranslated(selx - xpos, sely - ypos, selz - zpos);
		renderDestroy(seldes, 0, 0, 0);
		glTranslated(-selx + xpos, -sely + ypos, -selz + zpos);
	}
	glBindTexture(GL_TEXTURE_2D, BlockTextures);
	Particles::renderall(xpos, ypos, zpos);

	glDisable(GL_TEXTURE_2D);
	if (GUIrenderswitch&&sel) {
		glTranslated(selx - xpos, sely - ypos, selz - zpos);
		drawBorder(0, 0, 0);
		glTranslated(-selx + xpos, -sely + ypos, -selz + zpos);
	}

	MutexUnlock(Mutex);

	glLoadIdentity();
	glRotated(plookupdown, 1, 0, 0);
	glRotated(360.0 - pheading, 0, 1, 0);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_CULL_FACE);

	glEnableClientState(GL_COLOR_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnableClientState(GL_VERTEX_ARRAY);
	
	if (MergeFace) {
		glDisable(GL_TEXTURE_2D);
		glEnable(GL_TEXTURE_3D);
		glBindTexture(GL_TEXTURE_3D, BlockTextures3D);
	}
	else glBindTexture(GL_TEXTURE_2D, BlockTextures);

	for (int i = 0; i < renderedChunk; i++) {
		RenderChunk cr = displayChunks[i];
		if (cr.vtxs[1] == 0) continue;
		glPushMatrix();
		glTranslated(cr.cx * 16.0 - xpos, cr.cy * 16.0 - cr.loadAnim - ypos, cr.cz * 16.0 - zpos);
		Renderer::renderbuffer(cr.vbuffers[1], cr.vtxs[1], TexcoordCount, 3);
		glPopMatrix();
	}
	glDisable(GL_CULL_FACE);
	for (int i = 0; i < renderedChunk; i++) {
		RenderChunk cr = displayChunks[i];
		if (cr.vtxs[2] == 0) continue;
		glPushMatrix();
		glTranslated(cr.cx * 16.0 - xpos, cr.cy * 16.0 - cr.loadAnim - ypos, cr.cz * 16.0 - zpos);
		Renderer::renderbuffer(cr.vbuffers[2], cr.vtxs[2], TexcoordCount, 3);
		glPopMatrix();
	}

	glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
	glDisableClientState(GL_COLOR_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_VERTEX_ARRAY);
	if (MergeFace) {
		glDisable(GL_TEXTURE_3D);
		glEnable(GL_TEXTURE_2D);
	}

	glLoadIdentity();
	glRotated(plookupdown, 1, 0, 0);
	glRotated(360.0 - pheading, 0, 1, 0);
	glTranslated(-xpos, -ypos, -zpos);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_CULL_FACE);

	MutexLock(Mutex);

	//Time_renderscene = timer() - Time_renderscene;
	//Time_renderGUI_ = timer();

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, windowwidth, windowheight, 0, -1.0, 1.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	if (World::getblock(RoundInt(xpos), RoundInt(ypos), RoundInt(zpos)) == Blocks::WATER) {
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
		glBindTexture(GL_TEXTURE_2D, BlockTextures);
		double tcX = Textures::getTexcoordX(Blocks::WATER, 1);
		double tcY = Textures::getTexcoordY(Blocks::WATER, 1);
		glBegin(GL_QUADS);
		glTexCoord2d(tcX, tcY + 1 / 8.0); glVertex2i(0, 0);
		glTexCoord2d(tcX, tcY); glVertex2i(0, windowheight);
		glTexCoord2d(tcX + 1 / 8.0, tcY); glVertex2i(windowwidth, windowheight);
		glTexCoord2d(tcX + 1 / 8.0, tcY + 1 / 8.0); glVertex2i(windowwidth, 0);
		glEnd();
	}
	if (GUIrenderswitch) {
		drawGUI();
		drawBag();
	}

	glDisable(GL_TEXTURE_2D);
	if (curtime - screenshotAnimTimer <= 1.0 && !shouldGetScreenshot) {
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
	
	if (shouldGetScreenshot) {
		shouldGetScreenshot = false;
		screenshotAnimTimer = curtime;
		time_t t = time(0);
		char tmp[64];
		tm* timeinfo = new tm;
#ifdef NEWORLD_COMPILE_DISABLE_SECURE
		timeinfo = localtime(&t);
#else
		localtime_s(timeinfo, &t);
#endif
		strftime(tmp, sizeof(tmp), "%Y年%m月%d日%H时%M分%S秒", timeinfo);
		std::stringstream ss;
		ss << "Screenshots/" << tmp << ".bmp";
		saveScreenshot(0, 0, windowwidth, windowheight, ss.str());
	}
	if (shouldGetThumbnail) {
		shouldGetThumbnail = false;
		createThumbnail();
	}

	//屏幕刷新，千万别删，后果自负！！！
	//====refresh====//
	MutexUnlock(Mutex);
	glfwSwapBuffers(MainWindow);
	glfwPollEvents();
	MutexLock(Mutex);
	//==refresh end==//

	lastframe = curtime;
	//Time_screensync = timer() - Time_screensync;

}

void drawBorder(int x, int y, int z) {
	//绘制选择边框，建议用GL_LINE_LOOP，别学我QAQ
	static float extrize = 0.002f; //实际上这个边框应该比方块大一些，否则很难看
	glEnable(GL_LINE_SMOOTH);
	glLineWidth(1.0f);
	glColor3f(0.2f, 0.2f, 0.2f);
	// Top Face
	glBegin(GL_LINES);
	glVertex3f(-(0.5f + extrize) + x, (0.5f + extrize) + y, -(0.5f + extrize) + z);
	glVertex3f(-(0.5f + extrize) + x, (0.5f + extrize) + y, (0.5f + extrize) + z);
	glVertex3f(-(0.5f + extrize) + x, (0.5f + extrize) + y, (0.5f + extrize) + z);
	glVertex3f((0.5f + extrize) + x, (0.5f + extrize) + y, (0.5f + extrize) + z);
	glVertex3f((0.5f + extrize) + x, (0.5f + extrize) + y, (0.5f + extrize) + z);
	glVertex3f((0.5f + extrize) + x, (0.5f + extrize) + y, -(0.5f + extrize) + z);
	glVertex3f(-(0.5f + extrize) + x, (0.5f + extrize) + y, -(0.5f + extrize) + z);
	glVertex3f((0.5f + extrize) + x, (0.5f + extrize) + y, -(0.5f + extrize) + z);
	glEnd();
	// Bottom Face
	glBegin(GL_LINES);
	glVertex3f(-(0.5f + extrize) + x, -(0.5f + extrize) + y, -(0.5f + extrize) + z);
	glVertex3f((0.5f + extrize) + x, -(0.5f + extrize) + y, -(0.5f + extrize) + z);
	glVertex3f((0.5f + extrize) + x, -(0.5f + extrize) + y, (0.5f + extrize) + z);
	glVertex3f((0.5f + extrize) + x, -(0.5f + extrize) + y, -(0.5f + extrize) + z);
	glVertex3f((0.5f + extrize) + x, -(0.5f + extrize) + y, (0.5f + extrize) + z);
	glVertex3f(-(0.5f + extrize) + x, -(0.5f + extrize) + y, (0.5f + extrize) + z);
	glVertex3f(-(0.5f + extrize) + x, -(0.5f + extrize) + y, -(0.5f + extrize) + z);
	glVertex3f(-(0.5f + extrize) + x, -(0.5f + extrize) + y, (0.5f + extrize) + z);
	glEnd();
	// Left Face
	glBegin(GL_LINES);
	glVertex3f((0.5f + extrize) + x, -(0.5f + extrize) + y, -(0.5f + extrize) + z);
	glVertex3f((0.5f + extrize) + x, (0.5f + extrize) + y, -(0.5f + extrize) + z);
	glVertex3f((0.5f + extrize) + x, (0.5f + extrize) + y, -(0.5f + extrize) + z);
	glVertex3f((0.5f + extrize) + x, (0.5f + extrize) + y, (0.5f + extrize) + z);
	glVertex3f((0.5f + extrize) + x, (0.5f + extrize) + y, (0.5f + extrize) + z);
	glVertex3f((0.5f + extrize) + x, -(0.5f + extrize) + y, (0.5f + extrize) + z);
	glVertex3f((0.5f + extrize) + x, -(0.5f + extrize) + y, -(0.5f + extrize) + z);
	glVertex3f((0.5f + extrize) + x, -(0.5f + extrize) + y, (0.5f + extrize) + z);
	glEnd();
	// Right Face
	glBegin(GL_LINES);
	glVertex3f(-(0.5f + extrize) + x, -(0.5f + extrize) + y, -(0.5f + extrize) + z);
	glVertex3f(-(0.5f + extrize) + x, -(0.5f + extrize) + y, (0.5f + extrize) + z);
	glVertex3f(-(0.5f + extrize) + x, -(0.5f + extrize) + y, (0.5f + extrize) + z);
	glVertex3f(-(0.5f + extrize) + x, (0.5f + extrize) + y, (0.5f + extrize) + z);
	glVertex3f(-(0.5f + extrize) + x, (0.5f + extrize) + y, (0.5f + extrize) + z);
	glVertex3f(-(0.5f + extrize) + x, (0.5f + extrize) + y, -(0.5f + extrize) + z);
	glVertex3f(-(0.5f + extrize) + x, -(0.5f + extrize) + y, -(0.5f + extrize) + z);
	glVertex3f(-(0.5f + extrize) + x, (0.5f + extrize) + y, -(0.5f + extrize) + z);
	glEnd();
	// Front Face
	glBegin(GL_LINES);
	glVertex3f(-(0.5f + extrize) + x, -(0.5f + extrize) + y, (0.5f + extrize) + z);
	glVertex3f((0.5f + extrize) + x, -(0.5f + extrize) + y, (0.5f + extrize) + z);
	glVertex3f((0.5f + extrize) + x, -(0.5f + extrize) + y, (0.5f + extrize) + z);
	glVertex3f((0.5f + extrize) + x, (0.5f + extrize) + y, (0.5f + extrize) + z);
	glVertex3f((0.5f + extrize) + x, (0.5f + extrize) + y, (0.5f + extrize) + z);
	glVertex3f(-(0.5f + extrize) + x, (0.5f + extrize) + y, (0.5f + extrize) + z);
	glVertex3f(-(0.5f + extrize) + x, -(0.5f + extrize) + y, (0.5f + extrize) + z);
	glVertex3f(-(0.5f + extrize) + x, (0.5f + extrize) + y, (0.5f + extrize) + z);
	glEnd();
	// Back Face
	glBegin(GL_LINES);
	glVertex3f(-(0.5f + extrize) + x, -(0.5f + extrize) + y, -(0.5f + extrize) + z);
	glVertex3f(-(0.5f + extrize) + x, (0.5f + extrize) + y, -(0.5f + extrize) + z);
	glVertex3f(-(0.5f + extrize) + x, (0.5f + extrize) + y, -(0.5f + extrize) + z);
	glVertex3f((0.5f + extrize) + x, (0.5f + extrize) + y, -(0.5f + extrize) + z);
	glVertex3f((0.5f + extrize) + x, (0.5f + extrize) + y, -(0.5f + extrize) + z);
	glVertex3f((0.5f + extrize) + x, -(0.5f + extrize) + y, -(0.5f + extrize) + z);
	glVertex3f(-(0.5f + extrize) + x, -(0.5f + extrize) + y, -(0.5f + extrize) + z);
	glVertex3f((0.5f + extrize) + x, -(0.5f + extrize) + y, -(0.5f + extrize) + z);
	glEnd();
	glLineWidth(1);
	glDisable(GL_LINE_SMOOTH);
}

void drawGUI(){

	glDepthFunc(GL_ALWAYS);
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_LINE_SMOOTH);
	float seldes_100 = seldes / 100.0f;
	int disti = (int)(seldes_100*linedist);

	if (DebugMode) {

		if (selb != Blocks::AIR) {
			glLineWidth(1);
			glBegin(GL_LINES);
				glColor4f(GUI::FgR, GUI::FgG, GUI::FgB, 0.8f);
				glVertex2i(windowwidth / 2, windowheight / 2);
				glVertex2i(windowwidth / 2 + 50, windowheight / 2 + 50);
				glVertex2i(windowwidth / 2 + 50, windowheight / 2 + 50);
				glVertex2i(windowwidth / 2 + 250, windowheight / 2 + 50);
			glEnd();
			TextRenderer::setFontColor(1.0f, 1.0f, 1.0f, 0.8f);
			glEnable(GL_TEXTURE_2D);
			glDisable(GL_CULL_FACE);
			std::stringstream ss;
			ss << BlockInfo(selb).getBlockName() << " (ID " << (int)selb << ")";
			TextRenderer::renderString(windowwidth / 2 + 50, windowheight / 2 + 50 - 16, ss.str());
			glDisable(GL_TEXTURE_2D);
			glEnable(GL_CULL_FACE);
			glColor4f(0.0f, 0.0f, 0.0f, 0.9f);
		}
		else {
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

	if (seldes>0.0) {

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

	glDisable(GL_CULL_FACE);

	glColor4d(0.8, 0.0, 0.0, 0.3);
	glBegin(GL_QUADS);
	glVertex2d(10, 10);
	glVertex2d(200, 10);
	glVertex2d(200, 30);
	glVertex2d(10, 30);
	glEnd();

	double healthPercent = (double)Player::health / Player::healthMax;
	glColor4d(1.0, 0.0, 0.0, 0.5);
	glBegin(GL_QUADS);
	glVertex2d(20, 15);
	glVertex2d(20 + healthPercent * 170, 15);
	glVertex2d(20 + healthPercent * 170, 25);
	glVertex2d(20, 25);
	glEnd();
	TextRenderer::setFontColor(1.0f, 1.0f, 1.0f, 0.9f);
	if (chatmode) {
		glColor4f(GUI::FgR, GUI::FgG, GUI::FgB, GUI::FgA);
		glDisable(GL_TEXTURE_2D);
		glBegin(GL_QUADS);
		glVertex2i(1, windowheight - 33);
		glVertex2i(windowwidth - 1, windowheight - 33);
		glVertex2i(windowwidth - 1, windowheight - 51);
		glVertex2i(1, windowheight - 51);
		glEnd();
		glEnable(GL_TEXTURE_2D);
		TextRenderer::renderString(0, windowheight - 50, chatword);
	}

	if (DebugMode) {
		std::stringstream ss;
		ss << std::fixed << std::setprecision(4);
		ss << "NEWorld v" << VERSION << " [OpenGL " << GLVersionMajor << "." << GLVersionMinor << "|" << GLVersionRev << "]";
		debugText(ss.str()); ss.str("");
		ss << "Fps:" << fps << "|" << "Ups:" << ups;
		debugText(ss.str()); ss.str("");

		ss << "Flying:" << boolstr(Player::Flying);
		debugText(ss.str()); ss.str("");
		ss << "Debug Mode:" << boolstr(DebugMode);
		debugText(ss.str()); ss.str("");
		ss << "Cross Wall:" << boolstr(Player::CrossWall);
		debugText(ss.str()); ss.str("");
		ss << "Gliding Enabled:" << boolstr(Player::Glide);
		debugText(ss.str()); ss.str("");

		ss << "Xpos:" << Player::xpos;
		debugText(ss.str()); ss.str("");
		ss << "Ypos:" << Player::ypos;
		debugText(ss.str()); ss.str("");
		ss << "Zpos:" << Player::zpos;
		debugText(ss.str()); ss.str("");
		ss << "Direction:" << Player::heading;
		debugText(ss.str()); ss.str("");
		ss << "Head:" << Player::lookupdown;
		debugText(ss.str()); ss.str("");
		ss << "On ground:" << boolstr(Player::OnGround);
		debugText(ss.str()); ss.str("");
		ss << "Jump speed:" << Player::jump;
		debugText(ss.str()); ss.str("");
		ss << "Near wall:" << boolstr(Player::NearWall);
		debugText(ss.str()); ss.str("");
		ss << "In water:" << boolstr(Player::inWater);
		debugText(ss.str()); ss.str("");

		ss << "Gliding:" << boolstr(Player::glidingNow);
		debugText(ss.str()); ss.str("");
		ss << "Energy:" << Player::glidingEnergy;
		debugText(ss.str()); ss.str("");
		ss << "Speed:" << Player::glidingSpeed;
		debugText(ss.str()); ss.str("");

		ss << World::loadedChunks << " chunks loaded";
		debugText(ss.str()); ss.str("");
		ss << displayChunks.size() << " chunks rendered";
		debugText(ss.str()); ss.str("");
		ss << World::unloadedChunks << " chunks unloaded";
		debugText(ss.str()); ss.str("");
		ss << World::updatedChunks << " chunks updated";
		debugText(ss.str()); ss.str("");

		if (multiplayer) {
			MutexLock(Network::mutex);
			ss << Network::getRequestCount() << "/" << networkRequestMax << " network requests";
			debugText(ss.str()); ss.str("");
			MutexUnlock(Network::mutex);
		}

#ifdef NEWORLD_DEBUG_PERFORMANCE_REC
		ss << c_getChunkPtrFromCPA << " CPA requests";
		debugText(ss.str()); ss.str("");
		ss << c_getChunkPtrFromSearch << " search requests";
		debugText(ss.str()); ss.str("");
		ss << c_getHeightFromHMap << " heightmap requests";
		debugText(ss.str()); ss.str("");
		ss << c_getHeightFromWorldGen << " worldgen requests";
		debugText(ss.str()); ss.str("");
#endif
		
		debugText("", true);
	}
	else {

		TextRenderer::setFontColor(1.0f, 1.0f, 1.0f, 0.9f);
		std::stringstream ss;
		ss << "v" << VERSION;
		TextRenderer::renderString(0, 0, ss.str());
		ss.clear();
		ss.str("");
		ss << "Fps:" << fps;
		TextRenderer::renderString(0, 16, ss.str());

	}

	//检测帧速率
	if (timer() - fctime >= 1.0) {
		fps = fpsc;
		fpsc = 0;
		fctime = timer();
	}
	fpsc++;
}

void drawCloud(double px, double pz) {
	//glFogf(GL_FOG_START, 100.0);
	//glFogf(GL_FOG_END, 300.0);
	static double ltimer;
	static bool generated;
	static unsigned int cloudvb[128];
	static int vtxs[128];
	static float f;
	static int l;
	if (ltimer == 0.0) ltimer = timer();
	f += (float)(timer() - ltimer)*0.25f;
	ltimer = timer();
	if (f >= 1.0) {
		l += int(f);
		f -= int(f);
		l %= 128;
	}

	if (!generated) {
		generated = true;
		for (int i = 0; i != 128; i++) {
			for (int j = 0; j != 128; j++) {
				World::cloud[i][j] = int(rnd() * 2);
			}
		}
		glGenBuffersARB(128, cloudvb);
		for (int i = 0; i != 128; i++) {
			Renderer::Init(0, 0);
			for (int j = 0; j != 128; j++) {
				if (World::cloud[i][j]!=0) {
					Renderer::Vertex3d(j*cloudwidth, 128.0, 0.0);
					Renderer::Vertex3d(j*cloudwidth, 128.0, cloudwidth);
					Renderer::Vertex3d((j + 1)*cloudwidth, 128.0, cloudwidth);
					Renderer::Vertex3d((j + 1)*cloudwidth, 128.0, 0.0);
				}
			}
			Renderer::Flush(cloudvb[i], vtxs[i]);
		}
	}

	glDisable(GL_TEXTURE_2D);
	glDisable(GL_CULL_FACE);
	glColor4f(1.0, 1.0, 1.0, 0.5);
	for (int i = 0; i < 128; i++) {
		glPushMatrix();
		glTranslated(-64.0 * cloudwidth - px, 0.0, cloudwidth*((l + i) % 128 + f) - 64.0 * cloudwidth - pz);
		Renderer::renderbuffer(cloudvb[i], vtxs[i], 0, 0);
		glPopMatrix();
	}
	//setupNormalFog();
}

void renderDestroy(float level, int x, int y, int z) {

	//float colors
	static float ES = 0.002f;

	glColor4f(1.0, 1.0, 1.0, 1.0);

	if (level < 100.0) {
		glBindTexture(GL_TEXTURE_2D, DestroyImage[int(level / 10) + 1]);
	}
	else {
		glBindTexture(GL_TEXTURE_2D, DestroyImage[10]);
	}

	glBegin(GL_QUADS);
	glTexCoord2f(0.0f, 0.0f); glVertex3f(-(0.5f + ES) + x, -(0.5f + ES) + y, (0.5f + ES) + z);
	glTexCoord2f(1.0f, 0.0f); glVertex3f((0.5f + ES) + x, -(0.5f + ES) + y, (0.5f + ES) + z);
	glTexCoord2f(1.0f, 1.0f); glVertex3f((0.5f + ES) + x, (0.5f + ES) + y, (0.5f + ES) + z);
	glTexCoord2f(0.0f, 1.0f); glVertex3f(-(0.5f + ES) + x, (0.5f + ES) + y, (0.5f + ES) + z);
	glEnd();

	glBegin(GL_QUADS);
	glTexCoord2f(1.0f, 0.0f); glVertex3f(-(0.5f + ES) + x, -(0.5f + ES) + y, -(0.5f + ES) + z);
	glTexCoord2f(1.0f, 1.0f); glVertex3f(-(0.5f + ES) + x, (0.5f + ES) + y, -(0.5f + ES) + z);
	glTexCoord2f(0.0f, 1.0f); glVertex3f((0.5f + ES) + x, (0.5f + ES) + y, -(0.5f + ES) + z);
	glTexCoord2f(0.0f, 0.0f); glVertex3f((0.5f + ES) + x, -(0.5f + ES) + y, -(0.5f + ES) + z);
	glEnd();

	glBegin(GL_QUADS);
	glTexCoord2f(1.0f, 0.0f); glVertex3f((0.5f + ES) + x, -(0.5f + ES) + y, -(0.5f + ES) + z);
	glTexCoord2f(1.0f, 1.0f); glVertex3f((0.5f + ES) + x, (0.5f + ES) + y, -(0.5f + ES) + z);
	glTexCoord2f(0.0f, 1.0f); glVertex3f((0.5f + ES) + x, (0.5f + ES) + y, (0.5f + ES) + z);
	glTexCoord2f(0.0f, 0.0f); glVertex3f((0.5f + ES) + x, -(0.5f + ES) + y, (0.5f + ES) + z);
	glEnd();

	glBegin(GL_QUADS);
	glTexCoord2f(0.0f, 0.0f); glVertex3f(-(0.5f + ES) + x, -(0.5f + ES) + y, -(0.5f + ES) + z);
	glTexCoord2f(1.0f, 0.0f); glVertex3f(-(0.5f + ES) + x, -(0.5f + ES) + y, (0.5f + ES) + z);
	glTexCoord2f(1.0f, 1.0f); glVertex3f(-(0.5f + ES) + x, (0.5f + ES) + y, (0.5f + ES) + z);
	glTexCoord2f(0.0f, 1.0f); glVertex3f(-(0.5f + ES) + x, (0.5f + ES) + y, -(0.5f + ES) + z);
	glEnd();

	glBegin(GL_QUADS);
	glTexCoord2f(0.0f, 1.0f); glVertex3f(-(0.5f + ES) + x, (0.5f + ES) + y, -(0.5f + ES) + z);
	glTexCoord2f(0.0f, 0.0f); glVertex3f(-(0.5f + ES) + x, (0.5f + ES) + y, (0.5f + ES) + z);
	glTexCoord2f(1.0f, 0.0f); glVertex3f((0.5f + ES) + x, (0.5f + ES) + y, (0.5f + ES) + z);
	glTexCoord2f(1.0f, 1.0f); glVertex3f((0.5f + ES) + x, (0.5f + ES) + y, -(0.5f + ES) + z);
	glEnd();

	glBegin(GL_QUADS);
	glTexCoord2f(1.0f, 1.0f); glVertex3f(-(0.5f + ES) + x, -(0.5f + ES) + y, -(0.5f + ES) + z);
	glTexCoord2f(0.0f, 1.0f); glVertex3f((0.5f + ES) + x, -(0.5f + ES) + y, -(0.5f + ES) + z);
	glTexCoord2f(0.0f, 0.0f); glVertex3f((0.5f + ES) + x, -(0.5f + ES) + y, (0.5f + ES) + z);
	glTexCoord2f(1.0f, 0.0f); glVertex3f(-(0.5f + ES) + x, -(0.5f + ES) + y, (0.5f + ES) + z);
	glEnd();
}

void drawBagRow(int row, int itemid, int xbase, int ybase, int spac, float alpha) {
	//画出背包的一行
	for (int i = 0; i < 10; i++) {
		if (i == itemid) glBindTexture(GL_TEXTURE_2D, tex_select);
		else glBindTexture(GL_TEXTURE_2D, tex_unselect);
		glColor4f(1.0f, 1.0f, 1.0f, alpha);
		glBegin(GL_QUADS);
			glTexCoord2f(0.0, 1.0);glVertex2d(xbase + i * (32 + spac), ybase);
			glTexCoord2f(0.0, 0.0);glVertex2d(xbase + i * (32 + spac) + 32, ybase);
			glTexCoord2f(1.0, 0.0);glVertex2d(xbase + i * (32 + spac) + 32, ybase + 32);
			glTexCoord2f(1.0, 1.0);glVertex2d(xbase + i * (32 + spac), ybase + 32);
		glEnd();
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
		if (Player::inventory[row][i] != Blocks::AIR) {
			glBindTexture(GL_TEXTURE_2D, BlockTextures);
			double tcX = Textures::getTexcoordX(Player::inventory[row][i], 1);
			double tcY = Textures::getTexcoordY(Player::inventory[row][i], 1);
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
			ss << (int)Player::inventoryAmount[row][i];
			TextRenderer::renderString(xbase + i * (32 + spac), ybase, ss.str());
		}
	}
}

void drawBag() {
	//背包界面与更新
	static int si, sj, sf;
	int csi = -1, csj = -1;
	int leftp = (windowwidth - 392) / 2;
	int upp = windowheight - 152 - 16;
	static int mousew, mouseb, mousebl;
	static block indexselected = Blocks::AIR;
	static short Amountselected = 0;
	double curtime = timer();
	double TimeDelta = curtime - bagAnimTimer;
	float bagAnim = (float)(1.0 - pow(0.9, TimeDelta*60.0) + pow(0.9, bagAnimDuration*60.0) / bagAnimDuration * TimeDelta);

	if (bagOpened) {

		glfwSetInputMode(MainWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		mousew = mw;
		mouseb = mb;
		glDepthFunc(GL_ALWAYS);
		glDisable(GL_CULL_FACE);
		glDisable(GL_TEXTURE_2D);

		if (curtime - bagAnimTimer > bagAnimDuration) glColor4f(0.2f, 0.2f, 0.2f, 0.6f);
		else glColor4f(0.2f, 0.2f, 0.2f, 0.6f*bagAnim);
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

		if (curtime - bagAnimTimer > bagAnimDuration) {
			for (int i = 0; i < 4; i++) {
				for (int j = 0; j < 10; j++) {
					if (mx >= j*(32 + 8) + leftp && mx <= j*(32 + 8) + 32 + leftp &&
						my >= i*(32 + 8) + upp && my <= i*(32 + 8) + 32 + upp) {
						csi = si = i; csj = sj = j; sf = 1;
						if (mousebl == 0 && mouseb == 1 && indexselected == Player::inventory[i][j]) {
							if (Player::inventoryAmount[i][j] + Amountselected <= 255) {
								Player::inventoryAmount[i][j] += Amountselected;
								Amountselected = 0;
							}
							else
							{
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
			glBindTexture(GL_TEXTURE_2D, BlockTextures);
			double tcX = Textures::getTexcoordX(indexselected, 1);
			double tcY = Textures::getTexcoordY(indexselected, 1);
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
			ss << Amountselected;
			TextRenderer::renderString((int)mx - 16, (int)my - 16, ss.str());
		}
		if (Player::inventory[si][sj] != 0 && sf == 1) {
			glColor4f(1.0, 1.0, 0.0, 1.0);
			TextRenderer::renderString((int)mx, (int)my - 16, BlockInfo(Player::inventory[si][sj]).getBlockName());
		}

		int xbase = 0, ybase = 0, spac = 0;
		float alpha = 0.5f + 0.5f*bagAnim;
		if (curtime - bagAnimTimer <= bagAnimDuration) {
			xbase = (int)round(((windowwidth - 392) / 2)*bagAnim);
			ybase = (int)round((windowheight - 152 - 16 + 120 - (windowheight - 32))*bagAnim + (windowheight - 32));
			spac = (int)round(8 * bagAnim);
			drawBagRow(3, -1, xbase, ybase, spac, alpha);
			xbase = (int)round(((windowwidth - 392) / 2 - windowwidth)*bagAnim + windowwidth);
			ybase = (int)round((windowheight - 152 - 16 - (windowheight - 32))*bagAnim + (windowheight - 32));
			for (int i = 0; i < 3; i++) {
				glColor4f(1.0f, 1.0f, 1.0f, bagAnim);
				drawBagRow(i, -1, xbase, ybase + i * 40, spac, alpha);
			}
		}

		glEnable(GL_TEXTURE_2D);
		glDisable(GL_CULL_FACE);
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

		mousebl = mouseb;
	}
	else {

		glEnable(GL_TEXTURE_2D);
		glDisable(GL_CULL_FACE);
		if (curtime - bagAnimTimer <= bagAnimDuration) {
			glDisable(GL_TEXTURE_2D);
			glColor4f(0.2f, 0.2f, 0.2f, 0.6f - 0.6f*bagAnim);
			glBegin(GL_QUADS);
			glVertex2i(0, 0);
			glVertex2i(windowwidth, 0);
			glVertex2i(windowwidth, windowheight);
			glVertex2i(0, windowheight);
			glEnd();
			glEnable(GL_TEXTURE_2D);
			int xbase = 0, ybase = 0, spac = 0;
			float alpha = 1.0f - 0.5f*bagAnim;
			xbase = (int)round(((windowwidth - 392) / 2) - ((windowwidth - 392) / 2)*bagAnim);
			ybase = (int)round((windowheight - 152 - 16 + 120 - (windowheight - 32)) - (windowheight - 152 - 16 + 120 - (windowheight - 32))*bagAnim + (windowheight - 32));
			spac = (int)round(8 - 8 * bagAnim);
			glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
			drawBagRow(3, Player::indexInHand, xbase, ybase, spac, alpha);
			xbase = (int)round(((windowwidth - 392) / 2 - windowwidth) - ((windowwidth - 392) / 2 - windowwidth)*bagAnim + windowwidth);
			ybase = (int)round((windowheight - 152 - 16 - (windowheight - 32)) - (windowheight - 152 - 16 - (windowheight - 32))*bagAnim + (windowheight - 32));
			for (int i = 0; i < 3; i++) {
				glColor4f(1.0f, 1.0f, 1.0f, 1.0f - bagAnim);
				drawBagRow(i, -1, xbase, ybase + i * 40, spac, alpha);
			}
		}
		else drawBagRow(3, Player::indexInHand, 0, windowheight - 32, 0, 0.5f);
	}
}
void saveScreenshot(int x, int y, int w, int h, string filename){
	Textures::TEXTURE_RGB scrBuffer;
	int bufw = w, bufh = h;
	while (bufw % 4 != 0){ bufw += 1; }
	while (bufh % 4 != 0){ bufh += 1; }
	scrBuffer.sizeX = bufw;
	scrBuffer.sizeY = bufh;
	scrBuffer.buffer = unique_ptr<ubyte[]>(new byte[bufw*bufh * 3]);
	glReadPixels(x, y, bufw, bufh, GL_RGB , GL_UNSIGNED_BYTE, scrBuffer.buffer.get());
	Textures::SaveRGBImage(filename, scrBuffer);
}

void createThumbnail(){
	std::stringstream ss;
	ss << "Worlds/" << World::worldname << "/Thumbnail.bmp";
	saveScreenshot(0, 0, windowwidth, windowheight, ss.str());
}

template<typename T>
void loadoption(std::map<string, string> &m, char* name, T &value) {
	if (m.find(name) == m.end()) return;
	std::stringstream ss;
	ss << m[name]; ss >> value;
}

void loadoptions() {
	std::map<string, string> options;
	std::ifstream filein("Configs/options.ini", std::ios::in);
	if (!filein.is_open()) return;
	string name, value;
	while (!filein.eof()) {
		filein >> name >> value;
		options[name] = value;
	}
	filein.close();
	loadoption(options, "FOV", FOVyNormal);
	loadoption(options, "RenderDistance", viewdistance);
	loadoption(options, "Sensitivity", mousemove);
	loadoption(options, "CloudWidth", cloudwidth);
	loadoption(options, "SmoothLighting", SmoothLighting);
	loadoption(options, "FancyGrass", NiceGrass);
	loadoption(options, "MergeFaceRendering", MergeFace);
	loadoption(options, "GUIBackgroundBlur", GUIScreenBlur);
	loadoption(options, "ForceUnicodeFont", TextRenderer::useUnicodeASCIIFont);
}

template<typename T>
void saveoption(std::ofstream &out, char* name, T &value) {
	out << string(name) << " " << value << endl;
}

void saveoptions() {
	std::map<string, string> options;
	std::ofstream fileout("Configs/options.ini", std::ios::out);
	if (!fileout.is_open()) return;
	saveoption(fileout, "FOV", FOVyNormal);
	saveoption(fileout, "RenderDistance", viewdistance);
	saveoption(fileout, "Sensitivity", mousemove);
	saveoption(fileout, "CloudWidth", cloudwidth);
	saveoption(fileout, "SmoothLighting", SmoothLighting);
	saveoption(fileout, "FancyGrass", NiceGrass);
	saveoption(fileout, "MergeFaceRendering", MergeFace);
	saveoption(fileout, "GUIBackgroundBlur", GUIScreenBlur);
	saveoption(fileout, "ForceUnicodeFont", TextRenderer::useUnicodeASCIIFont);
	fileout.close();
}