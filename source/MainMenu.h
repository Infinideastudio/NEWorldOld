#ifndef MAINMENU_H
#define MAINMENU_H

#include "GUI.h"

class MainMenu : public gui::UIView
{
	
	gui::UIButton startbtn       = gui::UIButton("������Ϸ");
	gui::UIButton multiplayerbtn = gui::UIButton("������Ϸ");
	gui::UIButton optionsbtn     = gui::UIButton(">> ѡ��...");
	gui::UIButton quitbtn        = gui::UIButton("�˳�");
	public:
		MainMenu();
		~MainMenu();
	protected:
};

#endif
