#ifndef GAMEMANAGER_H
#define GAMEMANAGER_H
class NEWorld :public wxApp
{
public:
	virtual bool OnInit();
};
class GameManagerWindow :public wxFrame
{
public:
	GameManagerWindow(const wxString& title);
	void Refresh();
	void OnEnter(wxCommandEvent&);
	void OnDelete(wxCommandEvent&);
	void OnCreate(wxCommandEvent&);
	void OnConnectServer(wxCommandEvent&);
	void OnExecuteServer(wxCommandEvent&);
	void OnModifyFOVy(wxCommandEvent&);
	void OnModifyMouseMove(wxCommandEvent&);
	void OnModifyViewDistance(wxCommandEvent&);
	void OnModifyAllUnicodeFont(wxCommandEvent&);
private:
	DECLARE_EVENT_TABLE()
	wxListBox* worlds;
	wxButton *enterworld, *deleteworld, *newworld;
	wxTextCtrl* serveriptb;
	wxSlider *scFOVy, *scmousemove, *scviewdistance;
	wxCheckBox* cbAllUnicodeFont;
};
#endif