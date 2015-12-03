#include "Menus.h"

extern bool gamebegin;

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

void worldmenu(){
	//世界选择菜单
	WorldMenu Menu = WorldMenu();
	gui::UIEnter((gui::UIView*)&Menu);
}