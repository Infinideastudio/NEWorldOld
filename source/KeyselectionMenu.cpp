#include "Menus.h"
#include "TextRenderer.h"
#include <shellapi.h>
//��Ĭ�ϰ�����̫ˬ����һ������ѡ��<^-^>
void Keyselection(){
	//��Ⱦ���ò˵�
	gui::Form MainForm;
	int upp = 60;
	int lspc = 36;
	int leftp = windowwidth / 2 - 250;
	int rightp = windowwidth / 2 + 250;
	int midp = windowwidth / 2;
	int downp = windowheight - 20;
	bool f = false;
	MainForm.Init();
	TextRenderer::setFontColor(1.0, 1.0, 1.0, 1.0);
	gui::label*  title = MainForm.createlabel("==============<  �� Ⱦ ѡ ��  >==============");
	gui::button*  VSync = MainForm.createbutton("ʹ�ô�ֱͬ��");
	gui::button*  backbtn = MainForm.createbutton("<< ����ѡ��˵�");
	do{
		leftp = windowwidth / 2 - 250;
		rightp = windowwidth / 2 + 250;
		midp = windowwidth / 2;
		downp = windowheight - 20;
		title->resize(midp - 225, midp + 225, 20, 36);
		backbtn->resize(leftp, rightp, downp - 24, downp);
		VSync->resize(leftp, midp - 10, upp + lspc * 0, upp + lspc * 0 + 24);
		//����GUI
		glfwGetCursorPos(MainWindow, &mx, &my);
		MainForm.mousedata((int)mx, (int)my, mw, mb);
		MainForm.update();
		if (!GetVSyncAvaiablity()) {
			VSync->text = "��ֱͬ��������";
			VSync->enabled = false;
		}
		else
		if (IsVSyncEnabled()) VSync->text = "��ֱͬ��������"; else VSync->text = "��ֱͬ��δ����";
		if (VSync->clicked){
			if (IsVSyncEnabled()) SetVSyncState(false); else SetVSyncState(true);
		}
		if (backbtn->clicked) f = true;
		MainForm.render();
		glfwSwapBuffers(MainWindow);
		glfwPollEvents();
		if (glfwWindowShouldClose(MainWindow)) exit(0);
	} while (!f);
	MainForm.cleanu