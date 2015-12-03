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
	gui::UILabel   title = gui::UILabel("==============<  多 人 游 戏  >==============");
	gui::UITextBox serveriptb = gui::UITextBox("输入服务器IP");
	gui::UIButton  runbtn = gui::UIButton("运行服务器（开服）");
	gui::UIButton  okbtn = gui::UIButton("确定");
	gui::UIButton  backbtn = gui::UIButton("<< 返回");

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

	gui::UIButton startbtn = gui::UIButton("单人游戏");
	gui::UIButton multiplayerbtn = gui::UIButton("多人游戏");
	gui::UIButton optionsbtn = gui::UIButton(">> 选项...");
	gui::UIButton quitbtn = gui::UIButton("退出");
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
	gui::UILabel	 title   = gui::UILabel("=================<  选 项  >=================");
	gui::UITrackBar  FOVyBar = gui::UITrackBar(strWithVar("视野角度：", FOVyNormal), 120, (int)(FOVyNormal - 1));
	gui::UITrackBar  mmsBar  = gui::UITrackBar(strWithVar("鼠标灵敏度：", mousemove), 120, (int)(mousemove * 40 * 2 - 1));
	gui::UITrackBar  viewdistBar = gui::UITrackBar(strWithVar("渲染距离：", viewdistance), 120, (viewdistance - 1) * 8 - 1);
	gui::UIButton	 rdstbtn  = gui::UIButton(">> 渲染选项...");
	gui::UIButton	 gistbtn  = gui::UIButton(">> 图形界面选项...");
	gui::UIButton	 backbtn  = gui::UIButton("<< 返回主菜单");
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
	gui::UILabel   title   = gui::UILabel("==============<  渲 染 选 项  >==============");
	gui::UIButton  backbtn = gui::UIButton("<< 返回选项菜单");

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
	gui::UILabel  title   = gui::UILabel("===============< 图形界面选项 >==============");
	gui::UIButton fontbtn = gui::UIButton("全部使用Unicode字体：" + boolstr(TextRenderer::useUnicodeASCIIFont));
	gui::UIButton backbtn = gui::UIButton("<< 返回选项菜单");

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

	gui::UILabel          title = gui::UILabel("==============<  选 择 世 界  >==============");
	gui::UIVerticalScroll UIVerticalScroll = gui::UIVerticalScroll(100, 0);
	gui::UIButton         enterbtn = gui::UIButton("进入选定的世界");
	gui::UIButton         deletebtn = gui::UIButton("删除选定的世界");
	gui::UIButton         backbtn = gui::UIButton("<< 返回主菜单");

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
	gui::UILabel   title       = gui::UILabel("==============<  新 建 世 界  >==============");
	gui::UITextBox worldnametb = gui::UITextBox("输入世界名称");
	gui::UIButton  okbtn       = gui::UIButton("确定");
	gui::UIButton  backbtn = gui::UIButton("<< 返回世界菜单");

	bool worldnametbChanged = false;


public:
	CreateWorldMenu();
	~CreateWorldMenu();

	void OnResize();
	void OnUpdate();
	void OnRender();
};



