#include "Menus.h"
#include "TextRenderer.h"

class RenderOptionsMenu :public gui::Form {
private:
	gui::label title;
	gui::button smoothlightingbtn, fancygrassbtn, mergefacebtn, backbtn;
	void onLoad() {
		title = gui::label("==============<  渲 染 选 项  >==============", -225, 225, 20, 36, 0.5, 0.5, 0.0, 0.0);
		smoothlightingbtn = gui::button("平滑光照：", -250, -10, 60, 84, 0.5, 0.5, 0.0, 0.0);
		fancygrassbtn = gui::button("草方块材质连接：", 10, 250, 60, 84, 0.5, 0.5, 0.0, 0.0);
		mergefacebtn = gui::button("合并面渲染：", -250, -10, 96, 120, 0.5, 0.5, 0.0, 0.0);
		backbtn = gui::button("<< 返回选项菜单", -250, 250, -44, -20, 0.5, 0.5, 1.0, 1.0);
		registerControls(5, &title, &smoothlightingbtn, &fancygrassbtn, &mergefacebtn, &backbtn);
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
		if (backbtn.clicked) ExitSignal = true;
		smoothlightingbtn.text = "平滑光照：" + BoolEnabled(SmoothLighting);
		fancygrassbtn.text = "草方块材质连接：" + BoolEnabled(NiceGrass);
		mergefacebtn.text = "合并面渲染：" + BoolEnabled(MergeFace);
	}
};
void Renderoptions() { RenderOptionsMenu Menu; Menu.start(); }