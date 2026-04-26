module menus;
import std;
import types;
import ui;
import globals;
import globalization;

namespace Menus {
using Globalization::GetStrbyKey;

class ShaderOptionsMenu: public ui::Menu {
private:
    static auto _shadow_resolution_to_position(int resolution) -> float {
        return (std::log2(static_cast<float>(resolution)) - 10.0f) / 3.0f;
    }

    static auto _position_to_shadow_resolution(float position) -> int {
        return static_cast<int>(std::pow(2.0f, std::round(position * 3.0f) + 10.0f));
    }

    auto build(ui::Context& ctx) -> ui::View override {
        using namespace ui;
        auto shaders_enable_key = ctx.generate_key();
        auto shadow_resolution_key = ctx.generate_key();
        auto shadow_distance_key = ctx.generate_key();
        auto soft_shadow_key = ctx.generate_key();
        auto volumetric_clouds_key = ctx.generate_key();
        auto ssao_key = ctx.generate_key();
        // clang-format off
        auto column = Column({.main_axis_size = MainAxisSize::MAX, .main_axis_alignment = MainAxisAlignment::CENTER},
            Sizer({.max_height = 32},
                Center({}, Label(GetStrbyKey("NEWorld.shaders.caption")))
            ),
            Spacer({.height = 8}),
            Sizer({.max_height = 32},
                Row({.main_axis_size = MainAxisSize::MAX},
                    FlexItem({.flex_grow = 1},
                        Button({
                            .label = Builder(shaders_enable_key, [](Key) {
                                return Label(GetStrbyKey("NEWorld.shaders.enable") + BoolYesNo(AdvancedRender));
                            }),
                            .on_click = [&ctx, shaders_enable_key] {
                                AdvancedRender = !AdvancedRender;
                                ctx.mark_for_update(shaders_enable_key);
                            }
                        })
                    ),
                    Spacer({.width = 8}),
                    FlexItem({.flex_grow = 1},
                        Slider({
                            .label = Builder(shadow_resolution_key, [](Key) {
                                return Label(GetStrbyKey("NEWorld.shaders.shadowres") + Var2Str(ShadowRes) + "x");
                            }),
                            .value = _shadow_resolution_to_position(ShadowRes),
                            .on_update = [&ctx, shadow_resolution_key](float value) {
                                ShadowRes = _position_to_shadow_resolution(value);
                                ctx.mark_for_update(shadow_resolution_key);
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
                            .label = Builder(shadow_distance_key, [](Key) {
                                return Label(GetStrbyKey("NEWorld.shaders.distance") + Var2Str(MaxShadowDistance));
                            }),
                            .value = (static_cast<float>(MaxShadowDistance) - 4.0f) / 28.0f,
                            .on_update = [&ctx, shadow_distance_key](float value) {
                                MaxShadowDistance = static_cast<int>(value * 28.0f + 4.0f);
                                ctx.mark_for_update(shadow_distance_key);
                            }
                        })
                    ),
                    Spacer({.width = 8}),
                    FlexItem({.flex_grow = 1},
                        Button({
                            .label = Builder(soft_shadow_key, [](Key) {
                                return Label(GetStrbyKey("NEWorld.shaders.softshadow") + BoolEnabled(SoftShadow));
                            }),
                            .on_click = [&ctx, soft_shadow_key] {
                                SoftShadow = !SoftShadow;
                                ctx.mark_for_update(soft_shadow_key);
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
                            .label = Builder(volumetric_clouds_key, [](Key) {
                                return Label(GetStrbyKey("NEWorld.shaders.clouds") + BoolEnabled(VolumetricClouds));
                            }),
                            .on_click = [&ctx, volumetric_clouds_key] {
                                VolumetricClouds = !VolumetricClouds;
                                ctx.mark_for_update(volumetric_clouds_key);
                            }
                        })
                    ),
                    Spacer({.width = 8}),
                    FlexItem({.flex_grow = 1},
                        Button({
                            .label = Builder(ssao_key, [](Key) {
                                return Label(GetStrbyKey("NEWorld.shaders.ssao") + BoolEnabled(AmbientOcclusion));
                            }),
                            .on_click = [&ctx, ssao_key] {
                                AmbientOcclusion = !AmbientOcclusion;
                                ctx.mark_for_update(ssao_key);
                            }
                        })
                    )
                )
            ),
            FlexItem({.flex_grow = 1}, Spacer({.height = std::numeric_limits<float>::infinity()})),
            Sizer({.max_height = 32},
                Button({.label = Label(GetStrbyKey("NEWorld.shaders.back")), .on_click = [this] { exit(); }})
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

void shaderoptions() {
    ShaderOptionsMenu().run(MainWindow);
}

}
