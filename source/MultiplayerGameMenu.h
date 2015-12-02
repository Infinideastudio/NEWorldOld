#ifndef MULTIPLAYERGAMEMENU_H
#define MULTIPLAYERGAMEMENU_H

#include "GUI.h"
#include "World.h"
#include "Textures.h"
#include "TextRenderer.h"
#include <shellapi.h>


class MultiplayerGameMenu : public gui::UILabel
{
	private	
		gui::UILabel   title      = gui::UILabel  ("==============<  多 人 游 戏  >==============");
		gui::UITextBox serveriptb = gui::UITextBox("输入服务器IP");
		gui::UIButton  runbtn     = gui::UIButton ("运行服务器（开服）");
		gui::UIButton  okbtn      = gui::UIButton ("确定");
		gui::UIButton  backbtn    = gui::UIButton ("<< 返回");
		
		bool serveripChanged = false;
	public:
		MultiplayerGameMenu();
		~MultiplayerGameMenu();
		
		void OnResize();
		void OnUpdate();
		void OnRender();
};

#endif
