#include "Menus.h"

class GUIOptions : public gui::UIView
{
private:
	gui::UILabel title = gui::UILabel("===============< 图形界面选项 >==============");
	gui::UIButton fontbtn = gui::UIButton("全部使用Unicode字体：" + boolstr(TextRenderer::useUnicodeASCIIFont));
	gui::UIButton backbtn = gui::UIButton("<< 返回选项菜单");

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
		fontbtn.text = "全部使用Unicode字体：" + boolstr(TextRenderer::useUnicodeASCIIFont);
	}

	void OnRender(){

	}
};

void GUIoptions(){
	//GUI设置菜单
	GUIOptions Menu = GUIOptions();
	gui::UIEnter((gui::UIView*)&Menu);

}