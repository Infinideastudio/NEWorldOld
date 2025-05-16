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

// A generic element in the element tree.
// During layout, "constraints go down, sizes go up, parent sets position".
export class Element {
public:
    // Updates positions and sizes in the subtree.
    // Returns the size of the element in logical pixels.
    virtual auto layout(Context const& ctx, Constraint const& constraint) -> Size = 0;

    // Updates subtree.
    virtual void update(Context const& ctx, Point position) = 0;

    // Renders subtree.
    virtual void render(Context const& ctx, Point position, uint8_t clip_layer) const = 0;

    virtual ~Element() = default;

protected:
    Element() = default;
    Element(Element const&) = default;
    Element(Element&&) = default;
    auto operator=(Element const&) -> Element& = default;
    auto operator=(Element&&) -> Element& = default;
};

// A generic dynamic element in the element tree.
// Whenever a state is changed, a builder should be notified (TODO: a simple reactive framework)
// by having its key added to the `keys_to_update` list in the context.
export class Builder: public Element {
public:
    // A builder is constructed from a unique key and a `build` function.
    template <typename F>
    requires std::invocable<F, Key>
    explicit Builder(Key key, F build):
        _key(key),
        _build([f = std::move(build)](Key key) {
            using T = decltype(std::invoke(f, key));
            static_assert(std::derived_from<T, Element>, "build function should return an Element");
            return std::make_unique<T>(std::invoke(f, key));
        }) {}

    // Only computes layout if the child element has been built.
    auto layout(Context const& ctx, Constraint const& constraint) -> Size override {
        return _child ? _child->layout(ctx, constraint) : Size();
    }

    // The child element is rebuilt if this is the first run or the key is updated.
    void update(Context const& ctx, Point position) override {
        if (!_child || ctx.updated(_key)) {
            _child = _build(_key);
        }
        _child->update(ctx, position);
    }

    // Only renders if the child element has been built.
    void render(Context const& ctx, Point position, uint8_t clip_layer) const override {
        return _child ? _child->render(ctx, position, clip_layer) : void();
    }

private:
    Key _key;
    std::function<auto(Key)->std::unique_ptr<Element>> _build;
    std::unique_ptr<Element> _child;
};

// The root of the element tree.
// It bootstraps the update and rendering processes.
export class View {
public:
    template <typename T>
    requires std::derived_from<T, Element>
    explicit View(T&& child):
        _child(std::make_unique<T>(std::forward<T>(child))) {}

    // Updates the element tree.
    void update(Context& ctx) {
        ctx.refresh_keys();
        auto should_relayout =
            true; // ctx.has_updated_keys() || ctx.view_size != _view_size || ctx.scaling_factor != _scaling_factor;
        _child->update(ctx, Point());
        if (should_relayout) {
            _view_size = ctx.view_size;
            _scaling_factor = ctx.scaling_factor;
            _child->layout(ctx, Constraint(_view_size / _scaling_factor));
        }
    }

    // Renders the element tree.
    void render(Context const& ctx) {
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

// A convenience wrapper for implicitly converting any `Element` to a `std::unique_ptr<Element>`.
// Useful for constructors that take different types of `Element`s as arguments.
export class ElementHandle {
public:
    template <typename T>
    requires std::derived_from<T, Element>
    ElementHandle(T&& element):
        _element(std::make_unique<T>(std::forward<T>(element))) {}

    operator bool() const noexcept {
        return bool(_element);
    }

    operator std::unique_ptr<Element>() && noexcept {
        return std::move(_element);
    }

private:
    std::unique_ptr<Element> _element;
};

}
