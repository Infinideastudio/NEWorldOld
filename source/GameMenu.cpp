#include "Menus.h"
#include "TextRenderer.h"
#include "World.h"

namespace Menus {
	class GameMenu :public GUI::Form {
	private:
		GUI::label title;
		GUI::button resumebtn, exitbtn;
		void onLoad() {
			title = GUI::label(GetStrbyKey("NEWorld.pause.caption"), -225, 225, -76, -60, 0.5, 0.5, 0.5, 0.5);
			title.centered = true;
			resumebtn = GUI::button(GetStrbyKey("NEWorld.pause.continue"), -200, 200, -36, -3, 0.5, 0.5, 0.5, 0.5);
			exitbtn = GUI::button(GetStrbyKey("NEWorld.pause.back"), -200, 200, 3, 36, 0.5, 0.5, 0.5, 0.5);
			registerControls(3, &title, &resumebtn, &exitbtn);
		}
		void onUpdate() {
			if (resumebtn.clicked) ExitSignal = true;
			if (exitbtn.clicked) gameexit = ExitSignal = true;
		}
		void onLeave() {
			if (gameexit) {
				World::saveAllChunks();
				World::destroyAllChunks();
			}
		}
	};
	void gamemenu() { GameMenu Menu; Menu.start(); }
}
