#include "Menus.h"

class MainMenu : public gui::UIView
{
private:
	int leftp = windowwidth / 2 - 200;
	int midp = windowwidth / 2;
	int rightp = windowwidth / 2 + 200;
	int upp = 280;

	gui::UIButton startbtn = gui::UIButton("单人游戏");
	gui::UIButton multiplayerbtn = gui::UIButton("多人游戏");
	gui::UIButton optionsbtn = gui::UIButton(">> 选项...");
	gui::UIButton quitbtn = gui::UIButton("退出");
public:
	MainMenu()
	{
		glfwSetInputMode(MainWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		glClearColor(0.0, 0.0, 0.0, 1.0);
		glDisable(GL_CULL_FACE);
		Init();
		RegisterUI(&startbtn);
		RegisterUI(&multiplayerbtn);
		RegisterUI(&optionsbtn);
		RegisterUI(&quitbtn);
	}

	~MainMenu()
	{

	}

	void OnResize(){
		leftp = windowwidth / 2 - 200;
		midp = windowwidth / 2;
		rightp = windowwidth / 2 + 200;
		upp = 280;
		startbtn.UISetRect(leftp, rightp, upp, upp + 32);
		multiplayerbtn.UISetRect(leftp, rightp, upp + 38, upp + 32 + 38);
		optionsbtn.UISetRect(leftp, midp - 3, upp + 38 * 2, upp + 72 + 38);
		quitbtn.UISetRect(midp + 3, rightp, upp + 38 * 2, upp + 72 + 38);

	}

	void OnUpdate(){
		if (startbtn.clicked) worldmenu();
		if (multiplayerbtn.clicked) MultiplayerGameMenu();
		if (gamebegin) gui::UIExit();
		if (optionsbtn.clicked) options();
		if (quitbtn.clicked) exit(0);
	}

	void OnRender(){
		glBindTexture(GL_TEXTURE_2D, guiImage[4]);
		glBegin(GL_QUADS);
		glTexCoord2f(0.0f, 1.0f), glVertex2i(midp - 256, 20);
		glTexCoord2f(0.0f, 0.5f), glVertex2i(midp - 256, 276);
		glTexCoord2f(1.0f, 0.5f), glVertex2i(midp + 256, 276);
		glTexCoord2f(1.0f, 1.0f), glVertex2i(midp + 256, 20);
		glEnd();
	}
};

void mainmenu(){
	MainMenu Menu = MainMenu();
	gui::UIEnter((gui::UIView*)&Menu);
}