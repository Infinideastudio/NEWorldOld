#include "Menus.h"
#include "World.h"
#include "Textures.h"
#include "TextRenderer.h"

namespace Menus {
	class WorldMenu :public GUI::Form {
	private:
		int leftp = static_cast<int>(windowwidth / 2.0 / stretch - 200);
		int midp = static_cast<int>(windowwidth / 2.0 / stretch);
		int rightp = static_cast<int>(windowwidth / 2.0 / stretch + 200);
		int downp = static_cast<int>(windowheight / stretch - 20);
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
		const int top = 48;
		const int itemHeight = 70;
		const int lineHeight = 16;
		const int borderWidth = 5;
		void onLoad() {
			title = GUI::label(GetStrbyKey("NEWorld.worlds.caption"), -225, 225, 20, 36, 0.5, 0.5, 0.0, 0.0);
			title.centered = true;
			vscroll = GUI::vscroll(100, 0, 275, 295, 36, -20, 0.5, 0.5, 0.0, 1.0);
			vscroll.defaultv = true;
			enterbtn = GUI::button(GetStrbyKey("NEWorld.worlds.enter"), -250, -10, -80, -56, 0.5, 0.5, 1.0, 1.0);
			enterbtn.enabled = false;
			deletebtn = GUI::button(GetStrbyKey("NEWorld.worlds.delete"), 10, 250, -80, -56, 0.5, 0.5, 1.0, 1.0);
			deletebtn.enabled = false;
			backbtn = GUI::button(GetStrbyKey("NEWorld.worlds.back"), -250, 250, -44, -20, 0.5, 0.5, 1.0, 1.0);
			registerControls(5, &title, &vscroll, &enterbtn, &deletebtn, &backbtn);
			World::worldname = "";
		}
		void onUpdate() {
			worldcount = (int)worldnames.size();
			leftp = static_cast<int>(windowwidth / 2.0 / stretch - 250);
			midp = static_cast<int>(windowwidth / 2.0 / stretch);
			rightp = static_cast<int>(windowwidth / 2.0 / stretch + 250);
			downp = static_cast<int>(windowheight / stretch - 20);

			vscroll.barheight = (downp - 72 - top) * (downp - 36 - 40) / (itemHeight * (worldcount + 1));
			if (vscroll.barheight > downp - 36 - 40) {
				vscroll.enabled = false;
				vscroll.barheight = downp - 36 - 40;
			}
			else vscroll.enabled = true;

			trs = vscroll.barpos*(itemHeight * (worldcount + 1)) / (downp - 36 - 40);
			mouseon = -1;
			if (mx >= midp - 250 && mx <= midp + 250 && my >= top && my <= downp - 72) {
				for (int i = 0; i < worldcount; i++) {
					if (my >= top + i * itemHeight - trs && my <= top + (i + 1) * itemHeight - trs) {
						if (mb == 1 && mbl == 0) {
							chosenWorldName = worldnames[i];
							selected = i;
						}
						mouseon = i;
					}
				}
				if (my >= top + worldcount * itemHeight - trs && my <= top + (worldcount + 1) * itemHeight - trs) {
					if (mb == 0 && mbl == 1) {
						createworldmenu();
						refresh = true;
					}
					mouseon = worldcount;
				}
			}
			if (enterbtn.clicked) {
				World::worldname = chosenWorldName;
				gamebegin = true;
			}
			if (deletebtn.clicked) {
				system((string("rd /s/q \"Worlds\\") + chosenWorldName + "\"").c_str());
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
				Textures::ImageRGB tmb;
				intptr_t hFile = 0;
				_finddata_t fileinfo;
				if ((hFile = _findfirst("Worlds\\*", &fileinfo)) != -1) {
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
			glScissor(0, windowheight - static_cast<int>((downp - 72) * stretch), windowwidth, static_cast<int>((downp - 72 - top + 1) * stretch));
			glTranslatef(0.0f, (float)-trs, 0.0f);
			for (int i = 0; i < worldcount; i++) {
				int xmin, xmax, ymin, ymax;
				xmin = midp - 250, xmax = midp + 250;
				ymin = top + i * itemHeight, ymax = top + (i + 1) * itemHeight;
				if (selected == (int)i) {
					glDisable(GL_TEXTURE_2D);
					glBegin(GL_QUADS);
					glColor4f(0.2f, 0.2f, 0.2f, 0.6f);
					UIVertex(midp - 255, top + i * itemHeight);
					UIVertex(midp - 255, top + (i + 1) * itemHeight);
					UIVertex(midp + 255, top + (i + 1) * itemHeight);
					UIVertex(midp + 255, top + i * itemHeight);
					glEnd();
					glEnable(GL_TEXTURE_2D);
				}
				if (thumbnails[i] == -1) {
					glDisable(GL_TEXTURE_2D);
					glBegin(GL_QUADS);
					if (mouseon == i) glColor4f(0.5, 0.5, 0.5, GUI::FgA);
					else glColor4f(GUI::FgR, GUI::FgG, GUI::FgB, GUI::FgA);
					UIVertex(midp - 250, top + i * itemHeight + borderWidth);
					UIVertex(midp - 250, top + (i + 1) * itemHeight - borderWidth);
					UIVertex(midp + 250, top + (i + 1) * itemHeight - borderWidth);
					UIVertex(midp + 250, top + i * itemHeight + borderWidth);
					glEnd();
					glEnable(GL_TEXTURE_2D);
				}
				else {
					bool marginOnSides;
					float w, h;
					//����������꣬���ָ߿�ȣ���ť��СΪ500x60������Сѧ��ѧ����������ϸ��һ��Ӧ���ܶ�QAQ
					if (texSizeX[i] * 60 / 500 < texSizeY[i]) {
						marginOnSides = true;
						w = 1.0f, h = texSizeX[i] * 60 / 500.0f / texSizeY[i];
					}
					else {
						marginOnSides = false;
						w = texSizeY[i] * 500 / 60.0f / texSizeX[i];
						h = 1.0f;
					}
					glBindTexture(GL_TEXTURE_2D, thumbnails[i]);
					if (mouseon == (int)i) glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
					else glColor4f(0.8f, 0.8f, 0.8f, 0.9f);
					glBegin(GL_QUADS);
					glTexCoord2f(0.5f - w / 2, 0.5f + h / 2), UIVertex(midp - 250, top + i * itemHeight + borderWidth);
					glTexCoord2f(0.5f - w / 2, 0.5f - h / 2), UIVertex(midp - 250, top + (i + 1) * itemHeight - borderWidth);
					glTexCoord2f(0.5f + w / 2, 0.5f - h / 2), UIVertex(midp + 250, top + (i + 1) * itemHeight - borderWidth);
					glTexCoord2f(0.5f + w / 2, 0.5f + h / 2), UIVertex(midp + 250, top + i * itemHeight + borderWidth);
					glEnd();
				}
				/*
				glBegin(GL_LINE_LOOP);
				glColor4f(GUI::FgR*0.9f, GUI::FgG*0.9f, GUI::FgB*0.9f, 0.9f);
				UIVertex(xmin, ymin);
				UIVertex(xmin, ymax);
				UIVertex(xmax, ymax);
				UIVertex(xmax, ymin);
				glEnd();
				*/
				TextRenderer::renderString(static_cast<int>(windowwidth / stretch - TextRenderer::getStrWidth(worldnames[i])) / 2,
					top + i * itemHeight + (itemHeight - lineHeight) / 2, worldnames[i]);
			}
			int i = worldcount;
			glDisable(GL_TEXTURE_2D);
			glBegin(GL_QUADS);
			if (mouseon == i) glColor4f(0.5f, 0.5f, 0.5f, GUI::FgA); else glColor4f(GUI::FgR, GUI::FgG, GUI::FgB, GUI::FgA);
			UIVertex(midp - 250, top + i * itemHeight + borderWidth);
			UIVertex(midp - 250, top + (i + 1) * itemHeight - borderWidth);
			UIVertex(midp + 250, top + (i + 1) * itemHeight - borderWidth);
			UIVertex(midp + 250, top + i * itemHeight + borderWidth);
			glEnd();
			glBegin(GL_LINE_LOOP);
			glColor4f(GUI::FgR * 0.9f, GUI::FgG * 0.9f, GUI::FgB * 0.9f, 0.9f);
			UIVertex(midp - 250, top + i * itemHeight + borderWidth);
			UIVertex(midp - 250, top + (i + 1) * itemHeight - borderWidth);
			UIVertex(midp + 250, top + (i + 1) * itemHeight - borderWidth);
			UIVertex(midp + 250, top + i * itemHeight + borderWidth);
			glEnd();
			glEnable(GL_TEXTURE_2D);
			TextRenderer::renderString(static_cast<int>(windowwidth / stretch - TextRenderer::getStrWidth(GetStrbyKey("NEWorld.worlds.new"))) / 2,
				top + i * itemHeight + (itemHeight - lineHeight) / 2, GetStrbyKey("NEWorld.worlds.new"));
			glDisable(GL_SCISSOR_TEST);
		}
	};
	void worldmenu() { WorldMenu Menu; Menu.start(); }
}