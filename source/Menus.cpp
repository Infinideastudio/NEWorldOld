#include "Menus.h"
extern bool gamebegin;


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
	gui::UIView MainForm;
	int leftp = windowwidth / 2 - 200;
	int midp = windowwidth / 2;
	int rightp = windowwidth / 2 + 200;
	int downp = windowheight - 20;
	glfwSetInputMode(MainWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	glClearColor(0.0, 0.0, 0.0, 1.0);
	glDisable(GL_CULL_FACE);
	bool refresh = true;
	int selected = 0, mouseon;
	ubyte mblast = 1;
	int  mwlast = 0;
	string chosenWorldName;
	vector<string> worldnames;
	vector<TextureID> thumbnails, texSizeX, texSizeY;
	int trs = 0;
	MainForm.Init();
	TextRenderer::setFontColor(1.0, 1.0, 1.0, 1.0);
	gui::UILabel title = gui::UILabel("==============<  选 择 世 界  >==============");
	gui::UIVerticalScroll UIVerticalScroll = gui::UIVerticalScroll(100, 0);
	gui::UIButton enterbtn = gui::UIButton("进入选定的世界");
	gui::UIButton deletebtn = gui::UIButton("删除选定的世界");
	gui::UIButton backbtn = gui::UIButton("<< 返回主菜单");
	world::worldname = "";
	enterbtn.enabled = false;
	deletebtn.enabled = false;
	UIVerticalScroll.defaultv = true;
	do{
		int worldcount = (int)worldnames.size();
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
		glfwGetCursorPos(MainWindow, &mx, &my);
		MainForm.mousedata((int)mx, (int)my, mw, mb);
		MainForm.update();
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
			break;
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

		if (backbtn.clicked) break;
		MainForm.render();
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
			glVertex2i(Left, Bottom+Height);
			glVertex2i(Left+Width, Bottom+Height);
			glVertex2i(Left+Width, Bottom);
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
		glfwSwapBuffers(MainWindow);
		glfwPollEvents();
		if (glfwGetKey(MainWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS|| glfwWindowShouldClose(MainWindow)) exit(0);
	} while (!gamebegin);
	MainForm.cleanup();
}

void createworldmenu(){
	gui::UIView MainForm;
	int leftp = windowwidth / 2 - 250;
	int rightp = windowwidth / 2 + 250;
	int midp = windowwidth / 2;
	int downp = windowheight - 20;

	bool worldnametbChanged = false;
	bool f = false;
	MainForm.Init();
	TextRenderer::setFontColor(1.0, 1.0, 1.0, 1.0);
	inputstr = "";
	gui::UILabel title = gui::UILabel("==============<  新 建 世 界  >==============");
	gui::UITextBox worldnametb = gui::UITextBox("输入世界名称");
	gui::UIButton okbtn = gui::UIButton("确定");
	gui::UIButton backbtn = gui::UIButton("<< 返回世界菜单");
	okbtn.enabled = false;
	do{
		leftp = windowwidth / 2 - 250;
		rightp = windowwidth / 2 + 250;
		midp = windowwidth / 2;
		downp = windowheight - 20;
		title.UISetRect(midp - 225, midp + 225, 20, 36);
		worldnametb.UISetRect(midp - 250, midp + 250, 48, 72);
		okbtn.UISetRect(midp - 250, midp + 250, 84, 120);
		backbtn.UISetRect(leftp, rightp, downp - 24, downp);
		//更新GUI
		glfwGetCursorPos(MainWindow, &mx, &my);
		MainForm.mousedata((int)mx, (int)my, mw, mb);
		MainForm.update();
		if (worldnametb.pressed && !worldnametbChanged){
			worldnametb.text = "";
			worldnametbChanged = true;
		}
		if (worldnametb.text == "" || !worldnametbChanged||worldnametb.text.find(" ")!=-1) okbtn.enabled = false; else okbtn.enabled = true;
		if (okbtn.clicked){
			f = true;
		}
		if (backbtn.clicked)f = true;
		inputstr = "";
		MainForm.render();
		glfwSwapBuffers(MainWindow);
		glfwPollEvents();
		if (glfwGetKey(MainWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS || glfwWindowShouldClose(MainWindow)) exit(0);
	} while (!f);
	if (worldnametbChanged){
		world::worldname = worldnametb.text;
		gamebegin = true;
		multiplayer = false;
	}
	MainForm.cleanup();
}
