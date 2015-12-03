#include "Menus.h"

RenderOptions::RenderOptions()
{
	Init();
	RegisterUI(&title);
	RegisterUI(&backbtn);
}

RenderOptions::~RenderOptions()
{

}

void RenderOptions::OnResize(){
	int leftp = windowwidth / 2 - 250;
	int rightp = windowwidth / 2 + 250;
	int midp = windowwidth / 2;
	int downp = windowheight - 20;

	title.UISetRect(midp - 225, midp + 225, 20, 36);
	backbtn.UISetRect(leftp, rightp, downp - 24, downp);
}

void RenderOptions::OnUpdate(){
	if (backbtn.clicked) gui::UIExit();
}

void RenderOptions::OnRender(){

}