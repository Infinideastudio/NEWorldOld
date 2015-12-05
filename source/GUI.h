#pragma once
#include "Definitions.h"
//ͼ�ν���ϵͳ����������OOP������
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
		//�ؼ����ֻ࣬Ҫ�ǿؼ����ü̳����
		virtual ~UIControl() {};
		int id, Left, Bottom, Height, Width;
		UIView* parent;
		virtual void update() {};  //Ī��������Ǵ�˵�е��麯����
		virtual void render() {};  //ò���ǵģ�
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
		//��ǩ
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
		//��ť
		string text;
		bool mouseon = false, focused = false, pressed = false, clicked = false, enabled = true;
		void update();
		void render();
		//void settext(string s)
		UIButton(string t);
	};

	class UITrackBar :public UIControl{
	public:
		//�ÿؼ����������Ҳ���
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
		//�ı���
		string text;
		bool mouseon = false, focused = false, pressed = false, enabled = true;

		void update();
		void render();
		UITextBox(string t);
	};
	class UIVerticalScroll :public UIControl{
	public:
		//��ֱ������
		int barheight, barpos;
		bool mouseon = false, focused = false, pressed = false, enabled = true;
		bool defaultv, msup, msdown, psup, psdown;

		void update();
		void render();
		UIVerticalScroll(int h, int p);
	};

	// ���� / ����
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
