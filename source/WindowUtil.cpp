#include"WindowUtil.h"
#include"Setup.h"
#include"GLProc.h"
#include"GUI.h"
#include"TextRenderer.h"

Window* CurWin = nullptr;

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

Font::Font() {
	if (FT_Init_FreeType(&library)) {
		//assert(false);
	}
	if (FT_New_Face(library, "Fonts/Font.ttf", 0, &fontface)) {
		//assert(false);
	}
	if (FT_Set_Pixel_Sizes(fontface, 16 * stretch, 16 * stretch)) {
		//assert(false);
	}
	slot = fontface->glyph;

}

Font::~Font() {
	for (int i = 0; i < 63356; i++) if (chars[i].aval) {
		chars[i].aval = false;
		glDeleteTextures(1, &chars[i].tex);
	}
}

void Font::SetColor(float r_, float g_, float b_, float a_) {
	r = r_; g = g_; b = b_; a = a_;
}

void Font::loadchar(unsigned int uc) {
	FT_Bitmap* bitmap;
	unsigned int index;
	ubyte *Tex, *Texsrc;
	int wid = (int)pow(2, ceil(log2(32 * stretch)));

	index = FT_Get_Char_Index(fontface, uc);
	FT_Load_Glyph(fontface, index, FT_LOAD_DEFAULT);
	FT_Render_Glyph(slot, FT_RENDER_MODE_NORMAL);
	bitmap = &(slot->bitmap);
	Texsrc = bitmap->buffer;
	Tex = new ubyte[wid * wid * 4];
	memset(Tex, 0, wid * wid * 4 * sizeof(ubyte));
	for (unsigned int i = 0; i < bitmap->rows; i++) {
		for (unsigned int j = 0; j < bitmap->width; j++) {
			Tex[(i * wid + j) * 4 + 0] = Tex[(i * wid + j) * 4 + 1] = Tex[(i * wid + j) * 4 + 2] = 255U;
			Tex[(i * wid + j) * 4 + 3] = *Texsrc; Texsrc++;
		}
	}
	glGenTextures(1, &chars[uc].tex);
	glBindTexture(GL_TEXTURE_2D, chars[uc].tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, wid, wid, 0, GL_RGBA, GL_UNSIGNED_BYTE, Tex);
	delete[] Tex;
	chars[uc].aval = true;
	chars[uc].width = bitmap->width;
	chars[uc].height = bitmap->rows;
	chars[uc].advance = slot->advance.x >> 6;
	chars[uc].xpos = slot->bitmap_left;
	chars[uc].ypos = slot->bitmap_top;
}

void MBToWC(const char* lpcszStr, wchar_t*& lpwszStr, int dwSize) {
	lpwszStr = (wchar_t*)malloc(dwSize);
	memset(lpwszStr, 0, dwSize);
	int iSize = (MByteToWChar(lpwszStr, lpcszStr, strlen(lpcszStr)) + 1)*sizeof(wchar_t);
	lpwszStr = (wchar_t*)realloc(lpwszStr, iSize);
}

int Font::getStrWidth(string s) {
	UnicodeChar c;
	int uc, res = 0;
	unsigned int i = 0;
	wchar_t* wstr = nullptr;
	MBToWC(s.c_str(), wstr, 128);
	for (unsigned int k = 0; k < wstrlen(wstr); k++) {
		if (s[i] >= 0 && s[i] <= 127) i++; else i += 2;
		uc = wstr[k];
		c = chars[uc];
		if (!c.aval) {
			loadchar(uc);
			c = chars[uc];
		}
		res += c.advance / stretch;
	}
	free(wstr);
	return res;
}

void Font::renderString(int x, int y, string glstring) {
	UnicodeChar c;
	int uc, span = 0;
	double wid = pow(2, ceil(log2(32 * stretch)));
	wchar_t* wstr = nullptr;
	MBToWC(glstring.c_str(), wstr, 128);

	glEnable(GL_TEXTURE_2D);
	for (unsigned int k = 0; k < wstrlen(wstr); k++) {

		uc = wstr[k];
		if (!c.aval) loadchar(uc);
		c = chars[uc];

		glBindTexture(GL_TEXTURE_2D, c.tex);

		UITrans(x + 1 + span, y + 1);
		glColor4f(0.5, 0.5, 0.5, a);
		glBegin(GL_QUADS);
		glTexCoord2d(0.0, 0.0);
		UIVertex(c.xpos / stretch, 15 - c.ypos / stretch);
		glTexCoord2d(c.width / wid, 0.0);
		UIVertex(c.xpos / stretch + c.width / stretch, 15 - c.ypos / stretch);
		glTexCoord2d(c.width / wid, c.height / wid);
		UIVertex(c.xpos / stretch + c.width / stretch, 15 + c.height / stretch - c.ypos / stretch);
		glTexCoord2d(0.0, c.height / wid);
		UIVertex(c.xpos / stretch, 15 + c.height / stretch - c.ypos / stretch);
		glEnd();

		UITrans(-1, -1);
		glColor4f(r, g, b, a);
		glBegin(GL_QUADS);
		glTexCoord2d(0.0, 0.0);
		UIVertex(c.xpos / stretch, 15 - c.ypos / stretch);
		glTexCoord2d(c.width / wid, 0.0);
		UIVertex(c.xpos / stretch + c.width / stretch, 15 - c.ypos / stretch);
		glTexCoord2d(c.width / wid, c.height / wid);
		UIVertex(c.xpos / stretch + c.width / stretch, 15 + c.height / stretch - c.ypos / stretch);
		glTexCoord2d(0.0, c.height / wid);
		UIVertex(c.xpos / stretch, 15 + c.height / stretch - c.ypos / stretch);
		glEnd();

		UITrans(-x - span, -y);
		span += c.advance / stretch;
	}
	glColor4f(1.0, 1.0, 1.0, 1.0);
	free(wstr);
}
