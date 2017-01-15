#pragma once

#ifdef NEWORLD_TARGET_WINDOWS
    #include <io.h>
    #include <direct.h>
    #include <Windows.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/dir.h>
#endif

namespace FileSystem
{
    inline bool exists(const std::string& path)
    {
#ifdef NEWORLD_TARGET_WINDOWS
        return _access(path.c_str(), 0) == 0;
#else
        return access(path.c_str(), 0) == 0;
#endif
    }

    inline void createDirectory(const std::string& path)
    {
#ifdef NEWORLD_TARGET_WINDOWS
        _mkdir(path.c_str());
#else
        mkdir(path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
#endif
    }

    inline void forInDirectory(const std::string& path, std::function<void(bool, std::string)> callback)
    {
#ifdef NEWORLD_TARGET_WINDOWS
        WIN32_FIND_DATA ffd;
        HANDLE hFind = FindFirstFileA((path + "\\*").c_str(), &ffd);
        do
        {
            callback((ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY), ffd.cFileName);
        }
        while (FindNextFileA(hFind, &ffd) != 0);
        FindClose(hFind);
#else
        auto pDir = opendir(path.c_str());
        struct dirent *ent;
        while ((ent = readdir(pDir)) != nullptr)
        {
            callback((ent->d_type & DT_DIR), ent->d_name);
        }
#endif
    }
}
