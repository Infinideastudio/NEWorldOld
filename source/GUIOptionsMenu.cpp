#include "Menus.h"

class GUIOptionsMenu :public gui::Form {
private:
	gui::label title;
	gui::button fontbtn, blurbtn, backbtn;
	void onLoad() {
		title = gui::label("===============< 图形界面选项 >==============", -225, 225, 20, 36, 0.5, 0.5, 0.0, 0.0);
		fontbtn = gui::button("全部使用Unicode字体：", -250, -10, 60, 84, 0.5, 0.5, 0.0, 0.0);
		blurbtn = gui::button("背景模糊：", 10, 250, 60, 84, 0.5, 0.5, 0.0, 0.0);
		backbtn = gui::button("<< 返回选项菜单", -250, 250, -44, -20, 0.5, 0.5, 1.0, 1.0);
		registerControls(4, &title, &fontbtn, &blurbtn, &backbtn);
	}
	void onUpdate() {
		if (fontbtn.clicked) TextRenderer::useUnicodeASCIIFont = !TextRenderer::useUnicodeASCIIFont;
		if (blurbtn.clicked) GUIScreenBlur = !GUIScreenBlur;
		if (backbtn.clicked) ExitSignal = true;
		fontbtn.text = "全部使用Unicode字体：" + BoolYesNo(TextRenderer::useUnicodeASCIIFont);
		blurbtn.text = "背景模糊：" + BoolEnabled(GUIScreenBlur);
	}
};
void GUIoptions() { GUIOptionsMenu Menu; Menu.start(); }