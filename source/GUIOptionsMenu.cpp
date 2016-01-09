#include "Menus.h"
#include "TextRenderer.h"

namespace Menus {
	class GUIOptionsMenu :public GUI::Form {
	private:
		GUI::label title;
		GUI::button fontbtn, blurbtn, backbtn;
		void onLoad() {
			title = GUI::label("===============< " + GetStrbyKey("gui:caption:GUI Options") + " >==============", -225, 225, 20, 36, 0.5, 0.5, 0.0, 0.0);
			fontbtn = GUI::button(GetStrbyKey("gui:All Unicode Font") + "£º", -250, -10, 60, 84, 0.5, 0.5, 0.0, 0.0);
			blurbtn = GUI::button(GetStrbyKey("gui:BackGround Blur") + "£º", 10, 250, 60, 84, 0.5, 0.5, 0.0, 0.0);
			backbtn = GUI::button("<< " + GetStrbyKey("gui:Back to Option Menu"), -250, 250, -44, -20, 0.5, 0.5, 1.0, 1.0);
			registerControls(4, &title, &fontbtn, &blurbtn, &backbtn);
		}
		void onUpdate() {
			if (fontbtn.clicked) TextRenderer::useUnicodeASCIIFont = !TextRenderer::useUnicodeASCIIFont;
			if (blurbtn.clicked) GUIScreenBlur = !GUIScreenBlur;
			if (backbtn.clicked) ExitSignal = true;
			fontbtn.text = GetStrbyKey("gui:All Unicode Font") + "£º" + BoolYesNo(TextRenderer::useUnicodeASCIIFont);
			blurbtn.text = GetStrbyKey("gui:BackGround Blur") + "£º" + BoolEnabled(GUIScreenBlur);
		}
	};
	void GUIoptions() { GUIOptionsMenu Menu; Menu.start(); }
}
