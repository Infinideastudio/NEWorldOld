#include "Definitions.h"
#include "GameManager.h"
#include "World.h"
#include "TextRenderer.h"
void MainLoop();
DECLARE_APP(NEWorld)
IMPLEMENT_APP(NEWorld)
bool NEWorld::OnInit()
{
	setlocale(LC_ALL, "zh_CN.UTF-8");
	wxString curdir = wxStandardPaths::Get().GetExecutablePath();
	curdir.Replace(L"\\", L"/");
	wxSetWorkingDirectory(curdir.substr(0, curdir.find_last_of('/')));
	GameManagerWindow* frame = new GameManagerWindow(L"NEWorld游戏管理器");
	frame->Show();
	return true;
}

BEGIN_EVENT_TABLE(GameManagerWindow, wxFrame)
EVT_BUTTON(10003,GameManagerWindow::OnEnter)
EVT_BUTTON(10004,GameManagerWindow::OnCreate)
EVT_BUTTON(10005,GameManagerWindow::OnDelete)
EVT_BUTTON(10008,GameManagerWindow::OnConnectServer)
EVT_BUTTON(10009,GameManagerWindow::OnExecuteServer)
EVT_SLIDER(10012,GameManagerWindow::OnModifyFOVy)
EVT_SLIDER(10014,GameManagerWindow::OnModifyMouseMove)
EVT_SLIDER(10016,GameManagerWindow::OnModifyViewDistance)
EVT_CHECKBOX(10018,GameManagerWindow::OnModifyAllUnicodeFont)
END_EVENT_TABLE()

GameManagerWindow::GameManagerWindow(const wxString & title)
	:wxFrame(NULL, wxID_ANY, title, wxDefaultPosition, wxSize(870, 500))
{
	wxNotebook* tab = new wxNotebook(this, 10000, wxPoint(0, 0), GetClientSize());
	wxPanel* singleplayer = new wxPanel(tab, 10001);
	tab->AddPage(singleplayer, L"单人游戏");
	worlds = new wxListBox(singleplayer, 10002, wxPoint(10, 10), wxSize(800, 350));
	Refresh();
	enterworld = new wxButton(singleplayer, 10003, L"进入选定的世界", wxPoint(10, 370), wxSize(250, 30));
	newworld = new wxButton(singleplayer, 10004, L"创建世界", wxPoint(285, 370), wxSize(250, 30));
	deleteworld = new wxButton(singleplayer, 10005, L"删除选定的世界", wxPoint(560, 370), wxSize(250, 30));
	wxPanel* multiplayerpage = new wxPanel(tab, 10006);
	tab->AddPage(multiplayerpage, L"多人游戏");
	serveriptb = new wxTextCtrl(multiplayerpage, 10007, L"输入服务器IP", wxPoint(10, 10), wxSize(800, 25));
	serveriptb->SelectAll();
	new wxButton(multiplayerpage, 10008, L"确定", wxPoint(10, 45), wxSize(800, 25));
	new wxButton(multiplayerpage, 10009, L"运行服务器", wxPoint(10, 360), wxSize(800, 25));
	wxPanel* optionspage = new wxPanel(tab, 10010);
	tab->AddPage(optionspage, L"选项");
	new wxStaticText(optionspage, 10011, L"视野角度：", wxPoint(10, 15));
	scFOVy = new wxSlider(optionspage, 10012, (int)FOVyNormal, 1, 120, wxPoint(70, 13), wxSize(300, -1), wxSL_HORIZONTAL | wxSL_LABELS);
	new wxStaticText(optionspage, 10013, L"鼠标灵敏度：", wxPoint(380, 15));
	scmousemove = new wxSlider(optionspage, 10014, (int)(mousemove * 1000), 25, 1000, wxPoint(450, 13), wxSize(300, -1), wxSL_HORIZONTAL | wxSL_LABELS);
	new wxStaticText(optionspage, 10015, L"渲染距离：", wxPoint(10, 55));
	scviewdistance = new wxSlider(optionspage, 10016, viewdistance, 2, 16, wxPoint(70, 53), wxSize(300, -1), wxSL_HORIZONTAL | wxSL_LABELS);
	new wxStaticBox(optionspage, 10017, L"图形界面选项", wxPoint(5, 95), wxSize(800, 70));
	cbAllUnicodeFont = new wxCheckBox(optionspage, 10018, L"全部使用Unicode字体", wxPoint(15, 125));
}

void GameManagerWindow::Refresh()
{
	worlds->Clear();
	if (!wxDir::Exists(L"Worlds"))
		wxDir::Make(L"Worlds");
	wxDir dir(L"Worlds");
	wxString filename;
	bool hasnext = dir.GetFirst(&filename, L"*", wxDIR_DIRS);
	while (hasnext)
	{
		worlds->Insert(filename, worlds->GetCount());
		hasnext = dir.GetNext(&filename);
	}
}

void GameManagerWindow::OnEnter(wxCommandEvent &)
{
	gamebegin = true;
	world::worldname = worlds->GetStringSelection();
	MainLoop();
}
void DeleteDir(wxString dir)
{
	wxDir curdir(dir);
	wxString filename;
	bool hasnext = curdir.GetFirst(&filename, L"*", wxDIR_DIRS);
	while (hasnext)
	{
		DeleteDir(dir + L"/" + filename);
		hasnext = curdir.GetNext(&filename);
	}
	hasnext = curdir.GetFirst(&filename, L"*", wxDIR_FILES);
	while (hasnext)
	{
		wxRemoveFile(dir + L"/" + filename);
		hasnext = curdir.GetNext(&filename);
	}
	wxRmdir(dir);
}
void GameManagerWindow::OnDelete(wxCommandEvent &)
{
	DeleteDir(L"Worlds/" + worlds->GetStringSelection());
	Refresh();
}

void GameManagerWindow::OnCreate(wxCommandEvent &)
{
	wxTextEntryDialog dialog(this, L"世界名称：", L"NEWorld", L"NEWorld", wxOK);
	dialog.ShowModal();
	wxString worldname = dialog.GetValue();
	world::worldname = worldname;
	gamebegin = true;
	multiplayer = false;
	MainLoop();
	Refresh();
}

void GameManagerWindow::OnConnectServer(wxCommandEvent &)
{
	serverip = serveriptb->GetValue();
	gamebegin = true;
	multiplayer = true;
	world::worldname = "Multiplayer";
	MainLoop();
	Refresh();
}

void GameManagerWindow::OnExecuteServer(wxCommandEvent &)
{
	wxExecute(L"NEWorldServer");
}

void GameManagerWindow::OnModifyFOVy(wxCommandEvent &)
{
	FOVyNormal = scFOVy->GetValue();
}

void GameManagerWindow::OnModifyMouseMove(wxCommandEvent &)
{
	mousemove = (float)scmousemove->GetValue() / 1000;;
}

void GameManagerWindow::OnModifyViewDistance(wxCommandEvent &)
{
	viewdistance = scviewdistance->GetValue();
}

void GameManagerWindow::OnModifyAllUnicodeFont(wxCommandEvent &)
{
	TextRenderer::useUnicodeASCIIFont = cbAllUnicodeFont->IsChecked();
}
