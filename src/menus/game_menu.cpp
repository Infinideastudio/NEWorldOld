module menus;
import std;
import types;
import ui;
import gui;
import globals;
import globalization;

namespace Menus {
using Globalization::GetStrbyKey;

class GameMenu: public GUI::Form {
private:
    ui::Context ctx = ui::Context();
    ui::View view = ui::View(ui::Spacer({}));

    void onLoad() override {
        using namespace ui;
        // clang-format off
        auto column = Column({},
            Sizer({.max_height = 32},
                Center({}, Label(GetStrbyKey("NEWorld.pause.caption")))
            ),
            Spacer({.height = 8}),
            Sizer({.max_height = 32},
                Row({.main_axis_size = MainAxisSize::MAX},
                    FlexItem({.flex_grow = 1},
                        Button({.label = Label(GetStrbyKey("NEWorld.pause.continue")), .on_click = [this] { exit = true; }})
                    ),
                    Spacer({.width = 8}),
                    FlexItem({.flex_grow = 1},
                        Button({.label = Label(GetStrbyKey("NEWorld.pause.back")), .on_click = [this] { GameExit = exit = true; }})
                    )
                )
            )
        );
        // clang-format on
        view = View(Center({}, Sizer({.max_width = 512}, std::move(column))));
    }

    void onUpdate() override {
        // Temporary
        ctx.theme = ui::theme_dark();
        ctx.scaling_factor = static_cast<float>(Stretch);
        ctx.view_size = {static_cast<float>(WindowWidth), static_cast<float>(WindowHeight)};
        ctx.mouse_position = {static_cast<float>(mx), static_cast<float>(my)};
        ctx.mouse_motion = {static_cast<float>(mx - mxl), static_cast<float>(my - myl)};
        ctx.mouse_wheel_motion = {static_cast<float>(mw - mwl)};
        ctx.mouse_left_button_down = (mb == 1);
        ctx.mouse_left_button_acted = (mb == 1 && mbl == 0);
        ctx.mouse_left_button_released = (mb == 0 && mbl == 1);
        view.update(ctx);
    }

    void onRender() override {
        view.render(ctx);
    }
};

void gamemenu() {
    GameMenu Menu;
    Menu.start();
}
}
