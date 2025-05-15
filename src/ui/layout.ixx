export module ui:layout;
import std;
import types;
import debug;
import :context;
import :element;

namespace ui {

export enum class Alignment {
    TOP_LEFT,
    TOP_CENTER,
    TOP_RIGHT,
    CENTER_LEFT,
    CENTER,
    CENTER_RIGHT,
    BOTTOM_LEFT,
    BOTTOM_CENTER,
    BOTTOM_RIGHT,
};

export enum class FlexDirection {
    ROW,
    ROW_REVERSE,
    COLUMN,
    COLUMN_REVERSE,
};

export enum class MainAxisSize {
    MIN,
    MAX,
};

export enum class MainAxisAlignment {
    START,
    CENTER,
    END,
    SPACE_BETWEEN,
    SPACE_AROUND,
    SPACE_EVENLY,
};

export enum class CrossAxisSize {
    MIN,
    MAX,
};

export enum class CrossAxisAlignment {
    START,
    CENTER,
    END,
};

// Whitespace element.
export class Spacer: public Element {
public:
    struct Args {
        float width = 0.0f;
        float height = 0.0f;
    };

    explicit Spacer(Args args):
        _width(args.width),
        _height(args.height) {}

    auto layout(Context const&, Constraint const& constraint) -> Size override {
        auto width = std::min(_width, constraint.max_width());
        auto height = std::min(_height, constraint.max_height());
        return {width, height};
    }

    void update(Context const&, Point) override {}

    void render(Context const&, Point, uint8_t) const override {}

private:
    float _width;
    float _height;
};

// Puts a constant limit on the size of the given child.
export class Sizer: public Element {
public:
    struct Args {
        float max_width = std::numeric_limits<float>::infinity();
        float max_height = std::numeric_limits<float>::infinity();
    };

    template <typename T>
    requires std::derived_from<T, Element>
    explicit Sizer(Args args, T&& child):
        _max_width(args.max_width),
        _max_height(args.max_height),
        _child(std::make_unique<T>(std::forward<T>(child))) {}

    auto layout(Context const& ctx, Constraint const& constraint) -> Size override {
        auto max_width = std::min(_max_width, constraint.max_width());
        auto max_height = std::min(_max_height, constraint.max_height());
        auto child_size = _child->layout(ctx, Constraint(max_width, max_height));
        return child_size;
    }

    void update(Context const& ctx, Point position) override {
        _child->update(ctx, position);
    }

    void render(Context const& ctx, Point position, uint8_t clip_layer) const override {
        _child->render(ctx, position, clip_layer);
    }

private:
    float _max_width;
    float _max_height;
    std::unique_ptr<Element> _child;
};

// Adds a constant padding around the given child.
export class Padding: public Element {
public:
    struct Args {
        float left = 0.0f;
        float top = 0.0f;
        float right = 0.0f;
        float bottom = 0.0f;
    };

    template <typename T>
    requires std::derived_from<T, Element>
    explicit Padding(Args args, T&& child):
        _left(args.left),
        _top(args.top),
        _right(args.right),
        _bottom(args.bottom),
        _child(std::make_unique<T>(std::forward<T>(child))) {}

    auto layout(Context const& ctx, Constraint const& constraint) -> Size override {
        auto max_width = std::max(0.0f, constraint.max_width() - _left - _right);
        auto max_height = std::max(0.0f, constraint.max_height() - _top - _bottom);
        auto child_size = _child->layout(ctx, Constraint(max_width, max_height));
        auto width = std::min(constraint.max_width(), child_size.width() + _left + _right);
        auto height = std::min(constraint.max_height(), child_size.height() + _top + _bottom);
        return {width, height};
    }

    void update(Context const& ctx, Point position) override {
        _child->update(ctx, Point(position.x() + _left, position.y() + _top));
    }

    void render(Context const& ctx, Point position, uint8_t clip_layer) const override {
        _child->render(ctx, Point(position.x() + _left, position.y() + _top), clip_layer);
    }

private:
    float _left;
    float _top;
    float _right;
    float _bottom;
    std::unique_ptr<Element> _child;
};

// Stacked item wrapper. For use in the constructor of `Stack`.
export template <typename T>
requires std::derived_from<T, Element>
class StackItem {
public:
    struct Args {
        Alignment alignment = Alignment::TOP_LEFT;
        size_t order = 0;
    };

    StackItem(T&& child):
        _child(std::move(child)),
        _alignment(_convert_alignment(Args{}.alignment)),
        _order(Args{}.order) {}

    StackItem(Args args, T&& child):
        _child(std::move(child)),
        _alignment(_convert_alignment(args.alignment)),
        _order(args.order) {}

private:
    T _child;
    Point _alignment;
    size_t _order;

    static constexpr auto _convert_alignment(Alignment alignment) -> Point {
        switch (alignment) {
            case Alignment::TOP_LEFT:
                return {0.0f, 0.0f};
            case Alignment::TOP_CENTER:
                return {0.5f, 0.0f};
            case Alignment::TOP_RIGHT:
                return {1.0f, 0.0f};
            case Alignment::CENTER_LEFT:
                return {0.0f, 0.5f};
            case Alignment::CENTER:
                return {0.5f, 0.5f};
            case Alignment::CENTER_RIGHT:
                return {1.0f, 0.5f};
            case Alignment::BOTTOM_LEFT:
                return {0.0f, 1.0f};
            case Alignment::BOTTOM_CENTER:
                return {0.5f, 1.0f};
            case Alignment::BOTTOM_RIGHT:
                return {1.0f, 1.0f};
            default:
                unreachable();
        }
    }

    friend class Stack;
};

// Stacked container. Later children are drawn on top of earlier ones.
export class Stack: public Element {
public:
    struct Args {};

    template <typename... T>
    explicit Stack(Args, T&&... children) {
        auto entries = std::array{_convert_item(std::forward<T>(children))...};
        std::stable_sort(entries.begin(), entries.end(), [](auto const& a, auto const& b) {
            return a.order < b.order;
        });
        for (auto&& entry: entries) {
            _children.emplace_back(std::move(entry));
        }
    }

    auto layout(Context const& ctx, Constraint const& constraint) -> Size override {
        auto size = Size(constraint);
        for (auto& entry: _children) {
            auto child_size = entry.child->layout(ctx, constraint);
            entry.position = Point((size - child_size) * entry.alignment);
        }
        return size;
    }

    void update(Context const& ctx, Point position) override {
        for (auto const& entry: _children) {
            entry.child->update(ctx, Point(position + entry.position));
        }
    }

    void render(Context const& ctx, Point position, uint8_t clip_layer) const override {
        for (auto const& entry: _children) {
            entry.child->render(ctx, Point(position + entry.position), clip_layer);
        }
    }

private:
    struct Entry {
        std::unique_ptr<Element> child;
        Point position;
        Point alignment;
        size_t order;
    };

    std::vector<Entry> _children;

    // Converts a `StackItem<T>` to an `Entry`.
    template <typename T>
    static auto _convert_item(StackItem<T>&& item) -> Entry { // NOLINT
        return {
            .child = std::make_unique<T>(std::move(item._child)),
            .position = Point(),
            .alignment = item._alignment,
            .order = item._order
        };
    }

    template <typename T>
    static auto _convert_item(T&& item) -> Entry {
        return _convert_item(StackItem<T>(std::forward<T>(item)));
    }
};

// Aligns a single child element within available space.
export class Align: public Stack {
public:
    struct Args {
        Alignment alignment = Alignment::TOP_LEFT;
    };

    template <typename T>
    requires std::derived_from<T, Element>
    explicit Align(Args args, T&& child):
        Stack(StackItem{{.alignment = args.alignment}, std::forward<T>(child)}) {}
};

// Centers the given child element within available space.
export class Center: public Stack {
public:
    template <typename T>
    requires std::derived_from<T, Element>
    explicit Center(T&& child):
        Stack(StackItem{{.alignment = Alignment::CENTER}, std::forward<T>(child)}) {}
};

// Flexible box item wrapper. For use in the constructor of `Flex`.
export template <typename T>
requires std::derived_from<T, Element>
class FlexItem {
public:
    struct Args {
        float flex_grow = 0.0f;
        size_t order = 0;
    };

    FlexItem(T&& child):
        _child(std::move(child)),
        _flex_grow(Args{}.flex_grow),
        _order(Args{}.order) {}

    FlexItem(Args args, T&& child):
        _child(std::move(child)),
        _flex_grow(args.flex_grow),
        _order(args.order) {}

private:
    T _child;
    float _flex_grow;
    size_t _order;

    friend class Flex;
};

// Flexible box container. Does not support wrap-around.
export class Flex: public Element {
public:
    struct Args {
        FlexDirection direction = FlexDirection::ROW;
        MainAxisSize main_axis_size = MainAxisSize::MIN;
        MainAxisAlignment main_axis_alignment = MainAxisAlignment::START;
        CrossAxisSize cross_axis_size = CrossAxisSize::MIN;
        CrossAxisAlignment cross_axis_alignment = CrossAxisAlignment::START;
    };

    template <typename... T>
    explicit Flex(Args args, T&&... children):
        _direction(args.direction),
        _main_axis_size(args.main_axis_size),
        _main_axis_alignment(args.main_axis_alignment),
        _cross_axis_size(args.cross_axis_size),
        _cross_axis_alignment(args.cross_axis_alignment) {
        auto entries = std::array{_convert_item(std::forward<T>(children))...};
        std::stable_sort(entries.begin(), entries.end(), [](auto const& a, auto const& b) {
            return a.order < b.order;
        });
        for (auto&& entry: entries) {
            _children.emplace_back(std::move(entry));
        }
    }

    // See: https://api.flutter.dev/flutter/widgets/Flex-class.html
    auto layout(Context const& ctx, Constraint const& constraint) -> Size override {
        auto swapped_constraint = _swap_if_vertical(constraint);
        auto max_length = swapped_constraint.max_width();
        auto max_height = swapped_constraint.max_height();
        // First component is main axis, second is cross axis.
        auto child_sizes = std::vector<Size>(_children.size());
        // Layout all non-expanding children with unbounded main axis constraint.
        auto sum_child_width = 0.0f;
        auto max_child_height = 0.0f;
        auto sum_flex_grow = 0.0f;
        for (auto i = 0uz; i < _children.size(); ++i) {
            auto const& child = _children[i];
            if (child.flex_grow == 0.0f) {
                auto child_constraint = Constraint(std::numeric_limits<float>::infinity(), max_height);
                child_sizes[i] = _swap_if_vertical(child.child->layout(ctx, _swap_if_vertical(child_constraint)));
                sum_child_width += child_sizes[i].width();
                max_child_height = std::max(max_child_height, child_sizes[i].height());
            }
            sum_flex_grow += child.flex_grow;
        }
        // Divide the remaining space among the expanding children.
        // If there is at least one expanding child, all remaining space will be allocated.
        auto remaining_length = std::max(0.0f, max_length - sum_child_width);
        for (auto i = 0uz; i < _children.size(); ++i) {
            auto const& child = _children[i];
            if (child.flex_grow != 0.0f) {
                auto child_constraint = Constraint(remaining_length * (child.flex_grow / sum_flex_grow), max_height);
                child_sizes[i] = _swap_if_vertical(child.child->layout(ctx, _swap_if_vertical(child_constraint)));
                sum_child_width += child_sizes[i].width();
                max_child_height = std::max(max_child_height, child_sizes[i].height());
            }
        }
        // Calculate self size.
        auto width = (_main_axis_size == MainAxisSize::MIN) ? std::min(sum_child_width, max_length) : max_length;
        auto height = (_cross_axis_size == CrossAxisSize::MIN) ? std::min(max_child_height, max_height) : max_height;
        // Calculate all child positions.
        // If there is remaining space but no expanding children, add spacing.
        auto main_axis_spacing = 0.0f;
        auto main_axis_position = 0.0f;
        switch (_main_axis_alignment) {
            case MainAxisAlignment::START:
                break;
            case MainAxisAlignment::CENTER:
                main_axis_position = remaining_length / 2.0f;
                break;
            case MainAxisAlignment::END:
                main_axis_spacing = remaining_length;
                break;
            case MainAxisAlignment::SPACE_BETWEEN:
                main_axis_spacing = remaining_length / static_cast<float>(_children.size() - 1);
                break;
            case MainAxisAlignment::SPACE_AROUND:
                main_axis_spacing = remaining_length / static_cast<float>(_children.size());
                main_axis_position = main_axis_spacing / 2.0f;
                break;
            case MainAxisAlignment::SPACE_EVENLY:
                main_axis_spacing = remaining_length / static_cast<float>(_children.size() + 1);
                main_axis_position = main_axis_spacing;
                break;
            default:
                unreachable();
        }
        for (auto i = 0uz; i < _children.size(); ++i) {
            auto& child = _children[i];
            auto cross_axis_position = 0.0f;
            switch (_cross_axis_alignment) {
                case CrossAxisAlignment::START:
                    break;
                case CrossAxisAlignment::CENTER:
                    cross_axis_position = (height - child_sizes[i].height()) / 2.0f;
                    break;
                case CrossAxisAlignment::END:
                    cross_axis_position = height - child_sizes[i].height();
                    break;
                default:
                    unreachable();
            }
            child.position = Point(main_axis_position, cross_axis_position);
            if (_flipped_main_axis(_direction)) {
                child.position.x() = width - child.position.x() - child_sizes[i].width();
            }
            main_axis_position += child_sizes[i].width() + main_axis_spacing;
        }
        // Return the size of this element.
        return _swap_if_vertical(Size(width, height));
    }

    void update(Context const& ctx, Point position) override {
        for (auto const& entry: _children) {
            entry.child->update(ctx, Point(position + _swap_if_vertical(entry.position)));
        }
    }

    void render(Context const& ctx, Point position, uint8_t clip_layer) const override {
        for (auto const& entry: _children) {
            entry.child->render(ctx, Point(position + _swap_if_vertical(entry.position)), clip_layer);
        }
    }

private:
    struct Entry {
        std::unique_ptr<Element> child;
        Point position;
        float flex_grow;
        size_t order;
    };

    FlexDirection _direction;
    MainAxisSize _main_axis_size;
    MainAxisAlignment _main_axis_alignment;
    CrossAxisSize _cross_axis_size;
    CrossAxisAlignment _cross_axis_alignment;
    std::vector<Entry> _children;

    static constexpr auto _vertical_main_axis(FlexDirection direction) -> bool {
        switch (direction) {
            case FlexDirection::ROW:
            case FlexDirection::ROW_REVERSE:
                return false;
            case FlexDirection::COLUMN:
            case FlexDirection::COLUMN_REVERSE:
                return true;
            default:
                unreachable();
        }
    }

    static constexpr auto _flipped_main_axis(FlexDirection direction) -> bool {
        switch (direction) {
            case FlexDirection::ROW:
            case FlexDirection::COLUMN:
                return false;
            case FlexDirection::ROW_REVERSE:
            case FlexDirection::COLUMN_REVERSE:
                return true;
            default:
                unreachable();
        }
    }

    // Swaps two components if the main axis is vertical.
    auto _swap_if_vertical(Point point) const -> Point {
        if (_vertical_main_axis(_direction)) {
            std::swap(point.x(), point.y());
        }
        return point;
    }

    // Swaps two components if the main axis is vertical.
    auto _swap_if_vertical(Size size) const -> Size {
        if (_vertical_main_axis(_direction)) {
            std::swap(size.width(), size.height());
        }
        return size;
    }

    // Swaps two components if the main axis is vertical.
    auto _swap_if_vertical(Constraint constraint) const -> Constraint {
        if (_vertical_main_axis(_direction)) {
            std::swap(constraint.max_width(), constraint.max_height());
        }
        return constraint;
    }

    // Converts a `FlexItem<T>` to an `Entry`.
    template <typename T>
    static auto _convert_item(FlexItem<T>&& item) -> Entry { // NOLINT
        return {
            .child = std::make_unique<T>(std::move(item._child)),
            .position = Point(),
            .flex_grow = item._flex_grow,
            .order = item._order
        };
    }

    template <typename T>
    static auto _convert_item(T&& item) -> Entry {
        return _convert_item(FlexItem<T>(std::forward<T>(item)));
    }
};

// Horizontal flexible box container.
export class Row: public Flex {
public:
    struct Args {
        MainAxisSize main_axis_size = MainAxisSize::MIN;
        MainAxisAlignment main_axis_alignment = MainAxisAlignment::START;
        CrossAxisSize cross_axis_size = CrossAxisSize::MIN;
        CrossAxisAlignment cross_axis_alignment = CrossAxisAlignment::START;
    };

    template <typename... T>
    explicit Row(Args args, T&&... children):
        Flex(
            Flex::Args{
                .direction = FlexDirection::ROW,
                .main_axis_size = args.main_axis_size,
                .main_axis_alignment = args.main_axis_alignment,
                .cross_axis_size = args.cross_axis_size,
                .cross_axis_alignment = args.cross_axis_alignment,
            },
            std::forward<T>(children)...
        ) {}
};

// Vertical flexible box container.
export class Column: public Flex {
public:
    struct Args {
        MainAxisSize main_axis_size = MainAxisSize::MIN;
        MainAxisAlignment main_axis_alignment = MainAxisAlignment::START;
        CrossAxisSize cross_axis_size = CrossAxisSize::MIN;
        CrossAxisAlignment cross_axis_alignment = CrossAxisAlignment::START;
    };

    template <typename... T>
    explicit Column(Args args, T&&... children):
        Flex(
            Flex::Args{
                .direction = FlexDirection::COLUMN,
                .main_axis_size = args.main_axis_size,
                .main_axis_alignment = args.main_axis_alignment,
                .cross_axis_size = args.cross_axis_size,
                .cross_axis_alignment = args.cross_axis_alignment,
            },
            std::forward<T>(children)...
        ) {}
};

}
