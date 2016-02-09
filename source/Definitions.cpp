#include "Definitions.h"

//Global Vars
float FOVyNormal = 60.0f;       //��Ұ�Ƕ�
float mousemove = 0.2f;         //���������
int viewdistance = 8;           //��Ұ����
int cloudwidth = 10;            //�ƵĿ��
int selectPrecision = 32;       //ѡ�񷽿�ľ���
int selectDistance = 8;         //ѡ�񷽿�ľ���
float walkspeed = 0.15f;        //���ǰ���ٶ�
float runspeed = 0.3f;          //����ܲ��ٶ�
int MaxAirJumps = 3 - 1;        //����N������
bool SmoothLighting = true;     //ƽ������
bool NiceGrass = true;          //�ݵز�������
bool MergeFace = false;         //�ϲ�����Ⱦ
bool GUIScreenBlur = false;     //GUI����ģ��  Void:����㷨�����ˣ��ҹص���  qiaozhanrong:23333��Ҳ��ص�
int linelength = 10;            //��F3��׼���йء�����
int linedist = 30;              //��F3��׼���йء�����
bool ppistretch = false;        //���鹦�ܣ�Ĭ�Ϲر�
float skycolorR = 0.7f;         //�����ɫRed
float skycolorG = 1.0f;         //�����ɫGreen
float skycolorB = 1.0f;         //�����ɫBlue
float FOVyRunning = 8.0f;
float FOVyExt;
double stretch = 1.0f;          //ppi���ű�������gui����ʹ�ã�
int Multisample = 0;            //���ز��������
bool vsync = false;             //��ֱͬ��
int gametime = 0;				//��Ϸʱ�� 0~2592000
//float daylight;

int windowwidth;     //���ڿ��
int windowheight;    //���ڿ��
bool gamebegin, gameexit, bagOpened;

//������Ϸ
bool multiplayer = false;
string serverip;
unsigned short port = 30001;

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
GLFWwindow* MainWindow;
GLFWcursor* MouseCursor;

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

