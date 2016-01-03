#include "Menus.h"
namespace InfinideaStudio
{
	namespace NEWorld
	{
		class GUIOptionsMenu:public UI::Form
		{
		private:
			UI::Label title;
			UI::button fontbtn, blurbtn, backbtn;
			void onLoad()
			{
				title = UI::Label("===============< ͼ�ν���ѡ�� >==============", -225, 225, 20, 36, 0.5, 0.5, 0.0, 0.0);
				fontbtn = UI::button("ȫ��ʹ��Unicode���壺", -250, -10, 60, 84, 0.5, 0.5, 0.0, 0.0);
				blurbtn = UI::button("����ģ����", 10, 250, 60, 84, 0.5, 0.5, 0.0, 0.0);
				backbtn = UI::button("<< ����ѡ��˵�", -250, 250, -44, -20, 0.5, 0.5, 1.0, 1.0);
				registerControls(4, &title, &fontbtn, &blurbtn, &backbtn);
			}
			void onUpdate()
			{
				if(fontbtn.clicked) TextRenderer::useUnicodeASCIIFont = !TextRenderer::useUnicodeASCIIFont;
				if(blurbtn.clicked) GUIScreenBlur = !GUIScreenBlur;
				if(backbtn.clicked) ExitSignal = true;
				fontbtn.text = "ȫ��ʹ��Unicode���壺" + BoolYesNo(TextRenderer::useUnicodeASCIIFont);
				blurbtn.text = "����ģ����" + BoolEnabled(GUIScreenBlur);
			}
		};
		void GUIoptions() { GUIOptionsMenu Menu; Menu.start(); }
	}
}