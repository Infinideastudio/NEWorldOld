#include "Menus.h"

class CreateWorldMenu : public gui::UIView
{
private:
	bool worldnametbChanged = false;

	gui::UILabel title = gui::UILabel("==============<  新 建 世 界  >==============");
	gui::UITextBox worldnametb = gui::UITextBox("输入世界名称");
	gui::UIButton okbtn = gui::UIButton("确定");
	gui::UIButton backbtn = gui::UIButton("<< 返回世界菜单");
public:
	CreateWorldMenu(){
		Init();
		RegisterUI(&title);
		RegisterUI(&worldnametb);
		RegisterUI(&okbtn);
		RegisterUI(&backbtn);
		inputstr = "";
		okbtn.enabled = false;
	}

	~CreateWorldMenu(){
		if (worldnametbChanged){
			world::worldname = worldnametb.text;
			gamebegin = true;
			multiplayer = false;
		}
	}

	void OnResize(){
		int leftp = windowwidth / 2 - 250;
		int rightp = windowwidth / 2 + 250;
		int midp = windowwidth / 2;
		int downp = windowheight - 20;

		title.UISetRect(midp - 225, midp + 225, 20, 36);
		worldnametb.UISetRect(midp - 250, midp + 250, 48, 72);
		okbtn.UISetRect(midp - 250, midp + 250, 84, 120);
		backbtn.UISetRect(leftp, rightp, downp - 24, downp);
	}

	void OnUpdate(){
		if (worldnametb.pressed && !worldnametbChanged){
			worldnametb.text = "";
			worldnametbChanged = true;
		}
		if (worldnametb.text == "" || !worldnametbChanged || worldnametb.text.find(" ") != -1) okbtn.enabled = false; else okbtn.enabled = true;
		if (okbtn.clicked){
			gui::UIExit();
		}
		if (backbtn.clicked) gui::UIExit();
		inputstr = "";
	}

	void OnRender(){

	}
};


void createworldmenu(){
	CreateWorldMenu Menu = CreateWorldMenu();
	gui::UIEnter((gui::UIView*)&Menu);

}