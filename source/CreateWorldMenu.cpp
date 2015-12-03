#include "Menus.h"

CreateWorldMenu::CreateWorldMenu()
{
	Init();
	RegisterUI(&title);
	RegisterUI(&fontbtn);
	RegisterUI(&backbtn);
}

CreateWorldMenu::~CreateWorldMenu()
{

}

void CreateWorldMenu::OnResize(){
	int leftp = windowwidth / 2 - 250;
	int rightp = windowwidth / 2 + 250;
	int midp = windowwidth / 2;
	int upp = 60;
	int downp = windowheight - 20;
	int lspc = 36;
	title.UISetRect(midp - 225, midp + 225, 20, 36);
	fontbtn.UISetRect(leftp, midp - 10, upp + lspc * 0, upp + lspc * 0 + 24);
	backbtn.UISetRect(leftp, rightp, downp - 24, downp);
}

void CreateWorldMenu::OnUpdate(){
	if (fontbtn.clicked) TextRenderer::useUnicodeASCIIFont = !TextRenderer::useUnicodeASCIIFont;
	if (backbtn.clicked) gui::UIExit();
	fontbtn.text = "全部使用Unicode字体：" + boolstr(TextRenderer::useUnicodeASCIIFont);
}

void CreateWorldMenu::OnRender(){

}