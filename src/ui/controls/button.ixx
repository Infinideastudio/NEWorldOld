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

namespace ui {

export class Button: public Element {
public:
    struct Args {
        std::string text;
        std::function<void()> on_click;
    };

    Button(Args args):
        _text(std::move(args.text)),
        _on_click(std::move(args.on_click)) {}

    auto layout(Context const& ctx, Constraint const& constraint) -> Size override {
        return _size = Size(constraint);
    }

    void update(Context const& ctx, Point position) override {
        auto top_left = position * ctx.scaling_factor;
        auto bottom_right = (position + _size) * ctx.scaling_factor;
        _hover =
            (ctx.mouse_position.x() >= top_left.x() && ctx.mouse_position.x() <= bottom_right.x()
             && ctx.mouse_position.y() >= top_left.y() && ctx.mouse_position.y() <= bottom_right.y());
        if (_hover) {
            if (ctx.mouse_left_button_acted) {
                _pressed = true;
            } else if (_pressed && ctx.mouse_left_button_released) {
                _pressed = false;
                if (_on_click) {
                    _on_click();
                }
            } else {
                _pressed = _pressed && ctx.mouse_left_button_down;
            }
        } else {
            _pressed = false;
        }
    }

    void render(Context const& ctx, Point position, uint8_t) const override {
        constexpr auto LINE_WIDTH = Vec2f(2.0f);
        auto top_left = position * ctx.scaling_factor;
        auto bottom_right = (position + _size) * ctx.scaling_factor;
        auto v = vertex_builder();
        v.color(_hover ? ctx.theme.primary_color_darken : ctx.theme.primary_color);
        draw_quad(v, top_left, bottom_right);
        v.color(_pressed ? ctx.theme.primary_color_darken : ctx.theme.primary_color);
        draw_quad(v, top_left + LINE_WIDTH * ctx.scaling_factor, bottom_right - LINE_WIDTH * ctx.scaling_factor);
        auto va = render::VertexArray::create(v, render::VertexArray::Primitive::TRIANGLES);
        BlockTextureArray.bind(0); // Temporary
        va.first.render();
        TextRenderer::set_font_color(ctx.theme.text_on_primary_color);
        auto text_width = TextRenderer::rendered_width(_text);
        auto text_height = TextRenderer::line_height();
        TextRenderer::render_string(
            static_cast<int>((top_left.x() + bottom_right.x()) / 2.0f) - text_width / 2,
            static_cast<int>((top_left.y() + bottom_right.y()) / 2.0f) - text_height / 2,
            _text
        );
    }

private:
    std::string _text;
    std::function<auto()->void> _on_click;
    Size _size = {0.0f, 0.0f};
    bool _hover = false;
    bool _pressed = false;
};

}
