#include "Menus.h"
#include "World.h"
#include "Textures.h"
#include "TextRenderer.h"

namespace Menus {
	class WorldMenu :public GUI::Form {
	private:
		int leftp = windowwidth / 2 - 200;
		int midp = windowwidth / 2;
		int rightp = windowwidth / 2 + 200;
		int downp = windowheight - 20;
		bool refresh = true;
		int selected = 0, mouseon;
		int worldcount;
		string chosenWorldName;
		vector<string> worldnames;
		vector<TextureID> thumbnails, texSizeX, texSizeY;
		int trs = 0;
		GUI::label title;
		GUI::vscroll vscroll;
		GUI::button enterbtn, deletebtn, backbtn;
		void onLoad() {
			title = GUI::label("==============<  选 择 世 界  >==============", -225, 225, 20, 36, 0.5, 0.5, 0.0, 0.0);
			vscroll = GUI::vscroll(100, 0, 275, 295, 36, -20, 0.5, 0.5, 0.0, 1.0);
			enterbtn = GUI::button("进入选定的世界", -250, -10, -80, -56, 0.5, 0.5, 1.0, 1.0);
			deletebtn = GUI::button("删除选定的世界", 10, 250, -80, -56, 0.5, 0.5, 1.0, 1.0);
			backbtn = GUI::button("<< 返回主菜单", -250, 250, -44, -20, 0.5, 0.5, 1.0, 1.0);
			registerControls(5, &title, &vscroll, &enterbtn, &deletebtn, &backbtn);
			World::worldname = "";
			enterbtn.enabled = false;
			deletebtn.enabled = false;
			vscroll.defaultv = true;
		}
		void onUpdate() {
			worldcount = (int)worldnames.size();
			leftp = windowwidth / 2 - 250;
			midp = windowwidth / 2;
			rightp = windowwidth / 2 + 250;
			downp = windowheight - 20;

			vscroll.barheight = (downp - 72 - 48)*(downp - 36 - 40) / (64 * worldcount + 64);
			if (vscroll.barheight > downp - 36 - 40) {
				vscroll.enabled = false;
				vscroll.barheight = downp - 36 - 40;
			}
			else vscroll.enabled = true;

			trs = vscroll.barpos*(64 * worldcount + 64) / (downp - 36 - 40);
			mouseon = -1;
			if (mx >= midp - 250 && mx <= midp + 250 && my >= 48 && my <= downp - 72) {
				for (int i = 0; i < worldcount; i++) {
					if (my >= 48 + i * 64 - trs&&my <= 48 + i * 64 + 60 - trs) {
						if (mb == 1 && mbl == 0) {
							chosenWorldName = worldnames[i];
							selected = i;
						}
						mouseon = i;
					}
				}
				if (my >= 48 + worldcount * 64 - trs&&my <= 48 + worldcount * 64 + 60 - trs) {
					if (mb == 0 && mbl == 1) {
						createworldmenu();
						refresh = true;
					}
					mouseon = worldcount;
				}
			}
			if (enterbtn.clicked) {
				gamebegin = true;
				World::worldname = chosenWorldName;
			}
			if (deletebtn.clicked) {
				//删除世界文件
				system((string("rd /s/q Worlds\\") + chosenWorldName).c_str());
				deletebtn.clicked = false;
				World::worldname = "";
				enterbtn.enabled = false;
				deletebtn.enabled = false;
				refresh = true;
			}
			if (refresh) {
				worldnames.clear();
				thumbnails.clear();
				texSizeX.clear();
				texSizeY.clear();
				worldcount = 0;
				selected = -1;
				mouseon = -1;
				vscroll.barpos = 0;
				chosenWorldName = "";
				//查找所有世界存档
				Textures::TEXTURE_RGB tmb;
				long hFile = 0;
				_finddata_t fileinfo;
				if ((hFile = _findfirst(string("Worlds\\*").c_str(), &fileinfo)) != -1) {
					do {
						if ((fileinfo.attrib &  _A_SUBDIR)) {
							if (strcmp(fileinfo.name, ".") != 0 && strcmp(fileinfo.name, "..") != 0) {
								worldnames.push_back(fileinfo.name);
								std::fstream file;
								file.open(("Worlds\\" + string(fileinfo.name) + "\\Thumbnail.bmp").c_str(), std::ios::in);
								thumbnails.push_back(0);
								texSizeX.push_back(0);
								texSizeY.push_back(0);
								if (file.is_open()) {
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
			if (backbtn.clicked) ExitSignal = true;
			if (gamebegin) ExitSignal = true;
		}
		void onRender() {
			glEnable(GL_SCISSOR_TEST);
			glScissor(0, windowheight - (downp - 72), windowwidth, downp - 72 - 48 + 1);
			glTranslatef(0.0f, (float)-trs, 0.0f);
			for (int i = 0; i < worldcount; i++) {
				int xmin, xmax, ymin, ymax;
				xmin = midp - 250, xmax = midp + 250;
				ymin = 48 + i * 64, ymax = 48 + i * 64 + 60;
				if (thumbnails[i] == -1) {
					glDisable(GL_TEXTURE_2D);
					if (mouseon == i) glColor4f(0.5, 0.5, 0.5, GUI::FgA);
					else glColor4f(GUI::FgR, GUI::FgG, GUI::FgB, GUI::FgA);
					glBegin(GL_QUADS);
					glVertex2i(midp - 250, 48 + i * 64);
					glVertex2i(midp + 250, 48 + i * 64);
					glVertex2i(midp + 250, 48 + i * 64 + 60);
					glVertex2i(midp - 250, 48 + i * 64 + 60);
					glEnd();
				}
				else {
					bool marginOnSides;
					float w, h;
					//计算材质坐标，保持高宽比（按钮大小为500x60），有小学数学基础的人仔细想一想应该能懂QAQ
					if (texSizeX[i] * 60 / 500 < texSizeY[i]) {
						marginOnSides = true;
						w = 1.0f, h = texSizeX[i] * 60 / 500.0f / texSizeY[i];
					}
					else {
						marginOnSides = false;
						w = texSizeY[i] * 500 / 60.0f / texSizeX[i];
						h = 1.0f;
					}
					glEnable(GL_TEXTURE_2D);
					glBindTexture(GL_TEXTURE_2D, thumbnails[i]);
					if (mouseon == (int)i) glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
					else glColor4f(0.8f, 0.8f, 0.8f, 0.9f);
					glBegin(GL_QUADS);
					glTexCoord2f(0.5f - w / 2, 0.5f + h / 2), glVertex2i(midp - 250, 48 + i * 64);
					glTexCoord2f(0.5f + w / 2, 0.5f + h / 2), glVertex2i(midp + 250, 48 + i * 64);
					glTexCoord2f(0.5f + w / 2, 0.5f - h / 2), glVertex2i(midp + 250, 48 + i * 64 + 60);
					glTexCoord2f(0.5f - w / 2, 0.5f - h / 2), glVertex2i(midp - 250, 48 + i * 64 + 60);
					glEnd();
				}
				glColor4f(GUI::FgR*0.9f, GUI::FgG*0.9f, GUI::FgB*0.9f, 0.9f);
				glDisable(GL_TEXTURE_2D);
				glLineWidth(1.0);
				glBegin(GL_LINE_LOOP);
				glVertex2i(xmin, ymin);
				glVertex2i(xmin, ymax);
				glVertex2i(xmax, ymax);
				glVertex2i(xmax, ymin);
				glEnd();
				if (selected == (int)i) {
					glLineWidth(2.0);
					glColor4f(0.0, 0.0, 0.0, 1.0);
					glBegin(GL_LINE_LOOP);
					glVertex2i(midp - 250, 48 + i * 64);
					glVertex2i(midp + 250, 48 + i * 64);
					glVertex2i(midp + 250, 48 + i * 64 + 60);
					glVertex2i(midp - 250, 48 + i * 64 + 60);
					glEnd();
				}
				TextRenderer::renderString((windowwidth - TextRenderer::getStrWidth(worldnames[i])) / 2, (140 + i * 128) / 2, worldnames[i]);
			}
			int i = worldcount;
			glDisable(GL_TEXTURE_2D);
			if (mouseon == i) glColor4f(0.5f, 0.5f, 0.5f, GUI::FgA); else glColor4f(GUI::FgR, GUI::FgG, GUI::FgB, GUI::FgA);
			glBegin(GL_QUADS);
			glVertex2i(midp - 250, 48 + i * 64);
			glVertex2i(midp + 250, 48 + i * 64);
			glVertex2i(midp + 250, 48 + i * 64 + 60);
			glVertex2i(midp - 250, 48 + i * 64 + 60);
			glEnd();
			glColor4f(GUI::FgR*0.9f, GUI::FgG*0.9f, GUI::FgB*0.9f, 0.9f);
			glDisable(GL_TEXTURE_2D);
			glLineWidth(1.0);
			glBegin(GL_LINE_LOOP);
			glVertex2i(midp - 250, 48 + i * 64);
			glVertex2i(midp + 250, 48 + i * 64);
			glVertex2i(midp + 250, 48 + i * 64 + 60);
			glVertex2i(midp - 250, 48 + i * 64 + 60);
			glEnd();
			TextRenderer::renderString((windowwidth - TextRenderer::getStrWidth(">>创建新的世界")) / 2, (140 + i * 128) / 2, ">>创建新的世界");
			glDisable(GL_SCISSOR_TEST);
		}
	};
	void worldmenu() { WorldMenu Menu; Menu.start(); }
}