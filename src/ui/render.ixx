module;

#include <glad/gl.h>

export module ui:render;
import std;
import types;
import debug;
import render;
import textures;
import :context;
import :element;

namespace ui {

// Returns the UI vertex builder.
auto vertex_builder() {
    namespace al = render::attrib_layout::spec;
    return render::AttribIndexBuilder<al::Coord<al::Vec2f>, al::TexCoord<al::Vec3f>, al::Color<al::Vec4u8>>();
}

// Draws a quad (two triangles) with the given coordinates and texture coordinates.
void draw_quad(
    decltype(vertex_builder())& v,
    Vec2f top_left,
    Vec2f bottom_right,
    Vec2f tex_top_left = {0.0f, 1.0f},
    Vec2f tex_bottom_right = {1.0f, 0.0f},
    float tex_z = 0.0f
) {
    v.tex_coord(tex_top_left.x(), tex_top_left.y(), tex_z);
    v.coord(top_left.x(), top_left.y());
    v.tex_coord(tex_top_left.x(), tex_bottom_right.y(), tex_z);
    v.coord(top_left.x(), bottom_right.y());
    v.tex_coord(tex_bottom_right.x(), tex_bottom_right.y(), tex_z);
    v.coord(bottom_right.x(), bottom_right.y());
    v.repeat_vertex(2);
    v.repeat_vertex(0);
    v.tex_coord(tex_bottom_right.x(), tex_top_left.y(), tex_z);
    v.coord(bottom_right.x(), top_left.y());
}

// Clips around the child element.
export class ClipRect: public Element {
public:
    explicit ClipRect(ElementHandle&& child):
        _child(std::move(child)) {}

    auto layout(Context const& ctx, Constraint const& constraint) -> Size override {
        return _size = _child->layout(ctx, constraint);
    }

    void update(Context const& ctx, Point position) override {
        _child->update(ctx, position);
    }

    void render(Context const& ctx, Point position, uint8_t clip_layer) const override {
        auto top_left = position * ctx.scaling_factor;
        auto bottom_right = (position + _size) * ctx.scaling_factor;
        auto next_layer = static_cast<uint8_t>(clip_layer + 1);
        auto v = vertex_builder();
        v.color(0, 0, 0, 0);
        draw_quad(v, top_left, bottom_right);
        auto va = render::VertexArray::create(v, render::VertexArray::Primitive::TRIANGLES);
        glStencilOp(GL_KEEP, GL_KEEP, GL_INCR_WRAP);
        BlockTextureArray.bind(0); // Temporary
        va.first.render();
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
        glStencilFunc(GL_EQUAL, next_layer, 0xFF);
        _child->render(ctx, position, next_layer);
        glStencilOp(GL_KEEP, GL_KEEP, GL_DECR_WRAP);
        BlockTextureArray.bind(0); // Temporary
        va.first.render();
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
        glStencilFunc(GL_EQUAL, clip_layer, 0xFF);
    }

private:
    std::unique_ptr<Element> _child;
    Size _size;
};

}
