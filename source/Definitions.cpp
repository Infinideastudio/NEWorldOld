#include "Definitions.h"

//Global Vars
const unsigned int VERSION = 37;
const string MAJOR_VERSION = "Alpha 0.";
const string MINOR_VERSION = "4.10";
const string EXT_VERSION = " [Preview Not Released]";
const int defaultwindowwidth  = 852; //Ĭ�ϴ��ڿ��
const int defaultwindowheight = 480; //Ĭ�ϴ��ڸ߶�
float FOVyNormal = 60.0f;       //��Ұ�Ƕ�
float mousemove = 0.2f;         //���������
int viewdistance = 8;           //��Ұ����
int cloudwidth = 10;            //�ƵĿ��
int selectPrecision = 100;      //ѡ�񷽿�ľ���
int selectDistance = 5;         //ѡ�񷽿�ľ���
float walkspeed = 0.15f;        //���ǰ���ٶ�
float runspeed = 0.3f;          //����ܲ��ٶ�
int MaxAirJumps = 3 - 1;        //����N������
bool UseCIArray = true;         //ʹ��CIA
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

//�߳�
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

//�����������
double mx, my, mxl, myl;
int mw, mb, mbp, mbl, mwl;
//������������
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
