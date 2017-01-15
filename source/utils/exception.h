#pragma once
#include <string>
#include <chrono>

namespace StringUtils
{
    template <typename... Args>
    std::string FormatString(const char* lpStr, Args&&... args);
}

namespace Exception
{
    namespace detail_
    {
        struct ExceptionStorage
        {
            ExceptionStorage(std::exception_ptr nestedException, std::chrono::system_clock::time_point const& time, std::string file, unsigned line, std::string src, std::string desc) noexcept
                    : mNestedException(nestedException), mTime(time), mFile(file), mLine(line), mSource(src), mDescription(desc)
            {
            }

            std::exception_ptr mNestedException;
            std::chrono::system_clock::time_point mTime;
            std::string mFile;
            unsigned mLine;
            std::string mSource;
            std::string mDescription;
        };
    }

    class Exception
            : protected detail_::ExceptionStorage, public virtual std::exception
    {
    public:
        template <typename... Args>
        Exception(std::exception_ptr nestedException, const char* Src, const char* File, unsigned Line, const char* Desc, Args&&... args) noexcept
                : detail_::ExceptionStorage{ nestedException, std::chrono::system_clock::now(), File, Line, Src, StringUtils::FormatString(Desc, std::forward<Args>(args)...) }
        {
        }

        template <typename... Args>
        Exception(const char* Src, const char* File, unsigned Line, const char* Desc, Args&&... args) noexcept
                : detail_::ExceptionStorage{ {}, std::chrono::system_clock::now(), File, Line, Src, StringUtils::FormatString(Desc, std::forward<Args>(args)...) }
        {
        }

        ~Exception() = default;

        std::chrono::system_clock::time_point GetTime() const noexcept
        {
            return mTime;
        }

        const char* GetFile() const noexcept
        {
            return mFile.c_str();
        }

        unsigned GetLine() const noexcept
        {
            return mLine;
        }

        const char* GetSource() const noexcept
        {
            return mSource.c_str();
        }

        const char* GetDesc() const noexcept
        {
            return mDescription.c_str();
        }

        std::exception_ptr GetNestedException() const noexcept
        {
            return mNestedException;
        }

        const char* what() const noexcept override
        {
            return mDescription.c_str();
        }
    };

#define DeclareException(ExceptionClass, ExtendException, DefaultDescription) \
class ExceptionClass : public ExtendException\
{\
public:\
    typedef ExtendException BaseException;\
    ExceptionClass(const char* Src, const char* File, unsigned Line) noexcept\
        : BaseException(Src, File, Line, DefaultDescription)\
    {\
    }\
    ExceptionClass(std::exception_ptr nestedException, const char* Src, const char* File, unsigned Line) noexcept\
        : BaseException(nestedException, Src, File, Line, DefaultDescription)\
    {\
    }\
    template <typename... Args>\
    ExceptionClass(const char* Src, const char* File, unsigned Line, const char* Desc, Args&&... args) noexcept\
        : BaseException(Src, File, Line, Desc, std::forward<Args>(args)...)\
    {\
    }\
    template <typename... Args>\
    ExceptionClass(std::exception_ptr nestedException, const char* Src, const char* File, unsigned Line, const char* Desc, Args&&... args) noexcept\
        : BaseException(nestedException, Src, File, Line, Desc, std::forward<Args>(args)...)\
    {\
    }\
}
}

#define nw_throw(ExceptionClass, ...) do { throw ExceptionClass{ __FUNCTION__, __FILE__, __LINE__, __VA_ARGS__ }; } while (false)
#define nw_throwwithnested(ExceptionClass, ...) do { throw ExceptionClass{ std::current_exception(), __FUNCTION__, __FILE__, __LINE__, __VA_ARGS__ }; } while (false)

#include "stringutils.h"
