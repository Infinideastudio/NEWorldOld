#pragma once
#include "Definitions.h"
#include "GUI.h"
<<<<<<< HEAD
#include "Menus.h"
#include "World.h"
#include "Textures.h"
#include "TextRenderer.h"
#include <shellapi.h>

template<typename T>
string strWithVar(string str, T var);
int getDotCount(string s);


=======
>>>>>>> parent of 7e2d023... GUI Improve Persave 1
void mainmenu();
void options();
void Renderoptions();
void GUIoptions();
void worldmenu();
void createworldmenu();
void MultiplayerGameMenu();

class NEMultiplayerGameMenu : public gui::UIView
{
private:
	gui::UILabel   title = gui::UILabel("==============<  �� �� �� Ϸ  >==============");
	gui::UITextBox serveriptb = gui::UITextBox("���������IP");
	gui::UIButton  runbtn = gui::UIButton("���з�������������");
	gui::UIButton  okbtn = gui::UIButton("ȷ��");
	gui::UIButton  backbtn = gui::UIButton("<< ����");

	bool serveripChanged = false;
public:
	NEMultiplayerGameMenu();
	~NEMultiplayerGameMenu();

	void OnResize();
	void OnUpdate();
	void OnRender();
};

class MainMenu : public gui::UIView
{
private:
	int leftp = windowwidth / 2 - 200;
	int midp = windowwidth / 2;
	int rightp = windowwidth / 2 + 200;
	int upp = 280;

	gui::UIButton startbtn = gui::UIButton("������Ϸ");
	gui::UIButton multiplayerbtn = gui::UIButton("������Ϸ");
	gui::UIButton optionsbtn = gui::UIButton(">> ѡ��...");
	gui::UIButton quitbtn = gui::UIButton("�˳�");
public:
	MainMenu();
	~MainMenu();

	void OnResize();
	void OnUpdate();
	void OnRender();
};

class Options : public gui::UIView
{
private:
	gui::UILabel	 title   = gui::UILabel("=================<  ѡ ��  >=================");
	gui::UITrackBar  FOVyBar = gui::UITrackBar(strWithVar("��Ұ�Ƕȣ�", FOVyNormal), 120, (int)(FOVyNormal - 1));
	gui::UITrackBar  mmsBar  = gui::UITrackBar(strWithVar("��������ȣ�", mousemove), 120, (int)(mousemove * 40 * 2 - 1));
	gui::UITrackBar  viewdistBar = gui::UITrackBar(strWithVar("��Ⱦ���룺", viewdistance), 120, (viewdistance - 1) * 8 - 1);
	gui::UIButton	 rdstbtn  = gui::UIButton(">> ��Ⱦѡ��...");
	gui::UIButton	 gistbtn  = gui::UIButton(">> ͼ�ν���ѡ��...");
	gui::UIButton	 backbtn  = gui::UIButton("<< �������˵�");
public:
	Options();
	~Options();

	void OnResize();
	void OnUpdate();
	void OnRender();
};

class RenderOptions : public gui::UIView
{
private:
	gui::UILabel   title   = gui::UILabel("==============<  �� Ⱦ ѡ ��  >==============");
	gui::UIButton  backbtn = gui::UIButton("<< ����ѡ��˵�");

public:
	RenderOptions();
	~RenderOptions();

	void OnResize();
	void OnUpdate();
	void OnRender();
};

class GUIOptions : public gui::UIView
{
private:
	gui::UILabel  title   = gui::UILabel("===============< ͼ�ν���ѡ�� >==============");
	gui::UIButton fontbtn = gui::UIButton("ȫ��ʹ��Unicode���壺" + boolstr(TextRenderer::useUnicodeASCIIFont));
	gui::UIButton backbtn = gui::UIButton("<< ����ѡ��˵�");

public:
	GUIOptions();
	~GUIOptions();

	void OnResize();
	void OnUpdate();
	void OnRender();
};

class NEWorldMenu : public gui::UIView
{
private:
	bool refresh = true;
	int selected = 0, mouseon;
	ubyte mblast = 1;
	int  mwlast = 0;
	string chosenWorldName;
	vector<string> worldnames;
	vector<TextureID> thumbnails, texSizeX, texSizeY;
	int trs = 0;
	int worldcount;
	int leftp = windowwidth / 2 - 200;
	int midp = windowwidth / 2;
	int rightp = windowwidth / 2 + 200;
	int downp = windowheight - 20;

	gui::UILabel          title = gui::UILabel("==============<  ѡ �� �� ��  >==============");
	gui::UIVerticalScroll UIVerticalScroll = gui::UIVerticalScroll(100, 0);
	gui::UIButton         enterbtn = gui::UIButton("����ѡ��������");
	gui::UIButton         deletebtn = gui::UIButton("ɾ��ѡ��������");
	gui::UIButton         backbtn = gui::UIButton("<< �������˵�");

public:
	NEWorldMenu();
	~NEWorldMenu();

	void OnResize();
	void OnUpdate();
	void OnRender();
};

class CreateWorldMenu : public gui::UIView
{
private:
	gui::UILabel   title       = gui::UILabel("==============<  �� �� �� ��  >==============");
	gui::UITextBox worldnametb = gui::UITextBox("������������");
	gui::UIButton  okbtn       = gui::UIButton("ȷ��");
	gui::UIButton  backbtn = gui::UIButton("<< ��������˵�");

	bool worldnametbChanged = false;


public:
	CreateWorldMenu();
	~CreateWorldMenu();

	void OnResize();
	void OnUpdate();
	void OnRender();
};



