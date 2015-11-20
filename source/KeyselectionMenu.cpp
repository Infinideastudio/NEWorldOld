#include "Menus.h"
#include "TextRenderer.h"
#include <shellapi.h>
//看默认按键不太爽，做一个按键选择<^-^>
void Keyselection(){
	//渲染设置菜单
	gui::Form MainForm;
	int upp = 60;
	int lspc = 36;
	int leftp = windowwidth / 2 - 250;
	int rightp = windowwidth / 2 + 250;
	int midp = windowwidth / 2;
	int downp = windowheight - 20;
	bool f = false;
	MainForm.Init();
	TextRenderer::setFontColor(1.0, 1.0, 1.0, 1.0);
	gui::label*  title = MainForm.createlabel("==============<  渲 染 选 项  >==============");
	gui::button*  VSync = MainForm.createbutton("使用垂直同步");
	gui::button*  backbtn = MainForm.createbutton("<< 返回选项菜单");
	do{
		leftp = windowwidth / 2 - 250;
		rightp = windowwidth / 2 + 250;
		midp = windowwidth / 2;
		downp = windowheight - 20;
		title->resize(midp - 225, midp + 225, 20, 36);
		backbtn->resize(leftp, rightp, downp - 24, downp);
		VSync->resize(leftp, midp - 10, upp + lspc * 0, upp + lspc * 0 + 24);
		//更新GUI
		glfwGetCursorPos(MainWindow, &mx, &my);
		MainForm.mousedata((int)mx, (int)my, mw, mb);
		MainForm.update();
		if (!GetVSyncAvaiablity()) {
			VSync->text = "垂直同步不可用";
			VSync->enabled = false;
		}
		else
		if (IsVSyncEnabled()) VSync->text = "垂直同步已启用"; else VSync->text = "垂直同步未启用";
		if (VSync->clicked){
			if (IsVSyncEnabled()) SetVSyncState(false); else SetVSyncState(true);
		}
		if (backbtn->clicked) f = true;
		MainForm.render();
		glfwSwapBuffers(MainWindow);
		glfwPollEvents();
		if (glfwWindowShouldClose(MainWindow)) exit(0);
	} while (!f);
	MainForm.cleanu