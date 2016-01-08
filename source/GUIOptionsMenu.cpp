#include "Menus.h"
#include "TextRenderer.h"

namespace Menus {
	class GUIOptionsMenu :public GUI::Form {
	private:
		GUI::label title;
		GUI::button fontbtn, blurbtn, backbtn;
		void onLoad() {
			title = GUI::label("===============< ͼ�ν���ѡ�� >==============", -225, 225, 20, 36, 0.5, 0.5, 0.0, 0.0);
			fontbtn = GUI::button("ȫ��ʹ��Unicode���壺", -250, -10, 60, 84, 0.5, 0.5, 0.0, 0.0);
			blurbtn = GUI::button("����ģ����", 10, 250, 60, 84, 0.5, 0.5, 0.0, 0.0);
			backbtn = GUI::button("<< ����ѡ��˵�", -250, 250, -44, -20, 0.5, 0.5, 1.0, 1.0);
			registerControls(4, &title, &fontbtn, &blurbtn, &backbtn);
		}
		void onUpdate() {
			if (fontbtn.clicked) TextRenderer::useUnicodeASCIIFont = !TextRenderer::useUnicodeASCIIFont;
			if (blurbtn.clicked) GUIScreenBlur = !GUIScreenBlur;
			if (backbtn.clicked) ExitSignal = true;
			fontbtn.text = "ȫ��ʹ��Unicode���壺" + BoolYesNo(TextRenderer::useUnicodeASCIIFont);
			blurbtn.text = "����ģ����" + BoolEnabled(GUIScreenBlur);
		}
	};
	void GUIoptions() { GUIOptionsMenu Menu; Menu.start(); }
}