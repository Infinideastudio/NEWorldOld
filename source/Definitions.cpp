#include "Definitions.h"

//Global Vars
const unsigned int VERSION = 37;
const string MAJOR_VERSION = "Alpha 0.";
const string MINOR_VERSION = "4.10";
const string EXT_VERSION = " [Preview Not Released]";
const int defaultwindowwidth  = 852; //默认窗口宽度
const int defaultwindowheight = 480; //默认窗口高度
float FOVyNormal = 60.0f;       //视野角度
float mousemove = 0.2f;         //鼠标灵敏度
int viewdistance = 8;           //视野距离
int cloudwidth = 10;            //云的宽度
int selectPrecision = 100;      //选择方块的精度
int selectDistance = 5;         //选择方块的距离
float playerheight = 1.2f;      //玩家的高度
float walkspeed = 0.15f;        //玩家前进速度
float runspeed = 0.3f;          //玩家跑步速度
int MaxAirJumps = 2;            //空中N段连跳
bool UseCIArray = true;         //使用CIA
bool SmoothLighting = true;     //平滑光照
bool NiceGrass = true;          //草地材质连接
int linelength = 10;            //跟F3的准星有关。。。
int linedist = 30;              //跟F3的准星有关。。。
float skycolorR = 0.7f;         //天空颜色Red
float skycolorG = 1.0f;         //天空颜色Green
float skycolorB = 1.0f;         //天空颜色Blue
float FOVyRunning = 8.0f;
float FOVyExt;
float playerHeightExt;

int windowwidth;     //窗口宽度
int windowheight;    //窗口宽度
bool gamebegin, bagOpened;
int k;

TextureID BlockTexture[20];
TextureID BlockTextures;
TextureID guiImage[6];
TextureID DestroyImage[11];

//线程
Mutex_t Mutex;
Thread_t updateThread;
double lastupdate, updateTimer;
bool updateThreadRun, updateThreadPaused;

bool mpclient, mpserver;
bool shouldGetScreenshot;
bool shouldGetThumbnail;
bool FirstUpdateThisFrame;
bool FirstFrameThisUpdate;
double SpeedupAnimTimer;
double TouchdownAnimTimer;
double screenshotAnimTimer;

//OpenGL
int GLVersionMajor, GLVersionMinor, GLVersionRev;
//GLFW
GLFWwindow* MainWindow;
GLFWcursor* MouseCursor;

//鼠标输入数据
double mx, my, mxl, myl;
int mw, mb, mbp, mbl, mwl;
//键盘输入数据
string inputstr;
bool ep, escp;
//OpenGL Procedure
PFNGLGENBUFFERSARBPROC glGenBuffersARB;
PFNGLBINDBUFFERARBPROC glBindBufferARB;
PFNGLBUFFERDATAARBPROC glBufferDataARB;
PFNGLDELETEBUFFERSARBPROC glDeleteBuffersARB;
PFNGLCREATESHADEROBJECTARBPROC glCreateShaderObjectARB;
PFNGLSHADERSOURCEARBPROC glShaderSourceARB;
PFNGLCOMPILESHADERARBPROC glCompileShaderARB;
PFNGLCREATEPROGRAMOBJECTARBPROC glCreateProgramObjectARB;
PFNGLATTACHOBJECTARBPROC glAttachObjectARB;
PFNGLLINKPROGRAMARBPROC glLinkProgramARB;
PFNGLUSEPROGRAMOBJECTARBPROC glUseProgramObjectARB;
PFNGLGETOBJECTPARAMETERIVARBPROC glGetObjectParameterivARB;
PFNGLGETINFOLOGARBPROC glGetInfoLogARB;
PFNGLDETACHOBJECTARBPROC glDetachObjectARB;
PFNGLDELETEOBJECTARBPROC glDeleteObjectARB;

#ifdef NEWORLD_DEBUG_PERFORMANCE_REC
int c_getChunkIndexFromCIA;
int c_getChunkIndexFromSearch;
int c_getHeightFromHMap;
int c_getHeightFromWorldGen;
#endif

#ifdef NEWORLD_USE_WINAPI
unsigned int MByteToWChar(wchar_t* dst, const char* src, unsigned int n){
	int res = MultiByteToWideChar(CP_ACP, 0, src, n, dst, n);
	return res;
}
unsigned int WCharToMByte(char* dst, const wchar_t* src, unsigned int n){
	return WideCharToMultiByte(CP_ACP, 0, src, n, dst, n * 2, NULL, NULL);
}
#else
void Sleep(unsigned int ms){
	unsigned int fr = clock();
	while (clock() - fr <= ms);
	return;
}
#endif

void DebugWarning(string msg){
	//SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 14);
	printf("[Debug][Warning]");
	//SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 7);
	printf("%s\n", msg.c_str());
}

void DebugError(string msg){
	//SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 12);
	printf("[Debug][Error]");
	//SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 7);
	printf("%s\n",msg.c_str());
}
