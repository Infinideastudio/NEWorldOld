#include "Menus.h"
extern bool gamebegin;

template<typename T>
string strWithVar(string str, T var){
	std::stringstream ss;
	ss << str << var;
	return ss.str();
}
int getDotCount(string s) {
	int ret = 0;
	for (unsigned int i = 0; i != s.size(); i++)
	if (s[i] == '.') ret++;
	return ret;
}

void MultiplayerGameMenu() {
	NEMultiplayerGameMenu Menu = NEMultiplayerGameMenu();
	gui::UIEnter((gui::UIView*)&Menu);
}

void mainmenu(){
<<<<<<< HEAD
	//主菜单
	gui::Form MainForm;
	int leftp = windowwidth / 2 - 200;
	int midp = windowwidth / 2;
	int rightp = windowwidth / 2 + 200;
	int upp = 280;
	glfwSetInputMode(MainWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	glClearColor(0.0, 0.0, 0.0, 1.0);
	glDisable(GL_CULL_FACE);
	bool f = false;
	MainForm.Init();
	TextRenderer::setFontColor(1.0, 1.0, 1.0, 1.0);
	gui::button* startbtn = MainForm.createbutton("开始游戏");
	//gui::button* multiplayerbtn = MainForm.createbutton("多人游戏");
	gui::button* optionsbtn = MainForm.createbutton(">> 选项...");
	gui::button* quitbtn = MainForm.createbutton("退出");
	do{
		leftp = windowwidth / 2 - 200;
		midp = windowwidth / 2;
		rightp = windowwidth / 2 + 200;
		startbtn->resize(leftp, rightp, upp, upp + 32);
		//multiplayerbtn->resize(leftp, rightp, upp + 38, upp + 32 + 38);
		//optionsbtn->resize(leftp, midp - 3, upp + 38 * 2, upp + 72 + 38);
		//quitbtn->resize(midp + 3, rightp, upp + 38 * 2, upp + 72 + 38);
		optionsbtn->resize(leftp, midp - 3, upp + 38, upp + 72);
		quitbtn->resize(midp + 3, rightp, upp + 38, upp + 72);
		mb = glfwGetMouseButton(MainWindow, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS ? 1 : 0;
		glfwGetCursorPos(MainWindow, &mx, &my);
		MainForm.mousedata((int)mx, (int)my, mw, mb);
		MainForm.update();
		if (startbtn->clicked) worldmenu();
		//if (multiplayerbtn->clicked) MultiplayerGameMenu();
		if (gamebegin) f = true;
		if (optionsbtn->clicked) options();
		if (quitbtn->clicked) exit(0);
		MainForm.render();
		glBindTexture(GL_TEXTURE_2D, guiImage[4]);
		glBegin(GL_QUADS);
		glTexCoord2f(0.0f, 1.0f), glVertex2i(midp - 256, 20);
		glTexCoord2f(0.0f, 0.5f), glVertex2i(midp - 256, 276);
		glTexCoord2f(1.0f, 0.5f), glVertex2i(midp + 256, 276);
		glTexCoord2f(1.0f, 1.0f), glVertex2i(midp + 256, 20);
		glEnd();
		glfwSwapBuffers(MainWindow);
		glfwPollEvents();
		if (glfwWindowShouldClose(MainWindow)) exit(0);
	} while (!f);
	MainForm.cleanup();
=======
	MainMenu Menu = MainMenu();
	gui::UIEnter((gui::UIView*)&Menu);
>>>>>>> refs/remotes/origin/master
}

void options(){
	//设置菜单
	Options Menu = Options();
	gui::UIEnter((gui::UIView*)&Menu);
}

void Renderoptions(){
    //渲染设置菜单
	RenderOptions Menu = RenderOptions();
	gui::UIEnter((gui::UIView*)&Menu);
}

void GUIoptions(){
    //GUI设置菜单
	GUIOptions Menu = GUIOptions();
	gui::UIEnter((gui::UIView*)&Menu);
}

void worldmenu(){
	//世界选择菜单
	NEWorldMenu Menu = NEWorldMenu();
	gui::UIEnter((gui::UIView*)&Menu);
}

void createworldmenu(){
	CreateWorldMenu Menu = CreateWorldMenu();
	gui::UIEnter((gui::UIView*)&Menu);
}
