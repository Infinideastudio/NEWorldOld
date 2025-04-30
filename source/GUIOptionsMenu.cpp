#include "Menus.h"
#include "TextRenderer.h"
#include "Setup.h"

namespace Menus {
	class GUIOptionsMenu : public GUI::Form {
	private:
		GUI::Label title = GUI::Label("", -225, 225, 20, 36, 0.5, 0.5, 0.0, 0.0);
		GUI::Trackbar fontbar = GUI::Trackbar("", 120, (TextRenderer::FontSize - 16) * 5 - 1, -250, -10, 60, 84, 0.5, 0.5, 0.0, 0.0);
		GUI::Button blurbtn = GUI::Button("", 10, 250, 60, 84, 0.5, 0.5, 0.0, 0.0);
		GUI::Button ppistretchbtn = GUI::Button("", -250, -10, 96, 120, 0.5, 0.5, 0.0, 0.0);
		GUI::Button backbtn = GUI::Button("", -250, 250, -44, -20, 0.5, 0.5, 1.0, 1.0);

		void onLoad() {
			title.centered = true;
			registerControls({ &title, &fontbar, &blurbtn, &ppistretchbtn, &backbtn });
		}

		void onUpdate() {
			title.text = GetStrbyKey("NEWorld.gui.caption");
			fontbar.text = GetStrbyKey("NEWorld.gui.fontsize") + Var2Str(TextRenderer::FontSize);
			blurbtn.text = GetStrbyKey("NEWorld.gui.blur") + BoolEnabled(UIBackgroundBlur);
			ppistretchbtn.text = GetStrbyKey("NEWorld.gui.stretch") + BoolEnabled(UIStretch);
			backbtn.text = GetStrbyKey("NEWorld.gui.back");

			TextRenderer::FontSize = (fontbar.barpos + 1) / 5 + 16;
			if (blurbtn.clicked) UIBackgroundBlur = !UIBackgroundBlur;
			if (ppistretchbtn.clicked) UIStretch = !UIStretch;
			if (backbtn.clicked) {
				initStretch();
				TextRenderer::initFont(true);
				exit = true;
			}
		}
	};

	void uioptions() { GUIOptionsMenu Menu; Menu.start(); }
}
