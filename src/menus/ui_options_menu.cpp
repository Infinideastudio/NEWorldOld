module menus;
import std;
import types;
import ui;
import gui;
import globals;
import globalization;
import setup;
import text_rendering;

namespace Menus {
using Globalization::GetStrbyKey;

class UIOptionsMenu: public GUI::Form {
private:
    ui::Context ctx = ui::Context();
    ui::View view = ui::View(ui::Spacer({}));

    void onLoad() override {
        using namespace ui;
        auto font_scale_key = ctx.generate_key();
        auto ui_stretch_key = ctx.generate_key();
        auto ui_background_blur_key = ctx.generate_key();
        // clang-format off
        auto column = Column({},
            Sizer({.max_height = 32},
                Center({}, Label(GetStrbyKey("NEWorld.gui.caption")))
            ),
            Spacer({.height = 8}),
            Sizer({.max_height = 32},
                Row({.main_axis_size = MainAxisSize::MAX},
                    FlexItem({.flex_grow = 1},
                        Slider({
                            .label = Builder(font_scale_key, [](Key) {
                                return Label(GetStrbyKey("NEWorld.gui.fontsize") + Var2Str(FontScale));
                            }),
                            .value = static_cast<float>(FontScale) - 0.5f,
                            .on_update = [this, font_scale_key](float value) {
                                FontScale = std::round((value + 0.5f) * 10.0f) / 10.0f;
                                TextRenderer::init_font(true);
                                ctx.mark_for_update(font_scale_key);
                            }
                        })
                    ),
                    Spacer({.width = 8}),
                    FlexItem({.flex_grow = 1},
                        Button({
                            .label = Builder(ui_stretch_key, [](Key) {
                                return Label(GetStrbyKey("NEWorld.gui.stretch") + BoolEnabled(UIAutoStretch));
                            }),
                            .on_click = [this, ui_stretch_key] {
                                toggle_stretch();
                                ctx.mark_for_update(ui_stretch_key);
                            }
                        })
                    ),
                )
            ),
            Spacer({.height = 8}),
            Sizer({.max_height = 32},
                Row({.main_axis_size = MainAxisSize::MAX},
                    FlexItem({.flex_grow = 1},
                        Button({
                            .label = Builder(ui_background_blur_key, [](Key) {
                                return Label(GetStrbyKey("NEWorld.gui.blur") + BoolEnabled(UIBackgroundBlur));
                            }),
                            .on_click = [this, ui_background_blur_key] {
                                UIBackgroundBlur = !UIBackgroundBlur;
                                ctx.mark_for_update(ui_background_blur_key);
                            }
                        })
                    ),
                    Spacer({.width = 8}),
                    FlexItem({.flex_grow = 1}, Spacer({})),
                )
            ),
            FlexItem({.flex_grow = 1}, Spacer({.height = std::numeric_limits<float>::infinity()})),
            Sizer({.max_height = 32},
                Button({.label = Label(GetStrbyKey("NEWorld.gui.back")), .on_click = [this]() {
                    exit = true;
                }}),
            ),
        );
        // clang-format on
        view = View(Center({}, Sizer({.max_width = 512}, Padding({.top = 32, .bottom = 32}, std::move(column)))));
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

void uioptions() {
    UIOptionsMenu Menu;
    Menu.start();
}
}
