module menus;
import std;
import types;
import ui;
import globals;
import globalization;
import textures;

namespace Menus {
using Globalization::GetStrbyKey;

class MainMenu: public ui::Menu {
private:
    auto build(ui::Context& ctx) -> ui::View override {
        using namespace ui;
        // clang-format off
        auto column = Column({.main_axis_size = MainAxisSize::MAX, .main_axis_alignment = MainAxisAlignment::CENTER},
            Sizer({.max_height = 256},
                ImageBox({.alignment = Alignment::CENTER, .fit = BoxFit::CONTAIN, .texture = TitleTexture})
            ),
            Sizer({.max_height = 40},
                Padding(
                    {.left = 32, .right = 32},
                    Button({.label = Label(GetStrbyKey("NEWorld.main.start")), .on_click = [this] {
                        worldmenu();
                        if (GameBegin) {
                            exit();
                        }
                    }})
                )
            ),
            Spacer({.height = 8}),
            Sizer({.max_height = 40},
                Padding(
                    {.left = 32, .right = 32},
                    Row({.main_axis_size = MainAxisSize::MAX},
                        FlexItem({.flex_grow = 1},
                            Button({.label = Label(GetStrbyKey("NEWorld.main.options")), .on_click = [] { options(); }})
                        ),
                        Spacer({.width = 8}),
                        FlexItem({.flex_grow = 1},
                            Button({.label = Label(GetStrbyKey("NEWorld.main.exit")), .on_click = [] { std::exit(0); }})
                        )
                    )
                )
            ),
            Spacer({.height = 60})
        );
        // clang-format on
        return View(Stack(
            {},
            Row({.main_axis_size = MainAxisSize::MAX, .main_axis_alignment = MainAxisAlignment::CENTER},
                FlexItem(
                    {.flex_grow = 1},
                    Padding(
                        {.left = 32, .top = 32, .right = 32, .bottom = 32},
                        Sizer({.max_width = 512}, std::move(column))
                    )
                )),
            StackItem(
                {.alignment = Alignment::BOTTOM_LEFT},
                Padding({.left = 8, .top = 8, .right = 8, .bottom = 8}, Label(GetStrbyKey("NEWorld.main.help")))
            )
        ));
    }
};

void mainmenu() {
    MainMenu().run(MainWindow);
}

}
