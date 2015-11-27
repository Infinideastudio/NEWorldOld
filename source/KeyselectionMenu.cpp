#include "Menus.h"
#include "TextRenderer.h"
#include <shellapi.h>
#include <fstream>
using namespace std;
//��Ĭ�ϰ�����̫ˬ����һ������ѡ��<^-^>

vector<int>keys;//����
vector<int>Defaultkeys;//����
map<int, string>Keynames;//������
int OnEdit = -1;
bool EditEnd = false;

void Loadkeys(){
	ifstream kslf("Settings\\KeySettings\\Keys.NEWKDefault");
	if (kslf.good()){
		while (!kslf.eof()){
			int t;
			kslf >> t;
			Defaultkeys.push_back(t);
			kslf >> Keynames[Defaultkeys.size() - 1];
		}
	}
	kslf.close();
	cout << "count=" << Defaultkeys.size() << endl;
	ifstream slf("Settings\\KeySettings\\Keys.NEWKSettings");
	if (slf.good()){
		for (int i = 0; i < Defaultkeys.size(); i++){
			int t;
			slf >> t;
			keys.push_back(t);
		}
	}
	else{
		for (int i = 0; i < Defaultkeys.size(); i++){
			keys.push_back(Defaultkeys[i]);
		}
	}
	slf.close();
}

string keyname(int key){
	switch (key)
	{
	case GLFW_KEY_RIGHT:
		break;
	case GLFW_KEY_LEFT:
		break;
	case GLFW_KEY_DOWN:
		break;
	case GLFW_KEY_UP:
		break;
	default:
		string ret(""+(char)key);
		return ret;
		break;
	}
}

void CharInputFunc2(GLFWwindow*, unsigned int c) {
	if (c < 128) {
		keys[OnEdit] = (int)c;
		ofstream slf("Settings\\KeySettings\\Keys.NEWKSettings");
		if (slf.good()){
			for (int i = 0; i < keys.size(); i++){
				slf << keys[i]<<endl;
			}
		}
		slf.close();
	}
	glfwSetCharCallback(MainWindow, &CharInputFunc);
	EditEnd = true;
}


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
	gui::label*  title = MainForm.createlabel("==============<  �� �� ѡ ��  >==============");
	gui::button*  godefault = MainForm.createbutton("�ָ�Ĭ��");
	gui::button*  backbtn = MainForm.createbutton("<< ����ѡ��˵�");
	gui::vscroll* bar = MainForm.createvscroll(2, 1);
	bar->barpos = 0;
	map<int, gui::button*>Keyslbtns;
	for (int i = 0; i < keys.size(); i++){
		Keyslbtns[i] = MainForm.createbutton(Keynames[i] + ":" + (char)keys[i]);
	}
	do{
		//��λ�ؼ�
		leftp = windowwidth / 2 - 250;
		rightp = windowwidth / 2 + 250;
		midp = windowwidth / 2;
		downp = windowheight - 20;
		title->resize(midp - 225, midp + 225, 20, 36);
		backbtn->resize(leftp, rightp, downp - 24, downp);
		godefault->resize(leftp, rightp, downp -60, downp - 36 );
		bar->resize(midp + 275, midp + 295, 36, downp);
		int linsperpage = (downp - upp - 60) / 36;

		if (((1 + Defaultkeys.size()) / 2)<linsperpage) bar->Visiable = false; else {
			bar->Visiable = true;
			bar->barheight = ((1 + Defaultkeys.size()) / 2) - linsperpage;
		}
		if (Keyslbtns.size() > 0){
			int j = -bar->barpos;
			for (int i = 0; i < Keyslbtns.size();){
				if (j >= 0){ 
					Keyslbtns[i]->Visiable = true;
					Keyslbtns[i]->resize(leftp, midp - 10, upp + lspc * j, upp + lspc * j + 24);
					i++;
					if (i < Keyslbtns.size()) {
						Keyslbtns[i]->resize(midp + 10, rightp, upp + lspc * j, upp + lspc * j + 24);
						Keyslbtns[i]->Visiable = true;
					}
					i++;
				}
				else {
					Keyslbtns[i]->Visiable = false;
					i++;
					if (i < Keyslbtns.size()) Keyslbtns[i]->Visiable = false;
					i++;
				}
				j++;
			}
		}
		//����GUI
		glfwGetCursorPos(MainWindow, &mx, &my);
		MainForm.mousedata((int)mx, (int)my, mw, mb);
		MainForm.update();

		//��⣬���±༭״��
		if (EditEnd){
			Keyslbtns[OnEdit]->text = Keynames[OnEdit] + ":" + (char)keys[OnEdit];
			OnEdit = -1;
			EditEnd = false;
		}

		if (Keyslbtns.size() > 0){
			for (int i = 0; i < Keyslbtns.size(); i++){
				if (Keyslbtns[i]->clicked) {
					if (OnEdit != -1) Keyslbtns[OnEdit]->text = Keynames[OnEdit] + ":" + (char)keys[OnEdit];
					OnEdit = i;
					Keyslbtns[OnEdit]->text = Keynames[OnEdit] + ":??";
					glfwSetCharCallback(MainWindow, &CharInputFunc2);
				}
			}
		}

		//��ⰴť״��

		if (backbtn->clicked) f = true;
		if (godefault->clicked) {
			for (int i = 0; i < Defaultkeys.size(); i++){
				keys[i]=Defaultkeys[i];
				Keyslbtns[i]->text = Keynames[i] + ":" + (char)keys[i];
			}
			DeleteFile("Settings\\KeySettings\\Keys.NEWKSettings");
		}

		MainForm.render();
		glfwSwapBuffers(MainWindow);
		glfwPollEvents();
		if (glfwWindowShouldClose(MainWindow)) exit(0);
	} while (!f);
	MainForm.cleanup();
}