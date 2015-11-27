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

	typedef void(UIEvent)(controls*);

	class Form;
	class controls{
	public:
		//控件基类，只要是控件都得继承这个
		bool Visiable = true;
		virtual ~controls() {};
		int id;
		Form* parent;
		UIEvent* onclick = nullptr;
		UIEvent* onupdate = nullptr;
		UIEvent* onrender = nullptr;
		virtual void update() {};  //莫非这个就是传说中的虚函数？
		virtual void render() {};  //貌似是的！
		virtual void destroy() {};

	};

	class label:public controls{
	public:
		//标签
		//int id
		//Form* parent
		string  text;
		int xmin, xmax, ymin, ymax;
		bool mouseon = false;
		bool focused = false;

		void update();
		void render();
		//void settext(string s)
		void resize(int xi, int xa, int yi, int ya);
	};

	class button :public controls{
	public:
		//按钮
		//int id
		//Form* parent
		string text;
		int xmin, xmax, ymin, ymax;
		bool mouseon = false, focused = false, pressed = false, clicked = false, enabled = false;
		void update();
		void render();
		//void settext(string s)
		void resize(int xi, int xa, int yi, int ya);
	};

	class trackbar :public controls{
	public:
		//该控件的中文名我不造
		//int id
		//Form* parent
		string text;
		int xmin, xmax, ymin, ymax;
		int barwidth;
		int barpos;
		bool mouseon = false, focused = false, pressed = false, enabled = false;

		void update();
		void render();
		void settext(string s);
		void resize(int xi, int xa, int yi, int ya);

	};

	class textbox :public controls{
	public:
		//文本框
		//int id
		//Form* parent
		string text;
		int xmin, xmax, ymin, ymax;
		bool mouseon = false, focused = false, pressed = false, enabled = false;

		void update();
		void render();
		void resize(int xi, int xa, int yi, int ya);
	};
	class vscroll :public controls{
	public:
		//垂直滚动条
		//int id
		//Form* parent
		int xmin, xmax, ymin, ymax;
		int barheight, barpos;
		bool mouseon = false, focused = false, pressed = false, enabled = false;
		bool defaultv, msup, msdown, psup, psdown;

		void update();
		void render();
		void resize(int xi, int xa, int yi, int ya);
	};

	// 窗体 / 容器
	class Form{
	public:
		vector<controls*> children;
		bool tabp, shiftp, enterp, enterpl;
		bool upkp, downkp, upkpl, downkpl, leftkp, rightkp, leftkpl, rightkpl, backspacep, backspacepl, updated;

		int maxid, currentid, focusid, childrenCount, mx, my, mw, mb, mxl, myl, mwl, mbl;

		void Init();
		void update();
		void render();
		void mousedata(int x, int y, int w, int b);
		label* createlabel(string t);
		button* createbutton(string t);
		trackbar* createtrackbar(string t, int w, int p);
		textbox* createtextbox(string t);
		vscroll* createvscroll(int h, int p);
		controls* getControlByID(int cid);
		void cleanup();
	};

}
