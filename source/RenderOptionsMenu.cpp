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
				title = UI::Label("==============<  渲 染 选 项  >==============", -225, 225, 20, 36, 0.5, 0.5, 0.0, 0.0);
				smoothlightingbtn = UI::button("平滑光照：", -250, -10, 60, 84, 0.5, 0.5, 0.0, 0.0);
				fancygrassbtn = UI::button("草方块材质连接：", 10, 250, 60, 84, 0.5, 0.5, 0.0, 0.0);
				mergefacebtn = UI::button("合并面渲染：", -250, -10, 96, 120, 0.5, 0.5, 0.0, 0.0);
				backbtn = UI::button("<< 返回选项菜单", -250, 250, -44, -20, 0.5, 0.5, 1.0, 1.0);
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
				smoothlightingbtn.text = "平滑光照：" + BoolEnabled(SmoothLighting);
				fancygrassbtn.text = "草方块材质连接：" + BoolEnabled(NiceGrass);
				mergefacebtn.text = "合并面渲染：" + BoolEnabled(MergeFace);
			}
		};
		void Renderoptions() { RenderOptionsMenu Menu; Menu.start(); }
	}
}