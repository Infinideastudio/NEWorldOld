#include "GUI.h"
#include "TextRenderer.h"

extern string inputstr;

//图形界面系统。。。正宗OOP！！！
namespace gui {
	float linewidth = 1.0f;
	float linealpha = 0.9f;
	float FgR = 0.2f;
	float FgG = 0.2f;
	float FgB = 0.2f;
	float FgA = 0.6f;
	float BgR = 0.2f;
	float BgG = 0.2f;
	float BgB = 0.2f;
	float BgA = 0.3f;

	void UILabel::update() {

		//更新标签状态

		if (parent->mx >= Left && parent->mx <= Left+Width && parent->my >= Bottom && parent->my <= Bottom+Height)               //鼠标悬停
			mouseon = true;
		else
			mouseon = false;

		if (parent->mb == 1 && parent->mbl == 0 && mouseon) parent->focusid = id;              //焦点在此
		focused = parent->focusid == id;   //焦点
	}

	void UILabel::render() {
		//渲染标签
		float fcR, fcG, fcB, fcA;
		fcR = FgR;
		fcG = FgG;
		fcB = FgB;
		fcA = FgA;
		if (mouseon) {
			fcR = FgR*1.2f;
			fcG = FgG*1.2f;
			fcB = FgB*1.2f;
			fcA = FgA*0.8f;
		}
		if (focused) {                                                //Focus
			glDisable(GL_TEXTURE_2D);
			glColor4f(FgR*0.6f, FgG*0.6f, FgB*0.6f, linealpha);
			glLineWidth(linewidth);
			//glBegin(GL_POINTS)
			//glVertex2i(Left - 1, Bottom)
			//glEnd()
			glBegin(GL_LINE_LOOP);
			glVertex2i(Left, Bottom);
			glVertex2i(Left, Bottom+Height);
			glVertex2i(Left+Width, Bottom+Height);
			glVertex2i(Left+Width, Bottom);
			glEnd();
		}
		glEnable(GL_TEXTURE_2D);
		TextRenderer::setFontColor(fcR, fcG, fcB, fcA);
		TextRenderer::renderString(Left, Bottom, text);
	}

	void UIButton::update() {

		//更新按钮状态
		if (enabled) {
			if (parent->mx >= Left && parent->mx <= Left+Width && parent->my >= Bottom && parent->my <= Bottom+Height)                 //鼠标悬停
				mouseon = true;
			else
				mouseon = false;
		}

		if ((parent->mb == 1 && mouseon || parent->enterp) && focused)           //鼠标按住
			pressed = true;
		else
			pressed = false;

		if (parent->mb == 1 && parent->mbl == 0 && mouseon) parent->focusid = id;                      //焦点在此
		if (parent->focusid == id) focused = true;
		else focused = false;                      //焦点

		clicked = (parent->mb == 0 && parent->mbl == 1 && mouseon || parent->enterpl && parent->enterp == false) && focused;//点击
		//clicked = lp&&!pressed

	}

	void UIButton::render() {

		//渲染按钮
		float fcR, fcG, fcB, fcA;
		fcR = FgR;
		fcG = FgG;
		fcB = FgB;
		fcA = FgA;
		if (mouseon) {
			fcR = FgR*1.2f;
			fcG = FgG*1.2f;
			fcB = FgB*1.2f;
			fcA = FgA*0.8f;
		}
		if (pressed) {
			fcR = FgR*0.8f;
			fcG = FgG*0.8f;
			fcB = FgB*0.8f;
			fcA = FgA*1.5f;
		}
		if (!enabled) {
			fcR = FgR*0.5f;
			fcG = FgG*0.5f;
			fcB = FgB*0.5f;
			fcA = FgA*0.3f;
		}
		glColor4f(fcR, fcG, fcB, fcA);

		glDisable(GL_TEXTURE_2D);    //Button
		glBegin(GL_QUADS);
		glVertex2i(Left, Bottom);
		glVertex2i(Left, Bottom+Height);
		glVertex2i(Left+Width, Bottom+Height);
		glVertex2i(Left+Width, Bottom);
		glEnd();
		glColor4f(FgR*0.9f, FgG*0.9f, FgB*0.9f, linealpha);

		if (!enabled) glColor4f(0.5f, 0.5f, 0.5f, linealpha);
		glLineWidth(linewidth);
		glBegin(GL_LINE_LOOP);
		glVertex2i(Left, Bottom);
		glVertex2i(Left, Bottom+Height);
		glVertex2i(Left+Width, Bottom+Height);
		glVertex2i(Left+Width, Bottom);
		glEnd();

		glBegin(GL_LINE_LOOP);

		if (focused)
			glColor4f(1.0f, 1.0f, 1.0f, linealpha);
		else
			glColor4f(0.8f, 0.8f, 0.8f, linealpha);
		glVertex2i(Left + 1, Bottom + 1);

		if (focused)
			glColor4f(1.0f, 1.0f, 1.0f, linealpha);
		else
			glColor4f(0.4f, 0.4f, 0.4f, linealpha);
		glVertex2i(Left + 1, Bottom+Height - 1);

		if (focused)
			glColor4f(1.0f, 1.0f, 1.0f, linealpha);
		else
			glColor4f(0.4f, 0.4f, 0.4f, linealpha);
		glVertex2i(Left+Width - 1, Bottom+Height - 1);

		if (focused)
			glColor4f(1.0f, 1.0f, 1.0f, linealpha);
		else
			glColor4f(0.8f, 0.8f, 0.8f, linealpha);
		glVertex2i(Left+Width - 1, Bottom + 1);

		glEnd();

		glEnable(GL_TEXTURE_2D);
		TextRenderer::setFontColor(1.0f, 1.0f, 1.0f, 1.0f);
		if (!enabled) TextRenderer::setFontColor(0.6f, 0.6f, 0.6f, 1.0f);
		TextRenderer::renderString((Left + Left+Width - TextRenderer::getStrWidth(text)) / 2, (Bottom + Bottom+Height - 20) / 2, text);
	}

	void UITrackBar::update() {

		//更新TrackBar（到底该怎么翻译呢？）状态
		if (parent->mx >= Left && parent->mx <= Left+Width && parent->my >= Bottom && parent->my <= Bottom+Height && parent->mb == 1)
			parent->focusid = id;
		if (parent->mx >= Left + barpos && parent->mx <= Left + barpos + barwidth && parent->my >= Bottom && parent->my <= Bottom+Height)          //鼠标悬停
			mouseon = true;
		else
			mouseon = false;

		if (parent->mb == 1 && mouseon && focused) {                      //鼠标按住
			pressed = true;
		} else {
			if (parent->mbl == 0) pressed = false;
		}

		if (parent->mb == 1 && parent->mbl == 0 && mouseon)
			parent->focusid = id;              //焦点在此

		focused = parent->focusid == id;                               //焦点
		if (pressed)                                                    //拖动
			barpos += parent->mx - parent->mxl;

		if (focused) {
			if (parent->upkp && !parent->upkpl) barpos -= 1;
			if (parent->downkp && !parent->downkpl) barpos += 1;
			if (parent->leftkp) barpos -= 1;
			if (parent->rightkp) barpos += 1;
		}
		if (barpos <= 0) barpos = 0;                                             //让拖动条不越界
		if (barpos >= Left+Width - Left - barwidth) barpos = Left+Width - Left - barwidth - 1;

	}

	void UITrackBar::render() {

		//渲染TrackBar（How can I translate it?）
		float fcR, fcG, fcB, fcA;
		float bcR, bcG, bcB, bcA;
		fcR = FgR;
		fcG = FgG;
		fcB = FgB;
		fcA = FgA;
		bcR = BgR;
		bcG = BgG;
		bcB = BgB;
		bcA = BgA;
		if (mouseon) {
			fcR = FgR*1.2f;
			fcG = FgG*1.2f;
			fcB = FgB*1.2f;
			fcA = FgA*0.8f;
		}
		if (pressed) {
			fcR = FgR*0.8f;
			fcG = FgG*0.8f;
			fcB = FgB*0.8f;
			fcA = FgA*1.5f;
		}
		if (!enabled) {
			fcR = FgR*0.5f;
			fcG = FgG*0.5f;
			fcB = FgB*0.5f;
			fcA = FgA*0.3f;
		}

		glColor4f(bcR, bcG, bcB, bcA);                                              //Track
		glDisable(GL_TEXTURE_2D);
		glBegin(GL_QUADS);
		glVertex2i(Left, Bottom);
		glVertex2i(Left+Width, Bottom);
		glVertex2i(Left+Width, Bottom+Height);
		glVertex2i(Left, Bottom+Height);
		glEnd();
		glDisable(GL_TEXTURE_2D);                                                //Bar
		glColor4f(fcR, fcG, fcB, fcA);
		glBegin(GL_QUADS);
		glVertex2i(Left + barpos, Bottom);
		glVertex2i(Left + barpos + barwidth, Bottom);
		glVertex2i(Left + barpos + barwidth, Bottom+Height);
		glVertex2i(Left + barpos, Bottom+Height);
		glEnd();
		glColor4f(FgR*0.9f, FgG*0.9f, FgB*0.9f, linealpha);

		if (!enabled) glColor4f(0.5f, 0.5f, 0.5f, linealpha);
		//glLineWidth(linewidth)                                                  //Focus
		glBegin(GL_LINE_LOOP);

		if (focused)
			glColor4f(1.0f, 1.0f, 1.0f, linealpha);
		else
			glColor4f(0.8f, 0.8f, 0.8f, linealpha);
		glVertex2i(Left + 1, Bottom + 1);

		if (focused)
			glColor4f(1.0f, 1.0f, 1.0f, linealpha);
		else
			glColor4f(0.4f, 0.4f, 0.4f, linealpha);
		glVertex2i(Left + 1, Bottom+Height - 1);

		if (focused)
			glColor4f(1.0f, 1.0f, 1.0f, linealpha);
		else
			glColor4f(0.4f, 0.4f, 0.4f, linealpha);
		glVertex2i(Left+Width - 1, Bottom+Height - 1);

		if (focused)
			glColor4f(1.0f, 1.0f, 1.0f, linealpha);
		else
			glColor4f(0.8f, 0.8f, 0.8f, linealpha);
		glVertex2i(Left+Width - 1, Bottom + 1);

		glEnd();
		glEnable(GL_TEXTURE_2D);
		TextRenderer::setFontColor(1.0f, 1.0f, 1.0f, 1.0f);
		if (!enabled) TextRenderer::setFontColor(0.6f, 0.6f, 0.6f, 1.0f);
		TextRenderer::renderString((Left + Left+Width - TextRenderer::getStrWidth(text)) / 2, Bottom, text);

	}

	void UITrackBar::settext(string s) {
		text = s;
	}

	void UITextBox::update() {
		static int delt = 0;
		static int ldel = 0;
		if (delt > INT_MAX - 2) delt = 0;
		if (ldel > INT_MAX - 2) delt = 0;
		//更新文本框状态
		if (enabled) {

			if (parent->mx >= Left && parent->mx <= Left+Width && parent->my >= Bottom && parent->my <= Bottom+Height)                 //鼠标悬停
				mouseon = true;
			else
				mouseon = false;

			if ((parent->mb == 1 && mouseon || parent->enterp) && focused)           //鼠标按住
				pressed = true;
			else
				pressed = false;

			if (parent->mb == 1 && parent->mbl == 0 && mouseon) parent->focusid = id;       //焦点在此
			if (parent->focusid == id) focused = true;
			else focused = false;                //焦点
			if (focused && inputstr != "") {
				text += inputstr;
			}
			delt++;
			if (parent->backspacep && (delt-ldel>50) && text.length() >= 1) {
				ldel = delt;
				int n = text[text.length() - 1];
				if (n > 0 && n <= 127)
					text = text.substr(0, text.length() - 1);
				else
					text = text.substr(0, text.length() - 2);
			}
		}

	}

	void UITextBox::render() {

		//渲染文本框
		float fcR, fcG, fcB, fcA;
		fcR = FgR;
		fcG = FgG;
		fcB = FgB;
		fcA = FgA;
		if (!enabled) {
			fcR = FgR*0.5f;
			fcG = FgG*0.5f;
			fcB = FgB*0.5f;
			fcA = FgA*0.3f;
		}
		glColor4f(fcR, fcG, fcB, fcA);

		glDisable(GL_TEXTURE_2D);    //Button
		glBegin(GL_QUADS);
		glVertex2i(Left, Bottom);
		glVertex2i(Left, Bottom+Height);
		glVertex2i(Left+Width, Bottom+Height);
		glVertex2i(Left+Width, Bottom);
		glEnd();
		glColor4f(FgR*0.9f, FgG*0.9f, FgB*0.9f, linealpha);

		if (!enabled) glColor4f(0.5f, 0.5f, 0.5f, linealpha);
		glLineWidth(linewidth);
		glBegin(GL_LINE_LOOP);
		glVertex2i(Left, Bottom);
		glVertex2i(Left, Bottom+Height);
		glVertex2i(Left+Width, Bottom+Height);
		glVertex2i(Left+Width, Bottom);
		glEnd();

		glBegin(GL_LINE_LOOP);

		if (focused)
			glColor4f(1.0f, 1.0f, 1.0f, linealpha);
		else
			glColor4f(0.8f, 0.8f, 0.8f, linealpha);
		glVertex2i(Left + 1, Bottom + 1);

		if (focused)
			glColor4f(1.0f, 1.0f, 1.0f, linealpha);
		else
			glColor4f(0.4f, 0.4f, 0.4f, linealpha);
		glVertex2i(Left + 1, Bottom+Height - 1);

		if (focused)
			glColor4f(1.0f, 1.0f, 1.0f, linealpha);
		else
			glColor4f(0.4f, 0.4f, 0.4f, linealpha);
		glVertex2i(Left+Width - 1, Bottom+Height - 1);

		if (focused)
			glColor4f(1.0f, 1.0f, 1.0f, linealpha);
		else
			glColor4f(0.8f, 0.8f, 0.8f, linealpha);
		glVertex2i(Left+Width - 1, Bottom + 1);

		glEnd();

		glEnable(GL_TEXTURE_2D);
		TextRenderer::setFontColor(1.0f, 1.0f, 1.0f, 1.0f);
		if (!enabled) TextRenderer::setFontColor(0.6f, 0.6f, 0.6f, 1.0f);
		TextRenderer::renderString(Left, (Bottom + Bottom+Height - 20) / 2, text);

	}

	void UIVerticalScroll::update() {
		static double lstime;
		msup = false;
		msdown = false;
		psup = false;
		psdown = false;

		//更新滚动条状态
		//鼠标悬停
		mouseon = (parent->my >= Bottom + barpos + 20 && parent->my <= Bottom + barpos + barheight + 20 && parent->mx >= Left && parent->mx <= Left+Width);
		if (parent->mx >= Left && parent->mx <= Left+Width && parent->my >= Bottom && parent->my <= Bottom+Height) {
			if (parent->mb == 1) parent->focusid = id;
			if (parent->my <= Bottom + 20) {
				msup = true;
				if (parent->mb == 1 && parent->mbl == 0) barpos -= 10;
				if (parent->mb == 1) psup = true;
			} else if (parent->my >= Bottom+Height - 20) {
				msdown = true;
				if (parent->mb == 1 && parent->mbl == 0)  barpos += 10;
				if (parent->mb == 1)  psdown = true;
			} else if (timer() - lstime > 0.1 && parent->mb == 1) {
				lstime = timer();
				if (parent->my<Bottom + barpos + 20) barpos -= 25;
				if (parent->my>Bottom + barpos + barheight + 20)  barpos += 25;
			}
		}
		if (parent->mb == 1 && mouseon && focused) {//鼠标按住
			pressed = true;
		} else {
			if (parent->mbl == 0) pressed = false;
		}

		if (parent->mb == 1 && parent->mbl == 0 && mouseon)  parent->focusid = id;     //焦点在此
		focused = (parent->focusid == id);   //焦点
		if (pressed) barpos += parent->my - parent->myl;                               //拖动
		if (focused) {
			if (parent->upkp)  barpos -= 1;
			if (parent->downkp)  barpos += 1;
			if (parent->leftkp && !parent->leftkpl)barpos -= 1;
			if (parent->rightkp && !parent->rightkpl) barpos += 1;
		}
		if (defaultv)
			barpos += (parent->mwl - parent->mw) * 15;
		if (barpos < 0) barpos = 0;                                                    //让拖动条不越界
		if (barpos >= Bottom+Height - Bottom - barheight - 40)
			barpos = Bottom+Height - Bottom - barheight - 40;
	}

	void UIVerticalScroll::render() {
		//渲染滚动条
		float fcR, fcG, fcB, fcA;
		float bcR, bcG, bcB, bcA;
		fcR = FgR;
		fcG = FgG;
		fcB = FgB;
		fcA = FgA;
		bcR = BgR;
		bcG = BgG;
		bcB = BgB;
		bcA = BgA;
		if (mouseon) {
			fcR = FgR*1.2f;
			fcG = FgG*1.2f;
			fcB = FgB*1.2f;
			fcA = FgA*0.8f;
		}
		if (pressed) {
			fcR = FgR*0.8f;
			fcG = FgG*0.8f;
			fcB = FgB*0.8f;
			fcA = FgA*1.5f;
		}
		if (!enabled) {
			fcR = FgR*0.5f;
			fcG = FgG*0.5f;
			fcB = FgB*0.5f;
			fcA = FgA*0.3f;
		}

		glColor4f(bcR, bcG, bcB, bcA);                                              //Track
		glDisable(GL_TEXTURE_2D);
		glBegin(GL_QUADS);
		glVertex2i(Left, Bottom);
		glVertex2i(Left+Width, Bottom);
		glVertex2i(Left+Width, Bottom+Height);
		glVertex2i(Left, Bottom+Height);
		glEnd();
		glDisable(GL_TEXTURE_2D);                                                //Bar
		glColor4f(fcR, fcG, fcB, fcA);
		glBegin(GL_QUADS);
		glVertex2i(Left, Bottom + barpos + 20);
		glVertex2i(Left, Bottom + barpos + barheight + 20);
		glVertex2i(Left+Width, Bottom + barpos + barheight + 20);
		glVertex2i(Left+Width, Bottom + barpos + 20);
		glEnd();

		if (msup) {
			glColor4f(1.0f, 1.0f, 1.0f, 0.7f);
			if (psup) glColor4f(FgR, FgG, FgB, 0.9f);
			glBegin(GL_QUADS);
			glVertex2i(Left, Bottom);
			glVertex2i(Left, Bottom + 20);
			glVertex2i(Left+Width, Bottom + 20);
			glVertex2i(Left+Width, Bottom);
			glEnd();
		}
		if (msdown) {
			glColor4f(1.0f, 1.0f, 1.0f, 0.7f);
			if (psdown) glColor4f(FgR, FgG, FgB, 0.9f);
			glBegin(GL_QUADS);
			glVertex2i(Left, Bottom+Height - 20);
			glVertex2i(Left, Bottom+Height);
			glVertex2i(Left+Width, Bottom+Height);
			glVertex2i(Left+Width, Bottom+Height - 20);
			glEnd();
		}

		glColor4f(FgR*0.9f, FgG*0.9f, FgB*0.9f, linealpha);
		if (!enabled)  glColor4f(0.5f, 0.5f, 0.5f, linealpha);
		glBegin(GL_LINE_LOOP);
		glVertex2i(Left, Bottom);
		glVertex2i(Left, Bottom+Height);
		glVertex2i(Left+Width, Bottom+Height);
		glVertex2i(Left+Width, Bottom);
		glEnd();

		glBegin(GL_LINE_LOOP);

		if (focused)
			glColor4f(1.0f, 1.0f, 1.0f, linealpha);
		else
			glColor4f(0.8f, 0.8f, 0.8f, linealpha);
		glVertex2i(Left + 1, Bottom + 1);

		if (focused)
			glColor4f(1.0f, 1.0f, 1.0f, linealpha);
		else
			glColor4f(0.4f, 0.4f, 0.4f, linealpha);
		glVertex2i(Left + 1, Bottom+Height - 1);

		if (focused)
			glColor4f(1.0f, 1.0f, 1.0f, linealpha);
		else
			glColor4f(0.4f, 0.4f, 0.4f, linealpha);
		glVertex2i(Left+Width - 1, Bottom+Height - 1);

		if (focused)
			glColor4f(1.0f, 1.0f, 1.0f, linealpha);
		else
			glColor4f(0.8f, 0.8f, 0.8f, linealpha);
		glVertex2i(Left+Width - 1, Bottom + 1);

		glEnd();

		glLineWidth(3.0);
		glBegin(GL_LINES);
		glColor4f(FgR, FgG, FgB, 1.0);
		if (psup) glColor4f(1.0f - FgR, 1.0f - FgG, 1.0f - FgB, 1.0f);
		glVertex2i((Left + Left+Width) / 2, Bottom + 8);
		glVertex2i((Left + Left+Width) / 2 - 4, Bottom + 12);
		glVertex2i((Left + Left+Width) / 2, Bottom + 8);
		glVertex2i((Left + Left+Width) / 2 + 4, Bottom + 12);
		glColor4f(FgR, FgG, FgB, 1.0);
		if (psdown) glColor4f(1.0f - FgR, 1.0f - FgG, 1.0f - FgB, 1.0f);
		glVertex2i((Left + Left+Width) / 2, Bottom+Height - 8);
		glVertex2i((Left + Left+Width) / 2 - 4, Bottom+Height - 12);
		glVertex2i((Left + Left+Width) / 2, Bottom+Height - 8);
		glVertex2i((Left + Left+Width) / 2 + 4, Bottom+Height - 12);
		glEnd();
	}

	void UIView::Init() {

		maxid = 0;
		currentid = 0;
		focusid = -1;
		childrenCount = 0;

	}

	void UIView::update() {
		int i;
		updated = false;

		if (glfwGetKey(MainWindow, GLFW_KEY_TAB) == GLFW_PRESS) {                            //TAB键切换焦点
			if (glfwGetKey(MainWindow, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(MainWindow, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS) {  //Shift+Tab
				updated = true;
				if (!tabp) focusid--;
				if (focusid == -2) focusid = maxid - 1;                                //到了最前一个ID
			} else {
				updated = true;
				if (!tabp) focusid++;
				if (focusid == maxid + 1) focusid = -1;                              //到了最后一个ID
			}
			tabp = true;
		}
		if (glfwGetKey(MainWindow, GLFW_KEY_TAB) != GLFW_PRESS) tabp = false;
		if (!(glfwGetKey(MainWindow, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(MainWindow, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS)) shiftp = false;

		enterpl = enterp;
		if (glfwGetKey(MainWindow, GLFW_KEY_ENTER) == GLFW_PRESS) {
			updated = true;
			enterp = true;
		}
		if (!(glfwGetKey(MainWindow, GLFW_KEY_ENTER) == GLFW_PRESS)) enterp = false;

		upkpl = upkp;                                                              //方向键上
		if (glfwGetKey(MainWindow, GLFW_KEY_UP) == GLFW_PRESS) {
			updated = true;
			upkp = true;
		}
		if (!(glfwGetKey(MainWindow, GLFW_KEY_UP) == GLFW_PRESS)) upkp = false;

		downkpl = downkp;                                                          //方向键下
		if (glfwGetKey(MainWindow, GLFW_KEY_DOWN) == GLFW_PRESS) {
			downkp = true;
		}
		if (!(glfwGetKey(MainWindow, GLFW_KEY_DOWN) == GLFW_PRESS)) downkp = false;

		leftkpl = leftkp;                                                          //方向键左
		if (glfwGetKey(MainWindow, GLFW_KEY_LEFT) == GLFW_PRESS) {
			leftkp = true;
		}
		if (!(glfwGetKey(MainWindow, GLFW_KEY_LEFT) == GLFW_PRESS)) leftkp = false;
		rightkpl = rightkp;                                                        //方向键右
		if (glfwGetKey(MainWindow, GLFW_KEY_RIGHT) == GLFW_PRESS) {
			rightkp = true;
		}
		if (glfwGetKey(MainWindow, GLFW_KEY_RIGHT) != GLFW_PRESS) rightkp = false;

		backspacepl = backspacep;
		if (glfwGetKey(MainWindow, GLFW_KEY_BACKSPACE) == GLFW_PRESS)
			backspacep = true;
		else
			backspacep = false;

		if (mb == 1 && mbl == 0) focusid = -1;                                   //空点击时使焦点清空

		for (i = 0; i != childrenCount; i++) {
			children[i]->update();                                               //更新子控件
		}

		OnUpdate();

	}

	void UIView::mousedata(int x, int y, int w, int b) {
		mxl = mx;
		myl = my;
		mwl = mw;
		mbl = mb;                                             //旧鼠标数据
		mx = x;
		my = y;
		mw = w;
		mb = b;                                                     //鼠标数据
	}

	void UIView::render() {
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0.0, windowwidth, windowheight, 0.0, -1.0, 1.0);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glDepthFunc(GL_ALWAYS);
		glDisable(GL_CULL_FACE);
		glEnable(GL_TEXTURE_2D);
		glColor4f(1.0, 1.0, 1.0, 1.0);
		glBindTexture(GL_TEXTURE_2D, guiImage[1]);
		glBegin(GL_QUADS);
		glTexCoord2f(0.0f, 1.0f);
		glVertex2i(0, 0);
		glTexCoord2f(1.0f, 1.0f);
		glVertex2i(windowwidth, 0);
		glTexCoord2f(1.0f, 0.45f);
		glVertex2i(windowwidth, windowheight);
		glTexCoord2f(0.0f, 0.45f);
		glVertex2i(0, windowheight);
		glEnd();
		glDisable(GL_TEXTURE_2D);
		glDisable(GL_LINE_SMOOTH);
		for (int i = 0; i != childrenCount; i++) {
			children[i]->render();
		}
		OnRender();
	}

	UILabel::UILabel(string t) {
		text = t;                                                             //文本
	}

	UIButton::UIButton(string t) {
		text = t;                                                             //文本
	}

	UITrackBar::UITrackBar(string t, int w, int p) {
		text = t;                                                            //文本
		barwidth = w;                                                        //滑动条宽度
		barpos = p;                                                          //滑动条位
	}

	UITextBox::UITextBox(string t) {
		text = t;                                                            //文本
	}

	UIVerticalScroll::UIVerticalScroll(int h, int p) {
		barheight = h;                                                      //滑动条高度
		barpos = p;                                                         //滑动条位置
	}
	
	void UIView::UISetRect(int xi, int xa, int yi, int ya){
		bool Resized=false;
		Left = xi;
		if (Width != xa - Left){
			Width = xa - Left;
			Resized=true;
		};
		Bottom = yi;
		if (Height != ya - Bottom){
			Height = ya - Bottom;
			Resized=true;
		};
		if (Resized) OnResize();
	}

	void UIView::cleanup() {
		for (int i = 0; i != childrenCount; i++) {
			children[i]->destroy();
			delete children[i];
		}
		childrenCount = 0;
	}

	void UIView::RegisterUI(UIControl* Control) {
		Control->id = currentid;                                                       //当前ID                                                         //文本
		Control->parent = this;

		children.push_back(Control);

		currentid++;
		maxid += 1;
		childrenCount += 1;
	}

	UIControl* UIView::getControlByID(int cid) {
		for (int i = 0; i != childrenCount; i++) {
			if (children[i]->id == cid) return children[i];
		}
		return nullptr;
	}

	bool UIsigExit=false;
	
	void UIEnter(UIView* View) {
		TextRenderer::setFontColor(1.0, 1.0, 1.0, 1.0);
		do {
			View->UISetRect(0,windowwidth,0,windowheight);
			mb = glfwGetMouseButton(MainWindow, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS ? 1 : 0;
			glfwGetCursorPos(MainWindow, &mx, &my);
			View->mousedata((int)mx, (int)my, mw, mb);
			View->update();
			View->render();
			glfwSwapBuffers(MainWindow);
			glfwPollEvents();
			if (glfwWindowShouldClose(MainWindow)) exit(0);
		} while (!UIsigExit);
		UIsigExit=false;
	}
	
	void UIExit(){
		UIsigExit=true;
	}


}
