module menus;
import std;
import types;
import ui;
import globals;
import globalization;
import render;
import textures;

namespace Menus {
using Globalization::GetStrbyKey;

class WorldMenu: public ui::Menu {
private:
    struct Entry {
        std::string world_name;
        std::shared_ptr<render::Texture> thumbnail;
    };
    std::vector<Entry> _entries;
    std::string _chosen_world_name;
    ui::Key _worlds_key = {};
    ui::Key _chosen_world_name_key = {};

    void _refresh() {
        _entries.clear();
        for (auto const& dir: std::filesystem::directory_iterator("worlds")) {
            if (dir.is_directory()) {
                auto entry = Entry();
                entry.world_name = dir.path().filename().string();
                if (std::filesystem::exists(dir.path() / "thumbnail.png")) {
                    auto texture = Textures::LoadTexture(dir.path() / "thumbnail.png", true);
                    entry.thumbnail = std::make_shared<render::Texture>(std::move(texture));
                }
                _entries.emplace_back(std::move(entry));
            }
        }
    }

    auto _build_column(ui::Context& ctx) {
        using namespace ui;
        auto items = std::vector<FlexItem>();
        items.emplace_back(Sizer({.max_height = 32}, Center({}, Label(GetStrbyKey("NEWorld.worlds.caption")))));
        for (auto const& entry: _entries) {
            items.emplace_back(Spacer({.height = 8}));
            items.emplace_back(Sizer(
                {.max_height = 72},
                Button(
                    {.label = Stack(
                         {},
                         Builder(
                             _chosen_world_name_key,
                             [this, entry](Key) {
                                 auto chosen = (_chosen_world_name == entry.world_name);
                                 auto padding = chosen ? 0.0f : 2.0f;
                                 return Padding(
                                     {.left = padding, .top = padding, .right = padding, .bottom = padding},
                                     ImageBox(
                                         {.alignment = Alignment::CENTER,
                                          .fit = BoxFit::COVER,
                                          .texture = entry.thumbnail}
                                     )
                                 );
                             }
                         ),
                         StackItem({.alignment = Alignment::CENTER}, Label(entry.world_name))
                     ),
                     .on_click = [this, &ctx, world_name = entry.world_name]() {
                         _chosen_world_name = world_name;
                         ctx.mark_for_update(_chosen_world_name_key);
                     }}
                )
            ));
        }
        items.emplace_back(Spacer({.height = 8}));
        items.emplace_back(Sizer(
            {.max_height = 72},
            Button({.label = Label(GetStrbyKey("NEWorld.worlds.new")), .on_click = [this, &ctx]() {
                        createworldmenu();
                        _refresh();
                        ctx.mark_for_update(_worlds_key);
                    }})
        ));
        items.emplace_back(FlexItem({.flex_grow = 1}, Spacer({.height = std::numeric_limits<float>::infinity()})));
        items.emplace_back(Sizer(
            {.max_height = 32},
            Row({.main_axis_size = MainAxisSize::MAX},
                FlexItem(
                    {.flex_grow = 1},
                    Button(
                        {.label = Label(GetStrbyKey("NEWorld.worlds.enter")),
                         .on_click =
                             [this]() {
                                 if (!_chosen_world_name.empty()) {
                                     Cur_WorldName = _chosen_world_name;
                                     GameBegin = true;
                                     exit();
                                 }
                             }}
                    )
                ),
                Spacer({.width = 8}),
                FlexItem(
                    {.flex_grow = 1},
                    Button({.label = Label(GetStrbyKey("NEWorld.worlds.delete")), .on_click = [this, &ctx]() {
                                if (!_chosen_world_name.empty()) {
                                    std::filesystem::remove_all(std::filesystem::path("worlds") / _chosen_world_name);
                                    _chosen_world_name = "";
                                    _refresh();
                                    ctx.mark_for_update(_worlds_key);
                                }
                            }})
                ))
        ));
        items.emplace_back(Spacer({.height = 8}));
        items.emplace_back(
            Sizer({.max_height = 32}, Button({.label = Label(GetStrbyKey("NEWorld.worlds.back")), .on_click = [this]() {
                                                  exit();
                                              }}))
        );
        auto column = Column(
            {.main_axis_size = MainAxisSize::MAX, .main_axis_alignment = MainAxisAlignment::CENTER},
            std::move(items)
        );
        return std::move(column);
    }

    auto build(ui::Context& ctx) -> ui::View override {
        _worlds_key = ctx.generate_key();
        _chosen_world_name_key = ctx.generate_key();
        _refresh();
        using namespace ui;
        return View(Row(
            {.main_axis_size = MainAxisSize::MAX, .main_axis_alignment = MainAxisAlignment::CENTER},
            FlexItem(
                {.flex_grow = 1},
                Padding(
                    {.left = 32, .top = 32, .right = 32, .bottom = 32},
                    Sizer({.max_width = 512}, Builder(_worlds_key, [this, &ctx](Key) { return _build_column(ctx); }))
                )
            )
        ));
    }
};

void worldmenu() {
    WorldMenu().run(MainWindow);
}

}
