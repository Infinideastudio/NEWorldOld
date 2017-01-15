#pragma once

#include "exception.h"
#include <cassert>
#include <sstream>
#include <tuple>
#include <typeinfo>
#include <locale>

namespace StringUtils
{
    namespace detail_
    {
        template <size_t I>
        struct visit_impl
        {
            template <typename T, typename F>
            static void visit(T&& tup, size_t idx, F fun)
            {
                if (idx == I - 1) fun(std::get<I - 1>(tup));
                else visit_impl<I - 1>::visit(tup, idx, fun);
            }
        };

        template <>
        struct visit_impl<0>
        {
            template <typename T, typename F>
            [[noreturn]] static void visit(T&&, size_t, F)
            {
                nw_throw(Exception::Exception, "Out of range.");
            }
        };

        template <bool test, bool test2, typename T>
        struct ExpectImpl
        {
            template <typename U>
            [[noreturn]] constexpr static T Get(U&&)
            {
                nw_throw(Exception::Exception, "Type {0} cannot be converted to {1}.", typeid(U).name(), typeid(T).name());
            }
        };

        template <bool test2, typename T>
        struct ExpectImpl<true, test2, T>
        {
            template <typename U>
            constexpr static T Get(U&& value)
            {
                return static_cast<T>(value);
            }
        };

        template <typename T>
        struct ExpectImpl<false, true, T>
        {
            template <typename U>
            constexpr static T Get(U&& value)
            {
                return dynamic_cast<T>(value);
            }
        };
    }

    template <typename T>
    struct Expect
    {
        template <typename U>
        constexpr static T Get(U&& value)
        {
            return detail_::ExpectImpl<std::is_convertible<U, T>::value, false, T>::Get(value);
        }

        constexpr static T&& Get(T&& value)
        {
            return value;
        }
    };

    template <typename T>
    struct Expect<T*>
    {
        template <typename U>
        constexpr static T* Get(U&& value)
        {
            return detail_::ExpectImpl<std::is_convertible<U, T*>::value, false, T*>::Get(value);
        }

        template <typename U>
        constexpr static T* Get(U* value)
        {
            return detail_::ExpectImpl<std::is_convertible<U*, T*>::value, std::is_base_of<U, T>::value, T*>::Get(value);
        }

        constexpr static T* Get(T* value)
        {
            return value;
        }
    };

    template <typename F, template <typename...> class T, typename... Ts>
    void visit_at(T<Ts...> const& tup, size_t idx, F fun)
    {
        assert(idx < sizeof...(Ts));
        detail_::visit_impl<sizeof...(Ts)>::visit(tup, idx, fun);
    }

    template <typename F, template <typename...> class T, typename... Ts>
    void visit_at(T<Ts...>&& tup, size_t idx, F fun)
    {
        assert(idx < sizeof...(Ts));
        detail_::visit_impl<sizeof...(Ts)>::visit(tup, idx, fun);
    }

    template <typename... Args>
    std::string FormatString(const char* lpStr, Args&&... args)
    {
        std::stringstream ss;
        unsigned index = 0;
        auto argsTuple = std::forward_as_tuple(args...);

        for (; *lpStr; ++lpStr)
        {
            switch (*lpStr)
            {
                case '%':
                    ++lpStr;
                Begin:
                    switch (*lpStr)
                    {
                        case '%':
                            ss << '%';
                            break;
                        case 'c':
                            visit_at(argsTuple, index++, [&ss](auto&& item) { ss << Expect<char>::Get(item); });
                            break;
                        case 's':
                            visit_at(argsTuple, index++, [&ss](auto&& item) { ss << Expect<std::string>::Get(item); });
                            break;
                        case 'd':
                        case 'i':
                            visit_at(argsTuple, index++, [&ss](auto&& item) { ss << Expect<int>::Get(item); });
                            break;
                        case 'o':
                            visit_at(argsTuple, index++, [&ss](auto&& item) { auto fmt = ss.setf(std::ios_base::oct, std::ios_base::basefield); ss << Expect<unsigned>::Get(item); ss.setf(fmt); });
                            break;
                        case 'x':
                            visit_at(argsTuple, index++, [&ss](auto&& item) { auto fmt = ss.setf(std::ios_base::hex, std::ios_base::basefield); ss << Expect<unsigned>::Get(item); ss.setf(fmt); });
                            break;
                        case 'X':
                            visit_at(argsTuple, index++, [&ss](auto&& item) { auto fmt = ss.setf(std::ios_base::hex, std::ios_base::basefield); ss.setf(std::ios_base::uppercase); ss << Expect<unsigned>::Get(item); ss.setf(fmt); });
                            break;
                        case 'u':
                            visit_at(argsTuple, index++, [&ss](auto&& item) { ss << Expect<unsigned>::Get(item); });
                            break;
                        case 'f':
                        case 'F':
                            visit_at(argsTuple, index++, [&ss](auto&& item) { ss << Expect<float>::Get(item); });
                            break;
                        case 'e':
                            visit_at(argsTuple, index++, [&ss](auto&& item) { auto fmt = ss.setf(std::ios_base::scientific, std::ios_base::floatfield); ss << Expect<float>::Get(item); ss.setf(fmt); });
                            break;
                        case 'E':
                            visit_at(argsTuple, index++, [&ss](auto&& item) { auto fmt = ss.setf(std::ios_base::scientific, std::ios_base::floatfield); ss.setf(std::ios_base::uppercase); ss << Expect<float>::Get(item); ss.setf(fmt); });
                            break;
                        case 'a':
                            visit_at(argsTuple, index++, [&ss](auto&& item) { auto fmt = ss.setf(std::ios_base::fixed | std::ios_base::scientific, std::ios_base::floatfield); ss << Expect<float>::Get(item); ss.setf(fmt); });
                            break;
                        case 'A':
                            visit_at(argsTuple, index++, [&ss](auto&& item) { auto fmt = ss.setf(std::ios_base::fixed | std::ios_base::scientific, std::ios_base::floatfield); ss.setf(std::ios_base::uppercase); ss << Expect<float>::Get(item); ss.setf(fmt); });
                            break;
                        case 'p':
                            visit_at(argsTuple, index++, [&ss](auto&& item) { ss << Expect<const void*>::Get(item); });
                            break;
                        case 'h':
                            switch (*++lpStr)
                            {
                                case 'd':
                                case 'i':
                                    visit_at(argsTuple, index++, [&ss](auto&& item) { ss << Expect<short>::Get(item); });
                                    break;
                                case 'o':
                                    visit_at(argsTuple, index++, [&ss](auto&& item) { auto fmt = ss.setf(std::ios_base::oct, std::ios_base::basefield); ss << Expect<unsigned short>::Get(item); ss.setf(fmt); });
                                    break;
                                case 'x':
                                    visit_at(argsTuple, index++, [&ss](auto&& item) { auto fmt = ss.setf(std::ios_base::hex, std::ios_base::basefield); ss << Expect<unsigned short>::Get(item); ss.setf(fmt); });
                                    break;
                                case 'X':
                                    visit_at(argsTuple, index++, [&ss](auto&& item) { auto fmt = ss.setf(std::ios_base::hex, std::ios_base::basefield); ss.setf(std::ios_base::uppercase); ss << Expect<unsigned short>::Get(item); ss.setf(fmt); });
                                    break;
                                case 'u':
                                    visit_at(argsTuple, index++, [&ss](auto&& item) { ss << Expect<unsigned short>::Get(item); });
                                    break;
                                case 'h':
                                    switch (*++lpStr)
                                    {
                                        case 'd':
                                        case 'i':
                                            visit_at(argsTuple, index++, [&ss](auto&& item) { ss << Expect<int8_t>::Get(item); });
                                            break;
                                        case 'o':
                                            visit_at(argsTuple, index++, [&ss](auto&& item) { auto fmt = ss.setf(std::ios_base::oct, std::ios_base::basefield); ss << Expect<uint8_t>::Get(item); ss.setf(fmt); });
                                            break;
                                        case 'x':
                                            visit_at(argsTuple, index++, [&ss](auto&& item) { auto fmt = ss.setf(std::ios_base::hex, std::ios_base::basefield); ss << Expect<uint8_t>::Get(item); ss.setf(fmt); });
                                            break;
                                        case 'X':
                                            visit_at(argsTuple, index++, [&ss](auto&& item) { auto fmt = ss.setf(std::ios_base::hex, std::ios_base::basefield); ss.setf(std::ios_base::uppercase); ss << Expect<uint8_t>::Get(item); ss.setf(fmt); });
                                            break;
                                        case 'u':
                                            visit_at(argsTuple, index++, [&ss](auto&& item) { ss << Expect<uint8_t>::Get(item); });
                                            break;
                                        default:
                                            nw_throw(Exception::Exception, "Unknown token '%c'", *lpStr);
                                    }
                                    break;
                                default:
                                    nw_throw(Exception::Exception, "Unknown token '%c'", *lpStr);
                            }
                            break;
                        case 'l':
                            switch (*++lpStr)
                            {
                                case 'd':
                                case 'i':
                                    visit_at(argsTuple, index++, [&ss](auto&& item) { ss << Expect<long>::Get(item); });
                                    break;
                                case 'o':
                                    visit_at(argsTuple, index++, [&ss](auto&& item) { auto fmt = ss.setf(std::ios_base::oct, std::ios_base::basefield); ss << Expect<unsigned long>::Get(item); ss.setf(fmt); });
                                    break;
                                case 'x':
                                    visit_at(argsTuple, index++, [&ss](auto&& item) { auto fmt = ss.setf(std::ios_base::hex, std::ios_base::basefield); ss << Expect<unsigned long>::Get(item); ss.setf(fmt); });
                                    break;
                                case 'X':
                                    visit_at(argsTuple, index++, [&ss](auto&& item) { auto fmt = ss.setf(std::ios_base::hex, std::ios_base::basefield); ss.setf(std::ios_base::uppercase); ss << Expect<unsigned long>::Get(item); ss.setf(fmt); });
                                    break;
                                case 'u':
                                    visit_at(argsTuple, index++, [&ss](auto&& item) { ss << Expect<unsigned long>::Get(item); });
                                    break;
                                case 'f':
                                case 'F':
                                    visit_at(argsTuple, index++, [&ss](auto&& item) { ss << Expect<double>::Get(item); });
                                    break;
                                case 'e':
                                    visit_at(argsTuple, index++, [&ss](auto&& item) { auto fmt = ss.setf(std::ios_base::scientific, std::ios_base::floatfield); ss << Expect<double>::Get(item); ss.setf(fmt); });
                                    break;
                                case 'E':
                                    visit_at(argsTuple, index++, [&ss](auto&& item) { auto fmt = ss.setf(std::ios_base::scientific, std::ios_base::floatfield); ss.setf(std::ios_base::uppercase); ss << Expect<double>::Get(item); ss.setf(fmt); });
                                    break;
                                case 'a':
                                    visit_at(argsTuple, index++, [&ss](auto&& item) { auto fmt = ss.setf(std::ios_base::fixed | std::ios_base::scientific, std::ios_base::floatfield); ss << Expect<double>::Get(item); ss.setf(fmt); });
                                    break;
                                case 'A':
                                    visit_at(argsTuple, index++, [&ss](auto&& item) { auto fmt = ss.setf(std::ios_base::fixed | std::ios_base::scientific, std::ios_base::floatfield); ss.setf(std::ios_base::uppercase); ss << Expect<double>::Get(item); ss.setf(fmt); });
                                    break;
                                case 'l':
                                    switch (*++lpStr)
                                    {
                                        case 'd':
                                        case 'i':
                                            visit_at(argsTuple, index++, [&ss](auto&& item) { ss << Expect<long long>::Get(item); });
                                            break;
                                        case 'o':
                                            visit_at(argsTuple, index++, [&ss](auto&& item) { auto fmt = ss.setf(std::ios_base::oct, std::ios_base::basefield); ss << Expect<unsigned long long>::Get(item); ss.setf(fmt); });
                                            break;
                                        case 'x':
                                            visit_at(argsTuple, index++, [&ss](auto&& item) { auto fmt = ss.setf(std::ios_base::hex, std::ios_base::basefield); ss << Expect<unsigned long long>::Get(item); ss.setf(fmt); });
                                            break;
                                        case 'X':
                                            visit_at(argsTuple, index++, [&ss](auto&& item) { auto fmt = ss.setf(std::ios_base::hex, std::ios_base::basefield); ss.setf(std::ios_base::uppercase); ss << Expect<unsigned long long>::Get(item); ss.setf(fmt); });
                                            break;
                                        case 'u':
                                            visit_at(argsTuple, index++, [&ss](auto&& item) { ss << Expect<unsigned long long>::Get(item); });
                                            break;
                                        default:
                                            break;
                                    }
                                    break;
                                default:
                                    nw_throw(Exception::Exception, "Unknown token '%c'", *lpStr);
                            }
                            break;
                        case 'L':
                            switch (*++lpStr)
                            {
                                case 'f':
                                case 'F':
                                    visit_at(argsTuple, index++, [&ss](auto&& item) { ss << Expect<long double>::Get(item); });
                                    break;
                                case 'e':
                                    visit_at(argsTuple, index++, [&ss](auto&& item) { auto fmt = ss.setf(std::ios_base::scientific, std::ios_base::floatfield); ss << Expect<long double>::Get(item); ss.setf(fmt); });
                                    break;
                                case 'E':
                                    visit_at(argsTuple, index++, [&ss](auto&& item) { auto fmt = ss.setf(std::ios_base::scientific, std::ios_base::floatfield); ss.setf(std::ios_base::uppercase); ss << Expect<long double>::Get(item); ss.setf(fmt); });
                                    break;
                                case 'a':
                                    visit_at(argsTuple, index++, [&ss](auto&& item) { auto fmt = ss.setf(std::ios_base::fixed | std::ios_base::scientific, std::ios_base::floatfield); ss << Expect<long double>::Get(item); ss.setf(fmt); });
                                    break;
                                case 'A':
                                    visit_at(argsTuple, index++, [&ss](auto&& item) { auto fmt = ss.setf(std::ios_base::fixed | std::ios_base::scientific, std::ios_base::floatfield); ss.setf(std::ios_base::uppercase); ss << Expect<long double>::Get(item); ss.setf(fmt); });
                                    break;
                                default:
                                    nw_throw(Exception::Exception, "Unknown token '%c'", *lpStr);
                            }
                            break;
                        default:
                            if (*lpStr == '0')
                            {
                                ss << std::setfill(*lpStr++);
                            }
                            else
                            {
                                nw_throw(Exception::Exception, "Unknown token '%c'", *lpStr);
                            }

                            unsigned tmpWidth = 0;
                            while (std::isdigit(*lpStr, std::locale{}))
                            {
                                tmpWidth = tmpWidth * 10 + (*lpStr++ - '0');
                            }
                            if (tmpWidth)
                            {
                                ss << std::setw(tmpWidth);
                            }
                            goto Begin;
                    }
                    break;
                case '{':
                {
                    unsigned tmpIndex = 0;
                    ++lpStr;
                    while (*lpStr)
                    {
                        if (std::isspace(*lpStr, std::locale{}))
                        {
                            ++lpStr;
                            continue;
                        }

                        if (std::isdigit(*lpStr, std::locale{}))
                        {
                            tmpIndex = tmpIndex * 10 + (*lpStr++ - '0');
                            continue;
                        }

                        break;
                    }

                    while (std::isspace(*lpStr, std::locale{})) { ++lpStr; }
                    if (*lpStr != '}')
                    {
                        nw_throw(Exception::Exception, "Expected '}', got '%c'", *lpStr);
                    }

                    visit_at(argsTuple, tmpIndex, [&ss](auto&& item) { ss << item; });
                    break;
                }
                default:
                    ss << *lpStr;
                    break;
            }
        }

        return ss.str();
    }

    template <>
    inline std::string FormatString(const char* lpStr)
    {
        return lpStr;
    }

    template <typename... Args>
    std::string FormatString(std::string const& Str, Args&&... args)
    {
        return FormatString(Str.c_str(), std::forward<Args>(args)...);
    }

    template <>
    inline std::string FormatString(std::string const& Str)
    {
        return Str;
    }

}
