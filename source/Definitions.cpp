#include "Definitions.h"

float FOVyNormal = 70.0f;
float FOVyRunning = 8.0f;
float MouseSpeed = 0.1f;
int RenderDistance = 8;
bool SmoothLighting = true;
bool NiceGrass = true;
bool MergeFace = false;
bool UIBackgroundBlur = false;
int Multisample = 0;
bool VerticalSync = false;
int GameTime = 0;
int WindowWidth = DefaultWindowWidth;
int WindowHeight = DefaultWindowHeight;
double Stretch = 1.0f;

TextureID SplashTexture;
TextureID TitleTexture;
TextureID UIBackgroundTextures[6];
TextureID SelectedTexture;
TextureID UnselectedTexture;
TextureID BlockTextureArray;
TextureID DefaultSkin;

int GLMajorVersion, GLMinorVersion, GLRevisionVersion;
GLFWwindow* MainWindow;
GLFWcursor* MouseCursor;

int mx, my, mxl, myl;
int mw, mb, mbp, mbl, mwl;
double mxdelta, mydelta;
std::string inputstr;

bool GameBegin, GameExit;
bool Multiplayer = false;
std::string ServerIP;
unsigned short ServerPort = 30001;

#ifdef NEWORLD_DEBUG_PERFORMANCE_REC
int c_getChunkPtrFromCPA;
int c_getChunkPtrFromSearch;
int c_getHeightFromHMap;
int c_getHeightFromWorldGen;
#endif
