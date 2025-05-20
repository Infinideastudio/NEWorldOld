export module ui:button;
import std;
import types;
import debug;
import math;
import render;
import textures;
import text_rendering;
import :context;
import :element;
import :render;
import :label;

namespace ui {

export class Button: public Element {
public:
    struct Args {
        ElementHandle label;
        std::function<void()> on_click;
    };

    explicit Button(Args&& args): // NOLINT
        _label(std::move(args.label)),
        _on_click(std::move(args.on_click)) {}

    auto layout(Context const& ctx, Constraint const& constraint) -> Size override {
        _size = Size(constraint);
        auto label_size = _label ? _label->layout(ctx, constraint) : Size();
        _label_position = Point((_size - label_size) / 2.0f);
        return _size;
    }

    void update(Context& ctx, Point position) override {
        auto top_left = position;
        auto bottom_right = position + _size;
        _hover =
            (ctx.mouse_position().x() >= top_left.x() && ctx.mouse_position().x() <= bottom_right.x()
             && ctx.mouse_position().y() >= top_left.y() && ctx.mouse_position().y() <= bottom_right.y());
        auto clicked = false;
        if (_hover) {
            if (ctx.mouse_left_button_acted()) {
                _pressed = true;
            } else if (_pressed && ctx.mouse_left_button_released()) {
                _pressed = false;
                clicked = true;
            } else {
                _pressed = _pressed && ctx.mouse_left_button_down();
            }
        } else {
            _pressed = false;
        }
        if (clicked && _on_click) {
            _on_click();
        }
        if (_label) {
            _label->update(ctx, Point(position + _label_position));
        }
    }

    void render(Context& ctx, Point position, uint8_t clip_layer) const override {
        constexpr auto LINE_WIDTH = 2.0f;

        auto top_left = position * ctx.scaling_factor();
        auto bottom_right = (position + _size) * ctx.scaling_factor();
        auto line_width = std::round(LINE_WIDTH * ctx.scaling_factor());

        auto v = vertex_builder();
        v.color(!_hover || _pressed ? ctx.theme().primary_color_darken : ctx.theme().primary_color);
        draw_quad(v, top_left, bottom_right);
        v.color(_pressed ? ctx.theme().primary_color_darken : ctx.theme().primary_color);
        draw_quad(v, top_left + line_width, bottom_right - line_width);
        auto va = render::VertexArray::create(v, render::VertexArray::Primitive::TRIANGLES);
        BlockTextureArray.bind(0); // Temporary
        va.first.render();

        if (_label) {
            _label->render(ctx, Point(position + _label_position), clip_layer);
        }
    }

private:
    std::unique_ptr<Element> _label;
    std::function<void()> _on_click;
    Size _size = {0.0f, 0.0f};
    Point _label_position = {0.0f, 0.0f};
    bool _hover = false;
    bool _pressed = false;
};

}
