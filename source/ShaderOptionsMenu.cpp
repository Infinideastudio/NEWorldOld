#include "Menus.h"
#include "Renderer.h"

namespace Menus {
	class ShaderOptionsMenu : public GUI::Form {
	private:
		GUI::Label title = GUI::Label("", -225, 225, 20, 36, 0.5, 0.5, 0.0, 0.0);
		GUI::Button enablebtn = GUI::Button("", -250, -10, 60, 84, 0.5, 0.5, 0.0, 0.0);
		GUI::Trackbar shadowresbar = GUI::Trackbar("", 120, (int)(log2(Renderer::ShadowRes) - 10) * 40 - 1, 10, 250, 60, 84, 0.5, 0.5, 0.0, 0.0);
		GUI::Trackbar shadowdistbar = GUI::Trackbar("", 120, (Renderer::MaxShadowDistance - 4) * 6 - 1, -250, -10, 96, 120, 0.5, 0.5, 0.0, 0.0);
		GUI::Button softshadowbtn = GUI::Button("", 10, 250, 96, 120, 0.5, 0.5, 0.0, 0.0);
		GUI::Button cloudsbtn = GUI::Button("", -250, -10, 132, 156, 0.5, 0.5, 0.0, 0.0);
		GUI::Button ssaobtn = GUI::Button("", 10, 250, 132, 156, 0.5, 0.5, 0.0, 0.0);
		GUI::Button backbtn = GUI::Button("", -250, 250, -44, -20, 0.5, 0.5, 1.0, 1.0);

		void onLoad() {
			title.centered = true;
			registerControls({ &title, &enablebtn, &shadowresbar, &shadowdistbar, &softshadowbtn, &cloudsbtn, &ssaobtn, &backbtn });
		}

		void onUpdate() {
			title.text = GetStrbyKey("NEWorld.shaders.caption");
			enablebtn.text = GetStrbyKey("NEWorld.shaders.enable") + BoolYesNo(Renderer::AdvancedRender);
			shadowresbar.text = GetStrbyKey("NEWorld.shaders.shadowres") + Var2Str(Renderer::ShadowRes) + "x";
			shadowdistbar.text = GetStrbyKey("NEWorld.shaders.distance") + Var2Str(Renderer::MaxShadowDistance);
			softshadowbtn.text = GetStrbyKey("NEWorld.shaders.softshadow") + BoolEnabled(Renderer::SoftShadow);
			cloudsbtn.text = GetStrbyKey("NEWorld.shaders.clouds") + BoolEnabled(Renderer::VolumetricClouds);
			ssaobtn.text = GetStrbyKey("NEWorld.shaders.ssao") + BoolEnabled(Renderer::AmbientOcclusion);
			backbtn.text = GetStrbyKey("NEWorld.render.back");

			if (enablebtn.clicked) Renderer::AdvancedRender = !Renderer::AdvancedRender;
			Renderer::ShadowRes = (int)pow(2, (shadowresbar.barpos + 1) / 40 + 10);
			Renderer::MaxShadowDistance = (shadowdistbar.barpos + 1) / 6 + 4;
			if (softshadowbtn.clicked) Renderer::SoftShadow = !Renderer::SoftShadow;
			if (cloudsbtn.clicked) Renderer::VolumetricClouds = !Renderer::VolumetricClouds;
			if (ssaobtn.clicked) Renderer::AmbientOcclusion = !Renderer::AmbientOcclusion;
			if (backbtn.clicked) exit = true;
			shadowresbar.enabled = shadowdistbar.enabled = softshadowbtn.enabled = cloudsbtn.enabled = ssaobtn.enabled = Renderer::AdvancedRender;
		}
	};

	void shaderoptions() { ShaderOptionsMenu Menu; Menu.start(); }
}
