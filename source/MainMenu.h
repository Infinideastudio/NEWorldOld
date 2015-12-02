#ifndef MAINMENU_H
#define MAINMENU_H

#include "GUI.h"

class MainMenu : public gui::UIView
{
	
	gui::UIButton startbtn       = gui::UIButton("单人游戏");
	gui::UIButton multiplayerbtn = gui::UIButton("多人游戏");
	gui::UIButton optionsbtn     = gui::UIButton(">> 选项...");
	gui::UIButton quitbtn        = gui::UIButton("退出");
	public:
		MainMenu();
		~MainMenu();
	protected:
};

#endif
