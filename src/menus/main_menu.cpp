module menus;
import std;
import types;
import ui;
import gui;
import globals;
import globalization;
import textures;

namespace Menus {
using Globalization::GetStrbyKey;

class MainMenu: public GUI::Form {
private:
    ui::Context ctx = ui::Context();
    ui::View view = ui::View(ui::Spacer({}));

    void onLoad() override {
        using namespace ui;
        // clang-format off
        view = View(
            Stack({},
                StackItem({.alignment = Alignment::CENTER},
                    Sizer({.max_width = 400},
                        Column({},
                            Sizer({.max_height = 34},
                                Button({.text = GetStrbyKey("NEWorld.main.start"), .on_click = [] { worldmenu(); }})
                            ),
                            Spacer({.height = 6}),
                            Sizer({.max_height = 34},
                                Row({.main_axis_size = MainAxisSize::MAX, .cross_axis_size = CrossAxisSize::MAX},
                                    FlexItem({.flex_grow = 1},
                                        Button({.text = GetStrbyKey("NEWorld.main.options"), .on_click = [] { options(); }})
                                    ),
                                    Spacer({.width = 6}),
                                    FlexItem({.flex_grow = 1},
                                        Button({.text = GetStrbyKey("NEWorld.main.exit"), .on_click = [] { std::exit(0); }})
                                    )
                                )
                            )
                        )
                    )
                ),
                StackItem({.alignment = Alignment::BOTTOM_LEFT},
                    Padding({.bottom = 0},
                        Label({.text = GetStrbyKey("NEWorld.main.help")})
                    )
                )
            )
        );
        // clang-format on
    }

    void onUpdate() override {
        // Temporary
        ctx.theme = ui::theme_dark();
        ctx.scaling_factor = static_cast<float>(Stretch);
        ctx.view_size = {static_cast<float>(WindowWidth), static_cast<float>(WindowHeight)};
        ctx.mouse_position = {static_cast<float>(mx * Stretch), static_cast<float>(my * Stretch)};
        ctx.mouse_motion = {static_cast<float>((mx - mxl) * Stretch), static_cast<float>((my - myl) * Stretch)};
        ctx.mouse_wheel_motion = {static_cast<float>(mw - mwl)};
        ctx.mouse_left_button_down = (mb == 1);
        ctx.mouse_left_button_acted = (mb == 1 && mbl == 0);
        ctx.mouse_left_button_released = (mb == 0 && mbl == 1);
        view.update(ctx);

        if (GameBegin) {
            exit = true;
        }
    }

    void onRender() override {
        view.render(ctx);
    }
};

void mainmenu() {
    MainMenu Menu;
    Menu.start();
}
}
