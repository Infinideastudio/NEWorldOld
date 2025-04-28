#include "Definitions.h"

//Global Vars
float FOVyNormal = 70.0f;       //��Ұ�Ƕ�
float mousemove = 0.2f;         //��������
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
bool GUIScreenBlur = false;      //GUI����ģ��  Void:����㷨�����ˣ��ҹص���
int linelength = 10;            //��F3��׼���йء�����
int linedist = 30;              //��F3��׼���йء�����
float skycolorR = 0.70f;         //�����ɫRed
float skycolorG = 0.80f;         //�����ɫGreen
float skycolorB = 0.86f;         //�����ɫBlue
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

TextureID BlockTextureArray;
TextureID tex_select, tex_unselect, tex_splash, tex_title, tex_mainmenu[6];
TextureID DefaultSkin;

//OpenGL
int GLVersionMajor, GLVersionMinor, GLVersionRev;
//GLFW
GLFWwindow* MainWindow;
GLFWcursor* MouseCursor;

//�����������
int mx, my, mxl, myl;
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
