#include "Definitions.h"
#include "GameManager.h"
#include "World.h"
#include "TextRenderer.h"
#include "Renderer.h"
#include <DYTDataStorage.h>
void MainLoop();
wxTextCtrl* GameManagerWindow::logtb;
DECLARE_APP(NEWorld)
IMPLEMENT_APP(NEWorld)

void LoadOptions()
{
	DataFile file(L"options");
	DataNode& node = file.RootNode;
	FOVyNormal = node.GetValue(L"FOVy").GetFloat();
	viewdistance = node.GetValue(L"ViewDistance").GetInt32();
	mousemove = node.GetValue(L"MouseMove").GetFloat();
	SmoothLighting = node.GetValue(L"SmoothLighting").GetBoolean();
	NiceGrass = node.GetValue(L"NiceGrass").GetBoolean();
	MergeFace = node.GetValue(L"MergeFace").GetBoolean();
	Multisample = node.GetValue(L"MultiSample").GetInt32();
	Renderer::AdvancedRender = node.GetValue(L"AdvancedRender").GetBoolean();
}

void SaveOptions()
{
	DataFile file;
	DataNode& node = file.RootNode;
	node.GetValue(L"FOVy").SetFloat(FOVyNormal);
	node.GetValue(L"ViewDistance").SetInt32(viewdistance);
	node.GetValue(L"MouseMove").SetFloat(mousemove);
	node.GetValue(L"CloudWidth").SetInt32(cloudwidth);
	node.GetValue(L"SmoothLighting").SetBoolean(SmoothLighting);
	node.GetValue(L"NiceGrass").SetBoolean(NiceGrass);
	node.GetValue(L"MergeFace").SetBoolean(MergeFace);
	node.GetValue(L"MultiSample").SetInt32(Multisample);
	node.GetValue(L"AdvancedRender").SetBoolean(Renderer::AdvancedRender);
	file.Save(L"options");
}

bool NEWorld::OnInit()
{
	wxSetWorkingDirectory(wxPathOnly(wxStandardPaths::Get().GetExecutablePath()));
	LoadOptions();
	(new GameManagerWindow(L"NEWorld游戏管理器"))->Show();
	return true;
}

int NEWorld::OnExit()
{
	SaveOptions();
	return 0;
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
EVT_CHECKBOX(ID_CHECKBOX_SMOOTHLIGHTING, GameManagerWindow::OnModifySmoothLighting)
EVT_CHECKBOX(ID_CHECKBOX_NICEGRASS, GameManagerWindow::OnModifyNiceGrass)
EVT_CHECKBOX(ID_CHECKBOX_MERGEFACE, GameManagerWindow::OnModifyMergeFace)
END_EVENT_TABLE()

GameManagerWindow::GameManagerWindow(const wxString & title)
{
	Create(NULL, wxID_ANY, title, wxDefaultPosition, wxSize(870, 500));
	wxNotebook* tab = new wxNotebook(this, wxID_ANY, wxPoint(0, 0), GetClientSize());
	wxPanel* singleplayer = new wxPanel(tab, wxID_ANY);
	tab->AddPage(singleplayer, L"单人游戏");
	lstWorlds = new wxListBox(singleplayer, ID_LISTBOX_WORLDS, wxPoint(10, 10), wxSize(800, 350));
	Refresh();
	btnEnter = new wxButton(singleplayer, ID_BUTTON_ENTERWORLD, L"进入选定的世界", wxPoint(10, 370), wxSize(250, 30));
	btnNew = new wxButton(singleplayer, ID_BUTTON_CREATEWORLD, L"创建世界", wxPoint(285, 370), wxSize(250, 30));
	btnDelete = new wxButton(singleplayer, ID_BUTTON_DELETEWORLD, L"删除选定的世界", wxPoint(560, 370), wxSize(250, 30));
	wxPanel* multiplayerpage = new wxPanel(tab, wxID_ANY);
	tab->AddPage(multiplayerpage, L"多人游戏");
	tbServerAddress = new wxTextCtrl(multiplayerpage, ID_TEXTBOX_SERVERADDRESS, L"输入服务器IP", wxPoint(10, 10), wxSize(800, 25));
	tbServerAddress->SelectAll();
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
	new wxStaticBox(optionspage, wxID_ANY, L"渲染选项", wxPoint(10, 95), wxSize(800, 100));
	cbSmoothLighting = new wxCheckBox(optionspage, ID_CHECKBOX_SMOOTHLIGHTING, L"平滑光照", wxPoint(25, 125), wxSize(300, -1));
	cbSmoothLighting->SetValue(SmoothLighting);
	cbNiceGrass = new wxCheckBox(optionspage, ID_CHECKBOX_NICEGRASS, L"草方块材质连接", wxPoint(450, 125), wxSize(300, -1));
	cbNiceGrass->SetValue(NiceGrass);
	cbMergeFace = new wxCheckBox(optionspage, ID_CHECKBOX_MERGEFACE, L"合并面渲染", wxPoint(25, 155), wxSize(300, -1));
	cbMergeFace->SetValue(MergeFace);
	if (MergeFace)
	{
		cbSmoothLighting->Disable();
		cbNiceGrass->Disable();
	}
	wxPanel* logpage = new wxPanel(tab, wxID_ANY);
	tab->AddPage(logpage, L"日志");
	logtb = new wxTextCtrl(logpage, wxID_ANY, L"", wxDefaultPosition, logpage->GetClientSize(), wxTE_MULTILINE | wxTE_READONLY);
}

void GameManagerWindow::Refresh()
{
	lstWorlds->Clear();
	if (!wxDir::Exists(L"Worlds"))
		wxDir::Make(L"Worlds");
	wxDir dir(L"Worlds");
	wxString filename;
	bool hasnext = dir.GetFirst(&filename, L"*", wxDIR_DIRS);
	while (hasnext)
	{
		lstWorlds->Insert(filename, lstWorlds->GetCount());
		hasnext = dir.GetNext(&filename);
	}
}

void GameManagerWindow::OnEnter(wxCommandEvent &)
{
	gamebegin = true;
	World::worldname = lstWorlds->GetStringSelection();
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
	DeleteDir(L"Worlds/" + lstWorlds->GetStringSelection());
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
	serverip = tbServerAddress->GetValue();
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

void GameManagerWindow::OnModifySmoothLighting(wxCommandEvent &)
{
	SmoothLighting = cbSmoothLighting->IsChecked();
}

void GameManagerWindow::OnModifyNiceGrass(wxCommandEvent &)
{
	NiceGrass = cbNiceGrass->GetValue();
}

void GameManagerWindow::OnModifyMergeFace(wxCommandEvent &)
{
	MergeFace = cbMergeFace->GetValue();
	if (MergeFace)
	{
		cbSmoothLighting->SetValue(false);
		cbSmoothLighting->Disable();
		cbNiceGrass->SetValue(false);
		cbNiceGrass->Disable();
	}
	else
	{
		cbSmoothLighting->Enable();
		cbNiceGrass->Enable();
	}
}
