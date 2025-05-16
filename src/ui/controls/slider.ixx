export module ui:slider;
import std;
import types;
import debug;
import math;
import render;
import text_rendering;
import :context;
import :element;
import :render;
import :label;

namespace ui {

export class Slider: public Element {
public:
    struct Args {
        ElementHandle label;
        float value = 0.0f;
        std::function<void(float)> on_update;
    };

    explicit Slider(Args&& args): // NOLINT
        _label(std::move(args.label)),
        _value(args.value),
        _on_update(std::move(args.on_update)) {}

    auto layout(Context const& ctx, Constraint const& constraint) -> Size override {
        _size = Size(constraint);
        auto label_size = _label ? _label->layout(ctx, constraint) : Size();
        _label_position = Point((_size - label_size) / 2.0f);
        return _size;
    }

    void update(Context const& ctx, Point position) override {
        constexpr auto SLIDER_WIDTH = 16.0f;
        auto top_left = position;
        auto bottom_right = position + _size;
        _hover =
            (ctx.mouse_position.x() >= top_left.x() && ctx.mouse_position.x() <= bottom_right.x()
             && ctx.mouse_position.y() >= top_left.y() && ctx.mouse_position.y() <= bottom_right.y());
        if (_hover) {
            if (ctx.mouse_left_button_acted) {
                _pressed = true;
            } else {
                _pressed = _pressed && ctx.mouse_left_button_down;
            }
        } else {
            _pressed = _pressed && ctx.mouse_left_button_down;
        }
        if (_pressed && _on_update) {
            auto half_width = SLIDER_WIDTH / 2.0f;
            auto x_min = top_left.x() + half_width;
            auto x_max = bottom_right.x() - half_width;
            auto x_sel = std::clamp(ctx.mouse_position.x(), x_min, x_max);
            auto value = (x_sel - x_min) / (x_max - x_min);
            _on_update(value);
            _value = value;
        }
        if (_label) {
            _label->update(ctx, Point(position + _label_position));
        }
    }

    void render(Context const& ctx, Point position, uint8_t clip_layer) const override {
        constexpr auto SLIDER_WIDTH = 16.0f;
        constexpr auto LINE_WIDTH = 2.0f;

        auto top_left = position * ctx.scaling_factor;
        auto bottom_right = (position + _size) * ctx.scaling_factor;
        auto slider_width = std::round(SLIDER_WIDTH * ctx.scaling_factor);
        auto line_width = std::round(LINE_WIDTH * ctx.scaling_factor);

        auto half_width = slider_width / 2.0f;
        auto x_min = top_left.x() + half_width;
        auto x_max = bottom_right.x() - half_width;
        auto x_sel = _value * (x_max - x_min) + x_min;

        auto slider_top_left = Point(x_sel - half_width, top_left.y());
        auto slider_bottom_right = Point(x_sel + half_width, bottom_right.y());

        auto v = vertex_builder();
        v.color(ctx.theme.secondary_color_darken);
        draw_quad(v, top_left, bottom_right);
        v.color(!_hover || _pressed ? ctx.theme.primary_color_darken : ctx.theme.primary_color);
        draw_quad(v, slider_top_left, slider_bottom_right);
        v.color(_pressed ? ctx.theme.primary_color_darken : ctx.theme.primary_color);
        draw_quad(v, slider_top_left + line_width, slider_bottom_right - line_width);
        auto va = render::VertexArray::create(v, render::VertexArray::Primitive::TRIANGLES);
        BlockTextureArray.bind(0); // Temporary
        va.first.render();

        if (_label) {
            _label->render(ctx, Point(position + _label_position), clip_layer);
        }
    }

private:
    std::unique_ptr<Element> _label;
    float _value;
    std::function<void(float)> _on_update;
    Size _size = {0.0f, 0.0f};
    Point _label_position = {0.0f, 0.0f};
    bool _hover = false;
    bool _pressed = false;
};

}
