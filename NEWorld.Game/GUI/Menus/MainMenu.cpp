#include "Menus.h"
#include "TextRenderer.h"
#include "../GUI.h"
#include "NsRender/GLFactory.h"
#include "NsGui/Grid.h"
#include "NsGui/Button.h"
#include "GameView.h"

namespace Menus {
    class MainMenu : public GUI::Scene {
    public:
        MainMenu() : GUI::Scene("MainMenu.xaml"){}
    private:
        void onLoad() override {
            mRoot->FindName<Noesis::Button>("startGame")->Click() += [](Noesis::BaseComponent* sender, const Noesis::RoutedEventArgs& args) {
                pushGameView();
            };
            mRoot->FindName<Noesis::Button>("exit")->Click() += [this](Noesis::BaseComponent* sender, const Noesis::RoutedEventArgs& args) {
                requestLeave();
            };
        }
    };

    std::unique_ptr<GUI::Scene> startMenu() {
        return std::make_unique<Menus::MainMenu>();
    }
}
