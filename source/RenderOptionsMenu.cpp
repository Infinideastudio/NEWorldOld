#include "Menus.h"
#include "TextRenderer.h"

namespace Menus {
	class RenderOptionsMenu :public GUI::Form {
	private:
		GUI::label title;
		GUI::button smoothlightingbtn, fancygrassbtn, mergefacebtn, backbtn;
		GUI::trackbar msaabar;
		void onLoad() {
			title = GUI::label("==============< " + GetStrbyKey("gui:caption:Render Options") + "  >==============", -225, 225, 20, 36, 0.5, 0.5, 0.0, 0.0);
			smoothlightingbtn = GUI::button(GetStrbyKey("gui:Smooth Lighting") + "��", -250, -10, 60, 84, 0.5, 0.5, 0.0, 0.0);
			fancygrassbtn = GUI::button(GetStrbyKey("gui:Grass Tex Gluing") + "��", 10, 250, 60, 84, 0.5, 0.5, 0.0, 0.0);
			mergefacebtn = GUI::button(GetStrbyKey("gui:Linar Face-Merging") + "��", -250, -10, 96, 120, 0.5, 0.5, 0.0, 0.0);
			msaabar = GUI::trackbar("���ز�������ݣ�", 120, Multisample == 0 ? 0 : (int)(log2(Multisample) - 1) * 30 - 1, 10, 250, 96, 120, 0.5, 0.5, 0.0, 0.0);
			backbtn = GUI::button("<< " + GetStrbyKey("gui:Back to Option Menu"), -250, 250, -44, -20, 0.5, 0.5, 1.0, 1.0);
			registerControls(6, &title, &smoothlightingbtn, &fancygrassbtn, &mergefacebtn, &backbtn);
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
			std::stringstream ss; ss << Multisample;
			msaabar.text = "���ز�������ݣ�" + (Multisample != 0 ? ss.str() + "x" : BoolEnabled(false));
			smoothlightingbtn.text = GetStrbyKey("gui:Smooth Lighting") + "��" + BoolEnabled(SmoothLighting);
			fancygrassbtn.text = GetStrbyKey("gui:Grass Tex Gluing") + "��" + BoolEnabled(NiceGrass);
			mergefacebtn.text = GetStrbyKey("gui:Linar Face-Merging") + "��" + BoolEnabled(MergeFace);
		}
	};
	void Renderoptions() { RenderOptionsMenu Menu; Menu.start(); }
}
