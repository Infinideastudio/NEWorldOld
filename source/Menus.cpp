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

class NEMultiplayerGameMenu : public gui::UIView
{
private:
	gui::UILabel   title = gui::UILabel("==============<  �� �� �� Ϸ  >==============");
	gui::UITextBox serveriptb = gui::UITextBox("���������IP");
	gui::UIButton  runbtn = gui::UIButton("���з�������������");
	gui::UIButton  okbtn = gui::UIButton("ȷ��");
	gui::UIButton  backbtn = gui::UIButton("<< ����");

	bool serveripChanged = false;
public:
	NEMultiplayerGameMenu();
	~NEMultiplayerGameMenu();

	void OnResize();
	void OnUpdate();
	void OnRender();
};

class MainMenu : public gui::UIView
{
private:
	int leftp = windowwidth / 2 - 200;
	int midp = windowwidth / 2;
	int rightp = windowwidth / 2 + 200;
	int upp = 280;

	gui::UIButton startbtn = gui::UIButton("������Ϸ");
	gui::UIButton multiplayerbtn = gui::UIButton("������Ϸ");
	gui::UIButton optionsbtn = gui::UIButton(">> ѡ��...");
	gui::UIButton quitbtn = gui::UIButton("�˳�");
public:
	MainMenu();
	~MainMenu();

	void OnResize();
	void OnUpdate();
	void OnRender();
};

class Options : public gui::UIView
{
private:
	gui::UILabel	 title = gui::UILabel("=================<  ѡ ��  >=================");
	gui::UITrackBar  FOVyBar = gui::UITrackBar(strWithVar("��Ұ�Ƕȣ�", FOVyNormal), 120, (int)(FOVyNormal - 1));
	gui::UITrackBar  mmsBar = gui::UITrackBar(strWithVar("��������ȣ�", mousemove), 120, (int)(mousemove * 40 * 2 - 1));
	gui::UITrackBar  viewdistBar = gui::UITrackBar(strWithVar("��Ⱦ���룺", viewdistance), 120, (viewdistance - 1) * 8 - 1);
	gui::UIButton	 rdstbtn = gui::UIButton(">> ��Ⱦѡ��...");
	gui::UIButton	 gistbtn = gui::UIButton(">> ͼ�ν���ѡ��...");
	gui::UIButton	 backbtn = gui::UIButton("<< �������˵�");
public:
	Options();
	~Options();

	void OnResize();
	void OnUpdate();
	void OnRender();
};

class RenderOptions : public gui::UIView
{
private:
	gui::UILabel   title = gui::UILabel("==============<  �� Ⱦ ѡ ��  >==============");
	gui::UIButton  backbtn = gui::UIButton("<< ����ѡ��˵�");

public:
	RenderOptions();
	~RenderOptions();

	void OnResize();
	void OnUpdate();
	void OnRender();
};

class GUIOptions : public gui::UIView
{
private:
	gui::UILabel  title = gui::UILabel("===============< ͼ�ν���ѡ�� >==============");
	gui::UIButton fontbtn = gui::UIButton("ȫ��ʹ��Unicode���壺" + boolstr(TextRenderer::useUnicodeASCIIFont));
	gui::UIButton backbtn = gui::UIButton("<< ����ѡ��˵�");

public:
	GUIOptions();
	~GUIOptions();

	void OnResize();
	void OnUpdate();
	void OnRender();
};

class NEWorldMenu : public gui::UIView
{
private:
	bool refresh = true;
	int selected = 0, mouseon;
	ubyte mblast = 1;
	int  mwlast = 0;
	string chosenWorldName;
	vector<string> worldnames;
	vector<TextureID> thumbnails, texSizeX, texSizeY;
	int trs = 0;
	int worldcount;
	int leftp = windowwidth / 2 - 200;
	int midp = windowwidth / 2;
	int rightp = windowwidth / 2 + 200;
	int downp = windowheight - 20;

	gui::UILabel          title = gui::UILabel("==============<  ѡ �� �� ��  >==============");
	gui::UIVerticalScroll UIVerticalScroll = gui::UIVerticalScroll(100, 0);
	gui::UIButton         enterbtn = gui::UIButton("����ѡ��������");
	gui::UIButton         deletebtn = gui::UIButton("ɾ��ѡ��������");
	gui::UIButton         backbtn = gui::UIButton("<< �������˵�");

public:
	NEWorldMenu();
	~NEWorldMenu();

	void OnResize();
	void OnUpdate();
	void OnRender();
};

class CreateWorldMenu : public gui::UIView
{
private:
	gui::UILabel   title = gui::UILabel("==============<  �� �� �� ��  >==============");
	gui::UITextBox worldnametb = gui::UITextBox("������������");
	gui::UIButton  okbtn = gui::UIButton("ȷ��");
	gui::UIButton  backbtn = gui::UIButton("<< ��������˵�");

	bool worldnametbChanged = false;


public:
	CreateWorldMenu();
	~CreateWorldMenu();

	void OnResize();
	void OnUpdate();
	void OnRender();
};

void MultiplayerGameMenu() {
	NEMultiplayerGameMenu Menu = NEMultiplayerGameMenu();
	gui::UIEnter((gui::UIView*)&Menu);
}

void mainmenu(){
	MainMenu Menu = MainMenu();
	gui::UIEnter((gui::UIView*)&Menu);
}

void options(){
	//���ò˵�
	Options Menu = Options();
	gui::UIEnter((gui::UIView*)&Menu);
}

void Renderoptions(){
    //��Ⱦ���ò˵�
	RenderOptions Menu = RenderOptions();
	gui::UIEnter((gui::UIView*)&Menu);
}

void GUIoptions(){
    //GUI���ò˵�
	GUIOptions Menu = GUIOptions();
	gui::UIEnter((gui::UIView*)&Menu);
}

void worldmenu(){
	//����ѡ��˵�
	NEWorldMenu Menu = NEWorldMenu();
	gui::UIEnter((gui::UIView*)&Menu);
}

void createworldmenu(){
	CreateWorldMenu Menu = CreateWorldMenu();
	gui::UIEnter((gui::UIView*)&Menu);
}


NEMultiplayerGameMenu::NEMultiplayerGameMenu() {
	Init();
	RegisterUI(&title);
	RegisterUI(&serveriptb);
	RegisterUI(&runbtn);
	RegisterUI(&okbtn);
	RegisterUI(&backbtn);

	inputstr = "";
	okbtn.enabled = false;
}

NEMultiplayerGameMenu::~NEMultiplayerGameMenu() {
	if (serveripChanged) {
		serverip = serveriptb.text;
		gamebegin = true;
		multiplayer = true;
	}
}

void NEMultiplayerGameMenu::OnResize() {
	int leftp = windowwidth / 2 - 250;
	int rightp = windowwidth / 2 + 250;
	int midp = windowwidth / 2;
	int downp = windowheight - 20;

	title.UISetRect(midp - 225, midp + 225, 20, 36);
	serveriptb.UISetRect(midp - 250, midp + 250, 48, 72);
	runbtn.UISetRect(leftp, rightp, downp - 24 - 50, downp - 50);
	okbtn.UISetRect(midp - 250, midp + 250, 84, 120);
	backbtn.UISetRect(leftp, rightp, downp - 24, downp);
}

void NEMultiplayerGameMenu::OnUpdate() {
	if (serveriptb.pressed && !serveripChanged) {
		serveriptb.text = "";
		serveripChanged = true;
	}

	if (serveriptb.text == "" || !serveripChanged || getDotCount(serveriptb.text) != 3) okbtn.enabled = false;
	else okbtn.enabled = true;
	if (okbtn.clicked || backbtn.clicked)  gui::UIExit;
	if (runbtn.clicked) WinExec("NEWorldServer.exe", SW_SHOWDEFAULT);
	inputstr = "";
}

void NEMultiplayerGameMenu::OnRender() {

}

Options::Options()
{
	Init();
	RegisterUI(&title);
	RegisterUI(&FOVyBar);
	RegisterUI(&mmsBar);
	RegisterUI(&viewdistBar);
	//RegisterUI(&ciArrayBtn = UIButton("ʹ�������������飺" + boolstr(UseCIArray))
	RegisterUI(&rdstbtn);
	RegisterUI(&gistbtn);
	RegisterUI(&backbtn);
	//ciArrayBtn.enabled = false
	//savebtn.enabled = false
	//gui::UIButton*	savebtn = UIButton("��������")
}

Options::~Options()
{

}

void Options::OnResize(){
	int leftp = windowwidth / 2 - 250;
	int rightp = windowwidth / 2 + 250;
	int midp = windowwidth / 2;
	int upp = 60;
	int downp = windowheight - 20;
	int lspc = 36;
	title.UISetRect(midp - 225, midp + 225, 20, 36);
	FOVyBar.UISetRect(leftp, midp - 10, upp + lspc * 0, upp + lspc * 0 + 24);
	mmsBar.UISetRect(midp + 10, rightp, upp + lspc * 0, upp + lspc * 0 + 24);
	viewdistBar.UISetRect(leftp, midp - 10, upp + lspc * 1, upp + lspc * 1 + 24);
	//ciArrayBtn.UISetRect(midp + 10, rightp, upp + lspc * 1, upp + lspc * 1 + 24)
	rdstbtn.UISetRect(leftp, midp - 10, upp + lspc * 4, upp + lspc * 4 + 24);
	gistbtn.UISetRect(midp + 10, rightp, upp + lspc * 4, upp + lspc * 4 + 24);
	backbtn.UISetRect(leftp, midp - 10, downp - 24, downp);
	//savebtn.UISetRect(midp + 10, rightp, downp - 24, downp)

}

void Options::OnUpdate(){
	FOVyNormal = static_cast<float>(FOVyBar.barpos + 1);
	mousemove = (mmsBar.barpos / 2 + 1) / 40.0f;
	viewdistance = viewdistBar.barpos / 8 + 2;
	FOVyBar.text = strWithVar("��Ұ�Ƕȣ�", FOVyNormal);
	mmsBar.text = strWithVar("��������ȣ�", mousemove);
	viewdistBar.text = strWithVar("��Ⱦ���룺", viewdistance);
	//ciArrayBtn.text = strWithVar("ʹ��������������, ", boolstr(UseCIArray))
	//if (ciArrayBtn.clicked) UseCIArray = !UseCIArray
	if (rdstbtn.clicked) Renderoptions();
	if (gistbtn.clicked) GUIoptions();
	if (backbtn.clicked) gui::UIExit();
}

void Options::OnRender(){

}

RenderOptions::RenderOptions()
{
	Init();
	RegisterUI(&title);
	RegisterUI(&backbtn);
}

RenderOptions::~RenderOptions()
{

}

void RenderOptions::OnResize(){
	int leftp = windowwidth / 2 - 250;
	int rightp = windowwidth / 2 + 250;
	int midp = windowwidth / 2;
	int downp = windowheight - 20;

	title.UISetRect(midp - 225, midp + 225, 20, 36);
	backbtn.UISetRect(leftp, rightp, downp - 24, downp);
}

void RenderOptions::OnUpdate(){
	if (backbtn.clicked) gui::UIExit();
}

void RenderOptions::OnRender(){

}

GUIOptions::GUIOptions()
{
	Init();
	RegisterUI(&title);
	RegisterUI(&fontbtn);
	RegisterUI(&backbtn);
}

GUIOptions::~GUIOptions()
{

}

void GUIOptions::OnResize(){
	int leftp = windowwidth / 2 - 250;
	int rightp = windowwidth / 2 + 250;
	int midp = windowwidth / 2;
	int upp = 60;
	int downp = windowheight - 20;
	int lspc = 36;
	title.UISetRect(midp - 225, midp + 225, 20, 36);
	fontbtn.UISetRect(leftp, midp - 10, upp + lspc * 0, upp + lspc * 0 + 24);
	backbtn.UISetRect(leftp, rightp, downp - 24, downp);
}

void GUIOptions::OnUpdate(){
	if (fontbtn.clicked) TextRenderer::useUnicodeASCIIFont = !TextRenderer::useUnicodeASCIIFont;
	if (backbtn.clicked) gui::UIExit();
	fontbtn.text = "ȫ��ʹ��Unicode���壺" + boolstr(TextRenderer::useUnicodeASCIIFont);
}

void GUIOptions::OnRender(){

}

MainMenu::MainMenu()
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

MainMenu::~MainMenu()
{

}

void MainMenu::OnResize(){
	leftp = windowwidth / 2 - 200;
	midp = windowwidth / 2;
	rightp = windowwidth / 2 + 200;
	upp = 280;
	startbtn.UISetRect(leftp, rightp, upp, upp + 32);
	multiplayerbtn.UISetRect(leftp, rightp, upp + 38, upp + 32 + 38);
	optionsbtn.UISetRect(leftp, midp - 3, upp + 38 * 2, upp + 72 + 38);
	quitbtn.UISetRect(midp + 3, rightp, upp + 38 * 2, upp + 72 + 38);

}

void MainMenu::OnUpdate(){
	if (startbtn.clicked) worldmenu();
	if (multiplayerbtn.clicked) MultiplayerGameMenu();
	if (gamebegin) gui::UIExit();
	if (optionsbtn.clicked) options();
	if (quitbtn.clicked) exit(0);
}

void MainMenu::OnRender(){
	glBindTexture(GL_TEXTURE_2D, guiImage[4]);
	glBegin(GL_QUADS);
	glTexCoord2f(0.0f, 1.0f), glVertex2i(midp - 256, 20);
	glTexCoord2f(0.0f, 0.5f), glVertex2i(midp - 256, 276);
	glTexCoord2f(1.0f, 0.5f), glVertex2i(midp + 256, 276);
	glTexCoord2f(1.0f, 1.0f), glVertex2i(midp + 256, 20);
	glEnd();
}


CreateWorldMenu::CreateWorldMenu()
{
	Init();
	okbtn.enabled = false;
	inputstr = "";
	RegisterUI(&title);
	RegisterUI(&worldnametb);
	RegisterUI(&okbtn);
	RegisterUI(&backbtn);
}

CreateWorldMenu::~CreateWorldMenu()
{
	if (worldnametbChanged){
		world::worldname = worldnametb.text;
		gamebegin = true;
		multiplayer = false;
	}
}

void CreateWorldMenu::OnResize(){
	int leftp = windowwidth / 2 - 250;
	int rightp = windowwidth / 2 + 250;
	int midp = windowwidth / 2;
	int downp = windowheight - 20;

	title.UISetRect(midp - 225, midp + 225, 20, 36);
	worldnametb.UISetRect(midp - 250, midp + 250, 48, 72);
	okbtn.UISetRect(midp - 250, midp + 250, 84, 120);
	backbtn.UISetRect(leftp, rightp, downp - 24, downp);
}

void CreateWorldMenu::OnUpdate(){

	if (worldnametb.pressed && !worldnametbChanged){
		worldnametb.text = "";
		worldnametbChanged = true;
	}
	if (worldnametb.text == "" || !worldnametbChanged || worldnametb.text.find(" ") != -1) okbtn.enabled = false; else okbtn.enabled = true;
	if (okbtn.clicked){
		gui::UIExit();
	}
	if (backbtn.clicked)gui::UIExit();;
	inputstr = "";
}

void CreateWorldMenu::OnRender(){

}