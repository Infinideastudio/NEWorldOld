#include "Setup.h"
#include "Definitions.h"
#include "Textures.h"
#include "TextRenderer.h"
#include "Renderer.h"
#include "World.h"
#include "Items.h"

// OpenGL debug callback
void APIENTRY glDebugCallback(GLenum, GLenum, GLuint, GLenum severity, GLsizei, const GLchar* msg, const void*) {
	if (severity != GL_DEBUG_SEVERITY_NOTIFICATION) std::cerr << "[Console][OpenGL]" << msg << std::endl;
}

void createWindow() {
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
	glfwSetCharCallback(MainWindow, &CharInputFunc);
	glfwSwapInterval(VerticalSync ? 1 : 0);

	auto glewStatus = glewInit();
	assert(glewStatus == GLEW_OK);

	GLMajorVersion = glfwGetWindowAttrib(MainWindow, GLFW_CONTEXT_VERSION_MAJOR);
	GLMinorVersion = glfwGetWindowAttrib(MainWindow, GLFW_CONTEXT_VERSION_MINOR);
	GLRevisionVersion = glfwGetWindowAttrib(MainWindow, GLFW_CONTEXT_REVISION);

#ifdef NEWORLD_DEBUG
	if (!glDebugMessageCallback) {
		DebugWarning("Note that you're in debug mode, but GL_KHR_debug is not supported.");
	}
	else {
		glDebugMessageCallback(glDebugCallback, nullptr);
		DebugInfo("GL_KHR_debug enabled.");
	}
#endif
}

void toggleFullScreen() {
	static bool fullscreen = false;
	static int ww = 0, wh = 0;

	const GLFWvidmode* mode;
	mode = glfwGetVideoMode(glfwGetPrimaryMonitor());

	fullscreen = !fullscreen;
	if (fullscreen) {
		ww = WindowWidth, wh = WindowHeight;
		WindowWidth = mode->width;
		WindowHeight = mode->height;
		glfwSetWindowMonitor(MainWindow, glfwGetPrimaryMonitor(), 0, 0, WindowWidth, WindowHeight, mode->refreshRate);
	}
	else {
		WindowWidth = ww, WindowHeight = wh;
		glfwSetWindowMonitor(MainWindow, nullptr, (mode->width - ww) / 2, (mode->height - wh) / 2, WindowWidth, WindowHeight, mode->refreshRate);
	}

	setupScreen();
}

void setupScreen() {
	// Set up default GL context states
	glViewport(0, 0, WindowWidth, WindowHeight);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_TEXTURE_2D);
	if (Multisample != 0) glEnable(GL_MULTISAMPLE);
	glPixelStorei(GL_PACK_ALIGNMENT, 4);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

	// Make sure everything is initialised
	TextRenderer::BuildFont(WindowWidth, WindowHeight);
	TextRenderer::setFontColor(1.0, 1.0, 1.0, 1.0);
	Renderer::initShaders();
}

void loadTextures() {
	SelectedTexture = Textures::LoadRGBTexture("textures/ui/select.bmp");
	UnselectedTexture = Textures::LoadRGBTexture("textures/ui/unselect.bmp");
	TitleTexture = Textures::LoadRGBATexture("textures/ui/title.bmp", "textures/ui/title_mask.bmp", true);

	for (int i = 0; i < 6; i++) {
		std::stringstream ss;
		ss << "textures/ui/background_" << i << ".bmp";
		UIBackgroundTextures[i] = Textures::LoadRGBTexture(ss.str(), true);
	}

	BlockTextureArray = Textures::LoadBlockTextureArray("textures/blocks/diffuse.bmp", "textures/blocks/diffuse_mask.bmp");
	loadItemsTextures();
	DefaultSkin = Textures::LoadRGBATexture("textures/skins/skin_xiaoqiao.bmp", "textures/skins/skin_xiaoqiao_mask.bmp");
}

void WindowSizeFunc(GLFWwindow* win, int width, int height) {
	if (width < 640) width = 640;
	if (height < 360) height = 360;
	WindowWidth = width;
	WindowHeight = height > 0 ? height : 1;
	glfwSetWindowSize(win, width, height);
	setupScreen();
}

void MouseButtonFunc(GLFWwindow *, int button, int action, int) {
	mb = 0;
	if (action == GLFW_PRESS) {
		if (button == GLFW_MOUSE_BUTTON_LEFT)mb += 1;
		if (button == GLFW_MOUSE_BUTTON_RIGHT)mb += 2;
		if (button == GLFW_MOUSE_BUTTON_MIDDLE)mb += 4;
	}
	else mb = 0;
}

void CharInputFunc(GLFWwindow *, unsigned int c) {
	if (c >= 128) {
		static char buffer[64];
		wchar_t unicode = static_cast<wchar_t>(c);
		int size = WCharToMByte(buffer, &unicode, 64, 1);
		inputstr += std::string(buffer, size);
	}
	else inputstr += (char)c;
}

void MouseScrollFunc(GLFWwindow *, double, double yoffset) {
	mw += (int)yoffset;
}

void splashScreen() {
	if (SplashTexture == 0) {
		SplashTexture = Textures::LoadRGBTexture("textures/ui/splash.bmp", true);
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
		glTexCoord2f(0.0f, 1.0f); glVertex2i(-1, 1);
		glTexCoord2f(0.0f, 0.0f); glVertex2i(-1, -1);
		glTexCoord2f(1.0f, 0.0f); glVertex2i(1, -1);
		glTexCoord2f(1.0f, 1.0f); glVertex2i(1, 1);
		glEnd();

		glfwSwapBuffers(MainWindow);
		glfwPollEvents();

		Sleep(10);
	}
}
