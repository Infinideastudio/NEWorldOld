module;

#include <GLFW/glfw3.h>
#undef assert

export module ui:context;
import std;
import types;
import debug;
import math;
import globals;

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

// Global element key.
// Used to identify the dynamic parts (builders) in the element tree.
export using Key = uint64_t;

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
        .primary_color = { 20,  20,  20, 255},
        .primary_color_darken = { 10,  10,  10, 255},
        .secondary_color = { 20,  20,  20, 153},
        .secondary_color_darken = { 10,  10,  10, 153},
        .text_color = {200, 200, 200, 255},
        .text_on_primary_color = {255, 255, 255, 255},
        .text_on_secondary_color = {200, 200, 200, 255},
    };
}

// Update and render context.
export class Context {
public:
    // Returns a unique key for the next element.
    auto generate_key() -> Key {
        auto res = _next_key++;
        return res;
    }

    // Checks if any key has been updated in the previous loop.
    auto has_updated_keys() const -> bool {
        return !_updated_keys.empty();
    }

    // Checks if a key has been updated in the previous loop.
    auto updated(Key key) const -> bool {
        return _updated_keys.contains(key);
    }

    // Mark a key for update.
    void mark_for_update(Key key) {
        _keys_to_update.insert(key);
    }

    // Refresh key lists for next loop.
    void refresh_keys() {
        _updated_keys = std::exchange(_keys_to_update, {});
    }

    auto scaling_factor() const -> float {
        return _scaling_factor;
    }

    auto theme() const -> Theme const& {
        return _theme;
    }

    auto view_size() const -> Size {
        return _view_size;
    }

    auto mouse_position() const -> Point {
        return _mouse_position;
    }

    auto mouse_motion() const -> Point {
        return _mouse_motion;
    }

    auto mouse_wheel_motion() const -> float {
        return _mouse_wheel_motion;
    }

    auto mouse_left_button_down() const -> bool {
        return _mouse_left_button_down;
    }

    auto mouse_left_button_acted() const -> bool {
        return _mouse_left_button_acted;
    }

    auto mouse_left_button_released() const -> bool {
        return _mouse_left_button_released;
    }

    auto input_chars() const -> std::u32string_view {
        return _input_chars;
    }

    auto backspace_acted() const -> bool {
        return _backspace_acted;
    }

    auto window_closing() const -> bool {
        return _window_closing;
    }

    // Temporary function to update the input states.
    void update_from(GLFWwindow const* window) {
        auto x = 0.0, y = 0.0;
        glfwGetCursorPos(MainWindow, &x, &y);
        auto mouse_position = Vec2f(Vec2d(x, y) / Stretch);
        auto mouse_wheel_position = static_cast<float>(getMouseScroll());
        auto mouse_button = getMouseButton();
        auto window_closing = glfwWindowShouldClose(MainWindow);

        _scaling_factor = static_cast<float>(Stretch);
        _view_size = {static_cast<float>(WindowWidth / Stretch), static_cast<float>(WindowHeight / Stretch)};
        _mouse_motion = Point(mouse_position - _mouse_position);
        _mouse_position = Point(mouse_position);
        _mouse_wheel_motion = mouse_wheel_position - _mouse_wheel_position;
        _mouse_wheel_position = mouse_wheel_position;
        _mouse_left_button_down = (mouse_button == 1);
        _mouse_left_button_acted = (mouse_button == 1 && _mouse_button == 0);
        _mouse_left_button_released = (mouse_button == 0 && _mouse_button == 1);
        _mouse_button = mouse_button;
        _input_chars = inputstr;
        _backspace_acted = backspace;
        _window_closing = window_closing;

        inputstr.clear();
        backspace = false;
    }

private:
    Key _next_key = 0;
    std::unordered_set<Key> _updated_keys = {};
    std::unordered_set<Key> _keys_to_update = {};

    float _scaling_factor = 1.0f;
    Theme _theme = theme_dark();
    Size _view_size = {0.0f, 0.0f};
    Point _mouse_motion = {0.0f, 0.0f};
    Point _mouse_position = {0.0f, 0.0f};
    float _mouse_wheel_motion = 0.0f;
    float _mouse_wheel_position = 0.0f;
    bool _mouse_left_button_down = false;
    bool _mouse_left_button_acted = false;
    bool _mouse_left_button_released = false;
    int _mouse_button = 0;
    std::u32string _input_chars = {};
    bool _backspace_acted = false;
    bool _window_closing = false;
};

}
