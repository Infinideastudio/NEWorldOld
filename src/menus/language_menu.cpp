module menus;
import std;
import types;
import ui;
import gui;
import globals;
import globalization;

namespace Menus {
using Globalization::GetStrbyKey;

class Language: public GUI::Form {
private:
    struct Entry {
        std::string symbol;
        std::string english_name;
        std::string native_name;
    };
    std::vector<Entry> entries;
    ui::Context ctx = ui::Context();
    ui::View view = ui::View(ui::Spacer({}));

    void onLoad() override {
        auto index = std::ifstream("lang/langs.txt");
        auto line = std::string();
        while (std::getline(index, line)) {
            if (line.empty())
                break;
            auto curr = Entry();
            curr.symbol = line;
            auto lang_file = std::ifstream("lang/" + curr.symbol + ".lang");
            std::getline(lang_file, curr.english_name);
            std::getline(lang_file, curr.native_name);
            entries.emplace_back(std::move(curr));
        }
        using namespace ui;
        auto items = std::vector<FlexItem>();
        items.emplace_back(Sizer({.max_height = 32}, Center({}, Label(GetStrbyKey("NEWorld.language.caption")))));
        for (auto const& entry: entries) {
            items.emplace_back(Spacer({.height = 8}));
            items.emplace_back(Sizer(
                {.max_height = 32},
                Button(
                    {.label = Label(entry.native_name),
                     .on_click =
                         [this, symbol = entry.symbol]() {
                             if (Cur_Lang != symbol) {
                                 Globalization::LoadLang(symbol);
                             }
                             exit = true;
                         }}
                )
            ));
        }
        items.emplace_back(FlexItem({.flex_grow = 1}, Spacer({.height = std::numeric_limits<float>::infinity()})));
        items.emplace_back(Sizer(
            {.max_height = 32},
            Button(
                {.label = Label(GetStrbyKey("NEWorld.language.back")),
                 .on_click =
                     [this]() {
                         exit = true;
                     }}
            )
        ));
        auto column = Column({}, std::move(items));
        view = View(Center({}, Sizer({.max_width = 512}, Padding({.top = 32, .bottom = 32}, std::move(column)))));
    }

    void onUpdate() override {
        // Temporary
        ctx.theme = ui::theme_dark();
        ctx.scaling_factor = static_cast<float>(Stretch);
        ctx.view_size = {static_cast<float>(WindowWidth), static_cast<float>(WindowHeight)};
        ctx.mouse_position = {static_cast<float>(mx), static_cast<float>(my)};
        ctx.mouse_motion = {static_cast<float>(mx - mxl), static_cast<float>(my - myl)};
        ctx.mouse_wheel_motion = {static_cast<float>(mw - mwl)};
        ctx.mouse_left_button_down = (mb == 1);
        ctx.mouse_left_button_acted = (mb == 1 && mbl == 0);
        ctx.mouse_left_button_released = (mb == 0 && mbl == 1);
        view.update(ctx);
    }

    void onRender() override {
        view.render(ctx);
    }
};

void languagemenu() {
    Language Menu;
    Menu.start();
}
}
