#include "Menus.h"
#include "World.h"
#include "Textures.h"
#include "TextRenderer.h"

extern bool gamebegin;

template<typename T>
string strWithVar(string str, T var){
	std::stringstream ss;
	ss << str << var;
	return ss.str();
}

void mainmenu(){
	//���˵�
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
	gui::button* startbtn = MainForm.createbutton("��ʼ��Ϸ");
	gui::button* optionsbtn = MainForm.createbutton(">> ѡ��...");
	gui::button* quitbtn = MainForm.createbutton("�˳�");
	do{
		leftp = windowwidth / 2 - 200;
		midp = windowwidth / 2;
		rightp = windowwidth / 2 + 200;
		startbtn->resize(leftp, rightp, upp, upp + 32);
		optionsbtn->resize(leftp, midp - 3, upp + 38, upp + 72);
		quitbtn->resize(midp + 3, rightp, upp + 38, upp + 72);
		mb = glfwGetMouseButton(MainWindow, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS ? 1 : 0;
		glfwGetCursorPos(MainWindow, &mx, &my);
		MainForm.mousedata((int)mx, (int)my, mw, mb);
		MainForm.update();
		if (startbtn->clicked) worldmenu();
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
}

void options(){
	//���ò˵�
	gui::Form MainForm;
	int leftp = windowwidth / 2 - 250;
	int rightp = windowwidth / 2 + 250;
	int midp = windowwidth / 2;
	int upp = 60;
	int downp = windowheight - 20;
	int lspc = 36;
	bool f = false;
	MainForm.Init();
	TextRenderer::setFontColor(1.0, 1.0, 1.0, 1.0);
	gui::label*		title = MainForm.createlabel("=================<  ѡ ��  >=================");
	gui::trackbar*  FOVyBar = MainForm.createtrackbar(strWithVar("��Ұ�Ƕȣ�", FOVyNormal), 120, (int)(FOVyNormal - 1));
	gui::trackbar*  mmsBar = MainForm.createtrackbar(strWithVar("��������ȣ�", mousemove), 120, (int)(mousemove * 40 * 2 - 1));
	gui::trackbar*  viewdistBar = MainForm.createtrackbar(strWithVar("��Ⱦ���룺", viewdistance), 120, (viewdistance - 1) * 8 - 1);
	//gui::button*	ciArrayBtn = MainForm.createbutton("ʹ�������������飺" + boolstr(UseCIArray))
	gui::button*	rdstbtn = MainForm.createbutton(">> ��Ⱦѡ��...");
	gui::button*	gistbtn = MainForm.createbutton(">> ͼ�ν���ѡ��...");
	gui::button*	backbtn = MainForm.createbutton("<< �������˵�");
	//gui::button*	savebtn = MainForm.createbutton("��������")
	//ciArrayBtn->enabled = false
	//savebtn->enabled = false
	do{
		leftp = windowwidth / 2 - 250;
		rightp = windowwidth / 2 + 250;
		midp = windowwidth / 2;
		downp = windowheight - 20;
		title->resize(midp - 225, midp + 225, 20, 36);
		FOVyBar->resize(leftp, midp - 10, upp + lspc * 0, upp + lspc * 0 + 24);
		mmsBar->resize(midp + 10, rightp, upp + lspc * 0, upp + lspc * 0 + 24);
		viewdistBar->resize(leftp, midp - 10, upp + lspc * 1, upp + lspc * 1 + 24);
		//ciArrayBtn->resize(midp + 10, rightp, upp + lspc * 1, upp + lspc * 1 + 24)
		rdstbtn->resize(leftp, midp - 10, upp + lspc * 4, upp + lspc * 4 + 24);
		gistbtn->resize(midp + 10, rightp, upp + lspc * 4, upp + lspc * 4 + 24);
		backbtn->resize(leftp, midp - 10, downp - 24, downp);
		//savebtn->resize(midp + 10, rightp, downp - 24, downp)
		mb = glfwGetMouseButton(MainWindow, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS ? 1 : 0;
		//����GUI
		glfwGetCursorPos(MainWindow, &mx, &my);
		MainForm.mousedata((int)mx, (int)my, mw, mb);
		MainForm.update();
		FOVyNormal = static_cast<float>(FOVyBar->barpos + 1);
		mousemove = (mmsBar->barpos / 2 + 1) / 40.0f;
		viewdistance = viewdistBar->barpos / 8 + 2;
		FOVyBar->text = strWithVar("��Ұ�Ƕȣ�", FOVyNormal);
		mmsBar->text = strWithVar("��������ȣ�", mousemove);
		viewdistBar->text = strWithVar("��Ⱦ���룺", viewdistance);
		//ciArrayBtn->text = strWithVar("ʹ��������������, ", boolstr(UseCIArray))
		//if (ciArrayBtn->clicked) UseCIArray = !UseCIArray
		if (rdstbtn->clicked) Renderoptions();
		if (gistbtn->clicked) GUIoptions();
		if (backbtn->clicked) f = true;
		MainForm.render();
		glfwSwapBuffers(MainWindow);
		glfwPollEvents();
		if (glfwWindowShouldClose(MainWindow)) exit(0);
	} while (!f);
	MainForm.cleanup();
}

void Renderoptions(){
    //��Ⱦ���ò˵�
	gui::Form MainForm;
	int leftp = windowwidth / 2 - 250;
	int rightp = windowwidth / 2 + 250;
	int midp = windowwidth / 2;
	int downp = windowheight - 20;
	bool f = false;
	MainForm.Init();
	TextRenderer::setFontColor(1.0, 1.0, 1.0, 1.0);
	gui::label*  title = MainForm.createlabel("==============<  �� Ⱦ ѡ ��  >==============");
	gui::button*  backbtn = MainForm.createbutton("<< ����ѡ��˵�");
	do{
		leftp = windowwidth / 2 - 250;
		rightp = windowwidth / 2 + 250;
		midp = windowwidth / 2;
		downp = windowheight - 20;
		title->resize(midp - 225, midp + 225, 20, 36);
		backbtn->resize(leftp, rightp, downp - 24, downp);
		//����GUI
		glfwGetCursorPos(MainWindow, &mx, &my);
		MainForm.mousedata((int)mx, (int)my, mw, mb);
		MainForm.update();
		if (backbtn->clicked) f = true;
		MainForm.render();
		glfwSwapBuffers(MainWindow);
		glfwPollEvents();
		if (glfwWindowShouldClose(MainWindow)) exit(0);
	} while (!f);
    MainForm.cleanup();
}

void GUIoptions(){
    //GUI���ò˵�
	gui::Form MainForm;
	int leftp = windowwidth / 2 - 250;
	int rightp = windowwidth / 2 + 250;
	int midp = windowwidth / 2;
	int upp = 60;
	int downp = windowheight - 20;
	int lspc = 36;
	bool f = false;
	MainForm.Init();
	TextRenderer::setFontColor(1.0, 1.0, 1.0, 1.0);
	gui::label* title = MainForm.createlabel("===============< ͼ�ν���ѡ�� >==============");
	gui::button* fontbtn = MainForm.createbutton("ȫ��ʹ��Unicode���壺" + boolstr(TextRenderer::useUnicodeASCIIFont));
	gui::button* backbtn = MainForm.createbutton("<< ����ѡ��˵�");
	do{
        leftp=windowwidth/2-250;
        rightp=windowwidth/2+250;
        midp=windowwidth/2;
        downp=windowheight-20;
        title->resize(midp-225,midp+225,20,36);
        fontbtn->resize(leftp,midp-10,upp+lspc*0,upp+lspc*0+24);
        backbtn->resize(leftp,rightp,downp-24,downp);
        //����GUI
		glfwGetCursorPos(MainWindow, &mx, &my);
		MainForm.mousedata((int)mx, (int)my, mw, mb);
        MainForm.update();
        if(fontbtn->clicked) TextRenderer::useUnicodeASCIIFont=!TextRenderer::useUnicodeASCIIFont;
        if(backbtn->clicked) f=true;
        fontbtn->text="ȫ��ʹ��Unicode���壺" + boolstr(TextRenderer::useUnicodeASCIIFont);
		MainForm.render();
		glfwSwapBuffers(MainWindow);
		glfwPollEvents();
		if (glfwWindowShouldClose(MainWindow)) exit(0);
	} while (!f);
    MainForm.cleanup();
}

void worldmenu(){
	//����ѡ��˵�
	gui::Form MainForm;
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
	gui::label* title = MainForm.createlabel("==============<  ѡ �� �� ��  >==============");
	gui::vscroll* vscroll = MainForm.createvscroll(100, 0);
	gui::button* enterbtn = MainForm.createbutton("����ѡ��������");
	gui::button* deletebtn = MainForm.createbutton("ɾ��ѡ��������");
	gui::button* backbtn = MainForm.createbutton("<< �������˵�");
	world::worldname = "";
	enterbtn->enabled = false;
	deletebtn->enabled = false;
	vscroll->defaultv = true;
	do{
		int worldcount = (int)worldnames.size();
		leftp = windowwidth / 2 - 250;
		midp = windowwidth / 2;
		rightp = windowwidth / 2 + 250;
		downp = windowheight - 20;
		title->resize(midp - 225, midp + 225, 20, 36);
		vscroll->resize(midp + 275, midp + 295, 36, downp);
		enterbtn->resize(leftp, midp - 10, downp - 60, downp - 36);
		deletebtn->resize(midp + 10, rightp, downp - 60, downp - 36);
		backbtn->resize(leftp, rightp, downp - 24, downp);
		vscroll->barheight = (downp - 72 - 48) / (64 * worldcount + 64)*(downp - 36 - 40);
		if (vscroll->barheight > downp - 36 - 40) vscroll->barheight = downp - 36 - 40;
		glfwGetCursorPos(MainWindow, &mx, &my);
		MainForm.mousedata((int)mx, (int)my, mw, mb);
		MainForm.update();
		trs = vscroll->barpos / (downp - 36 - 40)*(64 * worldcount + 64);
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
		if (enterbtn->clicked){
			gamebegin = true;
			world::worldname = chosenWorldName;
			break;
		}
		if (deletebtn->clicked){
			//ɾ�������ļ�
			system((string("rd /s/q Worlds\\") + chosenWorldName + "\\").c_str());
			deletebtn->clicked = false;
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
			vscroll->barpos = 0;
			chosenWorldName = "";
			//������������浵
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
								tmb = Textures::LoadRGBImage("Worlds\\" + string(fileinfo.name) + "\\Thumbnail.bmp");
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

		enterbtn->enabled = chosenWorldName != "";
		deletebtn->enabled = chosenWorldName != "";

		if (backbtn->clicked) break;
		MainForm.render();
		glEnable(GL_SCISSOR_TEST);
		glScissor(0, windowheight - (downp - 72), windowwidth, downp - 72 - 48 + 1);
		glTranslatef(0.0f, (float)-trs, 0.0f);
		for (int i = 0; i < worldcount; i++){
			int xmin, xmax, ymin, ymax;
			xmin = midp - 250, xmax = midp + 250;
			ymin = 48 + i * 64, ymax = 48 + i * 64 + 60;
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
				//����������꣬���ָ߿�ȣ���ť��СΪ500x60������Сѧ��ѧ����������ϸ��һ��Ӧ���ܶ�QAQ
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
			glVertex2i(xmin, ymin);
			glVertex2i(xmin, ymax);
			glVertex2i(xmax, ymax);
			glVertex2i(xmax, ymin);
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
		TextRenderer::renderString(static_cast<int>((windowwidth - TextRenderer::getStrWidth(">>�����µ�����")) / 2.0), static_cast<int>((140 + i * 128) / 2.0) - trs, ">>�����µ�����");
		glDisable(GL_SCISSOR_TEST);
		glTranslatef(0.0, (float)trs, 0.0);
		mblast = (ubyte)mb;
		mwlast = mw;
		glfwSwapBuffers(MainWindow);
		glfwPollEvents();
		if (glfwGetKey(MainWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS&&!escp || glfwWindowShouldClose(MainWindow)) exit(0);
		if (glfwGetKey(MainWindow, GLFW_KEY_ESCAPE) == GLFW_RELEASE)escp = false;
	} while (!gamebegin);
	MainForm.cleanup();
}

void createworldmenu(){
	gui::Form MainForm;
	int leftp = windowwidth / 2 - 250;
	int rightp = windowwidth / 2 + 250;
	int midp = windowwidth / 2;
	int downp = windowheight - 20;

	bool worldnametbChanged = false;
	bool f = false;
	MainForm.Init();
	TextRenderer::setFontColor(1.0, 1.0, 1.0, 1.0);
	inputstr = "";
	gui::label* title = MainForm.createlabel("==============<  �� �� �� ��  >==============");
	gui::textbox* worldnametb = MainForm.createtextbox("������������");
	gui::button* okbtn = MainForm.createbutton("ȷ��");
	gui::button* backbtn = MainForm.createbutton("<< ��������˵�");
	okbtn->enabled = false;
	do{
		leftp = windowwidth / 2 - 250;
		rightp = windowwidth / 2 + 250;
		midp = windowwidth / 2;
		downp = windowheight - 20;
		title->resize(midp - 225, midp + 225, 20, 36);
		worldnametb->resize(midp - 250, midp + 250, 48, 72);
		okbtn->resize(midp - 250, midp + 250, 84, 120);
		backbtn->resize(leftp, rightp, downp - 24, downp);
		//����GUI
		glfwGetCursorPos(MainWindow, &mx, &my);
		MainForm.mousedata((int)mx, (int)my, mw, mb);
		MainForm.update();
		if (worldnametb->pressed && !worldnametbChanged){
			worldnametb->text = "";
			worldnametbChanged = true;
		}
		if (worldnametb->text == "" || !worldnametbChanged||worldnametb->text.find(" ")!=-1) okbtn->enabled = false; else okbtn->enabled = true;
		if (okbtn->clicked){
			f = true;
		}
		if (backbtn->clicked)f = true;
		inputstr = "";
		MainForm.render();
		glfwSwapBuffers(MainWindow);
		glfwPollEvents();
		if (glfwGetKey(MainWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS || glfwWindowShouldClose(MainWindow)) exit(0);
	} while (!f);
	if (worldnametbChanged){
		world::worldname = worldnametb->text;
		gamebegin = true;
	}
	MainForm.cleanup();
}
