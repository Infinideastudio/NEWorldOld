module;

#include <glad/gl.h>

export module ui:element;
import std;
import types;
import debug;
import math;
import :context;

namespace ui {

// Maximum width and height in logical pixels.
export class Constraint: public Vec2f {
public:
    explicit Constraint(Vec2f v):
        Vec2f(v) {}

    Constraint():
        Vec2f(std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity()) {}
    Constraint(float max_width, float max_height):
        Vec2f(max_width, max_height) {}

    auto max_width() const -> float {
        return x();
    }
    auto max_width() -> float& {
        return x();
    }
    auto max_height() const -> float {
        return y();
    }
    auto max_height() -> float& {
        return y();
    }
};

// A generic element in the widget tree.
// During layout, "constraints go down, sizes go up, parent sets position".
export class Element {
public:
    Element() = default;

    // Updates positions and sizes in the subtree.
    // Returns the size of the element in logical pixels.
    virtual auto layout(Context const& ctx, Constraint const& constraint) -> Size = 0;

    // Updates subtree.
    virtual void update(Context const& ctx, Point position) = 0;

    // Renders subtree.
    virtual void render(Context const& ctx, Point position, uint8_t clip_layer) const = 0;

    virtual ~Element() = default;

protected:
    Element(Element const&) = default;
    Element(Element&&) = default;
    auto operator=(Element const&) -> Element& = default;
    auto operator=(Element&&) -> Element& = default;
};

// The root of the element tree.
export class View {
public:
    template <typename T>
    explicit View(T&& child):
        _child(std::make_unique<T>(std::forward<T>(child))) {}

    // Updates the element tree.
    void update(Context const& ctx) {
        if (ctx.view_size != _view_size || ctx.scaling_factor != _scaling_factor) {
            _view_size = ctx.view_size;
            _scaling_factor = ctx.scaling_factor;
            _child->layout(ctx, Constraint(_view_size / _scaling_factor));
        }
        _child->update(ctx, Point());
    }

    // Renders the element tree.
    void render(Context const& ctx) {
        if (ctx.view_size != _view_size || ctx.scaling_factor != _scaling_factor) {
            _view_size = ctx.view_size;
            _scaling_factor = ctx.scaling_factor;
            _child->layout(ctx, Constraint(_view_size / _scaling_factor));
        }
        auto clip_layer = uint8_t{0};
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
        glStencilFunc(GL_EQUAL, clip_layer, 0xFF);
        _child->render(ctx, Point(), clip_layer);
    }

private:
    Size _view_size = {0.0f, 0.0f};
    float _scaling_factor = 1.0f;
    std::unique_ptr<Element> _child;
};

}
