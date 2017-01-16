#include "utils/strings.h"
#include "utils/jsonhelper.h"
#include "Locale.h"
#include <deque>
#include <codecvt>
namespace Locale
{
    namespace
    {
        class ServiceImpl
        {
        public:
            ServiceImpl(const std::string& basePath):
                mBasePath(basePath)
            {
            }
            void setActiveLang(const std::string& lang)
            {
                mActiveLang = lang;
                mLang = readJsonFromFile(mBasePath + '/' + lang + ".json");
            }
            std::string getActiveLang() const
            {
                return mActiveLang;
            }
            std::string getStrByKey(const std::string& key, bool returnKeyifNotFound = true)
            {
                auto s = split(key, '.');
                Json* iter = &mLang;
                for (auto&& str :s)
                {
                    Json::iterator niter = iter->find(str);
                    if (niter != iter->end())
                        iter = &(*niter);
                    else
                        return returnKeyifNotFound ? key : std::string("");
                }
                return getJsonValue<std::string>(*iter);
            }
            auto& getConv() { return mConv; }
        private:
            std::wstring_convert<std::codecvt_utf8<wchar_t>> mConv;
            std::map<std::string, int> mIDTable;
            std::string mActiveLang, mBasePath;
            Json mLang;
        };
    }
    Service::Service(const std::string& path):
        mService(std::make_unique<ServiceImpl>(path))
    {
    }
    Service::~Service() = default;
    void Service::setActiveLang(const std::string& lang)
    {
        mService->setActiveLang(lang);
    }
    std::string Service::getActiveLang() const
    {
        return mService->getActiveLang();
    }
    std::string Service::operator[](const std::string& key) const
    {
        return mService->getStrByKey(key);
    }
    const std::string Service::w2cUtf8(const std::wstring& src)
    {
        return mService->getConv().to_bytes(src);
    }

    const std::wstring Service::c2wUtf8(const std::string& src)
    {
        return mService->getConv().from_bytes(src);
    }
}
