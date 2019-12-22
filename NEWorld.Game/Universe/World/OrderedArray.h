#pragma once

#include <array>
#include <utility>
#include <algorithm>
#include <functional>
#include <type_traits>

template <class Tk, class Td, size_t Size,
          template<class>class Compare = std::less>
class OrderedList {
public:
    static_assert(std::is_trivial_v<Tk> && std::is_trivial_v<Td>);

    OrderedList() noexcept : mEnd(mList.end()), mComp() {}
    using ArrayType = std::array<std::pair<Tk, Td>, Size>;
    using Iterator = typename ArrayType::iterator;
    using ConstIterator = typename ArrayType::const_iterator;
    [[nodiscard]] Iterator begin() noexcept { return mList.begin(); }
    [[nodiscard]] ConstIterator begin() const noexcept { return mList.begin(); }
    [[nodiscard]] Iterator end() noexcept { return mEnd; }
    [[nodiscard]] ConstIterator end() const noexcept { return mEnd; }

    void Insert(Tk key, Td data) noexcept {
        const auto first = std::lower_bound(
			begin(), end(), key,                                
			[this](const auto& l, const auto& r) noexcept { return mComp(l.first, r); }
		);
        if (first != mList.end()) {
            if (first != end()) {
                const auto last = std::min(mList.end() - 1, end());
                std::copy_backward(first, last, last + 1);
            }
            *first = std::pair<Tk, Td>(key, data);
            if (end() != mList.end()) ++mEnd;
        }
    }

    void Clear() noexcept { mEnd = begin(); }

    [[nodiscard]] auto Count() const noexcept { return end() - begin(); }
private:
    ArrayType mList {};
    Iterator mEnd;
    Compare<Tk> mComp;
};
