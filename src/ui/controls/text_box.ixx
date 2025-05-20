export module ui:text_box;
import std;
import types;
import debug;
import math;
import render;
import text_rendering;
import globals;
import :context;
import :element;
import :render;

namespace ui {

// Non-trivial state of `TextBox` that we want to share.
export struct TextBoxState {
    std::u32string text;
};

export class TextBox: public Element {
public:
    struct Args {
        std::string placeholder;
        std::shared_ptr<TextBoxState> state;
    };

    explicit TextBox(Args&& args): // NOLINT
        _placeholder(std::move(args.placeholder)),
        _state(std::move(args.state)) {}

    auto layout(Context const& ctx, Constraint const& constraint) -> Size override {
        return _size = Size(constraint);
    }

    void update(Context& ctx, Point position) override {
        auto top_left = position;
        auto bottom_right = position + _size;
        _hover =
            (ctx.mouse_position().x() >= top_left.x() && ctx.mouse_position().x() <= bottom_right.x()
             && ctx.mouse_position().y() >= top_left.y() && ctx.mouse_position().y() <= bottom_right.y());
        if (_hover) {
            if (ctx.mouse_left_button_acted()) {
                _focused = true;
                _cursor_blink_timer = _timer();
            }
        } else {
            if (ctx.mouse_left_button_acted()) {
                _focused = false;
            }
        }
        if (_focused && _state) {
            if (!ctx.input_chars().empty()) {
                _state->text += ctx.input_chars();
                _cursor_blink_timer = _timer();
            }
            if (ctx.backspace_acted()) {
                if (!_state->text.empty()) {
                    _state->text.pop_back();
                }
                _cursor_blink_timer = _timer();
            }
        }
    }

    void render(Context& ctx, Point position, uint8_t clip_layer) const override {
        constexpr auto CURSOR_WIDTH = 2.0f;

        auto top_left = position * ctx.scaling_factor();
        auto bottom_right = (position + _size) * ctx.scaling_factor();
        auto cursor_width = CURSOR_WIDTH * ctx.scaling_factor();

        auto v = vertex_builder();
        v.color(!_hover || _focused ? ctx.theme().secondary_color_darken : ctx.theme().secondary_color);
        draw_quad(v, top_left, bottom_right);
        auto va = render::VertexArray::create(v, render::VertexArray::Primitive::TRIANGLES);
        BlockTextureArray.bind(0); // Temporary
        va.first.render();

        if (_state) {
            auto text = (_state->text.empty() && !_focused) ? utf8_unicode(_placeholder) : _state->text;
            auto width = static_cast<float>(TextRenderer::rendered_width(text));
            auto height = static_cast<float>(TextRenderer::line_height());
            auto text_top_left = Vec2f(top_left.x(), (top_left.y() + bottom_right.y() - height) / 2.0f);

            TextRenderer::set_font_color(ctx.theme().text_color);
            TextRenderer::render_string(static_cast<int>(text_top_left.x()), static_cast<int>(text_top_left.y()), text);

            if (_focused && _cursor_blink()) {
                auto cursor_top_left = text_top_left + Vec2f(width, 0.0f);
                auto cursor_bottom_right = text_top_left + Vec2f(width + cursor_width, height);

                v.clear();
                v.color(ctx.theme().text_color);
                draw_quad(v, cursor_top_left, cursor_bottom_right);
                va = render::VertexArray::create(v, render::VertexArray::Primitive::TRIANGLES);
                BlockTextureArray.bind(0); // Temporary
                va.first.render();
            }
        }
    }

private:
    std::string _placeholder;
    std::shared_ptr<TextBoxState> _state;
    Size _size = {0.0f, 0.0f};
    bool _hover = false;
    bool _focused = false;
    double _cursor_blink_timer = 0.0;

    static auto _timer() -> double {
        return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    auto _cursor_blink() const -> bool {
        return std::fmod(_timer() - _cursor_blink_timer, 1.0) < 0.5;
    }
};

}
