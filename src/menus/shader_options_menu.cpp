module menus;
import std;
import types;
import gui;
import globals;
import globalization;

namespace Menus {
using Globalization::GetStrbyKey;

class ShaderOptionsMenu: public GUI::Form {
private:
    GUI::Label title = GUI::Label("", -225, 225, 20, 36, 0.5, 0.5, 0.0, 0.0);
    GUI::Button enablebtn = GUI::Button("", -250, -10, 60, 84, 0.5, 0.5, 0.0, 0.0);
    GUI::Trackbar shadowresbar =
        GUI::Trackbar("", 120, (int) (std::log2(ShadowRes) - 10) * 40 - 1, 10, 250, 60, 84, 0.5, 0.5, 0.0, 0.0);
    GUI::Trackbar shadowdistbar =
        GUI::Trackbar("", 120, (MaxShadowDistance - 4) * 6 - 1, -250, -10, 96, 120, 0.5, 0.5, 0.0, 0.0);
    GUI::Button softshadowbtn = GUI::Button("", 10, 250, 96, 120, 0.5, 0.5, 0.0, 0.0);
    GUI::Button cloudsbtn = GUI::Button("", -250, -10, 132, 156, 0.5, 0.5, 0.0, 0.0);
    GUI::Button ssaobtn = GUI::Button("", 10, 250, 132, 156, 0.5, 0.5, 0.0, 0.0);
    GUI::Button backbtn = GUI::Button("", -250, 250, -44, -20, 0.5, 0.5, 1.0, 1.0);

    void onLoad() override {
        title.centered = true;
        registerControls(
            {&title, &enablebtn, &shadowresbar, &shadowdistbar, &softshadowbtn, &cloudsbtn, &ssaobtn, &backbtn}
        );
    }

    void onUpdate() override {
        title.text = GetStrbyKey("NEWorld.shaders.caption");
        enablebtn.text = GetStrbyKey("NEWorld.shaders.enable") + BoolYesNo(AdvancedRender);
        shadowresbar.text = GetStrbyKey("NEWorld.shaders.shadowres") + Var2Str(ShadowRes) + "x";
        shadowdistbar.text = GetStrbyKey("NEWorld.shaders.distance") + Var2Str(MaxShadowDistance);
        softshadowbtn.text = GetStrbyKey("NEWorld.shaders.softshadow") + BoolEnabled(SoftShadow);
        cloudsbtn.text = GetStrbyKey("NEWorld.shaders.clouds") + BoolEnabled(VolumetricClouds);
        ssaobtn.text = GetStrbyKey("NEWorld.shaders.ssao") + BoolEnabled(AmbientOcclusion);
        backbtn.text = GetStrbyKey("NEWorld.render.back");

        if (enablebtn.clicked)
            AdvancedRender = !AdvancedRender;
        ShadowRes = (int) std::pow(2, (shadowresbar.barpos + 1) / 40 + 10);
        MaxShadowDistance = (shadowdistbar.barpos + 1) / 6 + 4;
        if (softshadowbtn.clicked)
            SoftShadow = !SoftShadow;
        if (cloudsbtn.clicked)
            VolumetricClouds = !VolumetricClouds;
        if (ssaobtn.clicked)
            AmbientOcclusion = !AmbientOcclusion;
        if (backbtn.clicked)
            exit = true;
        shadowresbar.enabled = shadowdistbar.enabled = softshadowbtn.enabled = cloudsbtn.enabled = ssaobtn.enabled =
            AdvancedRender;
    }
};

void shaderoptions() {
    ShaderOptionsMenu Menu;
    Menu.start();
}
}
