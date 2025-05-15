export module ui:label;
import std;
import types;
import debug;
import math;
import render;
import text_rendering;
import :context;
import :element;
import :render;

namespace ui {

export class Label: public Element {
public:
    // Allow implicit construction since this is quite often used.
    Label(std::string text):
        _text(std::move(text)) {}

    auto layout(Context const& ctx, Constraint const& constraint) -> Size override {
        auto width = static_cast<float>(TextRenderer::rendered_width(_text)) / ctx.scaling_factor;
        auto height = static_cast<float>(TextRenderer::line_height()) / ctx.scaling_factor;
        return {width, height};
    }

    void update(Context const& ctx, Point position) override {}

    void render(Context const& ctx, Point position, uint8_t) const override {
        auto top_left = position * ctx.scaling_factor;
        TextRenderer::set_font_color(ctx.theme.text_color);
        TextRenderer::render_string(static_cast<int>(top_left.x()), static_cast<int>(top_left.y()), _text);
    }

private:
    std::string _text;
};

}
