module;

module menus;
import std;
import types;
import ui;
import globals;
import globalization;

namespace Menus {
using Globalization::GetStrbyKey;

class CreateWorldMenu: public ui::Menu {
private:
    std::shared_ptr<ui::TextBoxState> world_name_state = std::make_shared<ui::TextBoxState>();

    auto build(ui::Context& ctx) -> ui::View override {
        using namespace ui;
        // clang-format off
        auto column = Column({.main_axis_size = MainAxisSize::MAX, .main_axis_alignment = MainAxisAlignment::CENTER},
            Sizer({.max_height = 32},
                Center({}, Label(GetStrbyKey("NEWorld.create.caption")))
            ),
            Spacer({.height = 8}),
            Sizer({.max_height = 32},
                TextBox({.placeholder = GetStrbyKey("NEWorld.create.inputname"), .state = world_name_state})
            ),
            Spacer({.height = 8}),
            Sizer({.max_height = 40},
                Row({.main_axis_size = MainAxisSize::MAX},
                    FlexItem({.flex_grow = 1},
                        Button({
                            .label = Label(GetStrbyKey("NEWorld.create.back")),
                            .on_click = [this]() { exit(); }
                        })
                    ),
                    Spacer({.width = 8}),
                    FlexItem({.flex_grow = 1},
                        Button({
                            .label = Label(GetStrbyKey("NEWorld.create.ok")),
                            .on_click = [this]() {
                                if (!world_name_state->text.empty()) {
                                    Cur_WorldName = unicode_utf8(world_name_state->text);
                                    std::filesystem::create_directories("worlds/" + Cur_WorldName);
                                    exit();
                                }
                            }
                        })
                    )
                )
            )
        );
        // clang-format on
        return View(
            Row({.main_axis_size = MainAxisSize::MAX, .main_axis_alignment = MainAxisAlignment::CENTER},
                FlexItem(
                    {.flex_grow = 1},
                    Padding(
                        {.left = 32, .top = 32, .right = 32, .bottom = 32},
                        Sizer({.max_width = 512}, std::move(column))
                    )
                ))
        );
    }
};

void createworldmenu() {
    CreateWorldMenu().run(MainWindow);
}

}
