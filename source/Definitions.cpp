#include "Definitions.h"

//Global Vars
float FOVyNormal = 60.0f;       //��Ұ�Ƕ�
float mousemove = 0.2f;         //���������
int viewdistance = 8;           //��Ұ����
int cloudwidth = 10;            //�ƵĿ��
int selectPrecision = 100;      //ѡ�񷽿�ľ���
int selectDistance = 5;         //ѡ�񷽿�ľ���
float walkspeed = 0.15f;        //���ǰ���ٶ�
float runspeed = 0.3f;          //����ܲ��ٶ�
int MaxAirJumps = 3 - 1;        //����N������
bool SmoothLighting = true;     //ƽ������
bool NiceGrass = true;          //�ݵز�������
bool MergeFace = false;         //�ϲ�����Ⱦ
bool GUIScreenBlur = true;      //GUI����ģ��
int linelength = 10;            //��F3��׼���йء�����
int linedist = 30;              //��F3��׼���йء�����
float skycolorR = 0.7f;         //�����ɫRed
float skycolorG = 1.0f;         //�����ɫGreen
float skycolorB = 1.0f;         //�����ɫBlue
float FOVyRunning = 8.0f;
float FOVyExt;

int windowwidth;     //���ڿ��
int windowheight;    //���ڿ��
bool gamebegin, gameexit, bagOpened;

TextureID BlockTextures, BlockTextures3D;
TextureID tex_select, tex_unselect, tex_title, tex_mainmenu[6];
TextureID DestroyImage[11];
TextureID DefaultSkin;

//�߳�
Mutex_t Mutex;
Thread_t updateThread;
double lastupdate, updateTimer;
double lastframe;
bool updateThreadRun, updateThreadPaused;

bool shouldGetScreenshot;
bool shouldGetThumbnail;
bool FirstUpdateThisFrame;
bool FirstFrameThisUpdate;
double SpeedupAnimTimer;
double TouchdownAnimTimer;
double screenshotAnimTimer;
double bagAnimTimer;
double bagAnimDuration = 0.5;

//OpenGL
int GLVersionMajor, GLVersionMinor, GLVersionRev;
//GLFW
GLFWwindow *MainWindow;
GLFWcursor *MouseCursor;

//�����������
double mx, my, mxl, myl;
int mw, mb, mbp, mbl, mwl;
double mxdelta, mydelta;
//������������
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
