#pragma once
#include "GUI.h"
#include "Globalization.h"
using Globalization::Yes;
using Globalization::No;
using Globalization::Enabled;
using Globalization::Disabled;
using Globalization::GetStrbyKey;
namespace Menus {
	void mainmenu();
	void options();
	void Renderoptions();
	void GUIoptions();
	void worldmenu();
	void createworldmenu();
	void gamemenu();
	void multiplayermenu();
	void languagemenu();
}