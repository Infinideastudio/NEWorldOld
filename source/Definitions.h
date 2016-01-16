﻿#ifndef DEFINITIONS_H
#define DEFINITIONS_H
//#pragma warning(disable:4710) //忽略STL高发警告：函数未内联
//#pragma warning(disable:4514) //忽略STL高发警告：未使用的内联函数已移除
//#pragma warning(disable:4350) //忽略STL高发警告：行为更改
//#pragma warning(push,0) //忽略头文件的警告

#include <math.h>
#include <time.h>
#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include <sstream>
#include <map>
#include <queue>
#include <functional>
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))

using std::string;
using std::vector;
using std::pair;
using std::unique_ptr;
using std::map;

//#pragma warning(pop)
//#pragma warning(disable:4820) //忽略不必要的警告：数据结构对齐
//#pragma warning(disable:4365) //忽略不必要的警告：有符号/无符号不匹配

//pthread
#ifdef _WIN32
#include <pthread/pthread.h>
#else
#include <pthread.h>
#endif
//wxWidgets
#include <wx/wx.h>
#include <wx/notebook.h>
#include <wx/file.h>
#include <wx/filefn.h>
#include <wx/thread.h>
#include <wx/dir.h>
#include <wx/stdpaths.h>
#include <wx/socket.h>
#include <wx/dynlib.h>
#include <wx/textfile.h>
#include <wx/slider.h>

#ifndef NEWORLD_SERVER
//GLFW
#define GLFW_INCLUDE_GLU
#include <GLFW/glfw3.h>
//GLEXT
#include <GL/glext.h>
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
typedef unsigned short item;
typedef unsigned int VBOID;
typedef int vtxCount;
typedef int SkinID;
typedef uint64 chunkid;
typedef unsigned int onlineid;
#ifndef NEWORLD_SERVER
typedef pthread_mutex_t Mutex_t;
typedef pthread_t Thread_t;
typedef void *(*ThreadFunc_t)(void *);
#define ThreadFunc void*
//Global Vars
const unsigned int VERSION = 38;
const string MAJOR_VERSION = "Alpha 0.";
const string MINOR_VERSION = "5";
const string EXT_VERSION = " [In Development]";
const int defaultwindowwidth = 852; //默认窗口宽度
const int defaultwindowheight = 480; //默认窗口高度
const int networkRequestFrequency = 3; //请求频率
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
extern int Multisample;

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
extern PFNGLGETUNIFORMLOCATIONARBPROC glGetUniformLocationARB;
extern PFNGLUNIFORM1FARBPROC glUniform1fARB;
extern PFNGLUNIFORM1IARBPROC glUniform1iARB;
extern PFNGLGETOBJECTPARAMETERIVARBPROC glGetObjectParameterivARB;
extern PFNGLGETINFOLOGARBPROC glGetInfoLogARB;
extern PFNGLDETACHOBJECTARBPROC glDetachObjectARB;
extern PFNGLDELETEOBJECTARBPROC glDeleteObjectARB;
extern PFNGLTEXIMAGE3DPROC glTexImage3D;
extern PFNGLTEXSUBIMAGE3DPROC glTexSubImage3D;

#ifdef NEWORLD_DEBUG_PERFORMANCE_REC
extern int c_getChunkPtrFromCPA;
extern int c_getChunkPtrFromSearch;
extern int c_getHeightFromHMap;
extern int c_getHeightFromWorldGen;
#endif

vector<string> split(string str, string pattern);
#define boolstr(b) ((string)(b ? "True" : "False"))
#define rnd() ((double)rand() / (RAND_MAX + 1))
#define timer() ((float)clock() / CLOCKS_PER_SEC)

inline Mutex_t MutexCreate() { pthread_mutex_t ret; pthread_mutex_init(&ret, nullptr); return ret; }
inline void MutexDestroy(Mutex_t _hMutex) { pthread_mutex_destroy(&_hMutex); }
inline void MutexLock(Mutex_t _hMutex) { pthread_mutex_lock(&_hMutex); }
inline void MutexUnlock(Mutex_t _hMutex) { pthread_mutex_unlock(&_hMutex); }
inline Thread_t ThreadCreate(ThreadFunc_t func, void* param) { pthread_t ret; pthread_create(&ret, nullptr, func, param); return ret; }
inline void ThreadWait(Thread_t _hThread) { pthread_join(_hThread, nullptr); }
inline void ThreadDestroy(Thread_t& _hThread) { memset(&_hThread, 0, sizeof(_hThread)); }
#define Sleep wxMilliSleep
#define wstrlen wcslen

inline int RoundInt(double d){ return int(floor(d + 0.5)); }
inline string itos(int i){
	char tmp[12];
	sprintf(tmp, "%d", i);
	return tmp;
}
inline string ftos(double i)
{
	char tmp[64];
	sprintf(tmp, "%g", i);
	return tmp;
}
#define DebugWarning(msg) printf("[Debug][Warning]%s\n",((string)msg).c_str())
#define DebugError(msg) printf("[Debug][Error]%s\n",((string)msg).c_str())
inline bool beginWith(string str, string begin) {
	if (str.size() < begin.size()) return false;
	for (size_t i = 0; i != begin.size(); i++) {
		if (str[i] != begin[i]) return false;
	}
	return true;
}
template<class T>
inline void conv(string str, T& ret) {
	std::stringstream s(str);
	s >> ret;
}
#endif
#endif