#include "GUI.h"
#include "TextRenderer.h"

extern string inputstr;

//图形界面系统。。。正宗OOP！！！
namespace gui{
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
	
	void label::update(){

		//更新标签状态
		
		if (parent->mx >= xmin && parent->mx <= xmax && parent->my >= ymin && parent->my <= ymax)               //鼠标悬停
			mouseon = true;
		else
			mouseon = false;

		if (parent->mb == 1 && parent->mbl == 0 && mouseon) parent->focusid = id;              //焦点在此
		focused = parent->focusid == id;   //焦点
	}

	void label::render(){
		//渲染标签
		float fcR, fcG, fcB, fcA;
		fcR = FgR; fcG = FgG; fcB = FgB; fcA = FgA;
		if (mouseon){
			fcR = FgR*1.2f; fcG = FgG*1.2f; fcB = FgB*1.2f; fcA = FgA*0.8f;
		}
		if (focused){                                                 //Focus
			glDisable(GL_TEXTURE_2D);
			glColor4f(FgR*0.6f, FgG*0.6f, FgB*0.6f, linealpha);
			glLineWidth(linewidth);
			//glBegin(GL_POINTS)
			//glVertex2i(xmin - 1, ymin)
			//glEnd()
			glBegin(GL_LINE_LOOP);
			glVertex2i(xmin, ymin);
			glVertex2i(xmin, ymax);
			glVertex2i(xmax, ymax);
			glVertex2i(xmax, ymin);
			glEnd();
		}
		glEnable(GL_TEXTURE_2D);
		TextRenderer::setFontColor(fcR, fcG, fcB, fcA);
		TextRenderer::renderString(xmin, ymin, text);
	}

	void label::resize(int xi, int xa, int yi, int ya){
		xmin = xi;
		xmax = xa;
		ymin = yi;
		ymax = ya;
	}

	void button::update(){

		//更新按钮状态
		if (enabled){
			if (parent->mx >= xmin && parent->mx <= xmax && parent->my >= ymin && parent->my <= ymax)                 //鼠标悬停
				mouseon = true;
			else
				mouseon = false;
		}

		if ((parent->mb == 1 && mouseon || parent->enterp) && focused)           //鼠标按住
			pressed = true;
		else
			pressed = false;

		if (parent->mb == 1 && parent->mbl == 0 && mouseon) parent->focusid = id;                      //焦点在此
		if (parent->focusid == id) focused = true; else focused = false;                      //焦点

		clicked = (parent->mb == 0 && parent->mbl == 1 && mouseon || parent->enterpl && parent->enterp == false) && focused;//点击
		//clicked = lp&&!pressed

	}

	void button::render(){

		//渲染按钮
		float fcR, fcG, fcB, fcA;
		fcR = FgR; fcG = FgG; fcB = FgB; fcA = FgA;
		if (mouseon){
			fcR = FgR*1.2f; fcG = FgG*1.2f; fcB = FgB*1.2f; fcA = FgA*0.8f;
		}
		if (pressed){
			fcR = FgR*0.8f; fcG = FgG*0.8f; fcB = FgB*0.8f; fcA = FgA*1.5f;
		}
		if (!enabled){
			fcR = FgR*0.5f; fcG = FgG*0.5f; fcB = FgB*0.5f; fcA = FgA*0.3f;
		}
		glColor4f(fcR, fcG, fcB, fcA);

		glDisable(GL_TEXTURE_2D);    //Button
		glBegin(GL_QUADS);
		glVertex2i(xmin, ymin);
		glVertex2i(xmin, ymax);
		glVertex2i(xmax, ymax);
		glVertex2i(xmax, ymin);
		glEnd();
		glColor4f(FgR*0.9f, FgG*0.9f, FgB*0.9f, linealpha);

		if (!enabled) glColor4f(0.5f, 0.5f, 0.5f, linealpha);
		glLineWidth(linewidth);
		glBegin(GL_LINE_LOOP);
		glVertex2i(xmin, ymin);
		glVertex2i(xmin, ymax);
		glVertex2i(xmax, ymax);
		glVertex2i(xmax, ymin);
		glEnd();

		glBegin(GL_LINE_LOOP);

		if (focused)
			glColor4f(1.0f, 1.0f, 1.0f, linealpha);
		else
			glColor4f(0.8f, 0.8f, 0.8f, linealpha);
		glVertex2i(xmin + 1, ymin + 1);

		if (focused)
			glColor4f(1.0f, 1.0f, 1.0f, linealpha);
		else
			glColor4f(0.4f, 0.4f, 0.4f, linealpha);
		glVertex2i(xmin + 1, ymax - 1);

		if (focused)
			glColor4f(1.0f, 1.0f, 1.0f, linealpha);
		else
			glColor4f(0.4f, 0.4f, 0.4f, linealpha);
		glVertex2i(xmax - 1, ymax - 1);

		if (focused)
			glColor4f(1.0f, 1.0f, 1.0f, linealpha);
		else
			glColor4f(0.8f, 0.8f, 0.8f, linealpha);
		glVertex2i(xmax - 1, ymin + 1);

		glEnd();

		glEnable(GL_TEXTURE_2D);
		TextRenderer::setFontColor(1.0f, 1.0f, 1.0f, 1.0f);
		if (!enabled) TextRenderer::setFontColor(0.6f, 0.6f, 0.6f, 1.0f);
		TextRenderer::renderString((xmin + xmax - TextRenderer::getStrWidth(text)) / 2, (ymin + ymax - 20) / 2, text);
	}

	void button::resize(int xi, int xa, int yi, int ya){
		xmin = xi;
		xmax = xa;
		ymin = yi;
		ymax = ya;
	}
	
	void trackbar::update(){

		//更新TrackBar（到底该怎么翻译呢？）状态
		if (parent->mx >= xmin && parent->mx <= xmax && parent->my >= ymin && parent->my <= ymax && parent->mb == 1)
			parent->focusid = id;
		if (parent->mx >= xmin + barpos && parent->mx <= xmin + barpos + barwidth && parent->my >= ymin && parent->my <= ymax)          //鼠标悬停
			mouseon = true;
		else
			mouseon = false;

		if (parent->mb == 1 && mouseon && focused){                       //鼠标按住
			pressed = true;
		}
		else{
			if (parent->mbl == 0) pressed = false;
		}

		if (parent->mb == 1 && parent->mbl == 0 && mouseon)
			parent->focusid = id;              //焦点在此

		focused = parent->focusid == id;                               //焦点
		if (pressed)                                                    //拖动
			barpos += parent->mx - parent->mxl;

		if (focused){
			if (parent->upkp && !parent->upkpl) barpos -= 1;
			if (parent->downkp && !parent->downkpl) barpos += 1;
			if (parent->leftkp) barpos -= 1;
			if (parent->rightkp) barpos += 1;
		}
		if (barpos <= 0) barpos = 0;                                             //让拖动条不越界
		if (barpos >= xmax - xmin - barwidth) barpos = xmax - xmin - barwidth - 1;

	}

	void trackbar::render(){

		//渲染TrackBar（How can I translate it?）
		float fcR, fcG, fcB, fcA;
		float bcR, bcG, bcB, bcA;
		fcR = FgR; fcG = FgG; fcB = FgB; fcA = FgA;
		bcR = BgR; bcG = BgG; bcB = BgB; bcA = BgA;
		if (mouseon){
			fcR = FgR*1.2f; fcG = FgG*1.2f; fcB = FgB*1.2f; fcA = FgA*0.8f;
		}
		if (pressed){
			fcR = FgR*0.8f; fcG = FgG*0.8f; fcB = FgB*0.8f; fcA = FgA*1.5f;
		}
		if (!enabled){
			fcR = FgR*0.5f; fcG = FgG*0.5f; fcB = FgB*0.5f; fcA = FgA*0.3f;
		}

		glColor4f(bcR, bcG, bcB, bcA);                                              //Track
		glDisable(GL_TEXTURE_2D);
		glBegin(GL_QUADS);
		glVertex2i(xmin, ymin);
		glVertex2i(xmax, ymin);
		glVertex2i(xmax, ymax);
		glVertex2i(xmin, ymax);
		glEnd();
		glDisable(GL_TEXTURE_2D);                                                //Bar
		glColor4f(fcR, fcG, fcB, fcA);
		glBegin(GL_QUADS);
		glVertex2i(xmin + barpos, ymin);
		glVertex2i(xmin + barpos + barwidth, ymin);
		glVertex2i(xmin + barpos + barwidth, ymax);
		glVertex2i(xmin + barpos, ymax);
		glEnd();
		glColor4f(FgR*0.9f, FgG*0.9f, FgB*0.9f, linealpha);

		if (!enabled) glColor4f(0.5f, 0.5f, 0.5f, linealpha);
		//glLineWidth(linewidth)                                                  //Focus
		glBegin(GL_LINE_LOOP);

		if (focused)
			glColor4f(1.0f, 1.0f, 1.0f, linealpha);
		else
			glColor4f(0.8f, 0.8f, 0.8f, linealpha);
		glVertex2i(xmin + 1, ymin + 1);

		if (focused)
			glColor4f(1.0f, 1.0f, 1.0f, linealpha);
		else
			glColor4f(0.4f, 0.4f, 0.4f, linealpha);
		glVertex2i(xmin + 1, ymax - 1);

		if (focused)
			glColor4f(1.0f, 1.0f, 1.0f, linealpha);
		else
			glColor4f(0.4f, 0.4f, 0.4f, linealpha);
		glVertex2i(xmax - 1, ymax - 1);

		if (focused)
			glColor4f(1.0f, 1.0f, 1.0f, linealpha);
		else
			glColor4f(0.8f, 0.8f, 0.8f, linealpha);
		glVertex2i(xmax - 1, ymin + 1);

		glEnd();
		glEnable(GL_TEXTURE_2D);
		TextRenderer::setFontColor(1.0f, 1.0f, 1.0f, 1.0f);
		if (!enabled) TextRenderer::setFontColor(0.6f, 0.6f, 0.6f, 1.0f);
		TextRenderer::renderString((xmin + xmax - TextRenderer::getStrWidth(text)) / 2, ymin, text);

	}
	
	void trackbar::settext(string s){
		text = s;
	}

	void trackbar::resize(int xi, int xa, int yi, int ya){
		xmin = xi;
		xmax = xa;
		ymin = yi;
		ymax = ya;
	}
	   
	void textbox::update(){
		static int delt = 0;
		static int ldel = 0;
		if (delt > INT_MAX - 2) delt = 0;
		if (ldel > INT_MAX - 2) delt = 0;
        //更新文本框状态
		if (enabled){
			
			if (parent->mx >= xmin && parent->mx <= xmax && parent->my >= ymin && parent->my <= ymax)                 //鼠标悬停
				mouseon = true;
			else
				mouseon = false;

			if ((parent->mb == 1 && mouseon || parent->enterp) && focused)           //鼠标按住
				pressed = true;
			else
				pressed = false;

			if (parent->mb == 1 && parent->mbl == 0 && mouseon) parent->focusid = id;       //焦点在此
			if (parent->focusid == id) focused = true; else focused = false;                //焦点
			if (focused && inputstr != ""){
				text += inputstr;
			}
			delt++;
			if (parent->backspacep && (delt-ldel>50) && text.length() >= 1){
				ldel = delt;
				int n = text[text.length() - 1];
				if (n > 0 && n <= 127)
					text = text.substr(0, text.length() - 1);
				else
					text = text.substr(0, text.length() - 2);
			}
		}
        
   }
    
	void textbox::render(){

		//渲染文本框
		float fcR, fcG, fcB, fcA;
		fcR = FgR; fcG = FgG; fcB = FgB; fcA = FgA;
		if (!enabled){
			fcR = FgR*0.5f; fcG = FgG*0.5f; fcB = FgB*0.5f; fcA = FgA*0.3f;
		}
		glColor4f(fcR, fcG, fcB, fcA);

		glDisable(GL_TEXTURE_2D);    //Button
		glBegin(GL_QUADS);
		glVertex2i(xmin, ymin);
		glVertex2i(xmin, ymax);
		glVertex2i(xmax, ymax);
		glVertex2i(xmax, ymin);
		glEnd();
		glColor4f(FgR*0.9f, FgG*0.9f, FgB*0.9f, linealpha);

		if (!enabled) glColor4f(0.5f, 0.5f, 0.5f, linealpha);
		glLineWidth(linewidth);
		glBegin(GL_LINE_LOOP);
		glVertex2i(xmin, ymin);
		glVertex2i(xmin, ymax);
		glVertex2i(xmax, ymax);
		glVertex2i(xmax, ymin);
		glEnd();

		glBegin(GL_LINE_LOOP);

		if (focused)
			glColor4f(1.0f, 1.0f, 1.0f, linealpha);
		else
			glColor4f(0.8f, 0.8f, 0.8f, linealpha);
		glVertex2i(xmin + 1, ymin + 1);

		if (focused)
			glColor4f(1.0f, 1.0f, 1.0f, linealpha);
		else
			glColor4f(0.4f, 0.4f, 0.4f, linealpha);
		glVertex2i(xmin + 1, ymax - 1);

		if (focused)
			glColor4f(1.0f, 1.0f, 1.0f, linealpha);
		else
			glColor4f(0.4f, 0.4f, 0.4f, linealpha);
		glVertex2i(xmax - 1, ymax - 1);

		if (focused)
			glColor4f(1.0f, 1.0f, 1.0f, linealpha);
		else
			glColor4f(0.8f, 0.8f, 0.8f, linealpha);
		glVertex2i(xmax - 1, ymin + 1);

		glEnd();

		glEnable(GL_TEXTURE_2D);
		TextRenderer::setFontColor(1.0f, 1.0f, 1.0f, 1.0f);
		if (!enabled) TextRenderer::setFontColor(0.6f, 0.6f, 0.6f, 1.0f);
		TextRenderer::renderString(xmin, (ymin + ymax - 20) / 2, text);

	}
    
	void textbox::resize(int xi, int xa, int yi, int ya){
		xmin = xi;
		xmax = xa;
		ymin = yi;
		ymax = ya;
	}
    
	void vscroll::update(){
		static double lstime;
		msup = false;
		msdown = false;
		psup = false;
		psdown = false;

		//更新滚动条状态
		//鼠标悬停
		mouseon = (parent->my >= ymin + barpos + 20 && parent->my <= ymin + barpos + barheight + 20 && parent->mx >= xmin && parent->mx <= xmax);
		if (parent->mx >= xmin && parent->mx <= xmax && parent->my >= ymin && parent->my <= ymax){
			if (parent->mb == 1) parent->focusid = id;
			if (parent->my <= ymin + 20) {
				msup = true;
				if (parent->mb == 1 && parent->mbl == 0) barpos -= 10;
				if (parent->mb == 1) psup = true;
			}
			else if (parent->my >= ymax - 20){
				msdown = true;
				if (parent->mb == 1 && parent->mbl == 0)  barpos += 10;
				if (parent->mb == 1)  psdown = true;
			}
			else if (timer() - lstime > 0.1 && parent->mb == 1){
				lstime = timer();
				if (parent->my<ymin + barpos + 20) barpos -= 25;
				if (parent->my>ymin + barpos + barheight + 20)  barpos += 25;
			}
		}
		if (parent->mb == 1 && mouseon && focused) {//鼠标按住
			pressed = true;
		}
		else{
			if (parent->mbl == 0) pressed = false;
		}

		if (parent->mb == 1 && parent->mbl == 0 && mouseon)  parent->focusid = id;     //焦点在此
		focused = (parent->focusid == id);   //焦点
		if (pressed) barpos += parent->my - parent->myl;                               //拖动
		if (focused){
			if (parent->upkp)  barpos -= 1;
			if (parent->downkp)  barpos += 1;
			if (parent->leftkp && !parent->leftkpl)barpos -= 1;
			if (parent->rightkp && !parent->rightkpl) barpos += 1;
		}
		if (defaultv)
			barpos += (parent->mwl - parent->mw) * 15;
		if (barpos < 0) barpos = 0;                                                    //让拖动条不越界
		if (barpos >= ymax - ymin - barheight - 40)
			barpos = ymax - ymin - barheight - 40;
	}
    
	void vscroll::render(){
		//渲染滚动条
		float fcR, fcG, fcB, fcA;
		float bcR, bcG, bcB, bcA;
		fcR = FgR; fcG = FgG; fcB = FgB; fcA = FgA;
		bcR = BgR; bcG = BgG; bcB = BgB; bcA = BgA;
		if (mouseon){
			fcR = FgR*1.2f; fcG = FgG*1.2f; fcB = FgB*1.2f; fcA = FgA*0.8f;
		}
		if (pressed){
			fcR = FgR*0.8f; fcG = FgG*0.8f; fcB = FgB*0.8f; fcA = FgA*1.5f;
		}
		if (!enabled){
			fcR = FgR*0.5f; fcG = FgG*0.5f; fcB = FgB*0.5f; fcA = FgA*0.3f;
		}

		glColor4f(bcR, bcG, bcB, bcA);                                              //Track
		glDisable(GL_TEXTURE_2D);
		glBegin(GL_QUADS);
		glVertex2i(xmin, ymin);
		glVertex2i(xmax, ymin);
		glVertex2i(xmax, ymax);
		glVertex2i(xmin, ymax);
		glEnd();
		glDisable(GL_TEXTURE_2D);                                                //Bar
		glColor4f(fcR, fcG, fcB, fcA);
		glBegin(GL_QUADS);
		glVertex2i(xmin, ymin + barpos + 20);
		glVertex2i(xmin, ymin + barpos + barheight + 20);
		glVertex2i(xmax, ymin + barpos + barheight + 20);
		glVertex2i(xmax, ymin + barpos + 20);
		glEnd();

		if (msup) {
			glColor4f(1.0f, 1.0f, 1.0f, 0.7f);
			if (psup) glColor4f(FgR, FgG, FgB, 0.9f);
			glBegin(GL_QUADS);
			glVertex2i(xmin, ymin);
			glVertex2i(xmin, ymin + 20);
			glVertex2i(xmax, ymin + 20);
			glVertex2i(xmax, ymin);
			glEnd();
		}
		if (msdown){
			glColor4f(1.0f, 1.0f, 1.0f, 0.7f);
			if (psdown) glColor4f(FgR, FgG, FgB, 0.9f);
			glBegin(GL_QUADS);
			glVertex2i(xmin, ymax - 20);
			glVertex2i(xmin, ymax);
			glVertex2i(xmax, ymax);
			glVertex2i(xmax, ymax - 20);
			glEnd();
		}

		glColor4f(FgR*0.9f, FgG*0.9f, FgB*0.9f, linealpha);
		if (!enabled)  glColor4f(0.5f, 0.5f, 0.5f, linealpha);
		glBegin(GL_LINE_LOOP);
		glVertex2i(xmin, ymin);
		glVertex2i(xmin, ymax);
		glVertex2i(xmax, ymax);
		glVertex2i(xmax, ymin);
		glEnd();

		glBegin(GL_LINE_LOOP);

		if (focused)
			glColor4f(1.0f, 1.0f, 1.0f, linealpha);
		else
			glColor4f(0.8f, 0.8f, 0.8f, linealpha);
		glVertex2i(xmin + 1, ymin + 1);

		if (focused)
			glColor4f(1.0f, 1.0f, 1.0f, linealpha);
		else
			glColor4f(0.4f, 0.4f, 0.4f, linealpha);
		glVertex2i(xmin + 1, ymax - 1);

		if (focused)
			glColor4f(1.0f, 1.0f, 1.0f, linealpha);
		else
			glColor4f(0.4f, 0.4f, 0.4f, linealpha);
		glVertex2i(xmax - 1, ymax - 1);

		if (focused)
			glColor4f(1.0f, 1.0f, 1.0f, linealpha);
		else
			glColor4f(0.8f, 0.8f, 0.8f, linealpha);
		glVertex2i(xmax - 1, ymin + 1);

		glEnd();

		glLineWidth(3.0);
		glBegin(GL_LINES);
			glColor4f(FgR, FgG, FgB, 1.0);
			if (psup) glColor4f(1.0f - FgR, 1.0f - FgG, 1.0f - FgB, 1.0f);
			glVertex2i((xmin + xmax) / 2, ymin + 8);
			glVertex2i((xmin + xmax) / 2 - 4, ymin + 12);
			glVertex2i((xmin + xmax) / 2, ymin + 8);
			glVertex2i((xmin + xmax) / 2 + 4, ymin + 12);
			glColor4f(FgR, FgG, FgB, 1.0);
			if (psdown) glColor4f(1.0f - FgR, 1.0f - FgG, 1.0f - FgB, 1.0f);
			glVertex2i((xmin + xmax) / 2, ymax - 8);
			glVertex2i((xmin + xmax) / 2 - 4, ymax - 12);
			glVertex2i((xmin + xmax) / 2, ymax - 8);
			glVertex2i((xmin + xmax) / 2 + 4, ymax - 12);
		glEnd();
	}

	void vscroll::resize(int xi, int xa, int yi, int ya){
		xmin = xi;
		xmax = xa;
		ymin = yi;
		ymax = ya;
	}

	void Form::Init(){

		maxid = 0;
		currentid = 0;
		focusid = -1;
		childrenCount = 0;

	}

	void Form::update(){
		int i;
		updated = false;

		if (glfwGetKey(MainWindow, GLFW_KEY_TAB) == GLFW_PRESS){                             //TAB键切换焦点
			if (glfwGetKey(MainWindow, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(MainWindow, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS){   //Shift+Tab
				updated = true;
				if (!tabp) focusid--;
				if (focusid == -2) focusid = maxid - 1;                                //到了最前一个ID
			}
			else{
				updated = true;
				if (!tabp) focusid++;
				if (focusid == maxid + 1) focusid = -1;                              //到了最后一个ID
			}
			tabp = true;
		}
		if (glfwGetKey(MainWindow, GLFW_KEY_TAB) != GLFW_PRESS) tabp = false;
		if (!(glfwGetKey(MainWindow, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(MainWindow, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS)) shiftp = false;

		enterpl = enterp;
		if (glfwGetKey(MainWindow, GLFW_KEY_ENTER) == GLFW_PRESS){
			updated = true;
			enterp = true;
		}
		if (!(glfwGetKey(MainWindow, GLFW_KEY_ENTER) == GLFW_PRESS)) enterp = false;

		upkpl = upkp;                                                              //方向键上
		if (glfwGetKey(MainWindow, GLFW_KEY_UP) == GLFW_PRESS){
			updated = true;
			upkp = true;
		}
		if (!(glfwGetKey(MainWindow, GLFW_KEY_UP) == GLFW_PRESS)) upkp = false;

		downkpl = downkp;                                                          //方向键下
		if (glfwGetKey(MainWindow, GLFW_KEY_DOWN) == GLFW_PRESS){
			downkp = true;
		}
		if (!(glfwGetKey(MainWindow, GLFW_KEY_DOWN) == GLFW_PRESS)) downkp = false;

		leftkpl = leftkp;                                                          //方向键左
		if (glfwGetKey(MainWindow, GLFW_KEY_LEFT) == GLFW_PRESS){
			leftkp = true;
		}
		if (!(glfwGetKey(MainWindow, GLFW_KEY_LEFT) == GLFW_PRESS)) leftkp = false;
		rightkpl = rightkp;                                                        //方向键右
		if (glfwGetKey(MainWindow, GLFW_KEY_RIGHT) == GLFW_PRESS){
			rightkp = true;
		}
		if (glfwGetKey(MainWindow, GLFW_KEY_RIGHT) != GLFW_PRESS) rightkp = false;

		backspacepl = backspacep;
		if (glfwGetKey(MainWindow, GLFW_KEY_BACKSPACE) == GLFW_PRESS)
			backspacep = true;
		else
			backspacep = false;

		if (mb == 1 && mbl == 0) focusid = -1;                                   //空点击时使焦点清空

		for (i = 0; i != childrenCount; i++){
			children[i]->update();                                               //更新子控件
		}

	}

	void Form::mousedata(int x, int y, int w, int b){
		mxl = mx; myl = my; mwl = mw; mbl = mb;                                             //旧鼠标数据
		mx = x; my = y; mw = w; mb = b;                                                     //鼠标数据
	}

	void Form::render(){
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
		glTexCoord2f(0.0f, 1.0f); glVertex2i(0, 0);
		glTexCoord2f(1.0f, 1.0f); glVertex2i(windowwidth, 0);
		glTexCoord2f(1.0f, 0.45f); glVertex2i(windowwidth, windowheight);
		glTexCoord2f(0.0f, 0.45f); glVertex2i(0, windowheight);
		glEnd();
		glDisable(GL_TEXTURE_2D);
		glDisable(GL_LINE_SMOOTH);
		for (int i = 0; i != childrenCount; i++){
			children[i]->render();
		}
	}

	label* Form::createlabel(string t){
		label* ret = new label;                                                          //创建标签
		
		//初始化标签
		ret->id = currentid;                                                       //当前ID
		ret->text = t;                                                             //文本
		ret->parent = this;

		children.push_back(ret);

		currentid++;
		maxid += 1;
		childrenCount += 1;

		return ret;
	}

	button* Form::createbutton(string t){

		button* ret = new button;                                      //创建按钮

		//初始化按钮
		ret->id = currentid;                                                       //当前ID
		ret->text = t;                                                             //文本
		ret->enabled = true;
		ret->parent = this;

		children.push_back(ret);

		currentid++;
		maxid += 1;
		childrenCount += 1;

		return ret;

	}

	trackbar* Form::createtrackbar(string t, int w, int p){

		trackbar* ret = new trackbar;                                  //创建TrackBar（又是到底该怎么翻译呢？）

		//初始化TrackBar
		ret->id = currentid;                                                       //当前ID
		ret->text = t;                                                            //文本
		ret->barwidth = w;                                                    //滑动条宽度
		ret->barpos = p;                                                    //滑动条位置
		ret->enabled = true;
		ret->parent = this;

		children.push_back(ret);

		currentid++;
		maxid += 1;
		childrenCount += 1;

		return ret;

	}
    
	textbox* Form::createtextbox(string t){
<<<<<<< HEAD

		textbox* ret = new textbox;                                   //创建文本框

		//初始化文本框
		ret->id = currentid;                                                  //当前ID
		ret->text = t;                                                  //文本
		ret->enabled = true;
		ret->parent = this;

=======

		textbox* ret = new textbox;                                   //创建文本框

		//初始化文本框
		ret->id = currentid;                                                  //当前ID
		ret->text = t;                                                  //文本
		ret->enabled = true;
		ret->parent = this;

>>>>>>> parent of 7e2d023... GUI Improve Persave 1
		children.push_back(ret);

		currentid++;
		maxid += 1;
		childrenCount += 1;

		return ret;

	}
    
	vscroll* Form::createvscroll(int h, int p){
<<<<<<< HEAD

		vscroll* ret = new vscroll;                                   //创建滚动条

		//初始化
		ret->id = currentid;                                                     //当前ID
		ret->barheight = h;                                                     //滑动条高度
		ret->barpos = p;                                                     //滑动条位置
		ret->enabled = true;
		ret->parent = this;

=======

		vscroll* ret = new vscroll;                                   //创建滚动条

		//初始化
		ret->id = currentid;                                                     //当前ID
		ret->barheight = h;                                                     //滑动条高度
		ret->barpos = p;                                                     //滑动条位置
		ret->enabled = true;
		ret->parent = this;

>>>>>>> parent of 7e2d023... GUI Improve Persave 1
		children.push_back(ret);

		currentid++;
		maxid += 1;
		childrenCount += 1;

		return ret;

	}
    
	void Form::cleanup(){
		for (int i = 0; i != childrenCount; i++){
			children[i]->destroy();
			delete children[i];
		}
		childrenCount = 0;
	}
    
	controls* Form::getControlByID(int cid){
		for (int i = 0; i != childrenCount; i++){
			if (children[i]->id == cid) return children[i];
		}
		return nullptr;
	}

<<<<<<< HEAD
<<<<<<< HEAD
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


=======
>>>>>>> parent of 7e2d023... GUI Improve Persave 1
=======
>>>>>>> parent of 7e2d023... GUI Improve Persave 1
}
