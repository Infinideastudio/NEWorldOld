#pragma once
#include "Definitions.h"
//图形界面系统。。。正宗OOP！！！
namespace gui{
	extern float linewidth;
	extern float linealpha;
	extern float FgR;
	extern float FgG;
	extern float FgB;
	extern float FgA;
	extern float BgR;
	extern float BgG;
	extern float BgB;
	extern float BgA;
	class UIView;
	class UIControl{
	public:
		//控件基类，只要是控件都得继承这个
		virtual ~UIControl() {};
		int id, Left, Bottom, Height, Width;
		UIView* parent;
		virtual void update() {};  //莫非这个就是传说中的虚函数？
		virtual void render() {};  //貌似是的！
		virtual void destroy() {};
		
		virtual void UISetRect(int xi, int xa, int yi, int ya){
			Left = xi;
			Width = xa - Left;
			Bottom = yi;
			Height = ya - Bottom;
		}

	};

	class UILabel:public UIControl{
	public:
		//标签
		string  text;
		bool mouseon = false;
		bool focused = false;

		void update();
		void render();
		//void settext(string s)
		UILabel(string t);
	};

	class UIButton :public UIControl{
	public:
		//按钮
		string text;
		bool mouseon = false, focused = false, pressed = false, clicked = false, enabled = true;
		void update();
		void render();
		//void settext(string s)
		UIButton(string t);
	};

	class UITrackBar :public UIControl{
	public:
		//该控件的中文名我不造
		string text;
		int barwidth;
		int barpos;
		bool mouseon = false, focused = false, pressed = false, enabled = true;

		void update();
		void render();
		void settext(string s);
		UITrackBar(string t, int w, int p);
	};

	class UITextBox :public UIControl{
	public:
		//文本框
		string text;
		bool mouseon = false, focused = false, pressed = false, enabled = true;

		void update();
		void render();
		UITextBox(string t);
	};
	class UIVerticalScroll :public UIControl{
	public:
		//垂直滚动条
		int barheight, barpos;
		bool mouseon = false, focused = false, pressed = false, enabled = true;
		bool defaultv, msup, msdown, psup, psdown;

		void update();
		void render();
		UIVerticalScroll(int h, int p);
	};

	// 窗体 / 容器
	class UIView :public UIControl{
	public:
		vector<UIControl*> children;
		bool tabp, shiftp, enterp, enterpl;
		bool upkp, downkp, upkpl, downkpl, leftkp, rightkp, leftkpl, rightkpl, backspacep, backspacepl, updated;

		int maxid, currentid, focusid, childrenCount, mx, my, mw, mb, mxl, myl, mwl, mbl;
		
		void Init();
		virtual void OnResize(){};
		virtual void OnUpdate(){};
		virtual void OnRender(){};
		void UISetRect(int xi, int xa, int yi, int ya);
		void update();
		void render();
		void mousedata(int x, int y, int w, int b);
		UILabel* createlabel(string t);
		UIButton* createbutton(string t);
		UITrackBar* createtrackbar(string t, int w, int p);
		UITextBox* createtextbox(string t);
		UIVerticalScroll* createvscroll(int h, int p);
		UIControl* getControlByID(int cid);
		void cleanup();
		void RegisterUI(UIControl* Control);
	};
	
	void UIEnter(UIView* View);
	void UIExit();

}
