module menus;
import std;
import types;
import ui;
import gui;
import globals;
import globalization;

namespace Menus {
using Globalization::GetStrbyKey;

class RenderOptionsMenu: public GUI::Form {
private:
    ui::Context ctx = ui::Context();
    ui::View view = ui::View(ui::Spacer({}));

    auto _msaa_to_position(int level) -> float {
        return level <= 1 ? 0.0f : std::log2(static_cast<float>(level)) / 3.0f;
    }

    auto _position_to_msaa(float position) -> int {
        auto level = static_cast<int>(std::pow(2.0f, std::round(position * 3.0f)));
        return level <= 1 ? 0 : level;
    }

    void onLoad() override {
        using namespace ui;
        auto smooth_lighting_key = ctx.generate_key();
        auto fancy_grass_key = ctx.generate_key();
        auto merge_face_key = ctx.generate_key();
        auto msaa_key = ctx.generate_key();
        auto vsync_key = ctx.generate_key();
        // clang-format off
        auto column = Column({},
            Sizer({.max_height = 32},
                Center({}, Label(GetStrbyKey("NEWorld.render.caption")))
            ),
            Spacer({.height = 8}),
            Sizer({.max_height = 32},
                Row({.main_axis_size = MainAxisSize::MAX},
                    FlexItem({.flex_grow = 1},
                        Button({
                            .label = Builder(smooth_lighting_key, [](Key) {
                                return Label(GetStrbyKey("NEWorld.render.smooth") + BoolEnabled(SmoothLighting));
                            }),
                            .on_click = [this, smooth_lighting_key] {
                                SmoothLighting = !SmoothLighting;
                                ctx.mark_for_update(smooth_lighting_key);
                            }
                        })
                    ),
                    Spacer({.width = 8}),
                    FlexItem({.flex_grow = 1},
                        Button({
                            .label = Builder(fancy_grass_key, [](Key) {
                                return Label(GetStrbyKey("NEWorld.render.grasstex") + BoolEnabled(NiceGrass));
                            }),
                            .on_click = [this, fancy_grass_key] {
                                NiceGrass = !NiceGrass;
                                ctx.mark_for_update(fancy_grass_key);
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
                            .label = Builder(merge_face_key, [](Key) {
                                return Label(GetStrbyKey("NEWorld.render.merge") + BoolEnabled(MergeFace));
                            }),
                            .on_click = [this, merge_face_key] {
                                MergeFace = !MergeFace;
                                ctx.mark_for_update(merge_face_key);
                            }
                        })
                    ),
                    Spacer({.width = 8}),
                    FlexItem({.flex_grow = 1},
                        Slider({
                            .label = Builder(msaa_key, [](Key) {
                                auto text = GetStrbyKey("NEWorld.render.multisample");
                                text += (Multisample != 0 ? Var2Str(Multisample) + "x" : BoolEnabled(false));
                                return Label(text);
                            }),
                            .value = _msaa_to_position(Multisample),
                            .on_update = [this, msaa_key](float value) {
                                Multisample = _position_to_msaa(value);
                                ctx.mark_for_update(msaa_key);
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
                            .label = Builder(vsync_key, [](Key) {
                                return Label(GetStrbyKey("NEWorld.render.vsync") + BoolEnabled(VerticalSync));
                            }),
                            .on_click = [this, vsync_key] {
                                VerticalSync = !VerticalSync;
                                ctx.mark_for_update(vsync_key);
                            }
                        })
                    ),
                    Spacer({.width = 8}),
                    FlexItem({.flex_grow = 1},
                        Button({.label = Label(GetStrbyKey("NEWorld.render.shaders")), .on_click = shaderoptions})
                    )
                )
            ),
            FlexItem({.flex_grow = 1}, Spacer({.height = std::numeric_limits<float>::infinity()})),
            Sizer({.max_height = 32},
                Button({.label =  Label(GetStrbyKey("NEWorld.render.back")), .on_click = [this] { exit = true; }})
            )
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

void renderoptions() {
    RenderOptionsMenu Menu;
    Menu.start();
}
}
