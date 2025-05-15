export module ui:context;
import std;
import types;
import debug;
import math;

namespace ui {

// Horizontal and vertical coordinates in logical pixels.
export class Point: public Vec2f {
public:
    explicit Point(Vec2f v):
        Vec2f(v) {}

    Point() = default;
    Point(float x, float y):
        Vec2f(x, y) {}
};

// Width and height in logical pixels.
export class Size: public Vec2f {
public:
    explicit Size(Vec2f v):
        Vec2f(v) {}

    Size() = default;
    Size(float width, float height):
        Vec2f(width, height) {}

    auto width() const -> float {
        return x();
    }
    auto width() -> float& {
        return x();
    }
    auto height() const -> float {
        return y();
    }
    auto height() -> float& {
        return y();
    }
};

// Theme used for rendering.
export class Theme {
public:
    Vec4u8 container_color;
    Vec4u8 primary_color;
    Vec4u8 primary_color_darken;
    Vec4u8 secondary_color;
    Vec4u8 secondary_color_darken;
    Vec4u8 text_color;
    Vec4u8 text_on_primary_color;
    Vec4u8 text_on_secondary_color;
};

// Default light theme.
export constexpr auto theme_light() -> Theme {
    return {
        .container_color = {255, 255, 255, 255},
        .primary_color = {  0,  80, 255, 255},
        .primary_color_darken = { 55, 156, 255, 255},
        .secondary_color = {156, 156, 156, 153},
        .secondary_color_darken = {200, 200, 200, 153},
        .text_color = {  5,   5,   5, 255},
        .text_on_primary_color = {255, 255, 255, 255},
        .text_on_secondary_color = {  5,   5,   5, 255},
    };
}

// Default dark theme.
export constexpr auto theme_dark() -> Theme {
    return {
        .container_color = {  2,   2,   2, 255},
        .primary_color = {  0,  80, 255, 255},
        .primary_color_darken = {  0,  32, 120, 255},
        .secondary_color = { 20,  20,  20, 153},
        .secondary_color_darken = { 32,  32,  32, 153},
        .text_color = {200, 200, 200, 255},
        .text_on_primary_color = {255, 255, 255, 255},
        .text_on_secondary_color = {200, 200, 200, 255},
    };
}

// Update and render context.
export class Context {
public:
    float scaling_factor = 1.0f;
    Theme theme = theme_light();
    Size view_size = {0.0f, 0.0f};
    Point mouse_position = {0.0f, 0.0f};
    Point mouse_motion = {0.0f, 0.0f};
    float mouse_wheel_motion = 0.0f;
    bool mouse_left_button_down = false;
    bool mouse_left_button_acted = false;
    bool mouse_left_button_released = false;
};

}
