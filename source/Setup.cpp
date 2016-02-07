#include "Setup.h"
#include "GUI.h"
#include "Definitions.h"
#include "Textures.h"
#include "TextRenderer.h"
#include "Renderer.h"
#include "World.h"
#include "Items.h"

void splashScreen() {
	TextureID splTex = Textures::LoadRGBTexture("Textures/GUI/splashscreen.bmp");
	glEnable(GL_TEXTURE_2D);
	for (int i = 0; i < 256; i += 2) {
		glfwSwapBuffers(MainWindow);
		glfwPollEvents();
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glBindTexture(GL_TEXTURE_2D, splTex);
		glColor4f((float)i / 256, (float)i / 256, (float)i / 256, 1.0);
		glBegin(GL_QUADS);
		glTexCoord2f(0.0, 1.0); glVertex2i(-1, 1);
		glTexCoord2f(850.0f / 1024.0f, 1.0); glVertex2i(1, 1);
		glTexCoord2f(850.0f / 1024.0f, 1.0 - 480.0f / 1024.0f); glVertex2i(1, -1);
		glTexCoord2f(0.0, 1.0 - 480.0f / 1024.0f); glVertex2i(-1, -1);
		glEnd();
		Sleep(10);
	}
	glDeleteTextures(1, &splTex);
	glfwSwapBuffers(MainWindow);
	glfwPollEvents();
}

void createWindow() {
	glfwSetErrorCallback([](int, const char* desc) { cout << desc << endl; });
	std::stringstream title;
	title << "NEWorld " << MAJOR_VERSION << MINOR_VERSION << EXT_VERSION;
	if (Multisample != 0) glfwWindowHint(GLFW_SAMPLES, Multisample);
	MainWindow = glfwCreateWindow(windowwidth, windowheight, title.str().c_str(), NULL, NULL);
	MouseCursor = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
	glfwMakeContextCurrent(MainWindow);
	glfwSetCursor(MainWindow, MouseCursor);
	glfwSetInputMode(MainWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	glfwSetWindowSizeCallback(MainWindow, &WindowSizeFunc);
	glfwSetMouseButtonCallback(MainWindow, &MouseButtonFunc);
	glfwSetScrollCallback(MainWindow, &MouseScrollFunc);
	glfwSetCharCallback(MainWindow, &CharInputFunc);
	if (ppistretch) GUI::InitStretch();
}

void setupScreen() {

	//��ȡOpenGL�汾
	GLVersionMajor = glfwGetWindowAttrib(MainWindow, GLFW_CONTEXT_VERSION_MAJOR);
	GLVersionMinor = glfwGetWindowAttrib(MainWindow, GLFW_CONTEXT_VERSION_MINOR);
	GLVersionRev = glfwGetWindowAttrib(MainWindow, GLFW_CONTEXT_REVISION);
	//��ȡOpenGL������ַ
	InitGLProc();

	//��Ⱦ��������
	glViewport(0, 0, windowwidth, windowheight);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glShadeModel(GL_SMOOTH);
	glDisable(GL_DITHER);
	glEnable(GL_CULL_FACE);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_ALPHA_TEST);
	glEnable(GL_BLEND);
	glEnable(GL_LINE_SMOOTH);
	glDepthFunc(GL_LEQUAL);
	glAlphaFunc(GL_GREATER, 0.0); //<--��һ������ȣ�(�����濴�������ȵĶ�����)
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
	glHint(GL_FOG_HINT, GL_FASTEST);
	glHint(GL_LINE_SMOOTH_HINT, GL_FASTEST);
	if (Multisample != 0) glEnable(GL_MULTISAMPLE_ARB);
	glPixelStorei(GL_PACK_ALIGNMENT, 4);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	glColor4f(0.0, 0.0, 0.0, 1.0);
	TextRenderer::BuildFont(windowwidth, windowheight);
	TextRenderer::setFontColor(1.0, 1.0, 1.0, 1.0);
	glClearColor(0.0, 0.0, 0.0, 1.0);
	glClearDepth(1.0);
	glGenBuffersARB(1, &World::EmptyBuffer);
	if (Renderer::AdvancedRender) Renderer::initShaders();
	if (vsync) glfwSwapInterval(1);
	else glfwSwapInterval(0);
}

void setupNormalFog() {
	float fogColor[4] = { skycolorR, skycolorG, skycolorB, 1.0f };
	glEnable(GL_FOG);
	glFogi(GL_FOG_MODE, GL_LINEAR);
	glFogfv(GL_FOG_COLOR, fogColor);
	glFogf(GL_FOG_START, viewdistance * 16.0f - 32.0f);
	glFogf(GL_FOG_END, viewdistance * 16.0f);
}

void loadTextures() {
	//��������
	Textures::Init();

	tex_select = Textures::LoadRGBATexture("Textures/GUI/select.bmp", "");
	tex_unselect = Textures::LoadRGBATexture("Textures/GUI/unselect.bmp", "");
	tex_title = Textures::LoadRGBATexture("Textures/GUI/title.bmp", "Textures/GUI/titlemask.bmp");
	for (int i = 0; i < 6; i++) {
		std::stringstream ss;
		ss << "Textures/GUI/mainmenu" << i << ".bmp";
		tex_mainmenu[i] = Textures::LoadRGBTexture(ss.str());
	}

	DefaultSkin = Textures::LoadRGBATexture("Textures/Player/skin_xiaoqiao.bmp", "Textures/Player/skinmask_xiaoqiao.bmp");

	for (int gloop = 1; gloop <= 10; gloop++) {
		string path = "Textures/blocks/destroy_" + itos(gloop) + ".bmp";
		DestroyImage[gloop] = Textures::LoadRGBATexture(path, path);
	}

	BlockTextures = Textures::LoadRGBATexture("Textures/blocks/Terrain.bmp", "Textures/blocks/Terrainmask.bmp");
	BlockTextures3D = Textures::LoadBlock3DTexture("Textures/blocks/Terrain3D.bmp", "Textures/blocks/Terrain3Dmask.bmp");
	loadItemsTextures();
}

void WindowSizeFunc(GLFWwindow * win, int width, int height) {
	if (width<640) width = 640;
	if (height<360) height = 360;
	windowwidth = width;
	windowheight = height > 0 ? height : 1;
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
		wchar_t* pwszUnicode = new wchar_t[2];
		pwszUnicode[0] = (wchar_t)c;
		pwszUnicode[1] = '\0';
		char* pszMultiByte;
		pszMultiByte = (char*)malloc((unsigned int)4);
		pszMultiByte = (char*)realloc(pszMultiByte, WCharToMByte(pszMultiByte, pwszUnicode, 4));
		inputstr += pszMultiByte;
		free(pszMultiByte);
		delete[] pwszUnicode;
	}
	else inputstr += (char)c;
}

void MouseScrollFunc(GLFWwindow *, double, double yoffset) {
	mw += (int)yoffset;
}
