#pragma once
#include "utils/stringutils.h"
#include <string>
#include <memory>
namespace Locale
{
    namespace
    {
        class ServiceImpl;
    }
    class Service
    {
    public:
        Service(const Service&) = delete;
        Service(const std::string& basePath);
        Service operator=(const Service&) = delete;
        ~Service();
        std::string getActiveLang() const;
        void setActiveLang(const std::string& lang);
        template <typename... Args>
        std::string format(const std::string& key, Args&&... args)
        {
            return StringUtils::FormatString((*this)[key].c_str(), std::forward<Args>(args)...);
        }
        std::string operator[](const std::string& key) const;
    private:
        std::unique_ptr<ServiceImpl> mService;
    };
}
