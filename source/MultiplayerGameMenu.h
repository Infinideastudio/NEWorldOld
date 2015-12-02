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
		gui::UILabel   title      = gui::UILabel  ("==============<  �� �� �� Ϸ  >==============");
		gui::UITextBox serveriptb = gui::UITextBox("���������IP");
		gui::UIButton  runbtn     = gui::UIButton ("���з�������������");
		gui::UIButton  okbtn      = gui::UIButton ("ȷ��");
		gui::UIButton  backbtn    = gui::UIButton ("<< ����");
		
		bool serveripChanged = false;
	public:
		MultiplayerGameMenu();
		~MultiplayerGameMenu();
		
		void OnResize();
		void OnUpdate();
		void OnRender();
};

#endif
