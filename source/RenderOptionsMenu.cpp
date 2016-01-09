#include "Menus.h"
#include "TextRenderer.h"

namespace Menus {
	class RenderOptionsMenu :public GUI::Form {
	private:
		GUI::label title;
		GUI::button smoothlightingbtn, fancygrassbtn, mergefacebtn, backbtn;
		GUI::trackbar msaabar;
		void onLoad() {
			title = GUI::label("==============<  渲 染 选 项  >==============", -225, 225, 20, 36, 0.5, 0.5, 0.0, 0.0);
			smoothlightingbtn = GUI::button("平滑光照：", -250, -10, 60, 84, 0.5, 0.5, 0.0, 0.0);
			fancygrassbtn = GUI::button("草方块材质连接：", 10, 250, 60, 84, 0.5, 0.5, 0.0, 0.0);
			mergefacebtn = GUI::button("合并面渲染：", -250, -10, 96, 120, 0.5, 0.5, 0.0, 0.0);
			msaabar = GUI::trackbar("多重采样抗锯齿：", 120, Multisample == 0 ? 0 : (int)(log2(Multisample) - 1) * 30 - 1, 10, 250, 96, 120, 0.5, 0.5, 0.0, 0.0);
			backbtn = GUI::button("<< 返回选项菜单", -250, 250, -44, -20, 0.5, 0.5, 1.0, 1.0);
			registerControls(6, &title, &smoothlightingbtn, &fancygrassbtn, &mergefacebtn, &msaabar, &backbtn);
			if (MergeFace) SmoothLighting = smoothlightingbtn.enabled = NiceGrass = fancygrassbtn.enabled = false;
		}
		void onUpdate() {
			if (smoothlightingbtn.clicked) SmoothLighting = !SmoothLighting;
			if (fancygrassbtn.clicked) NiceGrass = !NiceGrass;
			if (mergefacebtn.clicked) {
				MergeFace = !MergeFace;
				if (MergeFace) SmoothLighting = smoothlightingbtn.enabled = NiceGrass = fancygrassbtn.enabled = false;
				else SmoothLighting = smoothlightingbtn.enabled = NiceGrass = fancygrassbtn.enabled = true;
			}
			if (msaabar.barpos == 0) Multisample = 0;
			else Multisample = 1 << ((msaabar.barpos + 1) / 30 + 1);
			if (backbtn.clicked) ExitSignal = true;
			smoothlightingbtn.text = "平滑光照：" + BoolEnabled(SmoothLighting);
			fancygrassbtn.text = "草方块材质连接：" + BoolEnabled(NiceGrass);
			mergefacebtn.text = "合并面渲染：" + BoolEnabled(MergeFace);
			std::stringstream ss; ss << Multisample;
			msaabar.text = "多重采样抗锯齿：" + (Multisample != 0 ? ss.str() + "x" : BoolEnabled(false));
		}
	};
	void Renderoptions() { RenderOptionsMenu Menu; Menu.start(); }
}
