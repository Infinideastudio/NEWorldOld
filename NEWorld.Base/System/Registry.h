#pragma once

#include <memory>
#include <string>
#include <string_view>

namespace NEWorld::Registry {
    class Instance {
    public:
        [[nodiscard]] auto GetInt(std::string_view name)->int;
        [[nodiscard]] auto GetBool(std::string_view name)->bool;
        [[nodiscard]] auto GetFloat(std::string_view name)->double;
        [[nodiscard]] auto GetString(std::string_view name)->std::string;
        [[nodiscard]] auto GetBytes(std::string_view name)->std::unique_ptr<char>;
        void Set(std::string_view name, int val);
        void Set(std::string_view name, bool val);
        void Set(std::string_view name, double val);
        void Set(std::string_view name, char* mem, int size);
    };
}
