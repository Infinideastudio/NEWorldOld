#pragma once
#ifndef _DEBUG
#pragma comment(linker, "/SUBSYSTEM:\"WINDOWS\" /ENTRY:\"mainCRTStartup\"")
#endif
#include "StdInclude.h"

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
#ifdef NEWORLD_USE_WINAPI
typedef HANDLE Mutex_t;
typedef HANDLE Thread_t;
typedef PTHREAD_START_ROUTINE ThreadFunc_t;
#define ThreadFunc DWORD WINAPI
#else
typedef std::mutex* Mutex_t;
typedef std::thread* Thread_t;
typedef unsigned int(*ThreadFunc_t)(void* param);
#define ThreadFunc unsigned int
#endif

//Global Vars
const unsigned int VERSION = 37;
const string MAJOR_VERSION = "Alpha 0.";
const string MINOR_VERSION = "4.10";
const string EXT_VERSION = " [Preview Not Released]";
const int defaultwindowwidth = 852; //默认窗口宽度
const int defaultwindowheight = 480; //默认窗口高度
const int networkRequestFrequency = 2; //请求频率
const int networkRequestMax = 20; //理想最大请求队列长度
extern float FOVyNormal;
extern float mousemove;
extern int viewdistance;
extern int cloudwidth;
extern int selectPrecision;
extern int selectDistance;
extern float walkspeed;
extern float runspeed;
extern int MaxAirJumps;
extern bool SmoothLighting;
extern bool NiceGrass;
extern bool MergeFace;
extern bool GUIScreenBlur;
extern int linelength;
extern int linedist;
extern float skycolorR;
extern float skycolorG;
extern float skycolorB;
extern float FOVyRunning;
extern float FOVyExt;

extern int windowwidth;
extern int windowheight;
extern bool gamebegin, gameexit, bagOpened;

extern TextureID BlockTextures, BlockTextures3D;
extern TextureID tex_select, tex_unselect, tex_title, tex_mainmenu[6];
extern TextureID DestroyImage[11];
extern TextureID DefaultSkin;

extern bool multiplayer;
extern string serverip;
extern unsigned short port;

extern Mutex_t Mutex;
extern Thread_t updateThread;
extern double lastupdate, updateTimer;
extern double lastframe;
extern bool updateThreadRun, updateThreadPaused;

extern bool mpclient, mpserver;
extern bool shouldGetScreenshot;
extern bool shouldGetThumbnail;
extern bool FirstUpdateThisFrame;
extern bool FirstFrameThisUpdate;
extern double SpeedupAnimTimer;
extern double TouchdownAnimTimer;
extern double screenshotAnimTimer;
extern double bagAnimTimer;
extern double bagAnimDuration;

extern int GLVersionMajor, GLVersionMinor, GLVersionRev;
extern GLFWwindow* MainWindow;
extern GLFWcursor* MouseCursor;
extern double mx, my, mxl, myl;
extern int mw, mb, mbp, mbl, mwl;
extern double mxdelta, mydelta;
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
extern PFNGLTEXIMAGE3DPROC glTexImage3D;

#ifdef NEWORLD_DEBUG_PERFORMANCE_REC
extern int c_getChunkPtrFromCPA;
extern int c_getChunkPtrFromSearch;
extern int c_getHeightFromHMap;
extern int c_getHeightFromWorldGen;
#endif

inline string boolstr(bool b){ return b ? "True" : "False"; }
inline double rnd() { return (double)rand() / (RAND_MAX + 1); }
#ifdef NEWORLD_USE_WINAPI
inline double timer(){
	static LARGE_INTEGER counterFreq;
	if (counterFreq.QuadPart == 0) QueryPerformanceFrequency(&counterFreq);
	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);
	return (double)now.QuadPart / counterFreq.QuadPart;
}
#else
inline double timer() { return (double)clock() / CLOCKS_PER_SEC; }
#endif

#ifdef NEWORLD_USE_WINAPI
inline Mutex_t MutexCreate(){return CreateMutex(NULL, FALSE, "");}
inline void MutexDestroy(Mutex_t _hMutex){CloseHandle(_hMutex);}
inline void MutexLock(Mutex_t _hMutex){WaitForSingleObject(_hMutex, INFINITE);}
inline void MutexUnlock(Mutex_t _hMutex){ReleaseMutex(_hMutex);}
inline Thread_t ThreadCreate(ThreadFunc_t func, void* param){return CreateThread(NULL, 0, func, param, 0, NULL);}
inline void ThreadWait(Thread_t _hThread){WaitForSingleObject(_hThread, INFINITE);}
inline void ThreadDestroy(Thread_t _hThread){CloseHandle(_hThread);}
unsigned int MByteToWChar(wchar_t* dst, const char* src, unsigned int n);
unsigned int WCharToMByte(char* dst, const wchar_t* src, unsigned int n);
inline unsigned int wstrlen(const wchar_t* wstr){ return lstrlenW(wstr); }
#else
inline Mutex_t MutexCreate(){return new std::mutex;}
inline void MutexDestroy(Mutex_t _hMutex){delete _hMutex;}
inline void MutexLock(Mutex_t _hMutex){_hMutex->lock();}
inline void MutexUnlock(Mutex_t _hMutex){_hMutex->unlock();}
inline Thread_t ThreadCreate(ThreadFunc_t func, void* param){return new std::thread(func, param);}
inline void ThreadWait(Thread_t _hThread){_hThread->join();}
inline void ThreadDestroy(Thread_t _hThread){delete _hThread;}
inline unsigned int MByteToWChar(wchar_t* dst, const char* src, unsigned int n){ size_t res; mbstowcs_s(&res, dst, n, src, _TRUNCATE); return res; }
inline unsigned int WCharToMByte(char* dst, const wchar_t* src, unsigned int n){ size_t res; wcstombs_s(&res, dst, n, src, _TRUNCATE); return res; }
inline unsigned int wstrlen(const wchar_t* wstr){ return wcslen(wstr); }
void Sleep(unsigned int ms);
#endif

inline int RoundInt(double d){ return int(floor(d + 0.5)); }
inline string itos(int i){
	char a[12];
#ifdef NEWORLD_COMPILE_DISABLE_SECURE
	_itoa(i, a, 10);
#else
	_itoa_s(i, a, 12, 10);
#endif
	return string(a);
}

void DebugWarning(string msg);
void DebugError(string msg);
#endif