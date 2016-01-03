#include "Menus.h"
#include "World.h"

namespace InfinideaStudio
{
	namespace NEWorld
	{
		class CreateWorldMenu:public UI::Form
		{
		private:
			bool worldnametbChanged;
			UI::Label title;
			UI::textbox worldnametb;
			UI::button okbtn, backbtn;
			void onLoad()
			{
				title = UI::Label("==============<  新 建 世 界  >==============", -225, 225, 20, 36, 0.5, 0.5, 0.0, 0.0);
				worldnametb = UI::textbox("输入世界名称", -250, 250, 48, 72, 0.5, 0.5, 0.0, 0.0);
				okbtn = UI::button("确定", -250, 250, 84, 120, 0.5, 0.5, 0.0, 0.0);
				backbtn = UI::button("<< 返回世界菜单", -250, 250, -44, -20, 0.5, 0.5, 1.0, 1.0);
				registerControls(4, &title, &worldnametb, &okbtn, &backbtn);
				inputstr = "";
				okbtn.enabled = false;
				worldnametbChanged = false;
			}
			void onUpdate()
			{
				if(worldnametb.pressed && !worldnametbChanged)
				{
					worldnametb.text = "";
					worldnametbChanged = true;
				}
				if(worldnametb.text == "" || !worldnametbChanged || worldnametb.text.find(" ") != -1)
					okbtn.enabled = false;
				else okbtn.enabled = true;
				if(okbtn.clicked)
				{
					if(worldnametb.text != "")
					{
						world::worldname = worldnametb.text;
						gamebegin = true;
					}
					ExitSignal = true;
				}
				if(backbtn.clicked) ExitSignal = true;
				inputstr = "";
			}
		};
		void createworldmenu() { CreateWorldMenu Menu; Menu.start(); }
	}
}