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
	MainMenu Menu = MainMenu();
	gui::UIEnter((gui::UIView*)&Menu);
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
