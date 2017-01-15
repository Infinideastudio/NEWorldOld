#pragma once
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#define FUNCTION_ALIAS(A,B) template <typename... Args> auto B(Args&&... args) -> decltype(A(std::forward<Args>(args)...)){return A(std::forward<Args>(args)...);}
template<class T>
using defaultList = std::vector<T>;
template <template<typename T>class Container = defaultList>
Container<std::string> split(const std::string& s, char delim)
{
    Container<std::string> elems;
    std::stringstream ss;
    ss.str(s);
    std::string item;
    while (std::getline(ss, item, delim))
        elems.push_back(item);
    return elems;
};

inline void trim(std::string& s)
{
    if (s.empty())
        return;

    s.erase(0, s.find_first_not_of(" "));
    s.erase(s.find_last_not_of(" ") + 1);
}

inline void strtolower(std::string& s)
{
    transform(s.begin(), s.end(), s.begin(), tolower);
}

