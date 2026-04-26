module menus;
import std;
import types;
import ui;
import globals;
import globalization;

namespace Menus {
using Globalization::GetStrbyKey;

class GameMenu: public ui::Menu {
private:
    auto build(ui::Context& ctx) -> ui::View override {
        using namespace ui;
        // clang-format off
        auto column = Column({.main_axis_size = MainAxisSize::MAX, .main_axis_alignment = MainAxisAlignment::CENTER},
            Sizer({.max_height = 32},
                Center({}, Label(GetStrbyKey("NEWorld.pause.caption")))
            ),
            Spacer({.height = 8}),
            Sizer({.max_height = 40},
                Row({.main_axis_size = MainAxisSize::MAX},
                    FlexItem({.flex_grow = 1},
                        Button({.label = Label(GetStrbyKey("NEWorld.pause.back")), .on_click = [this] { GameExit = true; exit(); }})
                    ),
                    Spacer({.width = 8}),
                    FlexItem({.flex_grow = 1},
                        Button({.label = Label(GetStrbyKey("NEWorld.pause.continue")), .on_click = [this] { exit(); }})
                    )
                )
            )
        );
        // clang-format on
        return View(
            Row({.main_axis_size = MainAxisSize::MAX, .main_axis_alignment = MainAxisAlignment::CENTER},
                FlexItem(
                    {.flex_grow = 1},
                    Padding(
                        {.left = 32, .top = 32, .right = 32, .bottom = 32},
                        Sizer({.max_width = 512}, std::move(column))
                    )
                ))
        );
    }
};

void gamemenu() {
    GameMenu().run(MainWindow);
}

}
