#include "MultiplayerGameMenu.h"

MultiplayerGameMenu::MultiplayerGameMenu() {
	Init();
	RegisterUI(&title);
	RegisterUI(&serveriptb);
	RegisterUI(&runbtn);
	RegisterUI(&okbtn);
	RegisterUI(&btn);

	inputstr = "";
	okbtn.enabled = false;
}

MultiplayerGameMenu::~MultiplayerGameMenu() {
	if (serveripChanged) {
		serverip = serveriptb.text;
		gamebegin = true;
		multiplayer = true;
	}
}

void MultiplayerGameMenu::OnResize() {
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

void MultiplayerGameMenu::OnUpdate() {
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

void MultiplayerGameMenu::OnRender() {

}
