module menus;
import std;
import types;
import ui;
import globals;
import globalization;
import rendering;

namespace Menus {
using Globalization::GetStrbyKey;

class OptionsMenu: public ui::Menu {
private:
    auto build(ui::Context& ctx) -> ui::View override {
        using namespace ui;
        auto fov_key = ctx.generate_key();
        auto sensitivity_key = ctx.generate_key();
        auto render_distance_key = ctx.generate_key();
        // clang-format off
        auto column = Column({.main_axis_size = MainAxisSize::MAX, .main_axis_alignment = MainAxisAlignment::CENTER},
            Sizer({.max_height = 32},
                Center({}, Label(GetStrbyKey("NEWorld.options.caption")))
            ),
            Spacer({.height = 8}),
            Sizer({.max_height = 32},
                Row({.main_axis_size = MainAxisSize::MAX},
                    FlexItem({.flex_grow = 1},
                        Slider({
                            .label = Builder(fov_key, [](Key) {
                                return Label(strWithVar(GetStrbyKey("NEWorld.options.fov"), FOVyNormal));
                            }),
                            .value = (FOVyNormal - 60.0f) / 60.0f,
                            .on_update = [&ctx, fov_key](float value) {
                                FOVyNormal = std::round(value * 60.0f + 60.0f);
                                ctx.mark_for_update(fov_key);
                            }
                        })
                    ),
                    Spacer({.width = 8}),
                    FlexItem({.flex_grow = 1},
                        Slider({
                            .label = Builder(sensitivity_key, [](Key) {
                                return Label(strWithVar(GetStrbyKey("NEWorld.options.sensitivity"), MouseSpeed));
                            }),
                            .value = MouseSpeed * 4.0f,
                            .on_update = [&ctx, sensitivity_key](float value) {
                                MouseSpeed = std::round((value / 4.0f) * 100.0f) / 100.0f;
                                ctx.mark_for_update(sensitivity_key);
                            }
                        })
                    )
                )
            ),
            Spacer({.height = 8}),
            Sizer({.max_height = 32},
                Row({.main_axis_size = MainAxisSize::MAX},
                    FlexItem({.flex_grow = 1},
                        Slider({
                            .label = Builder(render_distance_key, [](Key) {
                                return Label(strWithVar(GetStrbyKey("NEWorld.options.distance"), RenderDistance));
                            }),
                            .value = (static_cast<float>(RenderDistance) - 4.0f) / 44.0f,
                            .on_update = [&ctx, render_distance_key](float value) {
                                RenderDistance = static_cast<int>(value * 44.0f + 4.0f);
                                ctx.mark_for_update(render_distance_key);
                            }
                        })
                    ),
                    Spacer({.width = 8}),
                    FlexItem({.flex_grow = 1},
                        Button({.label = Label(GetStrbyKey("NEWorld.options.rendermenu")), .on_click = renderoptions})
                    )
                )
            ),
            Spacer({.height = 8}),
            Sizer({.max_height = 32},
                Row({.main_axis_size = MainAxisSize::MAX},
                    FlexItem({.flex_grow = 1},
                        Button({.label = Label(GetStrbyKey("NEWorld.options.guimenu")), .on_click = uioptions})
                    ),
                    Spacer({.width = 8}),
                    FlexItem({.flex_grow = 1},
                        Button({.label = Label(GetStrbyKey("NEWorld.options.languagemenu")), .on_click = languagemenu})
                    )
                )
            ),
            FlexItem({.flex_grow = 1}, Spacer({.height = std::numeric_limits<float>::infinity()})),
            Sizer({.max_height = 32},
                Row({.main_axis_size = MainAxisSize::MAX},
                    FlexItem({.flex_grow = 1},
                        Button({.label = Label(GetStrbyKey("NEWorld.options.back")), .on_click = [this]() {
                            Renderer::init_pipeline(true, true);
                            exit();
                        }})
                    ),
                    Spacer({.width = 8}),
                    FlexItem({.flex_grow = 1},
                        Button({.label = Label(GetStrbyKey("NEWorld.options.save")), .on_click = save_options})
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

void options() {
    OptionsMenu().run(MainWindow);
}

}
