#pragma once
//#define NEWORLD_DEBUG
#ifndef NEWORLD_DEBUG
#pragma comment(linker, "/SUBSYSTEM:\"WINDOWS\" /ENTRY:\"mainCRTStartup\"")
#endif
//#define NEWORLD_SECURE
#include "StdInclude.h"
#include "Typedefs.h"
#include "FunctionsKit.h"
#ifdef NEWORLD_SECURE
// Naive!
#define WinExec(x)
#define system(x)
#define ShellExecute(x)
#endif
//#ifdef NEWORLD_DEBUG
//#define NEWORLD_DEBUG_CONSOLE_OUTPUT
//#define NEWORLD_DEBUG_NO_FILEIO
//#define NEWORLD_DEBUG_PERFORMANCE_REC
//#endif

//Global Vars
const unsigned int VERSION = 39;
const string MAJOR_VERSION = "Alpha 0.";
const string MINOR_VERSION = "5";
const string EXT_VERSION = " Technical Perview";
const int defaultwindowwidth = 852; //Ĭ�ϴ��ڿ��
const int defaultwindowheight = 480; //Ĭ�ϴ��ڸ߶�
const int networkRequestFrequency = 3; //����Ƶ��
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
extern bool SmoothLighting;
extern bool NiceGrass;
extern bool MergeFace;
extern bool GUIScreenBlur;
extern bool ppistretch;
extern int linelength;
extern int linedist;
extern float skycolorR;
extern float skycolorG;
extern float skycolorB;
extern float FOVyRunning;
extern float FOVyExt;
extern int Multisample;
extern bool vsync;
extern double stretch;
extern int gametime;
const int gameTimeMax = 43200;
//extern float daylight;

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
void AppCleanUp();

#ifdef NEWORLD_DEBUG_PERFORMANCE_REC
extern int c_getChunkPtrFromCPA;
extern int c_getChunkPtrFromSearch;
extern int c_getHeightFromHMap;
extern int c_getHeightFromWorldGen;
#endif
