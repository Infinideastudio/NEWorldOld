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
	class Form;
	class controls{
	public:
		//�ؼ����ֻ࣬Ҫ�ǿؼ����ü̳����
		virtual ~controls() {};
		int id;
		Form* parent;
		virtual void update() {};  //Ī��������Ǵ�˵�е��麯����
		virtual void render() {};  //ò���ǵģ�
		virtual void destroy() {};

	};

	class label:public controls{
	public:
		//��ǩ
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
		//��ť
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
		//�ÿؼ����������Ҳ���
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
		//�ı���
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
		//��ֱ������
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

	// ���� / ����
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
