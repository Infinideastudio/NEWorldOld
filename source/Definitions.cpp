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
bool UseCPArray = true;         //ʹ��CIA
bool SmoothLighting = true;     //ƽ������
bool NiceGrass = true;          //�ݵز�������
int linelength = 10;            //��F3��׼���йء�����
int linedist = 30;              //��F3��׼���йء�����
float skycolorR = 0.7f;         //�����ɫRed
float skycolorG = 1.0f;         //�����ɫGreen
float skycolorB = 1.0f;         //�����ɫBlue
float FOVyRunning = 8.0f;
float FOVyExt;

int windowwidth;     //���ڿ��
int windowheight;    //���ڿ��
bool gamebegin, bagOpened;

TextureID BlockTexture[20];
TextureID BlockTextures;
TextureID guiImage[6];
TextureID DestroyImage[11];
TextureID DefaultSkin;

//������Ϸ
bool multiplayer = false;
string serverip;
unsigned short port = 30001;

//�߳�
Mutex_t Mutex;
Thread_t updateThread;
double lastupdate, updateTimer;
bool updateThreadRun, updateThreadPaused;

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

//�����������
double mx, my, mxl, myl;
int mw, mb, mbp, mbl, mwl;
//������������
string inputstr;
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
int c_getChunkPtrFromCPA;
int c_getChunkPtrFromSearch;
int c_getHeightFromHMap;
int c_getHeightFromWorldGen;
#endif

void DebugWarning(string msg){
	printf("[Debug][Warning]%s\n", msg.c_str());
}

void DebugError(string msg){
	printf("[Debug][Error]%s\n",msg.c_str());
}
