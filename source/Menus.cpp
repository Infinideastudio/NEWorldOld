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
	gui::UILabel   title = gui::UILabel("==============<  多 人 游 戏  >==============");
	gui::UITextBox serveriptb = gui::UITextBox("输入服务器IP");
	gui::UIButton  runbtn = gui::UIButton("运行服务器（开服）");
	gui::UIButton  okbtn = gui::UIButton("确定");
	gui::UIButton  backbtn = gui::UIButton("<< 返回");

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

	gui::UIButton startbtn = gui::UIButton("单人游戏");
	gui::UIButton multiplayerbtn = gui::UIButton("多人游戏");
	gui::UIButton optionsbtn = gui::UIButton(">> 选项...");
	gui::UIButton quitbtn = gui::UIButton("退出");
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
	gui::UILabel	 title = gui::UILabel("=================<  选 项  >=================");
	gui::UITrackBar  FOVyBar = gui::UITrackBar(strWithVar("视野角度：", FOVyNormal), 120, (int)(FOVyNormal - 1));
	gui::UITrackBar  mmsBar = gui::UITrackBar(strWithVar("鼠标灵敏度：", mousemove), 120, (int)(mousemove * 40 * 2 - 1));
	gui::UITrackBar  viewdistBar = gui::UITrackBar(strWithVar("渲染距离：", viewdistance), 120, (viewdistance - 1) * 8 - 1);
	//gui::UIButton*	ciArrayBtn = UIButton("使用区块索引数组：" + boolstr(UseCIArray))
	gui::UIButton	rdstbtn = gui::UIButton(">> 渲染选项...");
	gui::UIButton	gistbtn = gui::UIButton(">> 图形界面选项...");
	gui::UIButton	backbtn = gui::UIButton("<< 返回主菜单");
	//gui::UIButton*	savebtn = UIButton("保存设置")
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

	gui::UILabel  title = gui::UILabel("==============<  渲 染 选 项  >==============");
	gui::UIButton  backbtn = gui::UIButton("<< 返回选项菜单");
public:
	RenderOptions();
	~RenderOptions();

	void OnResize();
	void OnUpdate();
	void OnRender();
};

class WorldMenu : public gui::UIView
{
private:
	int leftp = windowwidth / 2 - 200;
	int midp = windowwidth / 2;
	int rightp = windowwidth / 2 + 200;
	int downp = windowheight - 20;

	bool refresh = true;
	int selected = 0, mouseon;
	ubyte mblast = 1;
	int  mwlast = 0;
	string chosenWorldName;
	vector<string> worldnames;
	vector<TextureID> thumbnails, texSizeX, texSizeY;
	int trs = 0;
	int worldcount;

	gui::UILabel title = gui::UILabel("==============<  选 择 世 界  >==============");
	gui::UIVerticalScroll UIVerticalScroll = gui::UIVerticalScroll(100, 0);
	gui::UIButton enterbtn = gui::UIButton("进入选定的世界");
	gui::UIButton deletebtn = gui::UIButton("删除选定的世界");
	gui::UIButton backbtn = gui::UIButton("<< 返回主菜单");
public:
	WorldMenu(){
		Init();

		RegisterUI(&title);
		RegisterUI(&UIVerticalScroll);
		RegisterUI(&enterbtn);
		RegisterUI(&deletebtn);
		RegisterUI(&backbtn);
		glfwSetInputMode(MainWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		glClearColor(0.0, 0.0, 0.0, 1.0);
		glDisable(GL_CULL_FACE);

		world::worldname = "";
		enterbtn.enabled = false;
		deletebtn.enabled = false;
		UIVerticalScroll.defaultv = true;
	}
	~WorldMenu(){

	}

	void OnResize(){

	}

	void OnUpdate(){
		worldcount = (int)worldnames.size();
		leftp = windowwidth / 2 - 250;
		midp = windowwidth / 2;
		rightp = windowwidth / 2 + 250;
		downp = windowheight - 20;
		title.UISetRect(midp - 225, midp + 225, 20, 36);
		UIVerticalScroll.UISetRect(midp + 275, midp + 295, 36, downp);
		enterbtn.UISetRect(leftp, midp - 10, downp - 60, downp - 36);
		deletebtn.UISetRect(midp + 10, rightp, downp - 60, downp - 36);
		backbtn.UISetRect(leftp, rightp, downp - 24, downp);
		UIVerticalScroll.barheight = (downp - 72 - 48) / (64 * worldcount + 64)*(downp - 36 - 40);
		if (UIVerticalScroll.barheight > downp - 36 - 40) UIVerticalScroll.barheight = downp - 36 - 40;


		trs = UIVerticalScroll.barpos / (downp - 36 - 40)*(64 * worldcount + 64);
		mouseon = -1;
		if (mx >= midp - 250 && mx <= midp + 250 && my >= 48 && my <= downp - 72){
			for (int i = 0; i < worldcount; i++){
				if (my >= 48 + i * 64 - trs&&my <= 48 + i * 64 + 60 - trs){
					if (mb == 1 && mblast == 0){
						chosenWorldName = worldnames[i];
						selected = i;
					}
					mouseon = i;
				}
			}
			if (my >= 48 + worldcount * 64 - trs&&my <= 48 + worldcount * 64 + 60 - trs){
				if (mb == 0 && mblast == 1){
					createworldmenu();
					refresh = true;
				}
				mouseon = worldcount;
			}
		}
		if (enterbtn.clicked){
			gamebegin = true;
			world::worldname = chosenWorldName;
			gui::UIExit();
		}
		if (deletebtn.clicked){
			//删除世界文件
			system((string("rd /s/q Worlds\\") + chosenWorldName + "\\").c_str());
			deletebtn.clicked = false;
			refresh = true;
		}
		if (refresh){
			worldnames.clear();
			thumbnails.clear();
			texSizeX.clear();
			texSizeY.clear();
			worldcount = 0;
			selected = -1;
			mouseon = -1;
			UIVerticalScroll.barpos = 0;
			chosenWorldName = "";
			//查找所有世界存档
			Textures::TEXTURE_RGB tmb;
			long hFile = 0;
			_finddata_t fileinfo;
			if ((hFile = _findfirst(string("Worlds\\*").c_str(), &fileinfo)) != -1)
			{
				do
				{
					if ((fileinfo.attrib &  _A_SUBDIR))
					{
						if (strcmp(fileinfo.name, ".") != 0 && strcmp(fileinfo.name, "..") != 0){
							worldnames.push_back(fileinfo.name);
							std::fstream file;
							file.open(("Worlds\\" + string(fileinfo.name) + "\\Thumbnail.bmp").c_str(), std::ios::in);
							thumbnails.push_back(0);
							texSizeX.push_back(0);
							texSizeY.push_back(0);
							if (file.is_open()){
								Textures::LoadRGBImage(tmb, "Worlds\\" + string(fileinfo.name) + "\\Thumbnail.bmp");
								glGenTextures(1, &thumbnails[thumbnails.size() - 1]);
								glBindTexture(GL_TEXTURE_2D, thumbnails[thumbnails.size() - 1]);
								glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
								glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
								glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, tmb.sizeX, tmb.sizeY, 0, GL_RGB, GL_UNSIGNED_BYTE, tmb.buffer.get());
								texSizeX[texSizeX.size() - 1] = tmb.sizeX;
								texSizeY[texSizeY.size() - 1] = tmb.sizeY;
							}
							file.close();
						}
					}
				} while (_findnext(hFile, &fileinfo) == 0);
				_findclose(hFile);
			}

			refresh = false;
		}

		enterbtn.enabled = chosenWorldName != "";
		deletebtn.enabled = chosenWorldName != "";

		if (backbtn.clicked || gamebegin) gui::UIExit();

	}

	void OnRender(){
		glEnable(GL_SCISSOR_TEST);
		glScissor(0, windowheight - (downp - 72), windowwidth, downp - 72 - 48 + 1);
		glTranslatef(0.0f, (float)-trs, 0.0f);
		for (int i = 0; i < worldcount; i++){
			int Left, Width, Bottom, Height;
			Left = midp - 250, Width = midp + 250 - Left;
			Bottom = 48 + i * 64, Height = 48 + i * 64 + 60 - Bottom;
			if (thumbnails[i] == -1){
				glDisable(GL_TEXTURE_2D);
				if (mouseon == i)
					glColor4f(0.5, 0.5, 0.5, gui::FgA);
				else
					glColor4f(gui::FgR, gui::FgG, gui::FgB, gui::FgA);
				glBegin(GL_QUADS);
				glVertex2i(midp - 250, 48 + i * 64);
				glVertex2i(midp + 250, 48 + i * 64);
				glVertex2i(midp + 250, 48 + i * 64 + 60);
				glVertex2i(midp - 250, 48 + i * 64 + 60);
				glEnd();
			}
			else{
				bool marginOnSides;
				double w, h;
				//计算材质坐标，保持高宽比（按钮大小为500x60），有小学数学基础的人仔细想一想应该能懂QAQ
				if (texSizeX[i] * 60 / 500 < texSizeY[i]){
					marginOnSides = true;
					w = 1.0, h = texSizeX[i] * 60 / 500.0 / texSizeY[i];
				}
				else{
					marginOnSides = false;
					w = texSizeY[i] * 500 / 60.0 / texSizeX[i];
					h = 1.0;
				}
				glEnable(GL_TEXTURE_2D);
				glBindTexture(GL_TEXTURE_2D, thumbnails[i]);
				if (mouseon == (int)i)
					glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
				else
					glColor4f(0.8f, 0.8f, 0.8f, 0.8f);
				glBegin(GL_QUADS);
				glTexCoord2f(static_cast<GLfloat>(0.5 - w / 2), static_cast<GLfloat>(0.5 + h / 2)), glVertex2i(midp - 250, 48 + i * 64);
				glTexCoord2f(static_cast<GLfloat>(0.5 + w / 2), static_cast<GLfloat>(0.5 + h / 2)), glVertex2i(midp + 250, 48 + i * 64);
				glTexCoord2f(static_cast<GLfloat>(0.5 + w / 2), static_cast<GLfloat>(0.5 - h / 2)), glVertex2i(midp + 250, 48 + i * 64 + 60);
				glTexCoord2f(static_cast<GLfloat>(0.5 - w / 2), static_cast<GLfloat>(0.5 - h / 2)), glVertex2i(midp - 250, 48 + i * 64 + 60);
				glEnd();
			}
			glColor4f(gui::FgR*0.9f, gui::FgG*0.9f, gui::FgB*0.9f, 0.9f);
			glDisable(GL_TEXTURE_2D);
			glLineWidth(1.0);
			glBegin(GL_LINE_LOOP);
			glVertex2i(Left, Bottom);
			glVertex2i(Left, Bottom + Height);
			glVertex2i(Left + Width, Bottom + Height);
			glVertex2i(Left + Width, Bottom);
			glEnd();
			if (selected == (int)i){
				glLineWidth(2.0);
				glColor4f(0.0, 0.0, 0.0, 1.0);
				glBegin(GL_LINE_LOOP);
				glVertex2i(midp - 250, 48 + i * 64);
				glVertex2i(midp + 250, 48 + i * 64);
				glVertex2i(midp + 250, 48 + i * 64 + 60);
				glVertex2i(midp - 250, 48 + i * 64 + 60);
				glEnd();
			}
			TextRenderer::renderString((windowwidth - TextRenderer::getStrWidth(worldnames[i])) / 2, (140 + i * 128) / 2 - trs, worldnames[i]);
		}
		int i = worldcount;
		glDisable(GL_TEXTURE_2D);
		if (mouseon == i) glColor4f(0.5f, 0.5f, 0.5f, gui::FgA); else glColor4f(gui::FgR, gui::FgG, gui::FgB, gui::FgA);
		glBegin(GL_QUADS);
		glVertex2i(midp - 250, 48 + i * 64);
		glVertex2i(midp + 250, 48 + i * 64);
		glVertex2i(midp + 250, 48 + i * 64 + 60);
		glVertex2i(midp - 250, 48 + i * 64 + 60);
		glEnd();
		glColor4f(gui::FgR*0.9f, gui::FgG*0.9f, gui::FgB*0.9f, 0.9f);
		glDisable(GL_TEXTURE_2D);
		glLineWidth(1.0);
		glBegin(GL_LINE_LOOP);
		glVertex2i(midp - 250, 48 + i * 64);
		glVertex2i(midp + 250, 48 + i * 64);
		glVertex2i(midp + 250, 48 + i * 64 + 60);
		glVertex2i(midp - 250, 48 + i * 64 + 60);
		glEnd();
		TextRenderer::renderString(static_cast<int>((windowwidth - TextRenderer::getStrWidth(">>创建新的世界")) / 2.0), static_cast<int>((140 + i * 128) / 2.0) - trs, ">>创建新的世界");
		glDisable(GL_SCISSOR_TEST);
		glTranslatef(0.0, (float)trs, 0.0);
		mblast = (ubyte)mb;
		mwlast = mw;
	}
};

class GUIOptions : public gui::UIView
{
private:
	gui::UILabel title = gui::UILabel("===============< 图形界面选项 >==============");
	gui::UIButton fontbtn = gui::UIButton("全部使用Unicode字体：" + boolstr(TextRenderer::useUnicodeASCIIFont));
	gui::UIButton backbtn = gui::UIButton("<< 返回选项菜单");

public:
	GUIOptions(){
		Init();
		RegisterUI(&title);
		RegisterUI(&fontbtn);
		RegisterUI(&backbtn);
	}

	~GUIOptions(){

	}

	void OnResize(){
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

	void OnUpdate(){
		if (fontbtn.clicked) TextRenderer::useUnicodeASCIIFont = !TextRenderer::useUnicodeASCIIFont;
		if (backbtn.clicked) gui::UIExit();
		fontbtn.text = "全部使用Unicode字体：" + boolstr(TextRenderer::useUnicodeASCIIFont);
	}

	void OnRender(){

	}
};

class CreateWorldMenu : public gui::UIView
{
private:
	bool worldnametbChanged = false;

	gui::UILabel title = gui::UILabel("==============<  新 建 世 界  >==============");
	gui::UITextBox worldnametb = gui::UITextBox("输入世界名称");
	gui::UIButton okbtn = gui::UIButton("确定");
	gui::UIButton backbtn = gui::UIButton("<< 返回世界菜单");
public:
	CreateWorldMenu(){
		Init();
		RegisterUI(&title);
		RegisterUI(&worldnametb);
		RegisterUI(&okbtn);
		RegisterUI(&backbtn);
		inputstr = "";
		okbtn.enabled = false;
	}

	~CreateWorldMenu(){
		if (worldnametbChanged){
			world::worldname = worldnametb.text;
			gamebegin = true;
			multiplayer = false;
		}
	}

	void OnResize(){
		int leftp = windowwidth / 2 - 250;
		int rightp = windowwidth / 2 + 250;
		int midp = windowwidth / 2;
		int downp = windowheight - 20;

		title.UISetRect(midp - 225, midp + 225, 20, 36);
		worldnametb.UISetRect(midp - 250, midp + 250, 48, 72);
		okbtn.UISetRect(midp - 250, midp + 250, 84, 120);
		backbtn.UISetRect(leftp, rightp, downp - 24, downp);
	}

	void OnUpdate(){
		if (worldnametb.pressed && !worldnametbChanged){
			worldnametb.text = "";
			worldnametbChanged = true;
		}
		if (worldnametb.text == "" || !worldnametbChanged || worldnametb.text.find(" ") != -1) okbtn.enabled = false; else okbtn.enabled = true;
		if (okbtn.clicked){
			gui::UIExit();
		}
		if (backbtn.clicked) gui::UIExit();
		inputstr = "";
	}

	void OnRender(){

	}
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
	WorldMenu Menu = WorldMenu();
	gui::UIEnter((gui::UIView*)&Menu);
}

void createworldmenu(){
	CreateWorldMenu Menu = CreateWorldMenu();
	gui::UIEnter((gui::UIView*)&Menu);

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


Options::Options()
{
	Init();
	RegisterUI(&title);
	RegisterUI(&FOVyBar);
	RegisterUI(&mmsBar);
	RegisterUI(&viewdistBar);
	//RegisterUI(&ciArrayBtn = UIButton("使用区块索引数组：" + boolstr(UseCIArray))
	RegisterUI(&rdstbtn);
	RegisterUI(&gistbtn);
	RegisterUI(&backbtn);
	//ciArrayBtn.enabled = false
	//savebtn.enabled = false
	//gui::UIButton*	savebtn = UIButton("保存设置")
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
	FOVyBar.text = strWithVar("视野角度：", FOVyNormal);
	mmsBar.text = strWithVar("鼠标灵敏度：", mousemove);
	viewdistBar.text = strWithVar("渲染距离：", viewdistance);
	//ciArrayBtn.text = strWithVar("使用区块索引数组, ", boolstr(UseCIArray))
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
