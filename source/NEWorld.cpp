//==============================   Initialize   ================================//
//==============================初始化(包括闪屏)================================//

#include "Definitions.h"

//Functions/Subs define
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
void loadGame();
void saveGame();
ThreadFunc updateThreadFunc(void*);
void drawCloud(double px, double pz);
void updategame();
void debugText(string s, bool init=false);
void drawMain();
void drawBorder(int x,int y,int z);
void renderDestroy(float level,int x,int y,int z);
void drawGUI();
void drawBag();
void saveScreenshot(int x, int y, int w, int h, string filename);
void createThumbnail();

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
#ifdef NEWORLD_USE_WINAPI
	//#include "sockets.h"
	//#include "client.h"
	//#include "server.h"
	//#include "Multiplayer.h"
#endif
#include "Menus.h"
#include "Frustum.h"

struct RenderChunk{
	RenderChunk(world::chunk* c, double TimeDelta){
		cx = c->cx;
		cy = c->cy;
		cz = c->cz;
		memcpy(vbuffers, c->vbuffer, 3 * sizeof(unsigned int));
		memcpy(vtxs, c->vertexes, 3 * sizeof(int));
		loadAnim = c->loadAnim * pow(0.6, TimeDelta);
	}
	int cx, cy, cz;
	unsigned int vbuffers[3];
	int vtxs[3];
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

#if 0
	woca, 这样注释都行？！
	(这儿编译不过去的童鞋，你的FB编译器版本貌似
	和我的不一样，把这几行注释掉吧。。。)
	=======================================
	等等不对啊！！！明明都改成c++了。。。还说是FB。。。
	VC++编译器应该不会在这儿报错吧23333333
#endif

//==============================  Main Program  ================================//
//==============================     主程序     ================================//

int main(){
	//卧槽终于进入main函数了！激动人心的一刻！！！
	
#ifndef NEWORLD_USE_WINAPI
	setlocale(LC_ALL, "zh_CN.UTF-8");
#endif

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
	gamebegin = false;
	glDisable(GL_LINE_SMOOTH);
	mainmenu();
	glEnable(GL_LINE_SMOOTH);
	#if defined(_WIN32)
		//if(world::worldname.substr(0,6)=="client")mpclient=true;
		//if(world::worldname.substr(0,6)=="server")mpserver=true;
	#endif
	glEnable(GL_TEXTURE_2D);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glfwSwapBuffers(MainWindow);
	glfwPollEvents();

	Mutex = MutexCreate();
	MutexLock(Mutex);
	updateThread = ThreadCreate(&updateThreadFunc, NULL);
	
	//初始化游戏状态
	printf("[Console][Game]");
	printf("Init player...\n");
	player::InitHitbox();
	player::xpos = 0.0;
	player::ypos = 60.0;
	player::zpos = 0.0;
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
	glClearColor(skycolorR, skycolorG, skycolorB, 1.0);
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

		drawMain();

		if (glfwGetKey(MainWindow, GLFW_KEY_ESCAPE) == 1){
			createThumbnail();
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
			world::destroyAllChunks();
			printf("[Console][Game]");
			printf("Threads terminated\n");
			printf("[Console][Game]");
			printf("Returned to main menu\n");
			escp = true;
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
	//游戏更新线程函数

	//Wait until start...
	MutexLock(Mutex);
	while (!updateThreadRun){
		MutexUnlock(Mutex);
		MutexLock(Mutex);
	}
	MutexUnlock(Mutex);

	//Thread start
	MutexLock(Mutex);
	lastupdate = timer();

	while (updateThreadRun){

		MutexUnlock(Mutex);
		MutexLock(Mutex);

		while (updateThreadPaused){
			MutexUnlock(Mutex);
			MutexLock(Mutex);
			lastupdate = updateTimer = timer();
		}
		
		FirstUpdateThisFrame = true;
		updateTimer = timer();
		if (updateTimer - lastupdate >= 5.0) lastupdate = updateTimer;

		while ((updateTimer - lastupdate) >= 1.0 / 30.0 && upsc < 60){
			lastupdate += 1.0 / 30.0;
			upsc++;
			glfwPollEvents();
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
	if (width<650)width = 650;
	if (height<400)height = 400;
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

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0.0, windowwidth, windowheight, 0.0, -1.0, 1.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	TextureID splTex = Textures::LoadRGBTexture("Textures\\GUI\\SplashScreen.bmp");

	glClearColor(0.0, 0.0, 0.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_TEXTURE_2D);
	for (int i = 0; i < 256; i += 8){
		glfwSwapBuffers(MainWindow);
		glfwPollEvents();
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glLoadIdentity();
		glBindTexture(GL_TEXTURE_2D, splTex);

		glColor4f((float)i / 256, (float)i / 256, (float)i / 256, 1.0);
		glBegin(GL_QUADS);
		glTexCoord2f(0.0, 1.0); glVertex2i(0, 0);
		glTexCoord2f(850.0f / 1024.0f, 1.0); glVertex2i(windowwidth, 0);
		glTexCoord2f(850.0f / 1024.0f, 1.0 - 480.0f / 1024.0f); glVertex2i(windowwidth, windowheight);
		glTexCoord2f(0.0, 1.0 - 480.0f / 1024.0f); glVertex2i(0, windowheight);
		glEnd();
		TextRenderer::setFontColor(0.0, 0.0, 0.0, 1.0);
		std::stringstream ss;
		ss << MAJOR_VERSION << MINOR_VERSION << " [v" << VERSION << "]";
		TextRenderer::renderString(400, 200, ss.str().c_str());
		TextRenderer::renderString(10, 10, "Loading Textures...");
		TextRenderer::renderString(10, 26, "Please Wait...");
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
	glClearColor(1.0, 1.0, 1.0, 1.0);
	glClearDepth(1.0);
	glEnable(GL_CULL_FACE);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_ALPHA_TEST);
	glEnable(GL_BLEND);
	glEnable(GL_LINE_SMOOTH);
	glDepthFunc(GL_LEQUAL);
	glAlphaFunc(GL_GREATER, 0.0); //<--这家伙在卖萌？(往后面看看，卖萌的多着呢)
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	glHint(GL_FOG_HINT, GL_NICEST);
	glHint(GL_LINE_SMOOTH_HINT, GL_FASTEST);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glColor4f(0.0, 0.0, 0.0, 1.0);
	TextRenderer::BuildFont(windowwidth, windowheight);
	TextRenderer::setFontColor(1.0, 1.0, 1.0, 1.0);
	glClearColor(skycolorR, skycolorG, skycolorB, 1.0);

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
}

void setupNormalFog() {

	glEnable(GL_FOG);
	int fogMode[3] = { GL_EXP, GL_EXP2, GL_LINEAR };
	float fogColor[4] = { skycolorR, skycolorG, skycolorB, 1.0 };
	int fogfilter = 2;

	glFogi(GL_FOG_MODE, fogMode[fogfilter]);
	glFogfv(GL_FOG_COLOR, fogColor);
	//glFogf GL_FOG_DENSITY, 1.0
	glFogf(GL_FOG_START, viewdistance * 16.0f - 32.0f);
	glFogf(GL_FOG_END, viewdistance * 16.0f);


}

void LoadTextures(){
	//载入纹理
	Textures::Init();
	
	guiImage[1] = Textures::LoadRGBATexture("textures\\gui\\MainMenu.bmp", "");
	guiImage[2] = Textures::LoadRGBATexture("textures\\gui\\select.bmp", "");
	guiImage[3] = Textures::LoadRGBATexture("textures\\gui\\unselect.bmp", "");
	guiImage[4] = Textures::LoadRGBATexture("textures\\gui\\title.bmp", "textures\\gui\\titlemask.bmp");
	guiImage[5] = Textures::LoadRGBATexture("textures\\gui\\lives.bmp", "");

	for (int gloop = 1; gloop <= 10; gloop++){
		string path = "textures\\blocks\\destroy_" + itos(gloop) + ".bmp";
		DestroyImage[gloop] = Textures::LoadRGBATexture(path, path);
	}

	BlockTextures = Textures::LoadRGBATexture("textures\\blocks\\Terrain.bmp", "textures\\blocks\\Terrainmask.bmp");
	
}

void saveGame(){
	world::saveAllChunks();
	player::save(world::worldname);
}

void loadGame(){
	player::load(world::worldname);
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

void updategame(){
	//Time_updategame_ = timer();

	static double Wprstm;
	static bool WP;
	static double mxl, myl;
	glfwGetCursorPos(MainWindow, &mx, &my);

	player::BlockInHand = player::inventorybox[3][player::itemInHand];
	
	//world::unloadedChunks=0
	world::rebuiltChunks = 0;
	world::updatedChunks = 0;

	//ciArray move
	if (UseCIArray && world::ciArrayAval){
		if (world::ciArray.originX != player::cxt - viewdistance - 2 || world::ciArray.originY != player::cyt - viewdistance - 2 || world::ciArray.originZ != player::czt - viewdistance - 2){
			world::ciArray.moveTo(player::cxt - viewdistance - 2, player::cyt - viewdistance - 2, player::czt - viewdistance - 2);
		}
	}
	//HeightMap move
	if (world::HMap.originX != (player::cxt - viewdistance - 2) * 16 || world::HMap.originZ != (player::czt - viewdistance - 2) * 16){
		world::HMap.moveTo((player::cxt - viewdistance - 2) * 16, (player::czt - viewdistance - 2) * 16);
	}

	if (FirstUpdateThisFrame){
		world::sortChunkLoadUnloadList(RoundInt(player::xpos), RoundInt(player::ypos), RoundInt(player::zpos));

		//卸载区块(Unload chunks)
		int sumUnload;
		sumUnload = world::chunkUnloads > 4 ? 4 : world::chunkUnloads;
		for (int i = 0; i < sumUnload; i++) {
			int cx = world::chunkUnloadList[i][1];
			int cy = world::chunkUnloadList[i][2];
			int cz = world::chunkUnloadList[i][3];
			int ci = world::getChunkIndex(cx, cy, cz);
			world::chunks[ci].Unload();
			world::DeleteChunk(ci);
		}

		//加载区块(Load chunks)
		int sumLoad;
		sumLoad = world::chunkLoads > 4 ? 4 : world::chunkLoads;
		for (int i = 0; i < sumLoad; i++){
			int cx = world::chunkLoadList[i][1];
			int cy = world::chunkLoadList[i][2];
			int cz = world::chunkLoadList[i][3];
			world::AddChunk(cx, cy, cz)->Load();
		}
		
	}
	
	//加载动画
	for (int i = 0; i < world::loadedChunks; i++){
		world::chunk* cp = &world::chunks[i];
		if (cp->loadAnim <= 0.3f)
			cp->loadAnim = 0.0f;
		else
			cp->loadAnim *= 0.6f;
	}

	//随机状态更新
	for (int i = 0; i < world::loadedChunks; i++){
		int x, y, z;
		int cx = world::chunks[i].cx;
		int cy = world::chunks[i].cy;
		int cz = world::chunks[i].cz;
		x = int(rnd() * 16);
		y = int(rnd() * 16);
		z = int(rnd() * 16);
		if (world::chunks[i].getblock(x, y, z) == blocks::DIRT &&
			world::getblock(x + cx * 16, y + cy * 16 + 1, z + cz * 16) == blocks::AIR &&
			(world::getblock(x + cx * 16 + 1, y + cy * 16, z + cz * 16) == blocks::GRASS ||
			world::getblock(x + cx * 16 - 1, y + cy * 16, z + cz * 16) == blocks::GRASS ||
			world::getblock(x + cx * 16, y + cy * 16, z + cz * 16 + 1) == blocks::GRASS ||
			world::getblock(x + cx * 16, y + cy * 16, z + cz * 16 - 1) == blocks::GRASS ||
			world::getblock(x + cx * 16, y + cy * 16 + 1, z + cz * 16 - 1) == blocks::GRASS ||
			world::getblock(x + cx * 16, y + cy * 16 + 1, z + cz * 16 - 1) == blocks::GRASS ||
			world::getblock(x + cx * 16, y + cy * 16 + 1, z + cz * 16 - 1) == blocks::GRASS ||
			world::getblock(x + cx * 16, y + cy * 16 + 1, z + cz * 16 - 1) == blocks::GRASS ||
			world::getblock(x + cx * 16, y + cy * 16 - 1, z + cz * 16 - 1) == blocks::GRASS ||
			world::getblock(x + cx * 16, y + cy * 16 - 1, z + cz * 16 - 1) == blocks::GRASS ||
			world::getblock(x + cx * 16, y + cy * 16 - 1, z + cz * 16 - 1) == blocks::GRASS ||
			world::getblock(x + cx * 16, y + cy * 16 - 1, z + cz * 16 - 1) == blocks::GRASS)){
			world::chunks[i].setblock(x, y, z, blocks::GRASS);
			world::updateblock(x + cx * 16, y + cy * 16 + 1, z + cz * 16, true);
			world::setChunkUpdated(cx, cy, cz, true);
		}
	}
	
	//判断选中的方块
	double lx, ly, lz, sidedist[7];
	int sidedistmin;
	lx = player::xpos; ly = player::ypos + player::height + player::heightExt; lz = player::zpos;

	selx = 0;
	sely = 0;
	selz = 0;
	selbx = 0;
	selby = 0;
	selbz = 0;
	selcx = 0;
	selcy = 0;
	selcz = 0;
	selb = 0;
	selbr = 0;
	bool puted = false;     //标准的chinglish吧。。。主要是put已经被FB作为关键字了。。           --等等不对啊！这已经是c++了！！！
	
	if (!bagOpened) {
		//从玩家位置发射一条线段
		for (int i = 0; i < selectPrecision*selectDistance; i++) {
			//线段延伸
			lx += sin(M_PI / 180 * (player::heading - 180))*sin(M_PI / 180 * (player::lookupdown + 90)) / (double)selectPrecision;
			ly += cos(M_PI / 180 * (player::lookupdown + 90)) / (double)selectPrecision;
			lz += cos(M_PI / 180 * (player::heading - 180))*sin(M_PI / 180 * (player::lookupdown + 90)) / (double)selectPrecision;

			//碰到方块
			if (BlockInfo(world::getblock(RoundInt(lx), RoundInt(ly), RoundInt(lz))).isSolid()) {
				int x, y, z;
				x = RoundInt(lx);
				y = RoundInt(ly);
				z = RoundInt(lz);

				selx = x; sely = y; selz = z;

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

				if (chunkOutOfBound(selcx, selcy, selcz) == false) {
					selb = world::getChunkPtr(selcx, selcy, selcz)->getblock(selbx, selby, selbz);
				}

				switch (sidedistmin) {
				case 1:
					if (chunkOutOfBound(selcx, selcy, selcz) == false)
						selbr = world::getbrightness(selx, sely + 1, selz);
					break;
				case 2:
					if (chunkOutOfBound(selcx, selcy, selcz) == false)
						selbr = world::getbrightness(selx, sely - 1, selz);
					break;
				case 3:
					if (chunkOutOfBound(selcx, selcy, selcz) == false)
						selbr = world::getbrightness(selx + 1, sely, selz);
					break;
				case 4:
					if (chunkOutOfBound(selcx, selcy, selcz) == false)
						selbr = world::getbrightness(selx - 1, sely, selz);
					break;
				case 5:
					if (chunkOutOfBound(selcx, selcy, selcz) == false)
						selbr = world::getbrightness(selx, sely, selz + 1);
					break;
				case 6:
					if (chunkOutOfBound(selcx, selcy, selcz) == false)
						selbr = world::getbrightness(selx, sely, selz - 1);
					break;
				}

				if (mb == 1 || glfwGetKey(MainWindow, GLFW_KEY_ENTER) == GLFW_PRESS) {
					particles::throwParticle(world::getblock(x, y, z),
						float(x + rnd() - 0.5f), float(y + rnd() - 0.2f), float(z + rnd() - 0.5f),
						float(rnd()*0.2f - 0.1f), float(rnd()*0.2f - 0.1f), float(rnd()*0.2f - 0.1f),
						float(rnd()*0.01f + 0.02f), int(rnd() * 30) + 30);

					if (selx != oldselx || sely != oldsely || selz != oldselz)
						seldes = 0.0;
					else
						seldes += 5.0;
					if (seldes >= 100.0) {
						player::additem(world::getblock(x, y, z));
						for (int j = 1; j <= 25; j++) {
							particles::throwParticle(world::getblock(x, y, z),
								float(x + rnd() - 0.5f), float(y + rnd() - 0.2f), float(z + rnd() - 0.5f),
								float(rnd()*0.2f - 0.1f), float(rnd()*0.2f - 0.1f), float(rnd()*0.2f - 0.1f),
								float(rnd()*0.02 + 0.03), int(rnd() * 60) + 30);
						}
						world::pickblock(x, y, z);
					}
				}
				//放置方块
				if ((mb == 2 && mbp == false) || isPressed(GLFW_KEY_TAB) && player::inventorypcs[3][player::itemInHand] > 0) {
					puted = true;
					switch (sidedistmin) {
					case 1:
						if (player::putblock(x, y + 1, z, player::BlockInHand) == false) puted = false;
						break;
					case 2:
						if (player::putblock(x, y - 1, z, player::BlockInHand) == false) puted = false;
						break;
					case 3:
						if (player::putblock(x + 1, y, z, player::BlockInHand) == false) puted = false;
						break;
					case 4:
						if (player::putblock(x - 1, y, z, player::BlockInHand) == false) puted = false;
						break;
					case 5:
						if (player::putblock(x, y, z + 1, player::BlockInHand) == false) puted = false;
						break;
					case 6:
						if (player::putblock(x, y, z - 1, player::BlockInHand) == false) puted = false;
						break;
					}
					if (puted) {
						player::inventorypcs[3][player::itemInHand]--;
						if (player::inventorypcs[3][player::itemInHand] == 0) player::inventorypcs[3][player::itemInHand] = blocks::AIR;
					}
				}
				break;
			}
		}

		if (selx != oldselx || sely != oldsely || selz != oldselz || (mb == 0 && glfwGetKey(MainWindow, GLFW_KEY_ENTER) != GLFW_PRESS)) seldes = 0.0;
		oldselx = selx;
		oldsely = sely;
		oldselz = selz;
		
		player::intxpos = RoundInt(player::xpos);
		player::intypos = RoundInt(player::ypos);
		player::intzpos = RoundInt(player::zpos);

		//转头！你治好了我多年的颈椎病！
		player::xlookspeed = player::ylookspeed = 0.0;
		if (mx != mxl)player::xlookspeed -= (mx - mxl)*mousemove;
		if (my != myl)player::ylookspeed += (my - myl)*mousemove;
		if (glfwGetKey(MainWindow, GLFW_KEY_RIGHT) == 1) {
			player::xlookspeed -= mousemove * 4;
		}
		if (glfwGetKey(MainWindow, GLFW_KEY_LEFT) == 1) {
			player::xlookspeed += mousemove * 4;
		}
		if (glfwGetKey(MainWindow, GLFW_KEY_UP) == 1) {
			player::ylookspeed -= mousemove * 4;
		}
		if (glfwGetKey(MainWindow, GLFW_KEY_DOWN) == 1) {
			player::ylookspeed += mousemove * 4;
		}
		player::heading += player::xlookspeed;
		player::lookupdown += player::ylookspeed;

		//限制角度，别把头转掉下来了 ←_←
		if (player::lookupdown < -90){
			player::lookupdown = -90;
			player::ylookspeed = 0.0;
		}
		if (player::lookupdown > 90){
			player::lookupdown = 90;
			player::ylookspeed = 0.0;
		}
		mxl = mx; myl = my;

		//移动！(生命在于运动)
		if (glfwGetKey(MainWindow, GLFW_KEY_W) == 1) {
			if (!WP) {
				if (Wprstm == 0.0) {
					Wprstm = timer();
				}
				else {
					if (timer() - Wprstm <= 0.5) { player::Running = true; Wprstm = 0.0; }
					else Wprstm = timer();
				}
			}
			if (Wprstm != 0.0 && timer() - Wprstm > 0.5) Wprstm = 0.0;
			WP = true;
			player::xa = -sin(player::heading*M_PI / 180.0) * player::speed;
			player::za = -cos(player::heading*M_PI / 180.0) * player::speed;
		}
		else {
			player::Running = false;
			WP = false;
		}
		if (player::Running)player::speed = runspeed;
		else player::speed = walkspeed;

		if (glfwGetKey(MainWindow, GLFW_KEY_S) == GLFW_PRESS) {
			player::xa = sin(player::heading*M_PI / 180.0) * player::speed;
			player::za = cos(player::heading*M_PI / 180.0) * player::speed;
			Wprstm = 0.0;
		}

		if (glfwGetKey(MainWindow, GLFW_KEY_A) == GLFW_PRESS) {
			player::xa = sin((player::heading - 90)*M_PI / 180.0) * player::speed;
			player::za = cos((player::heading - 90)*M_PI / 180.0) * player::speed;
			Wprstm = 0.0;
		}

		if (glfwGetKey(MainWindow, GLFW_KEY_D) == GLFW_PRESS) {
			player::xa = -sin((player::heading - 90)*M_PI / 180.0) * player::speed;
			player::za = -cos((player::heading - 90)*M_PI / 180.0) * player::speed;
			Wprstm = 0.0;
		}

		if (glfwGetKey(MainWindow, GLFW_KEY_R) == GLFW_PRESS) {
			if (glfwGetKey(MainWindow, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
				player::xa = -sin(player::heading*M_PI / 180.0) * runspeed * 10;
				player::za = -cos(player::heading*M_PI / 180.0) * runspeed * 10;
			}
			else {
				player::xa = sin(M_PI / 180 * (player::heading - 180))*sin(M_PI / 180 * (player::lookupdown + 90)) * runspeed * 20;
				player::ya = cos(M_PI / 180 * (player::lookupdown + 90)) * runspeed * 20;
				player::za = cos(M_PI / 180 * (player::heading - 180))*sin(M_PI / 180 * (player::lookupdown + 90)) * runspeed * 20;
			}
		}

		if (glfwGetKey(MainWindow, GLFW_KEY_F) == GLFW_PRESS) {
			player::xa = sin(player::heading*M_PI / 180.0) * runspeed * 10;
			player::za = cos(player::heading*M_PI / 180.0) * runspeed * 10;
		}

		//切换方块
		if (isPressed(GLFW_KEY_Z) && player::itemInHand > 0) player::itemInHand--;
		if (isPressed(GLFW_KEY_X) && player::itemInHand < 9) player::itemInHand++;
		mwl = mw;

		//起跳！
		if (isPressed(GLFW_KEY_SPACE)){
			if (!player::inWater) {
				if ((player::OnGround || player::AirJumps < MaxAirJumps) && FLY == false && CROSS == false){
					if (player::OnGround == false) player::AirJumps++;
					player::jump = 0.25; player::OnGround = false;
				}
				if (FLY || CROSS) { player::ya += walkspeed; isPressed(GLFW_KEY_SPACE, true); }
				Wprstm = 0.0;
			}
			else{
				player::ya = walkspeed;
				isPressed(GLFW_KEY_SPACE, true);
			}
		}

		if (glfwGetKey(MainWindow, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(MainWindow, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS){
			if (CROSS || FLY){
				player::ya -= walkspeed;
			}
			else{
				if (player::heightExt > -0.59f)  player::heightExt -= 0.1f; else player::heightExt = -0.6f;
			}
			Wprstm = 0.0;
		}

		//各种设置切换

		if (isPressed(GLFW_KEY_F1)){
			FLY = !FLY;
			player::jump = 0.0;
		}

		if (isPressed(GLFW_KEY_F2)) shouldGetScreenshot = true;
		if (isPressed(GLFW_KEY_F3)) DebugMode = !DebugMode;
		if (isPressed(GLFW_KEY_F3) && glfwGetKey(MainWindow, GLFW_KEY_H) == GLFW_PRESS){
			DebugHitbox = !DebugHitbox;
			DebugMode = true;
		}
		if (isPressed(GLFW_KEY_F4) == GLFW_PRESS) CROSS = !CROSS;
		if (isPressed(GLFW_KEY_F5) == GLFW_PRESS) GUIrenderswitch = !GUIrenderswitch;

	}
	
	if (glfwGetKey(MainWindow, GLFW_KEY_E) == GLFW_PRESS && ep == false){
		bagOpened = !bagOpened;
		if (!bagOpened){
			glfwSetInputMode(MainWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		}
		else{
			shouldGetThumbnail = true;
		}
		ep = true;
	}
	if (glfwGetKey(MainWindow, GLFW_KEY_E) != GLFW_PRESS) ep = false;

	if (!bagOpened){
		if (glfwGetKey(MainWindow, GLFW_KEY_ESCAPE) != GLFW_PRESS) escp = false;
		if (isPressed(GLFW_KEY_L)) world::saveAllChunks();
	}

	//跳跃
	if (!player::inWater){
		if (FLY == false && CROSS == false){
			player::ya = -0.001;
			if (player::OnGround){
				player::jump = 0.0;
				player::AirJumps = 0;
				isPressed(GLFW_KEY_SPACE, true);
			}
			else{
				player::jump -= 0.02;
				player::ya = player::jump;
			}
		}
		else{
			player::jump = 0.0;
			player::AirJumps = 0;
		}
	}
	else{
		player::jump = 0.0;
		player::AirJumps = 0;
		isPressed(GLFW_KEY_SPACE, true);
		if (player::ya <= 0.001){
			player::ya = -0.001;
			if (!player::OnGround) player::ya = -0.1;
		}

	}

	//if (player::NearWall && FLY == false && CROSS == false){
	//	player::ya += walkspeed
	//	player::jump = 0.0
	//  //爬墙
	//}

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
	//cout << lastupdate << "," << std::setprecision(20) << timer() << endl;

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

void drawMain() {
	//画场景
	//	Time_renderscene_ = timer();

	double curtime = timer();
	double TimeDelta;
	double xpos, ypos, zpos;
	int renderedChunk = 0;

	if (player::Running){
		if (FOVyExt < 9.8){
			TimeDelta = curtime - SpeedupAnimTimer;
			FOVyExt = 10.0f - (10.0f - FOVyExt)*(float)pow(0.8, TimeDelta * 30);
			SpeedupAnimTimer = curtime;
		}
		else FOVyExt = 10.0;
	}
	else{
		if (FOVyExt>0.2){
			TimeDelta = curtime - SpeedupAnimTimer;
			FOVyExt *= (float)pow(0.8, TimeDelta * 30);
			SpeedupAnimTimer = curtime;
		}
		else FOVyExt = 0.0;
	}
	SpeedupAnimTimer = curtime;

	if (player::OnGround){
		//半蹲特效
		if (player::jump < -0.005){
			if (player::jump <= -(player::height - 0.5f))
				player::heightExt = -(player::height - 0.5f);
			else
				player::heightExt = (float)player::jump;
			TouchdownAnimTimer = curtime;
		}
		else{
			if (player::heightExt <= -0.005){
				player::heightExt *= (float)pow(0.8, (curtime - TouchdownAnimTimer) * 30);
				TouchdownAnimTimer = curtime;
			}
		}
	}
	
	xpos = player::xpos - player::xd + (curtime - lastupdate) * 30.0 * player::xd;
	ypos = player::ypos + player::height + player::heightExt - player::yd + (curtime - lastupdate) * 30.0 * player::yd;
	zpos = player::zpos - player::zd + (curtime - lastupdate) * 30.0 * player::zd;

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(FOVyNormal + FOVyExt, windowwidth / (double)windowheight, 0.1, 512.0);
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

	//更新区块显示列表
	world::sortChunkBuildRenderList(RoundInt(player::xpos), RoundInt(player::ypos), RoundInt(player::zpos));
	int brl = world::chunkBuildRenders > 4 ? 4 : world::chunkBuildRenders;
	for (int i = 0; i < brl; i++) {
		int ci = world::chunkBuildRenderList[i][1];
		world::chunks[ci].buildRender();
	}

	//删除已卸载区块的VBO
	if (world::vbuffersShouldDelete.size() > 0){
		glDeleteBuffersARB(world::vbuffersShouldDelete.size(), &*world::vbuffersShouldDelete.begin());
		world::vbuffersShouldDelete.clear();
	}
	
	double plookupdown = player::lookupdown - player::ylookspeed + (curtime - lastupdate) * 30.0 * player::ylookspeed;
	double pheading = player::heading - player::xlookspeed + (curtime - lastupdate) * 30.0 * player::xlookspeed;

	glLoadIdentity();
	glRotated(plookupdown, 1, 0, 0);
	glRotated(360.0 - pheading, 0, 1, 0);
	Frustum::calc();
	
	displayChunks.clear();
	for (int i = 0; i < world::loadedChunks; i++) {
		if (world::chunks[i].Empty)continue;
		if (!world::chunks[i].renderBuilt)continue;
		if (world::chunkInRange(world::chunks[i].cx, world::chunks[i].cy, world::chunks[i].cz,
			player::cxt, player::cyt, player::czt, viewdistance)) {
			if (Frustum::AABBInFrustum(world::chunks[i].getRelativeAABB(xpos, ypos, zpos))) {
				displayChunks.push_back(RenderChunk(&world::chunks[i], (curtime - lastupdate) * 30.0));
			}
		}
	}

	MutexUnlock(Mutex);
	renderedChunk = displayChunks.size();
	glBindTexture(GL_TEXTURE_2D, BlockTextures);
	glEnableClientState(GL_COLOR_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnableClientState(GL_VERTEX_ARRAY);
	for (int i = 0; i < renderedChunk; i++) {
		RenderChunk cr = displayChunks[i];
		if (cr.vtxs[0] == 0) continue;
		glPushMatrix();
		glTranslated(cr.cx * 16.0 - xpos, cr.cy * 16.0 - cr.loadAnim - ypos, cr.cz * 16.0 - zpos);
		renderer::renderbuffer(cr.vbuffers[0], cr.vtxs[0], true, true);
		glPopMatrix();
	}

	glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
	glDisableClientState(GL_COLOR_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_VERTEX_ARRAY);

	MutexLock(Mutex);

	if (seldes > 0.0) {
		glTranslated(selx - xpos, sely - ypos, selz - zpos);
		renderDestroy(seldes, 0, 0, 0);
		glTranslated(-selx + xpos, -sely + ypos, -selz + zpos);
	}
	glBindTexture(GL_TEXTURE_2D, BlockTextures);
	particles::renderall();

	glDisable(GL_TEXTURE_2D);
	if (GUIrenderswitch){
		glTranslated(selx - xpos, sely - ypos, selz - zpos);
		drawBorder(0, 0, 0);
		glTranslated(-selx + xpos, -sely + ypos, -selz + zpos);
	}

	/*
	Hitbox::AABB t;
	t.xmin = world::ciArray.originX * 16;
	t.xmax = world::ciArray.originX * 16 + world::ciArray.size * 16;
	t.ymin = world::ciArray.originY * 16;
	t.ymax = world::ciArray.originY * 16 + world::ciArray.size * 16;
	t.zmin = world::ciArray.originZ * 16;
	t.zmax = world::ciArray.originZ * 16 + world::ciArray.size * 16;

	Hitbox::AABB t2;
	t2.xmin = world::HMap.originX;
	t2.xmax = world::HMap.originX + world::HMap.size;
	t2.ymin = 36.0;
	t2.ymax = 36.0;
	t2.zmin = world::HMap.originZ;
	t2.zmax = world::HMap.originZ + world::HMap.size;
	*/

	MutexUnlock(Mutex);

	glLoadIdentity();
	glRotated(plookupdown, 1, 0, 0);
	glRotated(360.0 - pheading, 0, 1, 0);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_CULL_FACE);

	glEnableClientState(GL_COLOR_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnableClientState(GL_VERTEX_ARRAY);

	for (int i = 0; i < renderedChunk; i++) {
		RenderChunk cr = displayChunks[i];
		if (cr.vtxs[1] == 0) continue;
		glPushMatrix();
		glTranslated(cr.cx * 16.0 - xpos, cr.cy * 16.0 - cr.loadAnim - ypos, cr.cz * 16.0 - zpos);
		renderer::renderbuffer(cr.vbuffers[1], cr.vtxs[1], true, true);
		glPopMatrix();
	}
	glDisable(GL_CULL_FACE);
	for (int i = 0; i < renderedChunk; i++) {
		RenderChunk cr = displayChunks[i];
		if (cr.vtxs[2] == 0) continue;
		glPushMatrix();
		glTranslated(cr.cx * 16.0 - xpos, cr.cy * 16.0 - cr.loadAnim - ypos, cr.cz * 16.0 - zpos);
		renderer::renderbuffer(cr.vbuffers[2], cr.vtxs[2], true, true);
		glPopMatrix();
	}

	glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
	glDisableClientState(GL_COLOR_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_VERTEX_ARRAY);

	glLoadIdentity();
	glRotated(player::lookupdown, 1, 0, 0);
	glRotated(360.0 - player::heading, 0, 1, 0);
	glTranslated(-xpos, -ypos, -zpos);
	//glDisable(GL_TEXTURE_2D);
	//glEnableClientState(GL_VERTEX_ARRAY);
	//drawCloud(player::xpos, player::zpos);
	//glDisableClientState(GL_VERTEX_ARRAY);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_CULL_FACE);
	
	MutexLock(Mutex);

	//Time_renderscene = timer() - Time_renderscene;
	//Time_renderGUI_ = timer();

	if (GUIrenderswitch)drawGUI();
	else {
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0, windowwidth, windowheight, 0, -1.0, 1.0);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
	}
	if (world::getblock(int(player::xpos + 0.5), int(player::ypos + player::height + player::heightExt + 0.5), int(player::zpos + 0.5)) == blocks::WATER) {
		glColor4f(1.0, 1.0, 1.0, 1.0);
		glBindTexture(GL_TEXTURE_2D,BlockTextures);
		double tcX = Textures::getTexcoordX(blocks::WATER, 1);
		double tcY = Textures::getTexcoordY(blocks::WATER, 1);
		glBegin(GL_QUADS);
		glTexCoord2d(tcX, tcY + 1 / 8.0); glVertex2i(0, 0);
		glTexCoord2d(tcX, tcY); glVertex2i(0, windowheight);
		glTexCoord2d(tcX + 1 / 8.0, tcY); glVertex2i(windowwidth, windowheight);
		glTexCoord2d(tcX + 1 / 8.0, tcY + 1 / 8.0); glVertex2i(windowwidth, 0);
		glEnd();
	}

	if (bagOpened)drawBag();
	if (shouldGetScreenshot) {
		shouldGetScreenshot = false;
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
		ss << "\\screenshots\\" << tmp << ".bmp";
		saveScreenshot(0, 0, windowwidth, windowheight, ss.str());
	}
	if (shouldGetThumbnail) {
		shouldGetThumbnail = false;
		createThumbnail();
	}

	//屏幕刷新，千万别删，后果自负！！！
	//====refresh====//
	MutexUnlock(Mutex);
	glFinish();
	glfwSwapBuffers(MainWindow);
	glfwPollEvents();
	MutexLock(Mutex);
	//==refresh end==//

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

void drawGUI() {
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, windowwidth, windowheight, 0, -1.0, 1.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glDepthFunc(GL_ALWAYS);
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_LINE_SMOOTH);
	int seldes_100 = int(seldes / 100);

	if (DebugMode) {

		if (selb != blocks::AIR) {

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

		glVertex2i(windowwidth / 2 - linedist + seldes_100 * linedist, windowheight / 2 - linedist + seldes_100 * linedist);
		glVertex2i(windowwidth / 2 - linedist + seldes_100 * linedist, windowheight / 2 - linedist + linelength + seldes_100 * linedist);
		glVertex2i(windowwidth / 2 - linedist + seldes_100 * linedist, windowheight / 2 - linedist + seldes_100 * linedist);
		glVertex2i(windowwidth / 2 - linedist + linelength + seldes_100 * linedist, windowheight / 2 - linedist + seldes_100 * linedist);

		glVertex2i(windowwidth / 2 + linedist - seldes_100 * linedist, windowheight / 2 - linedist + seldes_100 * linedist);
		glVertex2i(windowwidth / 2 + linedist - seldes_100 * linedist, windowheight / 2 - linedist + linelength + seldes_100 * linedist);
		glVertex2i(windowwidth / 2 + linedist - seldes_100 * linedist, windowheight / 2 - linedist + seldes_100 * linedist);
		glVertex2i(windowwidth / 2 + linedist - linelength - seldes_100 * linedist, windowheight / 2 - linedist + seldes_100 * linedist);

		glVertex2i(windowwidth / 2 - linedist + seldes_100 * linedist, windowheight / 2 + linedist - seldes_100 * linedist);
		glVertex2i(windowwidth / 2 - linedist + seldes_100 * linedist, windowheight / 2 + linedist - linelength - seldes_100 * linedist);
		glVertex2i(windowwidth / 2 - linedist + seldes_100 * linedist, windowheight / 2 + linedist - seldes_100 * linedist);
		glVertex2i(windowwidth / 2 - linedist + linelength + seldes_100 * linedist, windowheight / 2 + linedist - seldes_100 * linedist);

		glVertex2i(windowwidth / 2 + linedist - seldes_100 * linedist, windowheight / 2 + linedist - seldes_100 * linedist);
		glVertex2i(windowwidth / 2 + linedist - seldes_100 * linedist, windowheight / 2 + linedist - linelength - seldes_100 * linedist);
		glVertex2i(windowwidth / 2 + linedist - seldes_100 * linedist, windowheight / 2 + linedist - seldes_100 * linedist);
		glVertex2i(windowwidth / 2 + linedist - linelength - seldes_100 * linedist, windowheight / 2 + linedist - seldes_100 * linedist);

		glEnd();

	}

	glLineWidth(4);
	glBegin(GL_LINES);
	glColor4f(0.0, 0.0, 0.0, 1.0);
	glVertex2i(windowwidth / 2 - 20, windowheight / 2);
	glVertex2i(windowwidth / 2 + 20, windowheight / 2);
	glVertex2i(windowwidth / 2, windowheight / 2 - 20);
	glVertex2i(windowwidth / 2, windowheight / 2 + 20);
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
		glVertex2i(windowwidth / 2 - 15 + seldes_100 * 15, windowheight / 2);

		glVertex2i(windowwidth / 2 + 15, windowheight / 2);
		glVertex2i(windowwidth / 2 + 15 - seldes_100 * 15, windowheight / 2);

		glVertex2i(windowwidth / 2, windowheight / 2 - 15);
		glVertex2i(windowwidth / 2, windowheight / 2 - 15 + seldes_100 * 15);

		glVertex2i(windowwidth / 2, windowheight / 2 + 15);
		glVertex2i(windowwidth / 2, windowheight / 2 + 15 - seldes_100 * 15);

		glEnd();

	}

	glEnable(GL_TEXTURE_2D);
	glDisable(GL_CULL_FACE);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	for (int i = 0; i < 10; i++) {
		if (i == player::itemInHand)
			glBindTexture(GL_TEXTURE_2D, guiImage[2]);
		else
			glBindTexture(GL_TEXTURE_2D, guiImage[3]);

		glColor4f(1.0f, 1.0f, 1.0f, 0.7f);
		glColor4f(1.0f, 1.0f, 1.0f, 0.7f);
		glBegin(GL_QUADS);
		glTexCoord2f(0.0, 1.0);
		glVertex2d(i * 32, windowheight - 32);
		glTexCoord2f(0.0, 0.0);
		glVertex2d(i * 32 + 32, windowheight - 32);
		glTexCoord2f(1.0, 0.0);
		glVertex2d(i * 32 + 32, windowheight);
		glTexCoord2f(1.0, 1.0);
		glVertex2d(i * 32, windowheight);
		glEnd();
		if (player::inventorybox[3][i] != blocks::AIR) {
			glColor4f(1.0, 1.0, 1.0, 1.0);
			glBindTexture(GL_TEXTURE_2D, BlockTextures);
			double tcX = Textures::getTexcoordX(player::inventorybox[3][i], 1);
			double tcY = Textures::getTexcoordY(player::inventorybox[3][i], 1);
			glBegin(GL_QUADS);
			glTexCoord2d(tcX, tcY + 1 / 8.0);
			glVertex2d(i * 32 + 2, windowheight - 32 + 2);
			glTexCoord2d(tcX, tcY);
			glVertex2d(i * 32 + 30, windowheight - 32 + 2);
			glTexCoord2d(tcX + 1 / 8.0, tcY);
			glVertex2d(i * 32 + 30, windowheight - 32 + 30);
			glTexCoord2d(tcX + 1 / 8.0, tcY + 1 / 8);
			glVertex2d(i * 32 + 2, windowheight - 32 + 30);
			glEnd();
			std::stringstream ss;
			ss << (int)player::inventorypcs[3][i];
			TextRenderer::renderString(i * 32, windowheight - 32, ss.str());
		}
	}

	if (DebugMode) {
		TextRenderer::setFontColor(1.0f, 1.0f, 1.0f, 0.9f);
		std::stringstream ss;
		ss << "NEWorld v" << VERSION << " [OpenGL " << GLVersionMajor << "." << GLVersionMinor << "|" << GLVersionRev << "]";
		debugText(ss.str()); ss.str("");
		ss << "Flying:" << boolstr(FLY);
		debugText(ss.str()); ss.str("");
		ss << "Debug Mode:" << boolstr(DebugMode);
		debugText(ss.str()); ss.str("");
		ss << "Crosswall:" << boolstr(CROSS);
		debugText(ss.str()); ss.str("");
		ss << "Block:" << BlockInfo(player::BlockInHand).getBlockName() << " (ID" << (int)player::BlockInHand << ")";
		debugText(ss.str()); ss.str("");
		ss << "Fps:" << fps;
		debugText(ss.str()); ss.str("");
		ss << "Ups(Tps):" << ups;
		debugText(ss.str()); ss.str("");

		ss << "Xpos:" << player::xpos;
		debugText(ss.str()); ss.str("");
		ss << "Ypos:" << player::ypos;
		debugText(ss.str()); ss.str("");
		ss << "Zpos:" << player::zpos;
		debugText(ss.str()); ss.str("");
		ss << "Direction:" << player::heading;
		debugText(ss.str()); ss.str("");
		ss << "Head:" << player::lookupdown;
		debugText(ss.str()); ss.str("");
		ss << "On ground:" << boolstr(player::OnGround);
		debugText(ss.str()); ss.str("");
		ss << "Jump speed:" << player::jump;
		debugText(ss.str()); ss.str("");
		ss << "Near wall:" << boolstr(player::NearWall);
		debugText(ss.str()); ss.str("");
		ss << "In water:" << boolstr(player::inWater);
		debugText(ss.str()); ss.str("");

		ss << world::loadedChunks << " / " << world::chunkArraySize << " chunks loaded";
		debugText(ss.str()); ss.str("");
		ss << world::unloadedChunks << " chunks unloaded";
		debugText(ss.str()); ss.str("");
		ss << world::updatedChunks << " chunks updated";
		debugText(ss.str()); ss.str("");

#ifdef NEWORLD_DEBUG_PERFORMANCE_REC
		ss << c_getChunkIndexFromCIA << " CIA requests";
		debugText(ss.str()); ss.str("");
		ss << c_getChunkIndexFromSearch << " search requests";
		debugText(ss.str()); ss.str("");
		ss << c_getHeightFromHMap << " heightmap requests";
		debugText(ss.str()); ss.str("");
		ss << c_getHeightFromWorldGen << " worldgen requests";
		debugText(ss.str()); ss.str("");
#endif

		ss << "CIC Index:" << world::ciCacheIndex;
		debugText(ss.str()); ss.str("");
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
	if (timer() - fctime >= 1) {
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
				world::cloud[i][j] = int(rnd() * 2);
			}
		}
		glGenBuffersARB(128, cloudvb);
		for (int i = 0; i != 128; i++) {
			renderer::Init();
			for (int j = 0; j != 128; j++) {
				if (world::cloud[i][j]!=0) {
					renderer::Vertex3d(j*cloudwidth, 128.0, 0.0);
					renderer::Vertex3d(j*cloudwidth, 128.0, cloudwidth);
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
	for (int i = 0; i < 128; i++) {
		glPushMatrix();
		glTranslated(-64.0 * cloudwidth - px, 0.0, cloudwidth*((l + i) % 128 + f) - 64.0 * cloudwidth - pz);
		renderer::renderbuffer(cloudvb[i], vtxs[i], false, false);
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

void drawBag() {
	//背包界面
	static int si, sj, sf;
	int leftp = (windowwidth - 392) / 2;
	int upp = windowheight - 152 - 16;
	static int mousew, mouseb, mousebl;
	static block itemselected = blocks::AIR;
	static block pcsselected = 0;
	glClearColor(skycolorR, skycolorG, skycolorB, 1.0);
	glfwSetInputMode(MainWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

	mousew = mw;
	mouseb = mb;
	glDepthFunc(GL_ALWAYS);
	glDisable(GL_CULL_FACE);
	glDisable(GL_TEXTURE_2D);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, windowwidth, windowheight, 0, -1.0, 1.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glColor4f(0.2f, 0.2f, 0.2f, 0.6f);
	glBegin(GL_QUADS);
	glVertex2i(0, 0);
	glVertex2i(windowwidth, 0);
	glVertex2i(windowwidth, windowheight);
	glVertex2i(0, windowheight);
	glEnd();

	glEnable(GL_TEXTURE_2D);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	sf = 0;
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 10; j++) {
			if (mx >= j*(32 + 8) + leftp && mx <= j*(32 + 8) + 32 + leftp &&
				my >= i*(32 + 8) + upp && my <= i*(32 + 8) + 32 + upp) {
				si = i; sj = j; sf = 1;
				glBindTexture(GL_TEXTURE_2D, guiImage[2]);
				if (mousebl == 0 && mouseb == 1 && itemselected == player::inventorybox[i][j]) {
					if (player::inventorypcs[i][j] + pcsselected <= 255) {
						player::inventorypcs[i][j] += pcsselected;
						pcsselected = 0;
					}
					else
					{
						pcsselected = player::inventorypcs[i][j] + pcsselected - 255;
						player::inventorypcs[i][j] = 255;
					}
				}
				if (mousebl == 0 && mouseb == 1 && itemselected != player::inventorybox[i][j]) {
					std::swap(pcsselected, player::inventorypcs[i][j]);
					std::swap(itemselected, player::inventorybox[i][j]);
				}
				if (mousebl == 0 && mouseb == 2 && itemselected == player::inventorybox[i][j] && player::inventorypcs[i][j] < 255) {
					pcsselected--;
					player::inventorypcs[i][j]++;
				}
				if (mousebl == 0 && mouseb == 2 && player::inventorybox[i][j] == blocks::AIR) {
					pcsselected--;
					player::inventorypcs[i][j] = 1;
					player::inventorybox[i][j] = itemselected;
				}

				if (pcsselected == 0) itemselected = blocks::AIR;
				if (itemselected == blocks::AIR) pcsselected = 0;
				if (player::inventorypcs[i][j] == 0) player::inventorybox[i][j] = blocks::AIR;
				if (player::inventorybox[i][j] == blocks::AIR) player::inventorypcs[i][j] = 0;
			}
			else {
				glBindTexture(GL_TEXTURE_2D, guiImage[3]);
			}
			glBegin(GL_QUADS);
			glTexCoord2f(0.0, 1.0);
			glVertex2d(j*(32 + 8) + leftp, i*(32 + 8) + upp);
			glTexCoord2f(0.0, 0.0);
			glVertex2d(j*(32 + 8) + 32 + leftp, i*(32 + 8) + upp);
			glTexCoord2f(1.0, 0.0);
			glVertex2d(j*(32 + 8) + 32 + leftp, i*(32 + 8) + 32 + upp);
			glTexCoord2f(1.0, 1.0);
			glVertex2d(j*(32 + 8) + leftp, i*(32 + 8) + 32 + upp);
			glEnd();
			if (player::inventorybox[i][j] != blocks::AIR) {
				glBindTexture(GL_TEXTURE_2D, BlockTextures);
				double tcX = Textures::getTexcoordX(player::inventorybox[i][j], 1);
				double tcY = Textures::getTexcoordY(player::inventorybox[i][j], 1);
				glBegin(GL_QUADS);
				glTexCoord2d(tcX, tcY + 1 / 8.0);
				glVertex2d(j*(32 + 8) + 2 + leftp, i*(32 + 8) + 2 + upp);
				glTexCoord2d(tcX, tcY);
				glVertex2d(j*(32 + 8) + 30 + leftp, i*(32 + 8) + 2 + upp);
				glTexCoord2d(tcX + 1 / 8.0, tcY);
				glVertex2d(j*(32 + 8) + 30 + leftp, i*(32 + 8) + 30 + upp);
				glTexCoord2d(tcX + 1 / 8.0, tcY + 1 / 8.0);
				glVertex2d(j*(32 + 8) + 2 + leftp, i*(32 + 8) + 30 + upp);
				glEnd();
				std::stringstream ss;
				ss << (int)player::inventorypcs[i][j];
				TextRenderer::renderString(j*(32 + 8) + 8 + leftp, (i*(32 + 16) + 8 + upp), ss.str());
			}
		}
	}
	if (itemselected != blocks::AIR) {
		glBindTexture(GL_TEXTURE_2D, BlockTextures);
		double tcX = Textures::getTexcoordX(itemselected, 1);
		double tcY = Textures::getTexcoordY(itemselected, 1);
		glBegin(GL_QUADS);
		glTexCoord2d(tcX, tcY + 1 / 8.0);
		glVertex2d(mx - 16, my - 16);
		glTexCoord2d(tcX, tcY);
		glVertex2d(mx + 16, my - 16);
		glTexCoord2d(tcX + 1 / 8.0, tcY);
		glVertex2d(mx + 16, my + 16);
		glTexCoord2d(tcX + 1 / 8.0, tcY + 1 / 8.0);
		glVertex2d(mx - 16, my + 16);
		glEnd();
		std::stringstream ss;
		ss << pcsselected;
		TextRenderer::renderString((int)mx + 4, (int)my + 16, ss.str());
	}
	if (player::inventorybox[si][sj] != 0 && sf == 1) {
		glColor4f(1.0, 1.0, 0.0, 1.0);
		TextRenderer::renderString((int)mx, (int)my - 16, BlockInfo(player::inventorybox[si][sj]).getBlockName());
	}
	mousebl = mouseb;
}

void saveScreenshot(int x, int y, int w, int h, string filename){
	Textures::TEXTURE_RGB scrBuffer;
	while (w % 4 != 0){ w -= 1; }
	while (h % 4 != 0){ h -= 1; }
	scrBuffer.sizeX = w;
	scrBuffer.sizeY = h;
	scrBuffer.buffer = (ubyte*)malloc(w*h * 3);
	glReadPixels(x, y, w, h, GL_RGB, GL_UNSIGNED_BYTE, scrBuffer.buffer);
	Textures::SaveRGBImage(filename, scrBuffer);
	free(scrBuffer.buffer);
}

void createThumbnail(){
	std::stringstream ss;
	ss << "Worlds\\" << world::worldname << "\\Thumbnail.bmp";
	saveScreenshot(0, 0, windowwidth, windowheight, ss.str());
}
