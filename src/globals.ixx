module;

#include <GLFW/glfw3.h>
#include <utf8/cpp20.h>

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
export bool UIStretch = false;
export int Multisample = 0;
export bool VerticalSync = false;
export bool UIBackgroundBlur = true;
export int FontSize = 16;
export double Stretch = 1.0f;
export int WindowWidth = DefaultWindowWidth;
export int WindowHeight = DefaultWindowHeight;
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
export uint32_t gSeed;

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

export void fastSrand(uint32_t seed) {
    gSeed = seed;
}

export auto fastRand() -> uint32_t {
    gSeed = (214013 * gSeed + 2531011);
    return (gSeed >> 16) & 0x7FFF;
}

export auto rnd() -> double {
    return static_cast<double>(fastRand()) / 0x8000;
}

export auto split(std::string str, std::string pattern) -> std::vector<std::string> {
    std::vector<std::string> ret;
    if (pattern.empty())
        return ret;
    size_t start = 0, index = str.find_first_of(pattern, 0);
    while (index != str.npos) {
        if (start != index)
            ret.push_back(str.substr(start, index - start));
        start = index + 1;
        index = str.find_first_of(pattern, start);
    }
    if (!str.substr(start).empty())
        ret.push_back(str.substr(start));
    return ret;
}

export auto Timer() -> double {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

export void Sleep(unsigned int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

export auto UTF8Unicode(std::string_view s) -> std::u32string {
    return utf8::utf8to32(s);
}

export auto UnicodeUTF8(std::u32string_view s) -> std::string {
    return utf8::utf32to8(s);
}

template <typename T>
void loadoption(std::map<std::string, std::string>& m, char const* name, T& value) {
    if (m.find(name) == m.end())
        return;
    std::stringstream ss;
    ss << m[name];
    ss >> value;
}

export void loadOptions() {
    std::map<std::string, std::string> options;
    std::ifstream filein("configs/options.ini", std::ios::in);
    if (!filein.is_open())
        return;
    std::string name, value;
    while (!filein.eof()) {
        filein >> name >> value;
        options[name] = value;
    }
    filein.close();
    loadoption(options, "Language", Cur_Lang);
    loadoption(options, "FOV", FOVyNormal);
    loadoption(options, "RenderDistance", RenderDistance);
    loadoption(options, "Sensitivity", MouseSpeed);
    loadoption(options, "SmoothLighting", SmoothLighting);
    loadoption(options, "FancyGrass", NiceGrass);
    loadoption(options, "MergeFaceRendering", MergeFace);
    loadoption(options, "MultiSample", Multisample);
    loadoption(options, "AdvancedRender", AdvancedRender);
    loadoption(options, "ShadowMapRes", ShadowRes);
    loadoption(options, "ShadowDistance", MaxShadowDistance);
    loadoption(options, "SoftShadow", SoftShadow);
    loadoption(options, "VolumetricClouds", VolumetricClouds);
    loadoption(options, "AmbientOcclusion", AmbientOcclusion);
    loadoption(options, "VerticalSync", VerticalSync);
    loadoption(options, "UIFontSize", FontSize);
    loadoption(options, "UIStretch", UIStretch);
    loadoption(options, "UIBackgroundBlur", UIBackgroundBlur);
}

template <typename T>
void saveoption(std::ofstream& out, char const* name, T& value) {
    out << std::string(name) << " " << value << std::endl;
}

export void saveOptions() {
    std::map<std::string, std::string> options;
    std::ofstream fileout("configs/options.ini", std::ios::out);
    if (!fileout.is_open())
        return;
    saveoption(fileout, "Language", Cur_Lang);
    saveoption(fileout, "FOV", FOVyNormal);
    saveoption(fileout, "RenderDistance", RenderDistance);
    saveoption(fileout, "Sensitivity", MouseSpeed);
    saveoption(fileout, "SmoothLighting", SmoothLighting);
    saveoption(fileout, "FancyGrass", NiceGrass);
    saveoption(fileout, "MergeFaceRendering", MergeFace);
    saveoption(fileout, "MultiSample", Multisample);
    saveoption(fileout, "AdvancedRender", AdvancedRender);
    saveoption(fileout, "ShadowMapRes", ShadowRes);
    saveoption(fileout, "ShadowDistance", MaxShadowDistance);
    saveoption(fileout, "SoftShadow", SoftShadow);
    saveoption(fileout, "VolumetricClouds", VolumetricClouds);
    saveoption(fileout, "AmbientOcclusion", AmbientOcclusion);
    saveoption(fileout, "VerticalSync", VerticalSync);
    saveoption(fileout, "UIFontSize", FontSize);
    saveoption(fileout, "UIStretch", UIStretch);
    saveoption(fileout, "UIBackgroundBlur", UIBackgroundBlur);
    fileout.close();
}
