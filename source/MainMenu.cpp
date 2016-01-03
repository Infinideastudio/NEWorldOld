﻿#include "Menus.h"
#include "TextRenderer.h"

namespace InfinideaStudio
{
	namespace NEWorld
	{
		class MainMenu:public UI::Form
		{
private:
			UI::imagebox title;
			UI::button startbtn, optionsbtn, quitbtn;
			void onLoad()
			{
				title = UI::imagebox(0.0f, 1.0f, 0.5f, 1.0f, tex_title, -256, 256, 20, 276, 0.5, 0.5, 0.0, 0.0);
				startbtn = UI::button("开始游戏", -200, 200, 280, 312, 0.5, 0.5, 0.0, 0.0);
				optionsbtn = UI::button(">> 选项...", -200, -3, 318, 352, 0.5, 0.5, 0.0, 0.0);
				quitbtn = UI::button("退出", 3, 200, 318, 352, 0.5, 0.5, 0.0, 0.0);
		registerControls(4, &title, &startbtn, &optionsbtn, &quitbtn);
	}
			void onUpdate()
			{
				if(startbtn.clicked) worldmenu();
				if(gamebegin) ExitSignal = true;
				if(optionsbtn.clicked) options();
				if(quitbtn.clicked) exit(0);
	}
};
void mainmenu() { MainMenu Menu; Menu.start(); }
	}
}