#include"WindowUtil.h"
#include"Setup.h"
#include"GLProc.h"
#include"GUI.h"
#include"TextRenderer.h"

void Window::InitStretch() {
	ppistretch = true;
	int nScreenWidth, nScreenHeight;
	glfwGetMonitorPhysicalSize(glfwGetPrimaryMonitor(), &nScreenWidth,
		&nScreenHeight);
	int vmc;
	const GLFWvidmode* mode = glfwGetVideoModes(glfwGetPrimaryMonitor(), &vmc);
	double ppi = static_cast<double>(mode[vmc - 1].width) / (static_cast<double>(nScreenWidth) / 25.4f);
	stretch = ppi / 96.0f;
	//Calaulate the scale and resize the window
	windowwidth = static_cast<int>(windowwidth * stretch);
	windowheight = static_cast<int>(windowheight * stretch);
	glfwSetWindowSize(MainWindow, windowwidth, windowheight);
	TextRenderer::resize();
}

void Window::EndStretch() {
	ppistretch = false;
	windowwidth = static_cast<int>(windowwidth / stretch);
	windowheight = static_cast<int>(windowheight / stretch);
	stretch = 1.0;
	glfwSetWindowSize(MainWindow, windowwidth, windowheight);
	TextRenderer::resize();
}

Window::Window(string title) {
	if (Multisample != 0) glfwWindowHint(GLFW_SAMPLES, Multisample);
	MainWindow = glfwCreateWindow(windowwidth, windowheight, title.c_str(), NULL, NULL);
	MouseCursor = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
	glfwMakeContextCurrent(MainWindow);
	glfwSetCursor(MainWindow, MouseCursor);
	glfwSetInputMode(MainWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	if (ppistretch) InitStretch();
}

void Window::setupScreen() {
	//获取OpenGL版本
	GLVersionMajor = glfwGetWindowAttrib(MainWindow, GLFW_CONTEXT_VERSION_MAJOR);
	GLVersionMinor = glfwGetWindowAttrib(MainWindow, GLFW_CONTEXT_VERSION_MINOR);
	GLVersionRev = glfwGetWindowAttrib(MainWindow, GLFW_CONTEXT_REVISION);
	//获取OpenGL函数地址
	InitGLProc();

	//渲染参数设置
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
	glAlphaFunc(GL_GREATER, 0.0); //<--这家伙在卖萌？(往后面看看，卖萌的多着呢)
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
}

void Window::setupNormalFog() {
	float fogColor[4] = { skycolorR, skycolorG, skycolorB, 1.0f };
	glEnable(GL_FOG);
	glFogi(GL_FOG_MODE, GL_LINEAR);
	glFogfv(GL_FOG_COLOR, fogColor);
	glFogf(GL_FOG_START, viewdistance * 16.0f - 32.0f);
	glFogf(GL_FOG_END, viewdistance * 16.0f);
}

void WSF(GLFWwindow * win, int width, int height) {
	if (width<640) width = 640;
	if (height<360) height = 360;
	windowwidth = width;
	windowheight = height > 0 ? height : 1;
	glfwSetWindowSize(win, width, height);
	setupScreen();
}

void MBF(GLFWwindow *, int button, int action, int) {
	mb = 0;
	if (action == GLFW_PRESS) {
		if (button == GLFW_MOUSE_BUTTON_LEFT)mb += 1;
		if (button == GLFW_MOUSE_BUTTON_RIGHT)mb += 2;
		if (button == GLFW_MOUSE_BUTTON_MIDDLE)mb += 4;
	}
	else mb = 0;
}

void CF(GLFWwindow *, unsigned int c) {
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

void MSF(GLFWwindow *, double, double yoffset) {
	mw += (int)yoffset;
}


void InitWindowUtil()
{
	glfwSetErrorCallback([](int, const char* desc) { cout << desc << endl; });
	glfwSetInputMode(MainWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	glfwSetWindowSizeCallback(MainWindow, &WSF);
	glfwSetMouseButtonCallback(MainWindow, &MBF);
	glfwSetScrollCallback(MainWindow, &MSF);
	glfwSetCharCallback(MainWindow, &CF);
}
