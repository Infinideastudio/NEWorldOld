#include "Menus.h"
#include "TextRenderer.h"
#include "Setup.h"

namespace Menus {
	class GUIOptionsMenu :public GUI::Form {
	private:
		GUI::label title;
		GUI::trackbar fontbar;
		GUI::button blurbtn, ppistretchbtn, backbtn;
		void onLoad() {
			title = GUI::label(GetStrbyKey("NEWorld.gui.caption"), -225, 225, 20, 36, 0.5, 0.5, 0.0, 0.0);
			title.centered = true;
			fontbar = GUI::trackbar("", 120, (TextRenderer::FontSize - 16) * 5 - 1, -250, -10, 60, 84, 0.5, 0.5, 0.0, 0.0);
			blurbtn = GUI::button("", 10, 250, 60, 84, 0.5, 0.5, 0.0, 0.0);
			ppistretchbtn = GUI::button("", -250, -10, 96, 120, 0.5, 0.5, 0.0, 0.0);
			backbtn = GUI::button(GetStrbyKey("NEWorld.gui.back"), -250, 250, -44, -20, 0.5, 0.5, 1.0, 1.0);
			registerControls(5, &title, &fontbar, &blurbtn, &ppistretchbtn, &backbtn);
		}
		void onUpdate() {
			TextRenderer::FontSize = (fontbar.barpos + 1) / 5 + 16;
			if (blurbtn.clicked) UIBackgroundBlur = !UIBackgroundBlur;
			if (ppistretchbtn.clicked) UIStretch = !UIStretch;
			if (backbtn.clicked) {
				initStretch();
				TextRenderer::initFont(true);
				ExitSignal = true;
			}
			fontbar.text = GetStrbyKey("NEWorld.gui.fontsize") + Var2Str(TextRenderer::FontSize);
			blurbtn.text = GetStrbyKey("NEWorld.gui.blur") + BoolEnabled(UIBackgroundBlur);
			ppistretchbtn.text = GetStrbyKey("NEWorld.gui.stretch") + BoolEnabled(UIStretch);
		}
	};
	void GUIoptions() { GUIOptionsMenu Menu; Menu.start(); }
}
