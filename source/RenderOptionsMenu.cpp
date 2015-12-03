#include "Menus.h"

class RenderOptions : public gui::UIView
{
private:

	gui::UILabel  title = gui::UILabel("==============<  渲 染 选 项  >==============");
	gui::UIButton  backbtn = gui::UIButton("<< 返回选项菜单");
public:
	RenderOptions()
	{
		Init();
		RegisterUI(&title);
		RegisterUI(&backbtn);
	}

	~RenderOptions()
	{

	}

	void OnResize(){
		int leftp = windowwidth / 2 - 250;
		int rightp = windowwidth / 2 + 250;
		int midp = windowwidth / 2;
		int downp = windowheight - 20;

		title.UISetRect(midp - 225, midp + 225, 20, 36);
		backbtn.UISetRect(leftp, rightp, downp - 24, downp);
	}

	void OnUpdate(){
		if (backbtn.clicked) gui::UIExit();
	}

	void OnRender(){

	}
};

void Renderoptions(){
	//渲染设置菜单
	RenderOptions Menu = RenderOptions();
	gui::UIEnter((gui::UIView*)&Menu);
}