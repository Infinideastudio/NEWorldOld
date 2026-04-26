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

export constexpr auto alignment_to_fractions(Alignment alignment) -> Point {
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

export enum class BoxFit {
    NONE,
    FILL,
    CONTAIN,
    COVER,
    FIT_WIDTH,
    FIT_HEIGHT,
};

// Returns (fitted size of inner element, fitted size of container element).
// The fitted sizes refer to that of the corresponding areas of the inner and container elements.
// See: https://api.flutter.dev/flutter/painting/BoxFit.html
export constexpr auto apply_box_fit(BoxFit fit, Size inner_size, Size container_size) -> std::pair<Size, Size> {
    switch (fit) {
        case BoxFit::NONE: {
            inner_size.width() = std::min(inner_size.width(), container_size.width());
            inner_size.height() = std::min(inner_size.height(), container_size.height());
            return {inner_size, inner_size};
        }
        case BoxFit::FILL: {
            return {inner_size, container_size};
        }
        case BoxFit::CONTAIN: {
            auto scale =
                std::min(container_size.width() / inner_size.width(), container_size.height() / inner_size.height());
            return {inner_size, Size(inner_size * scale)};
        }
        case BoxFit::COVER: {
            auto scale =
                std::max(container_size.width() / inner_size.width(), container_size.height() / inner_size.height());
            return {Size(container_size / scale), container_size};
        }
        case BoxFit::FIT_WIDTH: {
            auto scale = container_size.width() / inner_size.width();
            auto width = inner_size.width();
            auto height = std::min(inner_size.height(), container_size.height() / scale);
            return {Size(width, height), Size(width * scale, height * scale)};
        }
        case BoxFit::FIT_HEIGHT: {
            auto scale = container_size.height() / inner_size.height();
            auto width = std::min(inner_size.width(), container_size.width() / scale);
            auto height = inner_size.height();
            return {Size(width, height), Size(width * scale, height * scale)};
        }
        default:
            unreachable();
    }
}

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

    void update(Context&, Point) override {}

    void render(Context&, Point, uint8_t) const override {}

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

    explicit Sizer(Args args, ElementHandle&& child):
        _max_width(args.max_width),
        _max_height(args.max_height),
        _child(std::move(child)) {}

    auto layout(Context const& ctx, Constraint const& constraint) -> Size override {
        auto max_width = std::min(_max_width, constraint.max_width());
        auto max_height = std::min(_max_height, constraint.max_height());
        auto child_size = _child->layout(ctx, Constraint(max_width, max_height));
        return child_size;
    }

    void update(Context& ctx, Point position) override {
        _child->update(ctx, position);
    }

    void render(Context& ctx, Point position, uint8_t clip_layer) const override {
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

    explicit Padding(Args args, ElementHandle&& child):
        _left(args.left),
        _top(args.top),
        _right(args.right),
        _bottom(args.bottom),
        _child(std::move(child)) {}

    auto layout(Context const& ctx, Constraint const& constraint) -> Size override {
        auto max_width = std::max(0.0f, constraint.max_width() - _left - _right);
        auto max_height = std::max(0.0f, constraint.max_height() - _top - _bottom);
        auto child_size = _child->layout(ctx, Constraint(max_width, max_height));
        auto width = std::min(constraint.max_width(), child_size.width() + _left + _right);
        auto height = std::min(constraint.max_height(), child_size.height() + _top + _bottom);
        return {width, height};
    }

    void update(Context& ctx, Point position) override {
        _child->update(ctx, Point(position.x() + _left, position.y() + _top));
    }

    void render(Context& ctx, Point position, uint8_t clip_layer) const override {
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
export class StackItem {
public:
    struct Args {
        Alignment alignment = Alignment::TOP_LEFT;
        size_t order = 0;
    };

    StackItem(ElementHandle&& child):
        _child(std::move(child)),
        _alignment(alignment_to_fractions(Args{}.alignment)),
        _order(Args{}.order) {}

    StackItem(Args args, ElementHandle&& child):
        _child(std::move(child)),
        _alignment(alignment_to_fractions(args.alignment)),
        _order(args.order) {}

private:
    std::unique_ptr<Element> _child;
    Point _alignment;
    size_t _order;
    Point _position = Point();

    friend class Stack;
};

// Since `std::initializer_list` cannot be moved from, we have to construct a vector from a parameter pack.
template <typename... T>
auto make_vector_of_stack_items(T... items) -> std::vector<StackItem> {
    auto arr = std::array{StackItem(std::forward<T>(items))...};
    auto res = std::vector<StackItem>();
    for (auto&& item: arr) {
        res.push_back(std::move(item));
    }
    return res;
}

// Stacked container. Later children are drawn on top of earlier ones.
export class Stack: public Element {
public:
    struct Args {};

    template <typename... T>
    explicit Stack(Args args, T... items):
        Stack(args, make_vector_of_stack_items(std::forward<T>(items)...)) {}

    explicit Stack(Args args, std::vector<StackItem> items):
        _items(std::move(items)) {
        std::ranges::stable_sort(_items, [](auto const& a, auto const& b) { return a._order < b._order; });
    }

    auto layout(Context const& ctx, Constraint const& constraint) -> Size override {
        auto size = Size(constraint);
        for (auto& item: _items) {
            auto child_size = item._child->layout(ctx, constraint);
            item._position = Point((size - child_size) * item._alignment);
        }
        return size;
    }

    void update(Context& ctx, Point position) override {
        for (auto const& item: _items) {
            item._child->update(ctx, Point(position + item._position));
        }
    }

    void render(Context& ctx, Point position, uint8_t clip_layer) const override {
        for (auto const& item: _items) {
            item._child->render(ctx, Point(position + item._position), clip_layer);
        }
    }

private:
    std::vector<StackItem> _items;
};

// Aligns a single child element within available space.
export class Align: public Stack {
public:
    struct Args {
        Alignment alignment = Alignment::TOP_LEFT;
    };

    explicit Align(Args args, ElementHandle&& child):
        Stack({}, StackItem({.alignment = args.alignment}, std::move(child))) {}
};

// Centers the given child element within available space.
export class Center: public Stack {
public:
    struct Args {};

    explicit Center(Args, ElementHandle&& child):
        Stack({}, StackItem({.alignment = Alignment::CENTER}, std::move(child))) {}
};

// Flexible box item wrapper. For use in the constructor of `Flex`.
export class FlexItem {
public:
    struct Args {
        float flex_grow = 0.0f;
        size_t order = 0;
    };

    FlexItem(ElementHandle&& child):
        _child(std::move(child)),
        _flex_grow(Args{}.flex_grow),
        _order(Args{}.order) {}

    FlexItem(Args args, ElementHandle&& child):
        _child(std::move(child)),
        _flex_grow(args.flex_grow),
        _order(args.order) {}

private:
    std::unique_ptr<Element> _child;
    float _flex_grow;
    size_t _order;
    Point _position = Point();

    friend class Flex;
};

// Since `std::initializer_list` cannot be moved from, we have to construct a vector from a parameter pack.
template <typename... T>
auto make_vector_of_flex_items(T... items) -> std::vector<FlexItem> {
    auto arr = std::array{FlexItem(std::forward<T>(items))...};
    auto res = std::vector<FlexItem>();
    for (auto&& item: arr) {
        res.push_back(std::move(item));
    }
    return res;
}

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
    explicit Flex(Args args, T... items):
        Flex(args, make_vector_of_flex_items(std::forward<T>(items)...)) {}

    explicit Flex(Args args, std::vector<FlexItem> items):
        _direction(args.direction),
        _main_axis_size(args.main_axis_size),
        _main_axis_alignment(args.main_axis_alignment),
        _cross_axis_size(args.cross_axis_size),
        _cross_axis_alignment(args.cross_axis_alignment),
        _items(std::move(items)) {
        std::ranges::stable_sort(_items, [](auto const& a, auto const& b) { return a._order < b._order; });
    }

    // See: https://api.flutter.dev/flutter/widgets/Flex-class.html
    auto layout(Context const& ctx, Constraint const& constraint) -> Size override {
        auto swapped_constraint = _swap_if_vertical(constraint);
        auto max_width = swapped_constraint.max_width();
        auto max_height = swapped_constraint.max_height();
        // First component is main axis, second is cross axis.
        auto child_sizes = std::vector<Size>(_items.size());
        // Layout all non-expanding children with unbounded main axis constraint.
        auto sum_child_width = 0.0f;
        auto max_child_height = 0.0f;
        auto sum_flex_grow = 0.0f;
        for (auto i = 0uz; i < _items.size(); ++i) {
            auto const& item = _items[i];
            if (item._flex_grow == 0.0f) {
                auto child_constraint = Constraint(std::numeric_limits<float>::infinity(), max_height);
                child_sizes[i] = _swap_if_vertical(item._child->layout(ctx, _swap_if_vertical(child_constraint)));
                sum_child_width += child_sizes[i].width();
                max_child_height = std::max(max_child_height, child_sizes[i].height());
            }
            sum_flex_grow += item._flex_grow;
        }
        // Divide the remaining space among the expanding children.
        // If there is at least one expanding child, all remaining space will be allocated.
        auto remaining_length = std::max(0.0f, max_width - sum_child_width);
        for (auto i = 0uz; i < _items.size(); ++i) {
            auto const& item = _items[i];
            if (item._flex_grow != 0.0f) {
                auto child_constraint = Constraint(remaining_length * (item._flex_grow / sum_flex_grow), max_height);
                child_sizes[i] = _swap_if_vertical(item._child->layout(ctx, _swap_if_vertical(child_constraint)));
                sum_child_width += child_sizes[i].width();
                max_child_height = std::max(max_child_height, child_sizes[i].height());
            }
        }
        // Calculate self size.
        auto width = (_main_axis_size == MainAxisSize::MIN) ? std::min(sum_child_width, max_width) : max_width;
        auto height = (_cross_axis_size == CrossAxisSize::MIN) ? std::min(max_child_height, max_height) : max_height;
        // Calculate all child positions.
        // If there is remaining space but no expanding children, add spacing.
        auto main_axis_spacing = 0.0f;
        auto main_axis_position = 0.0f;
        switch (_main_axis_alignment) {
            case MainAxisAlignment::START:
                break;
            case MainAxisAlignment::CENTER:
                main_axis_position = (width - sum_child_width) / 2.0f;
                break;
            case MainAxisAlignment::END:
                main_axis_spacing = (width - sum_child_width);
                break;
            case MainAxisAlignment::SPACE_BETWEEN:
                main_axis_spacing = (width - sum_child_width) / static_cast<float>(_items.size() - 1);
                break;
            case MainAxisAlignment::SPACE_AROUND:
                main_axis_spacing = (width - sum_child_width) / static_cast<float>(_items.size());
                main_axis_position = main_axis_spacing / 2.0f;
                break;
            case MainAxisAlignment::SPACE_EVENLY:
                main_axis_spacing = (width - sum_child_width) / static_cast<float>(_items.size() + 1);
                main_axis_position = main_axis_spacing;
                break;
            default:
                unreachable();
        }
        for (auto i = 0uz; i < _items.size(); ++i) {
            auto& item = _items[i];
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
            item._position = Point(main_axis_position, cross_axis_position);
            if (_flipped_main_axis(_direction)) {
                item._position.x() = width - item._position.x() - child_sizes[i].width();
            }
            main_axis_position += child_sizes[i].width() + main_axis_spacing;
        }
        // Return the size of this element.
        return _swap_if_vertical(Size(width, height));
    }

    void update(Context& ctx, Point position) override {
        for (auto const& item: _items) {
            item._child->update(ctx, Point(position + _swap_if_vertical(item._position)));
        }
    }

    void render(Context& ctx, Point position, uint8_t clip_layer) const override {
        for (auto const& item: _items) {
            item._child->render(ctx, Point(position + _swap_if_vertical(item._position)), clip_layer);
        }
    }

private:
    FlexDirection _direction;
    MainAxisSize _main_axis_size;
    MainAxisAlignment _main_axis_alignment;
    CrossAxisSize _cross_axis_size;
    CrossAxisAlignment _cross_axis_alignment;
    std::vector<FlexItem> _items;

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
    explicit Row(Args args, T... items):
        Row(args, make_vector_of_flex_items(std::forward<T>(items)...)) {}

    explicit Row(Args args, std::vector<FlexItem>&& items):
        Flex(
            Flex::Args{
                .direction = FlexDirection::ROW,
                .main_axis_size = args.main_axis_size,
                .main_axis_alignment = args.main_axis_alignment,
                .cross_axis_size = args.cross_axis_size,
                .cross_axis_alignment = args.cross_axis_alignment,
            },
            std::move(items)
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
    explicit Column(Args args, T... items):
        Column(args, make_vector_of_flex_items(std::forward<T>(items)...)) {}

    explicit Column(Args args, std::vector<FlexItem>&& items):
        Flex(
            Flex::Args{
                .direction = FlexDirection::COLUMN,
                .main_axis_size = args.main_axis_size,
                .main_axis_alignment = args.main_axis_alignment,
                .cross_axis_size = args.cross_axis_size,
                .cross_axis_alignment = args.cross_axis_alignment,
            },
            std::move(items)
        ) {}
};

}
