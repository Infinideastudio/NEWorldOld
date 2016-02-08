#pragma once
#include"Definitions.h"

class Window {
public:
	GLFWwindow* win;
	void InitStretch();
	void EndStretch();
	Window(string title);
	void setupScreen();
	void setupNormalFog();
};

map<GLFWwindow*, int> index;
vector<Window*>windows;

void InitWindowUtil();