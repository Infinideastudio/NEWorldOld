#include "Menus.h"
#include "TextRenderer.h"
namespace InfinideaStudio
{
	namespace NEWorld
	{

		class RenderOptionsMenu:public UI::Form
		{
		private:
			UI::Label title;
			UI::button smoothlightingbtn, fancygrassbtn, mergefacebtn, backbtn;
			void onLoad()
			{
				title = UI::Label("==============<  �� Ⱦ ѡ ��  >==============", -225, 225, 20, 36, 0.5, 0.5, 0.0, 0.0);
				smoothlightingbtn = UI::button("ƽ�����գ�", -250, -10, 60, 84, 0.5, 0.5, 0.0, 0.0);
				fancygrassbtn = UI::button("�ݷ���������ӣ�", 10, 250, 60, 84, 0.5, 0.5, 0.0, 0.0);
				mergefacebtn = UI::button("�ϲ�����Ⱦ��", -250, -10, 96, 120, 0.5, 0.5, 0.0, 0.0);
				backbtn = UI::button("<< ����ѡ��˵�", -250, 250, -44, -20, 0.5, 0.5, 1.0, 1.0);
				registerControls(5, &title, &smoothlightingbtn, &fancygrassbtn, &mergefacebtn, &backbtn);
				if(MergeFace) SmoothLighting = smoothlightingbtn.enabled = NiceGrass = fancygrassbtn.enabled = false;
			}
			void onUpdate()
			{
				if(smoothlightingbtn.clicked) SmoothLighting = !SmoothLighting;
				if(fancygrassbtn.clicked) NiceGrass = !NiceGrass;
				if(mergefacebtn.clicked)
				{
					MergeFace = !MergeFace;
					if(MergeFace) SmoothLighting = smoothlightingbtn.enabled = NiceGrass = fancygrassbtn.enabled = false;
					else SmoothLighting = smoothlightingbtn.enabled = NiceGrass = fancygrassbtn.enabled = true;
				}
				if(backbtn.clicked) ExitSignal = true;
				smoothlightingbtn.text = "ƽ�����գ�" + BoolEnabled(SmoothLighting);
				fancygrassbtn.text = "�ݷ���������ӣ�" + BoolEnabled(NiceGrass);
				mergefacebtn.text = "�ϲ�����Ⱦ��" + BoolEnabled(MergeFace);
			}
		};
		void Renderoptions() { RenderOptionsMenu Menu; Menu.start(); }
	}
}