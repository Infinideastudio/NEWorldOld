#pragma once
#include"Definitions.h"

class Window {
	bool ppistretch;
	double stretch;
public:
	int AccX, AccY, UscX, UscY;
	GLFWwindow* win;
	void InitStretch();
	void EndStretch();
	Window(string title);
	void setupScreen();
	void setupNormalFog();
};

struct UnicodeChar {
	bool aval;
	TextureID tex;
	VBOID buffer;
	unsigned int vtxs;
	int xpos, ypos;
	int width, height, advance;
	UnicodeChar() :aval(false), tex(0), buffer(0),
		vtxs(0), xpos(0), ypos(0),
		width(0), height(0), advance(0) {}
};

class Font {
public:
	FT_Library library;
	FT_Face fontface;
	FT_GlyphSlot slot;
	UnicodeChar chars[65536];
	float r, g, b, a;

	Font();
	void SetColor(float r, float g, float b, float a);
	void loadchar(unsigned int uc);
	int getStrWidth(string s);
	void renderString(int x, int y, string glstring);
	~Font();
};

map<GLFWwindow*, int> index;
vector<Window*>windows;
extern Window* CurWin;
void InitWindowUtil();