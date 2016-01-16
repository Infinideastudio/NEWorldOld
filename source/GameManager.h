#ifndef GAMEMANAGER_H
#define GAMEMANAGER_H
class NEWorld :public wxApp
{
public:
	virtual bool OnInit();
	wxLocale m_locale;
};
enum ControlID {
	ID_LISTBOX_WORLDS=10000,
	ID_BUTTON_ENTERWORLD,
	ID_BUTTON_CREATEWORLD,
	ID_BUTTON_DELETEWORLD,
	ID_TEXTBOX_SERVERADDRESS,
	ID_BUTTON_ENTERSERVER,
	ID_BUTTON_RUNSERVER,
	ID_SLIDER_FOVY,
	ID_SLIDER_MOUSEMOVE,
	ID_SLIDER_VIEWDISTANCE,
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
private:
	DECLARE_EVENT_TABLE()
	wxListBox* worlds;
	wxButton *enterworld, *deleteworld, *newworld;
	wxTextCtrl* serveriptb;
	wxSlider *scFOVy, *scmousemove, *scviewdistance;
	wxCheckBox* cbAllUnicodeFont;
};
#endif