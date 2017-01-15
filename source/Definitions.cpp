#include "Definitions.h"

//Global Vars
float FOVyNormal = 60.0f;       //视野角度
float mousemove = 0.2f;         //鼠标灵敏度
int viewdistance = 8;           //视野距离
int cloudwidth = 10;            //云的宽度
int selectPrecision = 100;      //选择方块的精度
int selectDistance = 5;         //选择方块的距离
float walkspeed = 0.15f;        //玩家前进速度
float runspeed = 0.3f;          //玩家跑步速度
int MaxAirJumps = 3 - 1;        //空中N段连跳
bool SmoothLighting = true;     //平滑光照
bool NiceGrass = true;          //草地材质连接
bool MergeFace = false;         //合并面渲染
bool GUIScreenBlur = true;      //GUI背景模糊
int linelength = 10;            //跟F3的准星有关。。。
int linedist = 30;              //跟F3的准星有关。。。
float skycolorR = 0.7f;         //天空颜色Red
float skycolorG = 1.0f;         //天空颜色Green
float skycolorB = 1.0f;         //天空颜色Blue
float FOVyRunning = 8.0f;
float FOVyExt;

int windowwidth;     //窗口宽度
int windowheight;    //窗口宽度
bool gamebegin, gameexit, bagOpened;

TextureID BlockTextures, BlockTextures3D;
TextureID tex_select, tex_unselect, tex_title, tex_mainmenu[6];
TextureID DestroyImage[11];
TextureID DefaultSkin;

//线程
std::mutex Mutex;
double lastupdate, updateTimer;
double lastframe;

bool shouldGetScreenshot;
bool shouldGetThumbnail;
bool FirstUpdateThisFrame;
bool FirstFrameThisUpdate;
double SpeedupAnimTimer;
double TouchdownAnimTimer;
double screenshotAnimTimer;
double bagAnimTimer;
double bagAnimDuration = 0.5;
Locale::Service locale("./locale");

//OpenGL
int GLVersionMajor, GLVersionMinor, GLVersionRev;
//GLFW
GLFWwindow *MainWindow;
GLFWcursor *MouseCursor;

//鼠标输入数据
double mx, my, mxl, myl;
int mw, mb, mbp, mbl, mwl;
double mxdelta, mydelta;
//键盘输入数据
string inputstr;

#ifdef NEWORLD_DEBUG_PERFORMANCE_REC
int c_getChunkPtrFromCPA;
int c_getChunkPtrFromSearch;
int c_getHeightFromHMap;
int c_getHeightFromWorldGen;
#endif

void Sleep(unsigned int ms)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}


void DebugWarning(string msg)
{
    //SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 14);
    printf("[Debug][Warning]");
    //SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 7);
    printf("%s\n", msg.c_str());
}

void DebugError(string msg)
{
    //SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 12);
    printf("[Debug][Error]");
    //SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 7);
    printf("%s\n", msg.c_str());
}
