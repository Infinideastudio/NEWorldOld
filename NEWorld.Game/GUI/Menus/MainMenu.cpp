#include "Menus.h"
#include "TextRenderer.h"
#include "../GUI.h"
#include "NsRender/GLFactory.h"
#include "NsGui/Grid.h"

namespace Menus {
    class MainMenu : public GUI::Scene {
    public:
        MainMenu() : GUI::Scene("MainMenu.xaml"){}
    private:
        void onUpdate() override {
        }

        void onLoad() override {
        }

        void onRender() override {
        }
    };

    std::unique_ptr<GUI::Scene> startMenu() {
        return std::make_unique<Menus::MainMenu>();
    }
}
