#include "Menus.h"

RenderOptions::RenderOptions()
{
	Init();
	RegisterUI(&title);
	RegisterUI(&FOVyBar);
	RegisterUI(&mmsBar);
	RegisterUI(&viewdistBar);
	//RegisterUI(&ciArrayBtn = UIButton("ʹ�������������飺" + boolstr(UseCIArray))
	RegisterUI(&rdstbtn);
	RegisterUI(&gistbtn);
	RegisterUI(&backbtn);
	//ciArrayBtn.enabled = false
	//savebtn.enabled = false
	//gui::UIButton*	savebtn = UIButton("��������")
}

RenderOptions::~RenderOptions()
{

}

void RenderOptions::OnResize(){
	int leftp = windowwidth / 2 - 250;
	int rightp = windowwidth / 2 + 250;
	int midp = windowwidth / 2;
	int upp = 60;
	int downp = windowheight - 20;
	int lspc = 36;
	title.UISetRect(midp - 225, midp + 225, 20, 36);
	FOVyBar.UISetRect(leftp, midp - 10, upp + lspc * 0, upp + lspc * 0 + 24);
	mmsBar.UISetRect(midp + 10, rightp, upp + lspc * 0, upp + lspc * 0 + 24);
	viewdistBar.UISetRect(leftp, midp - 10, upp + lspc * 1, upp + lspc * 1 + 24);
	//ciArrayBtn.UISetRect(midp + 10, rightp, upp + lspc * 1, upp + lspc * 1 + 24)
	rdstbtn.UISetRect(leftp, midp - 10, upp + lspc * 4, upp + lspc * 4 + 24);
	gistbtn.UISetRect(midp + 10, rightp, upp + lspc * 4, upp + lspc * 4 + 24);
	backbtn.UISetRect(leftp, midp - 10, downp - 24, downp);
	//savebtn.UISetRect(midp + 10, rightp, downp - 24, downp)

}

void RenderOptions::OnUpdate(){
	FOVyNormal = static_cast<float>(FOVyBar.barpos + 1);
	mousemove = (mmsBar.barpos / 2 + 1) / 40.0f;
	viewdistance = viewdistBar.barpos / 8 + 2;
	FOVyBar.text = strWithVar("��Ұ�Ƕȣ�", FOVyNormal);
	mmsBar.text = strWithVar("��������ȣ�", mousemove);
	viewdistBar.text = strWithVar("��Ⱦ���룺", viewdistance);
	//ciArrayBtn.text = strWithVar("ʹ��������������, ", boolstr(UseCIArray))
	//if (ciArrayBtn.clicked) UseCIArray = !UseCIArray
	if (rdstbtn.clicked) Renderoptions();
	if (gistbtn.clicked) GUIoptions();
	if (backbtn.clicked) gui::UIExit();
}

void RenderOptions::OnRender(){

}