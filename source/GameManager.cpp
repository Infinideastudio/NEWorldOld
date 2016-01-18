#include "Definitions.h"
#include "GameManager.h"
#include "World.h"
#include "TextRenderer.h"
void MainLoop();
wxTextCtrl* GameManagerWindow::logtb;
DECLARE_APP(NEWorld)
IMPLEMENT_APP(NEWorld)
bool NEWorld::OnInit()
{
	wxSetWorkingDirectory(wxPathOnly(wxStandardPaths::Get().GetExecutablePath()));
	(new GameManagerWindow(L"NEWorld游戏管理器"))->Show();
	return true;
}

BEGIN_EVENT_TABLE(GameManagerWindow, wxFrame)
EVT_BUTTON(ID_BUTTON_ENTERWORLD,GameManagerWindow::OnEnter)
EVT_BUTTON(ID_BUTTON_CREATEWORLD,GameManagerWindow::OnCreate)
EVT_BUTTON(ID_BUTTON_DELETEWORLD,GameManagerWindow::OnDelete)
EVT_BUTTON(ID_BUTTON_ENTERSERVER,GameManagerWindow::OnConnectServer)
EVT_BUTTON(ID_BUTTON_RUNSERVER,GameManagerWindow::OnExecuteServer)
EVT_SLIDER(ID_SLIDER_FOVY,GameManagerWindow::OnModifyFOVy)
EVT_SLIDER(ID_SLIDER_MOUSEMOVE,GameManagerWindow::OnModifyMouseMove)
EVT_SLIDER(ID_SLIDER_VIEWDISTANCE,GameManagerWindow::OnModifyViewDistance)
END_EVENT_TABLE()

GameManagerWindow::GameManagerWindow(const wxString& title)
{
	Create(NULL, wxID_ANY, title, wxDefaultPosition, wxSize(870, 500));
	wxNotebook* tab = new wxNotebook(this, wxID_ANY, wxPoint(0, 0), GetClientSize());
	wxPanel* singleplayer = new wxPanel(tab, wxID_ANY);
	tab->AddPage(singleplayer, L"单人游戏");
	worlds = new wxListBox(singleplayer, ID_LISTBOX_WORLDS, wxPoint(10, 10), wxSize(800, 350));
	Refresh();
	enterworld = new wxButton(singleplayer, ID_BUTTON_ENTERWORLD, L"进入选定的世界", wxPoint(10, 370), wxSize(250, 30));
	newworld = new wxButton(singleplayer, ID_BUTTON_CREATEWORLD, L"创建世界", wxPoint(285, 370), wxSize(250, 30));
	deleteworld = new wxButton(singleplayer, ID_BUTTON_DELETEWORLD, L"删除选定的世界", wxPoint(560, 370), wxSize(250, 30));
	wxPanel* multiplayerpage = new wxPanel(tab, wxID_ANY);
	tab->AddPage(multiplayerpage, L"多人游戏");
	serveriptb = new wxTextCtrl(multiplayerpage, ID_TEXTBOX_SERVERADDRESS, L"输入服务器IP", wxPoint(10, 10), wxSize(800, 25));
	serveriptb->SelectAll();
	new wxButton(multiplayerpage, ID_BUTTON_ENTERSERVER, L"进入", wxPoint(10, 45), wxSize(800, 25));
	new wxButton(multiplayerpage, ID_BUTTON_RUNSERVER, L"运行服务器", wxPoint(10, 360), wxSize(800, 25));
	wxPanel* optionspage = new wxPanel(tab, wxID_ANY);
	tab->AddPage(optionspage, L"选项");
	new wxStaticText(optionspage, wxID_ANY, L"视野角度：", wxPoint(10, 15));
	scFOVy = new wxSlider(optionspage, ID_SLIDER_FOVY, (int)FOVyNormal, 1, 120, wxPoint(70, 13), wxSize(300, -1), wxSL_HORIZONTAL | wxSL_LABELS);
	new wxStaticText(optionspage, wxID_ANY, L"鼠标灵敏度：", wxPoint(380, 15));
	scmousemove = new wxSlider(optionspage, ID_SLIDER_MOUSEMOVE, (int)(mousemove * 1000), 25, 1000, wxPoint(450, 13), wxSize(300, -1), wxSL_HORIZONTAL | wxSL_LABELS);
	new wxStaticText(optionspage, wxID_ANY, L"渲染距离：", wxPoint(10, 55));
	scviewdistance = new wxSlider(optionspage, ID_SLIDER_VIEWDISTANCE, viewdistance, 2, 16, wxPoint(70, 53), wxSize(300, -1), wxSL_HORIZONTAL | wxSL_LABELS);
	wxPanel* logpage = new wxPanel(tab, wxID_ANY);
	tab->AddPage(logpage, L"日志");
	logtb = new wxTextCtrl(logpage, wxID_ANY, L"", wxDefaultPosition, logpage->GetClientSize(), wxTE_MULTILINE);
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
	World::worldname = worlds->GetStringSelection();
	Hide();
	MainLoop();
	Show();
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
	if (dialog.ShowModal() == wxID_OK)
	{
		wxString worldname = dialog.GetValue();
		World::worldname = worldname;
		gamebegin = true;
		multiplayer = false;
		Hide();
		MainLoop();
		Show();
		Refresh();
	}
}

void GameManagerWindow::OnConnectServer(wxCommandEvent &)
{
	serverip = serveriptb->GetValue();
	gamebegin = true;
	multiplayer = true;
	World::worldname = "Multiplayer";
	Hide();
	MainLoop();
	Show();
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
	mousemove = (float)scmousemove->GetValue() / 1000;
}

void GameManagerWindow::OnModifyViewDistance(wxCommandEvent &)
{
	viewdistance = scviewdistance->GetValue();
}
