module;

#include <cassert>
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

export module setup;
import std;
import types;
import globals;
import rendering;
import text_rendering;
import textures;

void glDefaults() {
    // Set up default GL context states
    glViewport(0, 0, WindowWidth, WindowHeight);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_TEXTURE_2D);
    if (Multisample != 0)
        glEnable(GL_MULTISAMPLE);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
}

void WindowSizeFunc(GLFWwindow* win, int width, int height) {
    if (width < 640)
        width = 640;
    if (height < 360)
        height = 360;
    WindowWidth = width;
    WindowHeight = height > 0 ? height : 1;
    glfwSetWindowSize(win, width, height);
    glDefaults();
    Renderer::initShaders();
}

void MouseButtonFunc(GLFWwindow*, int button, int action, int) {
    mb = 0;
    if (action == GLFW_PRESS) {
        if (button == GLFW_MOUSE_BUTTON_LEFT)
            mb += 1;
        if (button == GLFW_MOUSE_BUTTON_RIGHT)
            mb += 2;
        if (button == GLFW_MOUSE_BUTTON_MIDDLE)
            mb += 4;
    } else
        mb = 0;
}

void KeyFunc(GLFWwindow*, int key, int /* scancode */, int action, int /* mods */) {
    if (key == GLFW_KEY_BACKSPACE && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
        backspace = true;
    }
}

void CharInputFunc(GLFWwindow*, unsigned int c) {
    char32_t unicode = static_cast<char32_t>(c);
    inputstr += unicode;
}

void MouseScrollFunc(GLFWwindow*, double, double yoffset) {
    mw += (int) yoffset;
}

// OpenGL debug callback
void APIENTRY glDebugCallback(GLenum, GLenum, GLuint, GLenum severity, GLsizei, GLchar const* msg, void const*) {
    switch (severity) {
        case GL_DEBUG_SEVERITY_HIGH:
            spdlog::error("[GL] {}", msg);
            break;
        case GL_DEBUG_SEVERITY_MEDIUM:
            spdlog::warn("[GL] {}", msg);
            break;
        case GL_DEBUG_SEVERITY_LOW:
            spdlog::debug("[GL] {}", msg);
            break;
        case GL_DEBUG_SEVERITY_NOTIFICATION:
            break;
        default:
            break;
    }
}

export void initStretch() {
    double const stdppi = 96.0;
    if (UIStretch && Stretch == 1.0) {
        // Get the screen physical size and set stretch
        int nScreenWidth, nScreenHeight;
        glfwGetMonitorPhysicalSize(glfwGetPrimaryMonitor(), &nScreenWidth, &nScreenHeight);
        int vmc;
        GLFWvidmode const* mode = glfwGetVideoModes(glfwGetPrimaryMonitor(), &vmc);
        double ppi = static_cast<double>(mode[vmc - 1].width) / (static_cast<double>(nScreenWidth) / 25.4f);
        Stretch = ppi / stdppi;
        // Compute the stretch and reset the window size
        WindowWidth = static_cast<int>(WindowWidth * Stretch);
        WindowHeight = static_cast<int>(WindowHeight * Stretch);
        glfwSetWindowSize(MainWindow, WindowWidth, WindowHeight);
    } else if (!UIStretch && Stretch != 1.0) {
        WindowWidth = static_cast<int>(WindowWidth / Stretch);
        WindowHeight = static_cast<int>(WindowHeight / Stretch);
        Stretch = 1.0;
        glfwSetWindowSize(MainWindow, WindowWidth, WindowHeight);
    }
}

export void createWindow() {
    std::stringstream title;
    title << "NEWorld " << MajorVersion << MinorVersion << VersionSuffix;

    auto glfwStatus = glfwInit();
    assert(glfwStatus == GLFW_TRUE);

    glfwWindowHint(GLFW_SAMPLES, Multisample);
#ifdef NEWORLD_DEBUG
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, true);
#endif

    MainWindow = glfwCreateWindow(WindowWidth, WindowHeight, title.str().c_str(), NULL, NULL);
    MouseCursor = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);

    glfwMakeContextCurrent(MainWindow);
    glfwSetCursor(MainWindow, MouseCursor);
    glfwSetInputMode(MainWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    glfwSetWindowSizeCallback(MainWindow, &WindowSizeFunc);
    glfwSetMouseButtonCallback(MainWindow, &MouseButtonFunc);
    glfwSetScrollCallback(MainWindow, &MouseScrollFunc);
    glfwSetKeyCallback(MainWindow, &KeyFunc);
    glfwSetCharCallback(MainWindow, &CharInputFunc);
    glfwSwapInterval(VerticalSync ? 1 : 0);

    int gladStatus = gladLoadGL(glfwGetProcAddress);
    assert(gladStatus != 0);

    GLMajorVersion = GLAD_VERSION_MAJOR(gladStatus);
    GLMinorVersion = GLAD_VERSION_MINOR(gladStatus);

#ifdef NEWORLD_DEBUG
    if (!glDebugMessageCallback) {
        spdlog::warn("Note that you're in debug mode, but GL_KHR_debug is not supported.", title.str());
    } else {
        glDebugMessageCallback(glDebugCallback, nullptr);
        spdlog::info("GL_KHR_debug enabled.");
    }
#endif

    // Make sure everything is initialised
    TextRenderer::initFont();
    initStretch();
    glDefaults();
    Renderer::initShaders();
}

export void toggleFullScreen() {
    static bool fullscreen = false;
    static int ww = 0, wh = 0;

    GLFWvidmode const* mode;
    mode = glfwGetVideoMode(glfwGetPrimaryMonitor());

    fullscreen = !fullscreen;
    if (fullscreen) {
        ww = WindowWidth, wh = WindowHeight;
        WindowWidth = mode->width;
        WindowHeight = mode->height;
        glfwSetWindowMonitor(MainWindow, glfwGetPrimaryMonitor(), 0, 0, WindowWidth, WindowHeight, mode->refreshRate);
    } else {
        WindowWidth = ww, WindowHeight = wh;
        glfwSetWindowMonitor(
            MainWindow,
            nullptr,
            (mode->width - ww) / 2,
            (mode->height - wh) / 2,
            WindowWidth,
            WindowHeight,
            mode->refreshRate
        );
    }

    glDefaults();
    Renderer::initShaders();
}

export void loadTextures() {
    SelectedTexture = Textures::loadRGBTexture("textures/ui/select.bmp", false);
    UnselectedTexture = Textures::loadRGBTexture("textures/ui/unselect.bmp", false);
    TitleTexture = Textures::loadRGBATexture("textures/ui/title.bmp", "textures/ui/title_mask.bmp", true);
    for (int i = 0; i < 6; i++) {
        std::stringstream ss;
        ss << "textures/ui/background_" << i << ".bmp";
        UIBackgroundTextures[i] = Textures::loadRGBTexture(ss.str(), true);
    }
    BlockTextureArray =
        Textures::loadBlockTextureArray("textures/blocks/diffuse.bmp", "textures/blocks/diffuse_mask.bmp");
}

export void splashScreen() {
    if (SplashTexture == 0) {
        SplashTexture = Textures::loadRGBTexture("textures/ui/splash.bmp", true);
    }

    for (int i = 0; i < 256; i += 2) {
        float ratio = static_cast<float>(i) / 256.0f;

        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClearDepth(1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        glBindTexture(GL_TEXTURE_2D, SplashTexture);
        glBegin(GL_QUADS);
        glColor4f(ratio, ratio, ratio, 1.0f);
        glTexCoord2f(0.0f, 1.0f);
        glVertex2i(-1, 1);
        glTexCoord2f(0.0f, 0.0f);
        glVertex2i(-1, -1);
        glTexCoord2f(1.0f, 0.0f);
        glVertex2i(1, -1);
        glTexCoord2f(1.0f, 1.0f);
        glVertex2i(1, 1);
        glEnd();

        glfwSwapBuffers(MainWindow);
        glfwPollEvents();

        Sleep(10);
    }
}
