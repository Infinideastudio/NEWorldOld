#pragma once
#ifndef NEWORLD_DEBUG
#pragma comment(linker, "/SUBSYSTEM:\"WINDOWS\" /ENTRY:\"mainCRTStartup\"")
#endif
#include "StdInclude.h"
#include "Typedefs.h"
#include "FunctionsKit.h"
// #ifdef NEWORLD_DEBUG
// #define NEWORLD_DEBUG_CONSOLE_OUTPUT
// #define NEWORLD_DEBUG_NO_FILEIO
// #define NEWORLD_DEBUG_PERFORMANCE_REC
// #endif

// Global constants
const unsigned int GameVersion = 39;
const std::string MajorVersion = "Alpha 0.";
const std::string MinorVersion = "5";
const std::string VersionSuffix = " [In Development]";
const int DefaultWindowWidth = 852;
const int DefaultWindowHeight = 480;
const int NetworkRequestFrequency = 3;
const int NetworkRequestMax = 20;
const double Pi = std::numbers::pi_v<double>;

// Global variables
extern float FOVyNormal;
extern float FOVyRunning;
extern float MouseSpeed;
extern int RenderDistance;
extern bool SmoothLighting;
extern bool NiceGrass;
extern bool MergeFace;
extern bool UIStretch;
extern bool UIBackgroundBlur;
extern int Multisample;
extern bool VerticalSync;
extern int GameTime;
extern int WindowWidth;
extern int WindowHeight;
extern double Stretch;

extern TextureID SplashTexture;
extern TextureID TitleTexture;
extern TextureID UIBackgroundTextures[6];
extern TextureID SelectedTexture;
extern TextureID UnselectedTexture;
extern TextureID BlockTextureArray;
extern TextureID DefaultSkin;

extern int GLMajorVersion, GLMinorVersion, GLRevisionVersion;
extern GLFWwindow* MainWindow;
extern GLFWcursor* MouseCursor;
extern int mx, my, mxl, myl;
extern int mw, mb, mbp, mbl, mwl;
extern double mxdelta, mydelta;
extern std::u32string inputstr;
extern bool backspace;

extern bool GameBegin, GameExit;

#ifdef NEWORLD_DEBUG_PERFORMANCE_REC
extern int c_getChunkPtrFromCPA;
extern int c_getChunkPtrFromSearch;
extern int c_getHeightFromHMap;
extern int c_getHeightFromWorldGen;
#endif
