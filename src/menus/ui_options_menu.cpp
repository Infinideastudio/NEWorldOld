module menus;
import std;
import types;
import ui;
import globals;
import globalization;
import setup;
import text_rendering;

namespace Menus {
using Globalization::GetStrbyKey;

class UIOptionsMenu: public ui::Menu {
private:
    auto build(ui::Context& ctx) -> ui::View override {
        using namespace ui;
        auto font_scale_key = ctx.generate_key();
        auto ui_stretch_key = ctx.generate_key();
        auto ui_background_blur_key = ctx.generate_key();
        // clang-format off
        auto column = Column({.main_axis_size = MainAxisSize::MAX, .main_axis_alignment = MainAxisAlignment::CENTER},
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
                            .on_update = [&ctx, font_scale_key](float value) {
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
                            .on_click = [&ctx, ui_stretch_key] {
                                toggle_stretch();
                                ctx.mark_for_update(ui_stretch_key);
                            }
                        })
                    )
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
                            .on_click = [&ctx, ui_background_blur_key] {
                                UIBackgroundBlur = !UIBackgroundBlur;
                                ctx.mark_for_update(ui_background_blur_key);
                            }
                        })
                    ),
                    Spacer({.width = 8}),
                    FlexItem({.flex_grow = 1}, Spacer({}))
                )
            ),
            FlexItem({.flex_grow = 1}, Spacer({.height = std::numeric_limits<float>::infinity()})),
            Sizer({.max_height = 32},
                Button({.label = Label(GetStrbyKey("NEWorld.gui.back")), .on_click = [this]() {
                    exit();
                }})
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

void uioptions() {
    UIOptionsMenu().run(MainWindow);
}

}
