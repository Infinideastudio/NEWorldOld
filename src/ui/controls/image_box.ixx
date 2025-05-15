export module ui:image_box;
import std;
import types;
import debug;
import math;
import render;
import :context;
import :element;
import :layout;
import :render;

namespace ui {

export class ImageBox: public Element {
public:
    struct Args {
        Alignment alignment = Alignment::TOP_LEFT;
        BoxFit fit = BoxFit::NONE;
        render::Texture const* texture = nullptr;
    };

    explicit ImageBox(Args args):
        _alignment(alignment_to_fractions(args.alignment)),
        _fit(args.fit),
        _texture(args.texture) {}

    auto layout(Context const& ctx, Constraint const& constraint) -> Size override {
        return _size = Size(constraint);
    }

    void update(Context const& ctx, Point position) override {}

    void render(Context const& ctx, Point position, uint8_t) const override {
        if (!_texture || !*_texture) {
            return;
        }
        auto inner_size = Size(static_cast<float>(_texture->width()), static_cast<float>(_texture->height()));
        auto container_size = Size(_size);
        auto [fitted_inner_size, fitted_container_size] = apply_box_fit(_fit, inner_size, container_size);

        auto inner_top_left = (inner_size - fitted_inner_size) * _alignment;
        auto inner_bottom_right = inner_top_left + fitted_inner_size;
        auto container_top_left = (container_size - fitted_container_size) * _alignment;
        auto container_bottom_right = container_top_left + fitted_container_size;

        auto v = vertex_builder();
        v.color(255, 255, 255, 255);
        draw_quad(
            v,
            (position + container_top_left) * ctx.scaling_factor,
            (position + container_bottom_right) * ctx.scaling_factor,
            {inner_top_left.x() / inner_size.x(), 1.0 - inner_top_left.y() / inner_size.y()},
            {inner_bottom_right.x() / inner_size.x(), 1.0 - inner_bottom_right.y() / inner_size.y()}
        );
        auto va = render::VertexArray::create(v, render::VertexArray::Primitive::TRIANGLES);
        _texture->bind(0);
        va.first.render();
    }

private:
    Point _alignment = {0.0f, 0.0f};
    BoxFit _fit = BoxFit::NONE;
    render::Texture const* _texture;
    Size _size = {0.0f, 0.0f};
    bool _hover = false;
    bool _pressed = false;
};
}
