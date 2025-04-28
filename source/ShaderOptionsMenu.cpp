#include "Menus.h"
#include "Renderer.h"

namespace Menus {
	class ShaderOptionsMenu :public GUI::Form {
	private:
		GUI::label title;
		GUI::button enablebtn, softshadowbtn, cloudsbtn, ssaobtn, backbtn;
		GUI::trackbar shadowresbar, shadowdistbar;
		void onLoad() {
			title = GUI::label(GetStrbyKey("NEWorld.shaders.caption"), -225, 225, 20, 36, 0.5, 0.5, 0.0, 0.0);
			title.centered = true;
			enablebtn = GUI::button("", -250, -10, 60, 84, 0.5, 0.5, 0.0, 0.0);
			shadowresbar = GUI::trackbar("", 120, (int)(log2(Renderer::ShadowRes) - 10) * 40 - 1, 10, 250, 60, 84, 0.5, 0.5, 0.0, 0.0);
			shadowdistbar = GUI::trackbar("", 120, (Renderer::MaxShadowDist - 2) * 4 - 1, -250, -10, 96, 120, 0.5, 0.5, 0.0, 0.0);
			softshadowbtn = GUI::button("", 10, 250, 96, 120, 0.5, 0.5, 0.0, 0.0);
			cloudsbtn = GUI::button("", -250, -10, 132, 156, 0.5, 0.5, 0.0, 0.0);
			ssaobtn = GUI::button("", 10, 250, 132, 156, 0.5, 0.5, 0.0, 0.0);
			backbtn = GUI::button(GetStrbyKey("NEWorld.render.back"), -250, 250, -44, -20, 0.5, 0.5, 1.0, 1.0);
			registerControls(8, &title, &enablebtn, &shadowresbar, &shadowdistbar, &softshadowbtn, &cloudsbtn, &ssaobtn, &backbtn);
		}
		void onUpdate() {
			if (enablebtn.clicked) Renderer::AdvancedRender = !Renderer::AdvancedRender;
			Renderer::ShadowRes = (int)pow(2, (shadowresbar.barpos + 1) / 40 + 10);
			Renderer::MaxShadowDist = (shadowdistbar.barpos + 1) / 4 + 2;
			if (softshadowbtn.clicked) Renderer::SoftShadow = !Renderer::SoftShadow;
			if (cloudsbtn.clicked) Renderer::VolumetricClouds = !Renderer::VolumetricClouds;
			if (ssaobtn.clicked) Renderer::AmbientOcclusion = !Renderer::AmbientOcclusion;
			if (backbtn.clicked) ExitSignal = true;
			enablebtn.text = GetStrbyKey("NEWorld.shaders.enable") + BoolYesNo(Renderer::AdvancedRender);
			std::stringstream ss; ss << Renderer::ShadowRes;
			shadowresbar.text = GetStrbyKey("NEWorld.shaders.shadowres") + ss.str() + "x";
			ss.str(""); ss << Renderer::MaxShadowDist;
			shadowdistbar.text = GetStrbyKey("NEWorld.shaders.distance") + ss.str();
			softshadowbtn.text = GetStrbyKey("NEWorld.shaders.softshadow") + BoolEnabled(Renderer::SoftShadow);
			cloudsbtn.text = GetStrbyKey("NEWorld.shaders.clouds") + BoolEnabled(Renderer::VolumetricClouds);
			ssaobtn.text = GetStrbyKey("NEWorld.shaders.ssao") + BoolEnabled(Renderer::AmbientOcclusion);
			shadowresbar.enabled = shadowdistbar.enabled = softshadowbtn.enabled = cloudsbtn.enabled = ssaobtn.enabled = Renderer::AdvancedRender;
		}
	};
	void Shaderoptions() { ShaderOptionsMenu Menu; Menu.start(); }
}
