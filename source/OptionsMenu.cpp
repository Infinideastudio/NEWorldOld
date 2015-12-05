#include "Menus.h"

template<typename T>
string strWithVar(string str, T var){
	std::stringstream ss;
	ss << str << var;
	return ss.str();
}

class Options : public gui::UIView
{
private:
	gui::UILabel	 title = gui::UILabel("=================<  选 项  >=================");
	gui::UITrackBar  FOVyBar = gui::UITrackBar(strWithVar("视野角度：", FOVyNormal), 120, (int)(FOVyNormal - 1));
	gui::UITrackBar  mmsBar = gui::UITrackBar(strWithVar("鼠标灵敏度：", mousemove), 120, (int)(mousemove * 40 * 2 - 1));
	gui::UITrackBar  viewdistBar = gui::UITrackBar(strWithVar("渲染距离：", viewdistance), 120, (viewdistance - 1) * 8 - 1);
	//gui::UIButton*	ciArrayBtn = UIButton("使用区块索引数组：" + boolstr(UseCIArray))
	gui::UIButton	rdstbtn = gui::UIButton(">> 渲染选项...");
	gui::UIButton	gistbtn = gui::UIButton(">> 图形界面选项...");
	gui::UIButton	backbtn = gui::UIButton("<< 返回主菜单");
	//gui::UIButton*	savebtn = UIButton("保存设置")
public:
	Options()
	{
		Init();
		RegisterUI(&title);
		RegisterUI(&FOVyBar);
		RegisterUI(&mmsBar);
		RegisterUI(&viewdistBar);
		//RegisterUI(&ciArrayBtn = UIButton("使用区块索引数组：" + boolstr(UseCIArray))
		RegisterUI(&rdstbtn);
		RegisterUI(&gistbtn);
		RegisterUI(&backbtn);
		//ciArrayBtn.enabled = false
		//savebtn.enabled = false
		//gui::UIButton*	savebtn = UIButton("保存设置")
	}

	~Options()
	{

	}

	void OnResize(){
		int leftp = windowwidth / 2 - 250;
		int rightp = windowwidth / 2 + 250;
		int midp = windowwidth / 2;
		int upp = 60;
		int downp = windowheight - 20;
		int lspc = 36;
		title.UISetRect(midp - 225, midp + 225, 20, 36);
		FOVyBar.UISetRect(leftp, midp - 10, upp + lspc * 0, upp + lspc * 0 + 24);
		mmsBar.UISetRect(midp + 10, rightp, upp + lspc * 0, upp + lspc * 0 + 24);
		viewdistBar.UISetRect(leftp, midp - 10, upp + lspc * 1, upp + lspc * 1 + 24);
		//ciArrayBtn.UISetRect(midp + 10, rightp, upp + lspc * 1, upp + lspc * 1 + 24)
		rdstbtn.UISetRect(leftp, midp - 10, upp + lspc * 4, upp + lspc * 4 + 24);
		gistbtn.UISetRect(midp + 10, rightp, upp + lspc * 4, upp + lspc * 4 + 24);
		backbtn.UISetRect(leftp, midp - 10, downp - 24, downp);
		//savebtn.UISetRect(midp + 10, rightp, downp - 24, downp)

	}

	void OnUpdate(){
		FOVyNormal = static_cast<float>(FOVyBar.barpos + 1);
		mousemove = (mmsBar.barpos / 2 + 1) / 40.0f;
		viewdistance = viewdistBar.barpos / 8 + 2;
		FOVyBar.text = strWithVar("视野角度：", FOVyNormal);
		mmsBar.text = strWithVar("鼠标灵敏度：", mousemove);
		viewdistBar.text = strWithVar("渲染距离：", viewdistance);
		//ciArrayBtn.text = strWithVar("使用区块索引数组, ", boolstr(UseCIArray))
		//if (ciArrayBtn.clicked) UseCIArray = !UseCIArray
		if (rdstbtn.clicked) Renderoptions();
		if (gistbtn.clicked) GUIoptions();
		if (backbtn.clicked) gui::UIExit();
	}

	void OnRender(){

	}
};

void options(){
	//设置菜单
	Options Menu = Options();
	gui::UIEnter((gui::UIView*)&Menu);
}
