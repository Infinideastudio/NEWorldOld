#include "Menus.h"

int getDotCount(string s) {
	int ret = 0;
	for (unsigned int i = 0; i != s.size(); i++)
	if (s[i] == '.') ret++;
	return ret;
}

class NEMultiplayerGameMenu : public gui::UIView
{
private:
	gui::UILabel   title = gui::UILabel("==============<  多 人 游 戏  >==============");
	gui::UITextBox serveriptb = gui::UITextBox("输入服务器IP");
	gui::UIButton  runbtn = gui::UIButton("运行服务器（开服）");
	gui::UIButton  okbtn = gui::UIButton("确定");
	gui::UIButton  backbtn = gui::UIButton("<< 返回");

	bool serveripChanged = false;
public:
	NEMultiplayerGameMenu() {
		Init();
		RegisterUI(&title);
		RegisterUI(&serveriptb);
		RegisterUI(&runbtn);
		RegisterUI(&okbtn);
		RegisterUI(&backbtn);

		inputstr = "";
		okbtn.enabled = false;
	}

	~NEMultiplayerGameMenu() {
		if (serveripChanged) {
			serverip = serveriptb.text;
			gamebegin = true;
			multiplayer = true;
		}
	}

	void OnResize() {
		int leftp = windowwidth / 2 - 250;
		int rightp = windowwidth / 2 + 250;
		int midp = windowwidth / 2;
		int downp = windowheight - 20;

		title.UISetRect(midp - 225, midp + 225, 20, 36);
		serveriptb.UISetRect(midp - 250, midp + 250, 48, 72);
		runbtn.UISetRect(leftp, rightp, downp - 24 - 50, downp - 50);
		okbtn.UISetRect(midp - 250, midp + 250, 84, 120);
		backbtn.UISetRect(leftp, rightp, downp - 24, downp);
	}

	void OnUpdate() {
		if (serveriptb.pressed && !serveripChanged) {
			serveriptb.text = "";
			serveripChanged = true;
		}

		if (serveriptb.text == "" || !serveripChanged || getDotCount(serveriptb.text) != 3) okbtn.enabled = false;
		else okbtn.enabled = true;
		if (okbtn.clicked || backbtn.clicked)  gui::UIExit;
		if (runbtn.clicked) WinExec("NEWorldServer.exe", SW_SHOWDEFAULT);
		inputstr = "";
	}

	void OnRender() {

	}

};

void MultiplayerGameMenu() {
	NEMultiplayerGameMenu Menu = NEMultiplayerGameMenu();
	gui::UIEnter((gui::UIView*)&Menu);
}
