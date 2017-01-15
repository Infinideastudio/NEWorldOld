#pragma once
#ifndef _DEBUG
#pragma comment(linker, "/SUBSYSTEM:\"WINDOWS\" /ENTRY:\"mainCRTStartup\"")
#endif
#include "StdInclude.h"
#include "Locale.h"
#include <codecvt>

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
//Global Vars
const unsigned int VERSION = 37;
const string MAJOR_VERSION = "Alpha 0.";
const string MINOR_VERSION = "4.10";
const string EXT_VERSION = "";
const int defaultwindowwidth = 852; //Ĭ�ϴ��ڿ��
const int defaultwindowheight = 480; //Ĭ�ϴ��ڸ߶�
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

extern std::mutex Mutex;
extern double lastupdate, updateTimer;
extern double lastframe;

extern bool shouldGetScreenshot;
extern bool shouldGetThumbnail;
extern bool FirstUpdateThisFrame;
extern bool FirstFrameThisUpdate;
extern double SpeedupAnimTimer;
extern double TouchdownAnimTimer;
extern double screenshotAnimTimer;
extern double bagAnimTimer;
extern double bagAnimDuration;
extern Locale::Service locale;

extern int GLVersionMajor, GLVersionMinor, GLVersionRev;
extern GLFWwindow *MainWindow;
extern GLFWcursor *MouseCursor;
extern double mx, my, mxl, myl;
extern int mw, mb, mbp, mbl, mwl;
extern double mxdelta, mydelta;
extern string inputstr;

#ifdef NEWORLD_DEBUG_PERFORMANCE_REC
extern int c_getChunkPtrFromCPA;
extern int c_getChunkPtrFromSearch;
extern int c_getHeightFromHMap;
extern int c_getHeightFromWorldGen;
#endif

inline string boolstr(bool b)
{
    return b ? "True" : "False";
}
inline double timer()
{
    return static_cast<double>(clock()) / CLOCKS_PER_SEC;
}
inline double rnd()
{
    static std::mt19937_64 r(timer());
    return static_cast<double>(r()) / r.max();
}

void Sleep(unsigned int ms);

inline int RoundInt(double d)
{
    return int(floor(d + 0.5));
}

inline const std::string w2cUtf8(const std::wstring& src)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
    return conv.to_bytes(src);
}

inline const std::wstring c2wUtf8(const std::string& src)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t> > conv;
    return conv.from_bytes(src);
}

void DebugWarning(string msg);
void DebugError(string msg);
#endif