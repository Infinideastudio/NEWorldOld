module;

#include <GLFW/glfw3.h>
#include <utf8/cpp20.h>
#undef assert

export module globals;
import std;
import types;

// Global constants
export constexpr uint32_t GameVersion = 39;
export constexpr std::string_view MajorVersion = "Alpha 0.";
export constexpr std::string_view MinorVersion = "5";
export constexpr std::string_view VersionSuffix = " [In Development]";
export constexpr int DefaultWindowWidth = 852;
export constexpr int DefaultWindowHeight = 480;
export constexpr double Pi = std::numbers::pi_v<double>;

// Global variables
export float FOVyNormal = 70.0f;
export float FOVyRunning = 8.0f;
export float MouseSpeed = 0.1f;
export int RenderDistance = 16;
export bool SmoothLighting = true;
export bool NiceGrass = true;
export bool MergeFace = false;
export bool AdvancedRender;
export int ShadowRes = 2048;
export int MaxShadowDistance = 16;
export bool SoftShadow = false;
export bool VolumetricClouds = false;
export bool AmbientOcclusion = false;
export bool UIAutoStretch = true;
export int Multisample = 4;
export bool VerticalSync = false;
export bool UIBackgroundBlur = true;
export double FontScale = 1.0;
export double Stretch = 1.0;
export int WindowWidth = 0;
export int WindowHeight = 0;
export std::string Cur_Lang = "zh_CN";
export std::string Cur_WorldName = "";

export int GLMajorVersion, GLMinorVersion;
export GLFWwindow* MainWindow;
export GLFWcursor* MouseCursor;

export int mx, my, mxl, myl;
export int mw, mb, mbp, mbl, mwl;
export double mxdelta, mydelta;
export std::u32string inputstr;
export bool backspace;

export bool GameBegin, GameExit;
export int GameTime = 0;

export int rendered_chunks;
export int unloaded_chunks;
export int meshed_chunks;
export int updated_blocks;

export auto getMouseScroll() -> int {
    return mw;
}
export auto getMouseButton() -> int {
    return mb;
}

uint32_t g_seed;

export void fast_srand(uint32_t seed) {
    g_seed = seed;
}

export auto fast_rand() -> uint32_t {
    g_seed = (214013 * g_seed + 2531011);
    return (g_seed >> 16) & 0x7FFF;
}

export auto rnd() -> double {
    return static_cast<double>(fast_rand()) / 0x8000;
}

export auto timer() -> double {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

export auto utf8_unicode(std::string_view s) -> std::u32string {
    return utf8::utf8to32(s);
}

export auto unicode_utf8(std::u32string_view s) -> std::string {
    return utf8::utf32to8(s);
}

template <typename T>
void load_option(std::map<std::string, std::string>& m, char const* name, T& value) {
    if (m.find(name) == m.end())
        return;
    std::stringstream ss;
    ss << m[name];
    ss >> value;
}

export auto load_options() -> bool {
    auto file = std::ifstream("configs/options.ini", std::ios::in);
    if (!file.is_open()) {
        return false;
    }
    auto options = std::map<std::string, std::string>();
    while (!file.eof()) {
        auto name = std::string();
        auto value = std::string();
        file >> name >> value;
        options[name] = value;
    }
    file.close();
    load_option(options, "Language", Cur_Lang);
    load_option(options, "FOV", FOVyNormal);
    load_option(options, "RenderDistance", RenderDistance);
    load_option(options, "Sensitivity", MouseSpeed);
    load_option(options, "SmoothLighting", SmoothLighting);
    load_option(options, "FancyGrass", NiceGrass);
    load_option(options, "MergeFaceRendering", MergeFace);
    load_option(options, "MultiSample", Multisample);
    load_option(options, "AdvancedRender", AdvancedRender);
    load_option(options, "ShadowMapRes", ShadowRes);
    load_option(options, "ShadowDistance", MaxShadowDistance);
    load_option(options, "SoftShadow", SoftShadow);
    load_option(options, "VolumetricClouds", VolumetricClouds);
    load_option(options, "AmbientOcclusion", AmbientOcclusion);
    load_option(options, "VerticalSync", VerticalSync);
    load_option(options, "UIFontScale", FontScale);
    load_option(options, "UIAutoStretch", UIAutoStretch);
    load_option(options, "UIBackgroundBlur", UIBackgroundBlur);
    return true;
}

template <typename T>
void save_option(std::ofstream& out, char const* name, T& value) {
    out << std::string(name) << " " << value << std::endl;
}

export auto save_options() -> bool {
    auto file = std::ofstream("configs/options.ini", std::ios::out);
    if (!file.is_open()) {
        return false;
    }
    save_option(file, "Language", Cur_Lang);
    save_option(file, "FOV", FOVyNormal);
    save_option(file, "RenderDistance", RenderDistance);
    save_option(file, "Sensitivity", MouseSpeed);
    save_option(file, "SmoothLighting", SmoothLighting);
    save_option(file, "FancyGrass", NiceGrass);
    save_option(file, "MergeFaceRendering", MergeFace);
    save_option(file, "MultiSample", Multisample);
    save_option(file, "AdvancedRender", AdvancedRender);
    save_option(file, "ShadowMapRes", ShadowRes);
    save_option(file, "ShadowDistance", MaxShadowDistance);
    save_option(file, "SoftShadow", SoftShadow);
    save_option(file, "VolumetricClouds", VolumetricClouds);
    save_option(file, "AmbientOcclusion", AmbientOcclusion);
    save_option(file, "VerticalSync", VerticalSync);
    save_option(file, "UIFontScale", FontScale);
    save_option(file, "UIAutoStretch", UIAutoStretch);
    save_option(file, "UIBackgroundBlur", UIBackgroundBlur);
    return true;
}
