#include "Menus.h"
#include "TextRenderer.h"

class GameMenu :public gui::Form {
private:
	gui::label title;
	gui::button resumebtn, exitbtn;
	void onLoad() {
		title = gui::label("==============<  游 戏 菜 单  >==============", -225, 225, 0, 16, 0.5, 0.5, 0.25, 0.25);
		resumebtn = gui::button("继续游戏", -200, 200, -35, -3, 0.5, 0.5, 0.5, 0.5);
		exitbtn = gui::button("<< 返回主菜单", -200, 200, 3, 35, 0.5, 0.5, 0.5, 0.5);
		registerControls(3, &title, &resumebtn, &exitbtn);
	}
	void onUpdate() {
		MutexUnlock(Mutex);
		//Make update thread realize that it should pause
		MutexLock(Mutex);
		if (resumebtn.clicked) ExitSignal = true;
		if (exitbtn.clicked) gameexit = ExitSignal = true;
	}
};
void gamemenu() { GameMenu Menu; Menu.start(); }