#include "Menus.h"

namespace InfinideaStudio
{
	namespace NEWorld
	{
		int getDotCount(string s)
		{
			int ret = 0;
			for(unsigned int i = 0; i != s.size(); i++)
				if(s [i] == '.') ret++;
			return ret;
		}

		class MultiplayerMenu:public UI::Form
		{
		private:
			UI::Label title;
			UI::textbox serveriptb;
			UI::button runbtn, okbtn, backbtn;
			void onLoad()
			{
				title = UI::Label("==============<  �� �� �� Ϸ  >==============", -225, 225, 20, 36, 0.5, 0.5, 0.0, 0.0);
				serveriptb = UI::textbox("���������IP", -225, 225, 20, 36, 0.5, 0.5, 0.0, 0.0);
				runbtn = UI::button("���з�������������", -225, 225, 20, 36, 0.5, 0.5, 0.0, 0.0);
				okbtn = UI::button("ȷ��", -225, 225, 20, 36, 0.5, 0.5, 0.0, 0.0);
				backbtn = UI::button("<< ����", -225, 225, 20, 36, 0.5, 0.5, 0.0, 0.0);
				inputstr = "";
				okbtn.enabled = false;
				registerControls(4, &title, &serveriptb, &runbtn, &okbtn, &backbtn);
			}
			void onUpdate()
			{
				static bool serveripChanged = false;
				if(runbtn.clicked) WinExec("NEWorldServer.exe", SW_SHOWDEFAULT);
				if(okbtn.clicked)
				{
					serverip = serveriptb.text;
					gamebegin = true;
					multiplayer = true;
				}
				if(backbtn.clicked) ExitSignal = true;
				if(serveriptb.pressed && !serveripChanged)
				{
					serveriptb.text = "";
					serveripChanged = true;
				}
				if(serveriptb.text == "" || !serveripChanged || getDotCount(serveriptb.text) != 3) { okbtn.enabled = false; }
				else { okbtn.enabled = true; }
				inputstr = "";
			}
		};

		void multiplayermenu() { MultiplayerMenu Menu; Menu.start(); }
	}
}