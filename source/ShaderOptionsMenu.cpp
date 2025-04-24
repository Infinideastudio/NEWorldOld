#include "Menus.h"
#include "Renderer.h"

namespace Menus {
	class ShaderOptionsMenu :public GUI::Form {
	private:
		GUI::label title;
		GUI::button enablebtn, cloudsbtn, backbtn;
		GUI::trackbar shadowresbar, shadowdistbar;
		void onLoad() {
			title = GUI::label(GetStrbyKey("NEWorld.shaders.caption"), -225, 225, 20, 36, 0.5, 0.5, 0.0, 0.0);
			enablebtn = GUI::button("", -250, -10, 60, 84, 0.5, 0.5, 0.0, 0.0);
			shadowresbar = GUI::trackbar("", 120, (int)(log2(Renderer::ShadowRes) - 10) * 40 - 1, 10, 250, 60, 84, 0.5, 0.5, 0.0, 0.0);
			shadowdistbar = GUI::trackbar("", 120, (Renderer::MaxShadowDist - 2) * 4 - 1, -250, -10, 96, 120, 0.5, 0.5, 0.0, 0.0);
			cloudsbtn = GUI::button("", 10, 250, 96, 120, 0.5, 0.5, 0.0, 0.0);
			backbtn = GUI::button(GetStrbyKey("NEWorld.render.back"), -250, 250, -44, -20, 0.5, 0.5, 1.0, 1.0);
			registerControls(6, &title, &enablebtn, &shadowresbar, &shadowdistbar, &cloudsbtn, &backbtn);
			Renderer::destroyShaders();
			if (!Renderer::AdvancedRender) shadowresbar.enabled = shadowdistbar.enabled = cloudsbtn.enabled = false;
		}
		void onUpdate() {
			if (enablebtn.clicked) {
				Renderer::AdvancedRender = !Renderer::AdvancedRender;
				if (Renderer::AdvancedRender) shadowresbar.enabled = shadowdistbar.enabled = cloudsbtn.enabled = true;
				else shadowresbar.enabled = shadowdistbar.enabled = cloudsbtn.enabled = false;
			}
			Renderer::ShadowRes = (int)pow(2, (shadowresbar.barpos + 1) / 40 + 10);
			Renderer::MaxShadowDist = (shadowdistbar.barpos + 1) / 4 + 2;
			if (cloudsbtn.clicked) Renderer::VolumetricClouds = !Renderer::VolumetricClouds;
			if (backbtn.clicked) {
				Renderer::initShaders();
				ExitSignal = true;
			}
			enablebtn.text = GetStrbyKey("NEWorld.shaders.enable") + BoolYesNo(Renderer::AdvancedRender);
			std::stringstream ss; ss << Renderer::ShadowRes;
			shadowresbar.text = GetStrbyKey("NEWorld.shaders.shadowres") + ss.str() + "x";
			ss.str(""); ss << Renderer::MaxShadowDist;
			shadowdistbar.text = GetStrbyKey("NEWorld.shaders.distance") + ss.str();
			cloudsbtn.text = GetStrbyKey("NEWorld.shaders.clouds") + BoolEnabled(Renderer::VolumetricClouds);
		}
	};
	void Shaderoptions() { ShaderOptionsMenu Menu; Menu.start(); }
}
