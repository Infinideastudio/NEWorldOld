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
    float scaling_factor = 1.0f;
    Theme theme = theme_light();
    Size view_size = {0.0f, 0.0f};
    Point mouse_position = {0.0f, 0.0f};
    Point mouse_motion = {0.0f, 0.0f};
    float mouse_wheel_motion = 0.0f;
    bool mouse_left_button_down = false;
    bool mouse_left_button_acted = false;
    bool mouse_left_button_released = false;

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

private:
    Key _next_key = 0;
    std::unordered_set<Key> _updated_keys = {};
    std::unordered_set<Key> _keys_to_update = {};
};

}
