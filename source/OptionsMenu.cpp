#include "Menus.h"

void saveoptions();

class OptionsMenu :public gui::Form {
private:
	gui::label title;
	gui::trackbar FOVyBar, mmsBar, viewdistBar;
	gui::button rdstbtn, gistbtn, backbtn, savebtn;
	void onLoad() {
		title = gui::label("=================<  选 项  >=================", -225, 225, 20, 36, 0.5, 0.5, 0.0, 0.0);
		FOVyBar = gui::trackbar(strWithVar("视野角度：", FOVyNormal), 120, (int)(FOVyNormal - 1), -250, -10, 60, 84, 0.5, 0.5, 0.0, 0.0);
		mmsBar = gui::trackbar(strWithVar("鼠标灵敏度：", mousemove), 120, (int)(mousemove * 40 * 2 - 1), 10, 250, 60, 84, 0.5, 0.5, 0.0, 0.0);
		viewdistBar = gui::trackbar(strWithVar("渲染距离：", viewdistance), 120, (viewdistance - 2) * 4 - 1, -250, -10, 96, 120, 0.5, 0.5, 0.0, 0.0);
		rdstbtn = gui::button(">> 渲染选项...", -250, -10, 204, 228, 0.5, 0.5, 0.0, 0.0);
		gistbtn = gui::button(">> 图形界面选项...", 10, 250, 204, 228, 0.5, 0.5, 0.0, 0.0);
		backbtn = gui::button("<< 返回主菜单", -250, -10, -44, -20, 0.5, 0.5, 1.0, 1.0);
		savebtn = gui::button("保存设置", 10, 250, -44, -20, 0.5, 0.5, 1.0, 1.0);
		registerControls(8, &title, &FOVyBar, &mmsBar, &viewdistBar, &rdstbtn, &gistbtn, &backbtn, &savebtn);
	}
	void onUpdate() {
		FOVyNormal = (float)(FOVyBar.barpos + 1);
		mousemove = (mmsBar.barpos / 2 + 1) / 40.0f;
		viewdistance = (viewdistBar.barpos + 1) / 4 + 2;
		if (rdstbtn.clicked) Renderoptions();
		if (gistbtn.clicked) GUIoptions();
		if (backbtn.clicked) ExitSignal = true;
		if (savebtn.clicked) saveoptions();
		FOVyBar.text = strWithVar("视野角度：", FOVyNormal);
		mmsBar.text = strWithVar("鼠标灵敏度：", mousemove);
		viewdistBar.text = strWithVar("渲染距离：", viewdistance);
	}
};
void options() { OptionsMenu Menu; Menu.start(); }