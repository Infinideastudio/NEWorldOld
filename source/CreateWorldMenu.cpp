#include "Menus.h"

CreateWorldMenu::CreateWorldMenu()
{
	Init(); 
	okbtn.enabled = false;
	inputstr = "";
	RegisterUI(&title);
	RegisterUI(&worldnametb);
	RegisterUI(&okbtn);
	RegisterUI(&backbtn);
}

CreateWorldMenu::~CreateWorldMenu()
{
	if (worldnametbChanged){
		world::worldname = worldnametb.text;
		gamebegin = true;
		multiplayer = false;
	}
}

void CreateWorldMenu::OnResize(){
	int leftp = windowwidth / 2 - 250;
	int rightp = windowwidth / 2 + 250;
	int midp = windowwidth / 2;
	int downp = windowheight - 20;

	title.UISetRect(midp - 225, midp + 225, 20, 36);
	worldnametb.UISetRect(midp - 250, midp + 250, 48, 72);
	okbtn.UISetRect(midp - 250, midp + 250, 84, 120);
	backbtn.UISetRect(leftp, rightp, downp - 24, downp);
}

void CreateWorldMenu::OnUpdate(){

	if (worldnametb.pressed && !worldnametbChanged){
		worldnametb.text = "";
		worldnametbChanged = true;
	}
	if (worldnametb.text == "" || !worldnametbChanged || worldnametb.text.find(" ") != -1) okbtn.enabled = false; else okbtn.enabled = true;
	if (okbtn.clicked){
		gui::UIExit();
	}
	if (backbtn.clicked)gui::UIExit();;
	inputstr = "";
}

void CreateWorldMenu::OnRender(){

}