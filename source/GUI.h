#pragma once
#include "Definitions.h"
#include "TextRenderer.h"
#include "Textures.h"

namespace InfinideaStudio
{
	namespace NEWorld
	{
		extern int getMouseButton();
		extern int getMouseScroll();
		inline string BoolYesNo(bool b) { return b ? "��" : "��"; }
		inline string BoolEnabled(bool b) { return b ? "����" : "�ر�"; }
		template<typename T>
		inline string strWithVar(string str, T var)
		{
			std::stringstream ss;
			ss << str << var;
			return ss.str();
		}

		//InfinideaStudio
		//ͼ�ν���ϵͳ����������OOP������
		namespace UI
		{

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

			extern unsigned int transitionList;
			extern unsigned int lastdisplaylist;
			extern double transitionTimer;
			extern bool transitionForward;

			void clearTransition();
			void screenBlur();
			void drawBackground();

			class Form;
			//ΪUI�ؼ��ṩ����
			class Control
			{
			public:
				virtual ~Control() { }
				int Id;
				int xmin, ymin, xmax, ymax;
				Form* parent;
				virtual void Update() { } 
				virtual void render() { } 
				virtual void Dispose() { }
				void updatepos();
				void resize(int xi_r, int xa_r, int yi_r, int ya_r, double xi_b, double xa_b, double yi_b, double ya_b);
			private:
				int _xmin_r, _ymin_r, _xmax_r, _ymax_r;
				double _xmin_b, _ymin_b, _xmax_b, _ymax_b;
			};

			//��ʾ��ʾ���ֵı�ǩ
			class Label:public Control
			{
			public:
				
				string Text;
				bool mouseon = false, focused = false;
				Label() { };
				Label(string t, int xi_r, int xa_r, int yi_r, int ya_r, double xi_b, double xa_b, double yi_b, double ya_b);
				void Update();
				void render();
			};

			class button:public Control
			{
			public:
				//��ť
				string text;
				bool mouseon = false, focused = false, pressed = false, clicked = false, enabled = false;
				button() { };
				button(string t,
					int xi_r, int xa_r, int yi_r, int ya_r, double xi_b, double xa_b, double yi_b, double ya_b);
				void Update();
				void render();
			};

			class trackbar:public Control
			{
			public:
				//�ÿؼ����������Ҳ���
				string text;
				int barwidth;
				int barpos;
				bool mouseon = false, focused = false, pressed = false, enabled = false;
				trackbar() { };
				trackbar(string t, int w, int s,
					int xi_r, int xa_r, int yi_r, int ya_r, double xi_b, double xa_b, double yi_b, double ya_b);
				void Update();
				void render();
			};

			class textbox:public Control
			{
			public:
				//�ı���
				string text;
				bool mouseon = false, focused = false, pressed = false, enabled = false;
				textbox() { };
				textbox(string t,
					int xi_r, int xa_r, int yi_r, int ya_r, double xi_b, double xa_b, double yi_b, double ya_b);
				void Update();
				void render();
			};

			class vscroll:public Control
			{
			public:
				//��ֱ������
				int barheight, barpos;
				bool mouseon = false, focused = false, pressed = false, enabled = false;
				bool defaultv, msup, msdown, psup, psdown;
				vscroll() { };
				vscroll(int h, int s,
					int xi_r, int xa_r, int yi_r, int ya_r, double xi_b, double xa_b, double yi_b, double ya_b);
				void Update();
				void render();
			};

			class imagebox:public Control
			{
			public:
				//ͼƬ��
				float txmin, txmax, tymin, tymax;
				TextureID imageid;
				imagebox() { };
				imagebox(float _txmin, float _txmax, float _tymin, float _tymax, TextureID iid,
					int xi_r, int xa_r, int yi_r, int ya_r, double xi_b, double xa_b, double yi_b, double ya_b);
				void Update();
				void render();
			};

			// ���� / ����
			class Form
			{
			public:
				vector<Control*> children;
				bool tabp, shiftp, enterp, enterpl;
				bool upkp, downkp, upkpl, downkpl, leftkp, rightkp, leftkpl, rightkpl, backspacep, backspacepl, updated;
				int maxid, currentid, focusid, childrenCount, mx, my, mw, mb, mxl, myl, mwl, mbl;
				unsigned int displaylist;
				bool ExitSignal, MouseOnTextbox;
				void Init();
				void registerControl(Control* c);
				void registerControls(int count, Control* c, ...);
				void Update();
				void render();
				Control* getControlByID(int cid);
				void cleanup();
				virtual void onLoad() { }
				virtual void onUpdate() { }
				virtual void Background() { drawBackground(); }
				virtual void onRender() { }
				virtual void onLeaving() { }
				virtual void onLeave() { }
				Form();
				void start();
				~Form();
			};
		}
	}
}