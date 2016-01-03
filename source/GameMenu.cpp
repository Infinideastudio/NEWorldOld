#include "Menus.h"
#include "TextRenderer.h"

namespace InfinideaStudio
{
	namespace NEWorld
	{
		class GameMenu:public UI::Form
		{
		private:
			UI::Label title;
			UI::button resumebtn, exitbtn;
			void onLoad()
			{
				title = UI::Label("==============<  �� Ϸ �� ��  >==============", -225, 225, 0, 16, 0.5, 0.5, 0.25, 0.25);
				resumebtn = UI::button("������Ϸ", -200, 200, -35, -3, 0.5, 0.5, 0.5, 0.5);
				exitbtn = UI::button("<< �������˵�", -200, 200, 3, 35, 0.5, 0.5, 0.5, 0.5);
				registerControls(3, &title, &resumebtn, &exitbtn);
			}
			void onUpdate()
			{
				MutexUnlock(Mutex);
				//Make update thread realize that it should pause
				MutexLock(Mutex);
				if(resumebtn.clicked) ExitSignal = true;
				if(exitbtn.clicked) gameexit = ExitSignal = true;
			}
		};
		void gamemenu() { GameMenu Menu; Menu.start(); }
	}
}