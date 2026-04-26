export module items;
import std;
import blocks;
import types;

export namespace items {

struct ItemStack {
    ItemStack() = default;
    ItemStack(blocks::Id id, size_t count):
        id(id),
        count(count) {}

    blocks::Id id;
    size_t count;

    bool empty() const noexcept {
        return count == 0;
    }
};

}
