module menus;
import std;
import types;
import ui;
import gui;
import globals;
import globalization;
import rendering;

namespace Menus {
using Globalization::GetStrbyKey;

class OptionsMenu: public GUI::Form {
private:
    ui::Context ctx = ui::Context();
    ui::View view = ui::View(ui::Spacer({}));

    void onLoad() override {
        using namespace ui;
        auto fov_key = ctx.generate_key();
        auto sensitivity_key = ctx.generate_key();
        auto render_distance_key = ctx.generate_key();
        // clang-format off
        auto column = Column({},
            Sizer({.max_height = 32},
                Center({}, Label(GetStrbyKey("NEWorld.options.caption")))
            ),
            Spacer({.height = 8}),
            Sizer({.max_height = 32},
                Row({.main_axis_size = MainAxisSize::MAX},
                    FlexItem({.flex_grow = 1},
                        Slider(Slider::Args<Builder>{
                            .label = Builder(fov_key, [](Key) {
                                return Label(strWithVar(GetStrbyKey("NEWorld.options.fov"), FOVyNormal));
                            }),
                            .value = (FOVyNormal - 60.0f) / 60.0f,
                            .on_update = [this, fov_key](float value) {
                                FOVyNormal = std::round(value * 60.0f + 60.0f);
                                ctx.mark_for_update(fov_key);
                            }
                        })
                    ),
                    Spacer({.width = 8}),
                    FlexItem({.flex_grow = 1},
                        Slider(Slider::Args<Builder>{
                            .label = Builder(sensitivity_key, [](Key) {
                                return Label(strWithVar(GetStrbyKey("NEWorld.options.sensitivity"), MouseSpeed));
                            }),
                            .value = MouseSpeed * 4.0f,
                            .on_update = [this, sensitivity_key](float value) {
                                MouseSpeed = std::round((value / 4.0f) * 100.0f) / 100.0f;
                                ctx.mark_for_update(sensitivity_key);
                            }
                        })
                    ),
                )
            ),
            Spacer({.height = 8}),
            Sizer({.max_height = 32},
                Row({.main_axis_size = MainAxisSize::MAX},
                    FlexItem({.flex_grow = 1},
                        Slider(Slider::Args<Builder>{
                            .label = Builder(render_distance_key, [](Key) {
                                return Label(strWithVar(GetStrbyKey("NEWorld.options.distance"), RenderDistance));
                            }),
                            .value = (static_cast<float>(RenderDistance) - 4.0f) / 44.0f,
                            .on_update = [this, render_distance_key](float value) {
                                RenderDistance = static_cast<int>(value * 44.0f + 4.0f);
                                ctx.mark_for_update(render_distance_key);
                            }
                        })
                    ),
                    Spacer({.width = 8}),
                    FlexItem({.flex_grow = 1},
                        Button({.label = GetStrbyKey("NEWorld.options.rendermenu"), .on_click = renderoptions}),
                    ),
                )
            ),
            Spacer({.height = 8}),
            Sizer({.max_height = 32},
                Row({.main_axis_size = MainAxisSize::MAX},
                    FlexItem({.flex_grow = 1},
                        Button({.label = GetStrbyKey("NEWorld.options.guimenu"), .on_click = uioptions}),
                    ),
                    Spacer({.width = 8}),
                    FlexItem({.flex_grow = 1},
                        Button({.label = GetStrbyKey("NEWorld.options.languagemenu"), .on_click = languagemenu}),
                    ),
                )
            ),
            FlexItem({.flex_grow = 1}, Spacer({.height = std::numeric_limits<float>::infinity()})),
            Sizer({.max_height = 32},
                Row({.main_axis_size = MainAxisSize::MAX},
                    FlexItem({.flex_grow = 1},
                        Button({.label = GetStrbyKey("NEWorld.options.back"), .on_click = [this]() {
                            Renderer::init_pipeline(true, false);
                            exit = true;
                        }}),
                    ),
                    Spacer({.width = 8}),
                    FlexItem({.flex_grow = 1},
                        Button({.label = GetStrbyKey("NEWorld.options.save"), .on_click = save_options}),
                    ),
                )
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
    }

    void onRender() override {
        view.render(ctx);
    }
};

void options() {
    OptionsMenu Menu;
    Menu.start();
}
}
