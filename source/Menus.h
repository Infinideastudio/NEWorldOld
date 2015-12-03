#pragma once
#include "Definitions.h"
#include "GUI.h"
#include "Menus.h"
#include "World.h"
#include "Textures.h"
#include "TextRenderer.h"
#include <shellapi.h>

template<typename T>
string strWithVar(string str, T var){
	std::stringstream ss;
	ss << str << var;
	return ss.str();
}
int getDotCount(string s) {
	int ret = 0;
	for (unsigned int i = 0; i != s.size(); i++)
	if (s[i] == '.') ret++;
	return ret;
}


void mainmenu();
void options();
void Renderoptions();
void GUIoptions();
void worldmenu();
void createworldmenu();

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
	//gui::UIButton*	ciArrayBtn = UIButton("使用区块索引数组：" + boolstr(UseCIArray))
	gui::UIButton	rdstbtn  = gui::UIButton(">> 渲染选项...");
	gui::UIButton	gistbtn  = gui::UIButton(">> 图形界面选项...");
	gui::UIButton	backbtn  = gui::UIButton("<< 返回主菜单");
	//gui::UIButton*	savebtn = UIButton("保存设置")
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


