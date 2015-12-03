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
	//gui::UIButton*	ciArrayBtn = UIButton("ʹ�������������飺" + boolstr(UseCIArray))
	gui::UIButton	rdstbtn  = gui::UIButton(">> ��Ⱦѡ��...");
	gui::UIButton	gistbtn  = gui::UIButton(">> ͼ�ν���ѡ��...");
	gui::UIButton	backbtn  = gui::UIButton("<< �������˵�");
	//gui::UIButton*	savebtn = UIButton("��������")
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


