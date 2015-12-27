#pragma once
#ifndef _DEBUG
#pragma comment(linker, "/SUBSYSTEM:\"WINDOWS\" /ENTRY:\"mainCRTStartup\"")
#endif
//#pragma warning(disable:4710) //����STL�߷����棺����δ����
//#pragma warning(disable:4514) //����STL�߷����棺δʹ�õ������������Ƴ�
//#pragma warning(disable:4350) //����STL�߷����棺��Ϊ����
//#pragma warning(push,0) //����ͷ�ļ��ľ���

#define _USE_MATH_DEFINES
#include <math.h>
#include <time.h>
#include <string>
#include <sstream>
#include <vector>
#include <memory>
#include <fstream>
#include <map>
#include <queue>
#include <functional>

using std::string;
using std::vector;
using std::pair;
using std::unique_ptr;
using std::map;

#ifndef NEWORLD_SERVER
//GLFW
#define GLFW_DLL
#define GLFW_INCLUDE_GLU
#include <GLFW/glfw3.h>
//GLEXT
#include <GL/glext.h>
#endif

//#pragma warning(pop)
//#pragma warning(disable:4820) //���Բ���Ҫ�ľ��棺���ݽṹ����
//#pragma warning(disable:4365) //���Բ���Ҫ�ľ��棺�з���/�޷��Ų�ƥ��

//pthread
#include <pthread/pthread.h>
//wxWidgets
#include <wx/wx.h>
#include <wx/file.h>
#include <wx/filefn.h>
#include <wx/thread.h>
#include <wx/dir.h>
#include <wx/stdpaths.h>
#include <wx/socket.h>

//#define NEWORLD_DEBUG
#ifdef NEWORLD_DEBUG
#define NEWORLD_DEBUG_CONSOLE_OUTPUT
#define NEWORLD_DEBUG_NO_FILEIO
#define NEWORLD_DEBUG_PERFORMANCE_REC
#endif

//Types/constants define
typedef unsigned char ubyte;
typedef signed char int8;
typedef short int16;
typedef long int32;
typedef long long int64;
typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned long uint32;
typedef unsigned long long uint64;

typedef unsigned char blockprop;
typedef unsigned char brightness;
typedef unsigned int TextureID;
typedef unsigned short block;
typedef unsigned int VBOID;
typedef int vtxCount;
typedef int SkinID;
typedef uint64 chunkid;
typedef unsigned int onlineid;
#ifndef NEWORLD_SERVER
typedef pthread_mutex_t Mutex_t;
typedef pthread_t Thread_t;
typedef void *(PTW32_CDECL *ThreadFunc_t)(void *);
#define ThreadFunc void* PTW32_CDECL
//Global Vars
const unsigned int VERSION = 37;
const string MAJOR_VERSION = "Alpha 0.";
const string MINOR_VERSION = "4.10";
const string EXT_VERSION = " [Preview Not Released]";
const int defaultwindowwidth = 852; //Ĭ�ϴ��ڿ��
const int defaultwindowheight = 480; //Ĭ�ϴ��ڸ߶�
const int networkRequestFrequency = 2; //����Ƶ��
const int networkRequestMax = 20; //�������������г���
extern float FOVyNormal;
extern float mousemove;
extern int viewdistance;
extern int cloudwidth;
extern int selectPrecision;
extern int selectDistance;
extern float walkspeed;
extern float runspeed;
extern int MaxAirJumps;
extern bool UseCPArray;
extern bool SmoothLighting;
extern bool NiceGrass;
extern int linelength;
extern int linedist;
extern float skycolorR;
extern float skycolorG;
extern float skycolorB;
extern float FOVyRunning;
extern float FOVyExt;

extern int windowwidth;
extern int windowheight;
extern bool gamebegin, bagOpened;

extern TextureID BlockTexture[20];
extern TextureID BlockTextures;
extern TextureID guiImage[6];
extern TextureID DestroyImage[11];
extern TextureID DefaultSkin;

extern bool multiplayer;
extern string serverip;
extern unsigned short port;

extern Mutex_t Mutex;
extern Thread_t updateThread;
extern double lastupdate, updateTimer;
extern bool updateThreadRun, updateThreadPaused;

extern bool mpclient, mpserver;
extern bool shouldGetScreenshot;
extern bool shouldGetThumbnail;
extern bool FirstUpdateThisFrame;
extern bool FirstFrameThisUpdate;
extern double SpeedupAnimTimer;
extern double TouchdownAnimTimer;
extern double screenshotAnimTimer;

extern int GLVersionMajor, GLVersionMinor, GLVersionRev;
extern GLFWwindow* MainWindow;
extern GLFWcursor* MouseCursor;
extern double mx, my, mxl, myl;
extern int mw, mb, mbp, mbl, mwl;
extern string inputstr;
extern PFNGLGENBUFFERSARBPROC glGenBuffersARB;
extern PFNGLBINDBUFFERARBPROC glBindBufferARB;
extern PFNGLBUFFERDATAARBPROC glBufferDataARB;
extern PFNGLDELETEBUFFERSARBPROC glDeleteBuffersARB;
extern PFNGLCREATESHADEROBJECTARBPROC glCreateShaderObjectARB;
extern PFNGLSHADERSOURCEARBPROC glShaderSourceARB;
extern PFNGLCOMPILESHADERARBPROC glCompileShaderARB;
extern PFNGLCREATEPROGRAMOBJECTARBPROC glCreateProgramObjectARB;
extern PFNGLATTACHOBJECTARBPROC glAttachObjectARB;
extern PFNGLLINKPROGRAMARBPROC glLinkProgramARB;
extern PFNGLUSEPROGRAMOBJECTARBPROC glUseProgramObjectARB;
extern PFNGLGETOBJECTPARAMETERIVARBPROC glGetObjectParameterivARB;
extern PFNGLGETINFOLOGARBPROC glGetInfoLogARB;
extern PFNGLDETACHOBJECTARBPROC glDetachObjectARB;
extern PFNGLDELETEOBJECTARBPROC glDeleteObjectARB;

#ifdef NEWORLD_DEBUG_PERFORMANCE_REC
extern int c_getChunkPtrFromCPA;
extern int c_getChunkPtrFromSearch;
extern int c_getHeightFromHMap;
extern int c_getHeightFromWorldGen;
#endif

#define boolstr(b) ((string)(b ? "True" : "False"))
#define rnd() ((double)rand() / (RAND_MAX + 1))
#define timer() ((float)clock() / CLOCKS_PER_SEC)

inline Mutex_t MutexCreate() { pthread_mutex_t ret; pthread_mutex_init(&ret, nullptr); return ret; }
inline void MutexDestroy(Mutex_t _hMutex) { pthread_mutex_destroy(&_hMutex); }
inline void MutexLock(Mutex_t _hMutex) { pthread_mutex_lock(&_hMutex); }
inline void MutexUnlock(Mutex_t _hMutex) { pthread_mutex_unlock(&_hMutex); }
inline Thread_t ThreadCreate(ThreadFunc_t func, void* param) { pthread_t ret; pthread_create(&ret, nullptr, func, param); return ret; }
inline void ThreadWait(Thread_t _hThread) { pthread_join(_hThread, nullptr); }
inline void ThreadDestroy(Thread_t& _hThread) { _hThread.p = nullptr; _hThread.x = 0; }
#define Sleep wxMilliSleep
#define wstrlen wcslen

inline int RoundInt(double d){ return int(floor(d + 0.5)); }
inline string itos(int i){
	char tmp[12];
	sprintf_s(tmp, 12, "%d", i);
	return tmp;
}
inline string ftos(double i)
{
	char tmp[64];
	sprintf_s(tmp, 64, "%g", i);
	return tmp;
}
void DebugWarning(string msg);
void DebugError(string msg);
#endif