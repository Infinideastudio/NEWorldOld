#include "Menus.h"
#include "TextRenderer.h"
#include <shellapi.h>
#include <fstream>
using namespace std;
//看默认按键不太爽，做一个按键选择<^-^>
vector<int>keys;//按键
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
	gui::button*  backbtn = MainForm.createbutton("<< 返回选项菜单");
	map<int, gui::button*>Keyslbtns;
	map<int, string>Keynames;
	ifstream kslf("keyslsetting.txt");
	if (kslf.good()){
		while (!kslf.eof()){
			int t;
			kslf >> t;
			keys.push_back(t);
			kslf >> Keynames[keys.size() - 1];
			Keyslbtns[keys.size() - 1] = MainForm.createbutton(Keynames[keys.size() - 1] + ":" + (char)keys[keys.size() - 1]);
		}
	}
	kslf.close();
	do{
		leftp = windowwidth / 2 - 250;
		rightp = windowwidth / 2 + 250;
		midp = windowwidth / 2;
		downp = windowheight - 20;
		title->resize(midp - 225, midp + 225, 20, 36);
		backbtn->resize(leftp, rightp, downp - 24, downp);
		if (Keyslbtns.size() > 0){
			int j = 0;
			for (int i = 0; i < Keyslbtns.size();){
				Keyslbtns[i]->resize(leftp, midp - 10, upp + lspc * j, upp + lspc * j + 24);
				i++;
				if (i<Keyslbtns.size()) Keyslbtns[i]->resize(midp + 10, rightp, upp + lspc * j, upp + lspc * j + 24);
				i++; j++;
			}
		}
		//VSync->resize(leftp, midp - 10, upp + lspc * 0, upp + lspc * 0 + 24);
		//更新GUI
		glfwGetCursorPos(MainWindow, &mx, &my);
		MainForm.mousedata((int)mx, (int)my, mw, mb);
		MainForm.update();
		
		if (backbtn->clicked) f = true;
		MainForm.render();
		glfwSwapBuffers(MainWindow);
		glfwPollEvents();
		if (glfwWindowShouldClose(MainWindow)) exit(0);
	} while (!f);
	MainForm.cleanup();
}