// Another previous impl for reference

/*
namespace ui {

class HScroll: public Control {
public:
    HScroll() = default;
    HScroll(
        Position const& ul,
        Position const& lr,
        double length_,
        double position_ = 0.0,
        double unit_ = 0.0,
        bool focusable = true
    ):
        Control(ul, lr, focusable),
        length(length_),
        position(position_),
        unit(unit_) {}
    double length = 0, position = 0, unit = 0;

    bool mouseHover() const {
        return mHover;
    }
    bool modified() const {
        return mModified;
    }

private:
    bool mHover = false, mSelecting = false, mModified = false;
    bool mLeftHover = false, mRightHover = false, mLeftPressed = false, mRightPressed = false;
    double mMouseOffset = 0;

    void update(Point2D const& ul, Point2D const& lr, Form& form) override;
    void render(Point2D const& ul, Point2D const& lr, Form const& form) const override;
};

class VScroll: public Control {
public:
    VScroll() = default;
    VScroll(
        Position const& ul,
        Position const& lr,
        double length_,
        double position_ = 0.0,
        double unit_ = 0.0,
        bool focusable = true
    ):
        Control(ul, lr, focusable),
        length(length_),
        position(position_),
        unit(unit_) {}
    double length = 0, position = 0, unit = 0;

    bool mouseHover() const {
        return mHover;
    }
    bool modified() const {
        return mModified;
    }

private:
    bool mHover = false, mSelecting = false, mModified = false;
    bool mUpHover = false, mDownHover = false, mUpPressed = false, mDownPressed = false;
    double mMouseOffset = 0;

    void update(Point2D const& ul, Point2D const& lr, Form& form) override;
    void render(Point2D const& ul, Point2D const& lr, Form const& form) const override;
};

class ScrollArea: public Control {
public:
    ScrollArea() = default;
    ScrollArea(
        Position const& ul,
        Position const& lr,
        Point2D const& size_,
        Point2D const& position_,
        bool draggable_ = false,
        bool scalable_ = false,
        bool focusable = false
    );
    Point2D size, position;
    float scale = 1.0f;
    bool draggable = false, scalable = false;

    void addChild(Control* c) override {
        mContent.addChild(c);
    }
    void addChild(std::initializer_list<Control*> c) override {
        mContent.addChild(c);
    }
    std::vector<Control*>& children() override {
        return mContent.children();
    }

private:
    ClipArea mContent, mView;
    HScroll mH;
    VScroll mV;
    bool mDragging = false, mScaling = false;

    void update(Point2D const& ul, Point2D const& lr, Form& form) override;
};

// Background transparency is not supported with subpixel font antialiasing enabled!

// Default sizes (when ScalingFactor = 1)
int const TrackBarWidth1 = 10;
int const ScrollButtonSize1 = 18;
int const MinScrollLength1 = 20;
int const LineWidth1 = 2;
int const PictureBoxBorderWidth1 = 5;
// Scaled sizes
int TrackBarWidth = TrackBarWidth1;
int ScrollButtonSize = ScrollButtonSize1;
int MinScrollLength = MinScrollLength1;
int LineWidth = LineWidth1;
int PictureBoxBorderWidth = PictureBoxBorderWidth1;
// Relative & fixed sizes
double const DefaultUnitFraction = 0.1; // Scroll unit length / viewport length
int const DefaultHScrollWidth = 18;
int const DefaultVScrollWidth = 18;

void TrackBar::render(Point2D const& ul, Point2D const& lr, Form const&) const {
    int hw = TrackBarWidth / 2, xmin = ul.x + hw, xmax = lr.x - hw;
    int xsel = (value - lower) / (upper - lower) * (xmax - xmin) + xmin;
    VertexArray va(120, VertexFormat(0, 4, 0, 3));
    // Background quad
    va.setColor(4, mSelecting ? BackColor1 : (mHover ? BackColor0 : BackColor1));
    drawQuad(va, ul.x, ul.y + LineWidth, lr.x, lr.y - LineWidth);
    VertexBuffer(va).render();
    // Text
    drawTextCentered(ul, lr, text, TextColor, mSelecting ? BackColor1 : (mHover ? BackColor0 : BackColor1));
    // Foreground Quad
    va.clear();
    va.setColor(4, (mButtonHover || mSelecting) ? ButtonColor1 : ButtonColor0);
    drawQuad(va, float(xsel - hw), ul.y, float(xsel + hw), lr.y);
    va.setColor(4, mSelecting ? ButtonColor1 : ButtonColor0);
    drawQuad(va, float(xsel - hw) + LineWidth, ul.y + LineWidth, float(xsel + hw) - LineWidth, lr.y - LineWidth);
    VertexBuffer(va).render();
}

void HScroll::update(Point2D const& ul, Point2D const& lr, Form& form) {
    mHover = mModified = false;
    mLeftHover = mLeftPressed = mRightHover = mRightPressed = false;
    double oldPosition = position;
    // Areas
    Point2D ulsa = ul + Point2D(ScrollButtonSize, 0), lrsa = lr - Point2D(ScrollButtonSize, 0); // Scroll area
    float len = std::max((lrsa.x - ulsa.x) * float(length), float(MinScrollLength));
    float left = ulsa.x + (lrsa.x - ulsa.x - len) * position;
    Point2D uls = Point2D(left, ulsa.y), lrs = Point2D(left + len, lrsa.y); // Scroll bar
    Point2D ulleft = ul, lrleft = Point2D(ul.x + ScrollButtonSize, lr.y);   // Left button
    Point2D ulright = Point2D(lr.x - ScrollButtonSize, ul.y), lrright = lr; // Right button
    // Begin selection
    if (focusable && form.mousePosition() >= uls && form.mousePosition() <= lrs) {
        mHover = true;
        if (form.mouseLeftDown()) {
            getFocus(form), mSelecting = true;
            mMouseOffset = form.mousePosition().x - left;
        }
    }
    // Update selection
    if (form.mouseLeftHeld() && mSelecting) {
        left = form.mousePosition().x - mMouseOffset;
        if (lrsa.x - ulsa.x - len > 0.0)
            position = (left - ulsa.x) / (lrsa.x - ulsa.x - len);
    }
    if (!form.mouseLeftPressed())
        mSelecting = false;
    // Update buttons
    double delta = (unit > 0.0 ? unit : length * DefaultUnitFraction);
    if (focusable && form.mousePosition() >= ulleft && form.mousePosition() <= lrleft) {
        mLeftHover = true;
        if (form.mouseLeftDown()) {
            getFocus(form);
            if (length < 1.0)
                position -= delta / (1.0 - length);
        }
        if (form.mouseLeftPressed())
            mLeftPressed = true;
    }
    if (focusable && form.mousePosition() >= ulright && form.mousePosition() <= lrright) {
        mRightHover = true;
        if (form.mouseLeftDown()) {
            getFocus(form);
            if (length < 1.0)
                position += delta / (1.0 - length);
        }
        if (form.mouseLeftPressed())
            mRightPressed = true;
    }
    position = std::min(std::max(position, 0.0), 1.0);
    mModified = (position != oldPosition);
}

void HScroll::render(Point2D const& ul, Point2D const& lr, Form const&) const {
    // Areas
    Point2D ulsa = ul + Point2D(ScrollButtonSize, 0), lrsa = lr - Point2D(ScrollButtonSize, 0); // Scroll area
    float len = std::max((lrsa.x - ulsa.x) * float(length), float(MinScrollLength));
    float left = ulsa.x + (lrsa.x - ulsa.x - len) * position;
    Point2D uls = Point2D(left, ulsa.y), lrs = Point2D(left + len, lrsa.y); // Scroll bar
    Point2D ulleft = ul, lrleft = Point2D(ul.x + ScrollButtonSize, lr.y);   // Left button
    Point2D ulright = Point2D(lr.x - ScrollButtonSize, ul.y), lrright = lr; // Right button
    // Render
    VertexArray va(120, VertexFormat(0, 4, 0, 3));
    // Background quad
    va.setColor(4, BackColor1);
    drawQuad(va, ul.x, ul.y, lr.x, lr.y);
    // Border
    va.setColor(4, (mHover || mSelecting) ? ButtonColor1 : ButtonColor0);
    drawQuad(va, float(left), ul.y, float(left + len), lr.y);
    // Foreground quad
    va.setColor(4, mSelecting ? ButtonColor1 : ButtonColor0);
    drawQuad(va, float(left) + LineWidth, ul.y + LineWidth, float(left + len) - LineWidth, lr.y - LineWidth);
    // Left Button
    if (mLeftHover) {
        va.setColor(4, BackColor1);
        drawQuad(va, ulleft.x, ulleft.y, lrleft.x, lrleft.y);
    }
    Point2D center = ((ulleft + lrleft) / 2.0f).round() + Point2D(0.0f, 0.0f);
    va.setColor(4, mLeftPressed ? ButtonColor1 : ButtonColor0);
    va.addVertex({center.x - 3, center.y + 1});
    va.addVertex({center.x, center.y + 1});
    va.addVertex({center.x + 4, center.y + 1 - 4});
    va.addVertex({center.x - 3, center.y + 1});
    va.addVertex({center.x + 4, center.y + 1 - 4});
    va.addVertex({center.x - 3 + 4, center.y + 1 - 4});
    va.addVertex({center.x, center.y});
    va.addVertex({center.x - 3, center.y});
    va.addVertex({center.x + 4, center.y + 4});
    va.addVertex({center.x + 4, center.y + 4});
    va.addVertex({center.x - 3, center.y});
    va.addVertex({center.x - 3 + 4, center.y + 4});
    // Right Button
    if (mRightHover) {
        va.setColor(4, BackColor1);
        drawQuad(va, ulright.x, ulright.y, lrright.x, lrright.y);
    }
    center = ((ulright + lrright) / 2.0f).round() + Point2D(1.0f, 0.0f);
    va.setColor(4, mRightPressed ? ButtonColor1 : ButtonColor0);
    va.addVertex({center.x, center.y + 1});
    va.addVertex({center.x + 3, center.y + 1});
    va.addVertex({center.x - 4, center.y + 1 - 4});
    va.addVertex({center.x - 4, center.y + 1 - 4});
    va.addVertex({center.x + 3, center.y + 1});
    va.addVertex({center.x + 3 - 4, center.y + 1 - 4});
    va.addVertex({center.x + 3, center.y});
    va.addVertex({center.x, center.y});
    va.addVertex({center.x - 4, center.y + 4});
    va.addVertex({center.x + 3, center.y});
    va.addVertex({center.x - 4, center.y + 4});
    va.addVertex({center.x + 3 - 4, center.y + 4});
    VertexBuffer(va).render();
}

void VScroll::update(Point2D const& ul, Point2D const& lr, Form& form) {
    mHover = mModified = false;
    mUpHover = mUpPressed = mDownHover = mDownPressed = false;
    double oldPosition = position;
    // Areas
    Point2D ulsa = ul + Point2D(0, ScrollButtonSize), lrsa = lr - Point2D(0, ScrollButtonSize); // Scroll area
    float len = std::max((lrsa.y - ulsa.y) * float(length), float(MinScrollLength));
    float up = ulsa.y + (lrsa.y - ulsa.y - len) * position;
    Point2D uls = Point2D(ulsa.x, up), lrs = Point2D(lrsa.x, up + len);   // Scroll bar
    Point2D ulup = ul, lrup = Point2D(lr.x, ul.y + ScrollButtonSize);     // Up button
    Point2D uldown = Point2D(ul.x, lr.y - ScrollButtonSize), lrdown = lr; // Down button
    // Begin selection
    if (focusable && form.mousePosition() >= uls && form.mousePosition() <= lrs) {
        mHover = true;
        if (form.mouseLeftDown()) {
            getFocus(form), mSelecting = true;
            mMouseOffset = form.mousePosition().y - up;
        }
    }
    // Update selection
    if (form.mouseLeftHeld() && mSelecting) {
        up = form.mousePosition().y - mMouseOffset;
        if (lrsa.y - ulsa.y - len > 0.0)
            position = (up - ulsa.y) / (lrsa.y - ulsa.y - len);
    }
    if (!form.mouseLeftPressed())
        mSelecting = false;
    // Update buttons
    double delta = (unit > 0.0 ? unit : length * DefaultUnitFraction);
    if (focusable && form.mousePosition() >= ulup && form.mousePosition() <= lrup) {
        mUpHover = true;
        if (form.mouseLeftDown()) {
            getFocus(form);
            if (length < 1.0)
                position -= delta / (1.0 - length);
        }
        if (form.mouseLeftPressed())
            mUpPressed = true;
    }
    if (focusable && form.mousePosition() >= uldown && form.mousePosition() <= lrdown) {
        mDownHover = true;
        if (form.mouseLeftDown()) {
            getFocus(form);
            if (length < 1.0)
                position += delta / (1.0 - length);
        }
        if (form.mouseLeftPressed())
            mDownPressed = true;
    }
    position = std::min(std::max(position, 0.0), 1.0);
    mModified = (position != oldPosition);
}

void VScroll::render(Point2D const& ul, Point2D const& lr, Form const&) const {
    // Areas
    Point2D ulsa = ul + Point2D(0, ScrollButtonSize), lrsa = lr - Point2D(0, ScrollButtonSize); // Scroll area
    float len = std::max((lrsa.y - ulsa.y) * float(length), float(MinScrollLength));
    float up = ulsa.y + (lrsa.y - ulsa.y - len) * position;
    Point2D uls = Point2D(ulsa.x, up), lrs = Point2D(lrsa.x, up + len);   // Scroll bar
    Point2D ulup = ul, lrup = Point2D(lr.x, ul.y + ScrollButtonSize);     // Up button
    Point2D uldown = Point2D(ul.x, lr.y - ScrollButtonSize), lrdown = lr; // Down button
    // Render
    VertexArray va(120, VertexFormat(0, 4, 0, 3));
    // Background quad
    va.setColor(4, BackColor1);
    drawQuad(va, ul.x, ul.y, lr.x, lr.y);
    // Border
    va.setColor(4, (mHover || mSelecting) ? ButtonColor1 : ButtonColor0);
    drawQuad(va, ul.x, float(up), lr.x, float(up + len));
    // Foreground quad
    va.setColor(4, mSelecting ? ButtonColor1 : ButtonColor0);
    drawQuad(va, ul.x + LineWidth, float(up) + LineWidth, lr.x - LineWidth, float(up + len) - LineWidth);
    // Up Button
    if (mUpHover) {
        va.setColor(4, BackColor1);
        drawQuad(va, ulup.x, ulup.y, lrup.x, lrup.y);
    }
    Point2D center = ((ulup + lrup) / 2.0f).round() + Point2D(0.5f, -0.5f);
    va.setColor(4, mUpPressed ? ButtonColor1 : ButtonColor0);
    va.addVertex({center.x + 1, center.y});
    va.addVertex({center.x + 1, center.y - 3});
    va.addVertex({center.x + 1 - 4, center.y + 4});
    va.addVertex({center.x + 1 - 4, center.y + 4});
    va.addVertex({center.x + 1, center.y - 3});
    va.addVertex({center.x + 1 - 4, center.y - 3 + 4});
    va.addVertex({center.x, center.y - 3});
    va.addVertex({center.x, center.y});
    va.addVertex({center.x + 4, center.y + 4});
    va.addVertex({center.x, center.y - 3});
    va.addVertex({center.x + 4, center.y + 4});
    va.addVertex({center.x + 4, center.y - 3 + 4});
    // Down Button
    if (mDownHover) {
        va.setColor(4, BackColor1);
        drawQuad(va, uldown.x, uldown.y, lrdown.x, lrdown.y);
    }
    center = ((uldown + lrdown) / 2.0f).round() + Point2D(0.5f, 0.5f);
    va.setColor(4, mDownPressed ? ButtonColor1 : ButtonColor0);
    va.addVertex({center.x + 1, center.y + 3});
    va.addVertex({center.x + 1, center.y});
    va.addVertex({center.x + 1 - 4, center.y - 4});
    va.addVertex({center.x + 1, center.y + 3});
    va.addVertex({center.x + 1 - 4, center.y - 4});
    va.addVertex({center.x + 1 - 4, center.y + 3 - 4});
    va.addVertex({center.x, center.y});
    va.addVertex({center.x, center.y + 3});
    va.addVertex({center.x + 4, center.y - 4});
    va.addVertex({center.x + 4, center.y - 4});
    va.addVertex({center.x, center.y + 3});
    va.addVertex({center.x + 4, center.y + 3 - 4});
    VertexBuffer(va).render();
}

ScrollArea::ScrollArea(
    Position const& ul,
    Position const& lr,
    Point2D const& size_,
    Point2D const& position_,
    bool draggable_,
    bool scalable_,
    bool focusable
):
    Control(ul, lr, focusable),
    size(size_),
    position(position_),
    draggable(draggable_),
    scalable(scalable_),
    mContent(Position(Point2D(0.5f, 0.5f), Point2D(0, 0)), Position(Point2D(0.5f, 0.5f), Point2D(0, 0))),
    mView(Position(Point2D(0.0f, 0.0f), Point2D(0, 0)), Position(Point2D(1.0f, 1.0f), Point2D(0, 0))),
    mH(Position(Point2D(0.0f, 1.0f), Point2D(0, -DefaultHScrollWidth)),
       Position(Point2D(1.0f, 1.0f), Point2D(0, 0)),
       1.0),
    mV(Position(Point2D(1.0f, 0.0f), Point2D(-DefaultVScrollWidth, 0)),
       Position(Point2D(1.0f, 1.0f), Point2D(0, 0)),
       1.0) {
    mView.addChild({&mContent});
    Control::addChild({&mView, &mH, &mV});
}

void ScrollArea::update(Point2D const& ul, Point2D const& lr, Form&) {
    mH.active = mV.active = false;
    mView.lowerRight.offset = Point2D(0, 0); // Default: no scroll bars
    Point2D vsize =
        (mView.lowerRight.compute(lr - ul) - mView.upperLeft.compute(lr - ul)) / ScalingFactor; // Viewport size
    if (size.x * scale > vsize.x)
        mH.active = true, mView.lowerRight.offset.y -= DefaultHScrollWidth,
        vsize.y -= DefaultHScrollWidth; // Enable HScroll
    if (size.y * scale > vsize.y)
        mV.active = true, mView.lowerRight.offset.x -= DefaultVScrollWidth,
        vsize.x -= DefaultVScrollWidth; // Enable VScroll
    if (size.x * scale > vsize.x && !mH.active)
        mH.active = true, mView.lowerRight.offset.y -= DefaultHScrollWidth,
        vsize.y -= DefaultHScrollWidth; // Enable HScroll
    if (mH.active && mV.active) {       // Both enabled
        mH.lowerRight.offset.x = -DefaultVScrollWidth - 1;
        mV.lowerRight.offset.y = -DefaultHScrollWidth - 1;
    } else {
        mH.lowerRight.offset.x = 0;
        mV.lowerRight.offset.y = 0;
    }
    // Calculate content area offset
    position.x = mH.position, position.y = mV.position;
    Point2D offset = size * scale - vsize;
    offset.x = std::max(0.0f, offset.x), offset.y = std::max(0.0f, offset.y);
    offset = offset * (position - Point2D(0.5f, 0.5f));
    mContent.upperLeft.offset = offset * -1.0f - size * scale / 2.0f + mView.lowerRight.offset / 2.0f;
    mContent.lowerRight.offset = offset * -1.0f + size * scale / 2.0f + mView.lowerRight.offset / 2.0f;
    mView.lowerRight.offset = Point2D(0, 0);
    // TODO: implement dragging & scaling
    Point2D fraction = vsize / (size * scale);
    fraction.x = std::min(1.0f, fraction.x), fraction.y = std::min(1.0f, fraction.y);
    mH.length = fraction.x, mV.length = fraction.y;
}
}
*/
