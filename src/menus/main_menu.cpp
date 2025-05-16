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
        auto column = Column({},
            Sizer({.max_height = 256},
                ImageBox({.alignment = Alignment::CENTER, .fit = BoxFit::COVER, .texture = &TitleTexture})
            ),
            Padding({.left = 48, .right = 48, .bottom = 60},
                Column({},
                    Sizer({.max_height = 40},
                        Button({.label = Label(GetStrbyKey("NEWorld.main.start")), .on_click = [] { worldmenu(); }})
                    ),
                    Spacer({.height = 8}),
                    Sizer({.max_height = 40},
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
                )
            )
        );
        // clang-format on
        view = View(Stack(
            {},
            StackItem({.alignment = Alignment::CENTER}, Sizer({.max_width = 512}, std::move(column))),
            StackItem(
                {.alignment = Alignment::BOTTOM_LEFT},
                Padding({.left = 8, .top = 8, .right = 8, .bottom = 8}, Label(GetStrbyKey("NEWorld.main.help")))
            )
        ));
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
