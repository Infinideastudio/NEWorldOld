#include "Menus.h"

class GUIOptions : public gui::UIView
{
private:
	gui::UILabel title = gui::UILabel("===============< ͼ�ν���ѡ�� >==============");
	gui::UIButton fontbtn = gui::UIButton("ȫ��ʹ��Unicode���壺" + boolstr(TextRenderer::useUnicodeASCIIFont));
	gui::UIButton backbtn = gui::UIButton("<< ����ѡ��˵�");

public:
	GUIOptions(){
		Init();
		RegisterUI(&title);
		RegisterUI(&fontbtn);
		RegisterUI(&backbtn);
	}

	~GUIOptions(){

	}

	void OnResize(){
		int leftp = windowwidth / 2 - 250;
		int rightp = windowwidth / 2 + 250;
		int midp = windowwidth / 2;
		int upp = 60;
		int downp = windowheight - 20;
		int lspc = 36;

		title.UISetRect(midp - 225, midp + 225, 20, 36);
		fontbtn.UISetRect(leftp, midp - 10, upp + lspc * 0, upp + lspc * 0 + 24);
		backbtn.UISetRect(leftp, rightp, downp - 24, downp);
	}

	void OnUpdate(){
		if (fontbtn.clicked) TextRenderer::useUnicodeASCIIFont = !TextRenderer::useUnicodeASCIIFont;
		if (backbtn.clicked) gui::UIExit();
		fontbtn.text = "ȫ��ʹ��Unicode���壺" + boolstr(TextRenderer::useUnicodeASCIIFont);
	}

	void OnRender(){

	}
};

void GUIoptions(){
	//GUI���ò˵�
	GUIOptions Menu = GUIOptions();
	gui::UIEnter((gui::UIView*)&Menu);

}