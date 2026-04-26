module menus;
import std;
import types;
import ui;
import globals;
import globalization;

namespace Menus {
using Globalization::GetStrbyKey;

class LanguageMenu: public ui::Menu {
private:
    struct Entry {
        std::string symbol;
        std::string english_name;
        std::string native_name;
    };
    std::vector<Entry> _entries;
    ui::Key _langs_key = {};
    ui::Key _chosen_lang_key = {};

    void _refresh() {
        _entries.clear();
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
            _entries.emplace_back(std::move(curr));
        }
    }

    auto _build_column() {
        using namespace ui;
        auto items = std::vector<FlexItem>();
        items.emplace_back(Sizer({.max_height = 32}, Center({}, Label(GetStrbyKey("NEWorld.language.caption")))));
        for (auto const& entry: _entries) {
            items.emplace_back(Spacer({.height = 8}));
            items.emplace_back(Sizer(
                {.max_height = 32},
                Button({.label = Label(entry.native_name), .on_click = [this, symbol = entry.symbol]() {
                            if (Cur_Lang != symbol) {
                                Globalization::LoadLang(symbol);
                            }
                            exit();
                        }})
            ));
        }
        items.emplace_back(FlexItem({.flex_grow = 1}, Spacer({.height = std::numeric_limits<float>::infinity()})));
        items.emplace_back(Sizer(
            {.max_height = 32},
            Button({.label = Label(GetStrbyKey("NEWorld.language.back")), .on_click = [this]() {
                        exit();
                    }})
        ));
        auto column = Column(
            {.main_axis_size = MainAxisSize::MAX, .main_axis_alignment = MainAxisAlignment::CENTER},
            std::move(items)
        );
        return std::move(column);
    }

    auto build(ui::Context& ctx) -> ui::View override {
        _langs_key = ctx.generate_key();
        _chosen_lang_key = ctx.generate_key();
        _refresh();
        using namespace ui;
        return View(
            Row({.main_axis_size = MainAxisSize::MAX, .main_axis_alignment = MainAxisAlignment::CENTER},
                FlexItem(
                    {.flex_grow = 1},
                    Padding(
                        {.left = 32, .top = 32, .right = 32, .bottom = 32},
                        Sizer({.max_width = 512}, Builder(_langs_key, [this](Key) { return _build_column(); }))
                    )
                ))
        );
    }
};

void languagemenu() {
    LanguageMenu().run(MainWindow);
}

}
