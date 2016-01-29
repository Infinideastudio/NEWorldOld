#include "ModLoader.h"
#include <Windows.h>
#include <fstream>
#include <io.h>
#include <functional>

//����һ���ļ��������е���Ŀ¼
std::vector<std::string> findFolders(std::string path) {
	std::vector<std::string> ret;
	long hFile = 0;
	_finddata_t fileinfo;
	if ((hFile = _findfirst((path+"*").c_str(), &fileinfo)) != -1) {
		do {
			if ((fileinfo.attrib &  _A_SUBDIR)) {
				if (strcmp(fileinfo.name, ".") != 0 && strcmp(fileinfo.name, "..") != 0) {
					ret.push_back(fileinfo.name);
				}
			}
		} while (_findnext(hFile, &fileinfo) == 0);
		_findclose(hFile);
	}
	return ret;
}

void Mod::ModLoader::loadMods()
{
	std::vector<std::string> modLoadList = findFolders("Mods\\"); //���Mods/�����е��ļ���
	bool successAtLeastOne = false; 
	do { //ѭ������Mod��ֱ��ĳһ��ѭ��һ���ɹ����صĶ�û��Ϊֹ����Ϊ�����
		successAtLeastOne = false;
		for (std::vector<std::string>::iterator iter = modLoadList.begin(); iter != modLoadList.end();) {
			ModLoadStatus status = loadSingleMod("Mods\\" + *iter + "\\entry.dll");
			if (status == Success) successAtLeastOne = true;
			if (status != MissDependence) iter = modLoadList.erase(iter); //ֻҪ������ȱ��������ʧ�ܾ�ɾ����¼
			else ++iter;
		}
	} while (successAtLeastOne);
}

//���ص���Mod��loadMods����øú���
Mod::ModLoader::ModLoadStatus Mod::ModLoader::loadSingleMod(std::string modPath)
{
	ModCall call=loadMod(modPath);
	std::function<ModInfo()>* getModInfo = (std::function<ModInfo()>*)getFunction(call, "getModInfo");
	std::function<bool()>* init = (std::function<bool()>*)getFunction(call, "init");
	ModInfo info = (*getModInfo)(); //���Mod��Ϣ
	if (info.dependence != "") { //�жϲ����������
		bool foundDependence = false;
		for (std::vector<ModInfo>::iterator iter = mods.begin(); iter != mods.end(); ++iter) {
			if (iter->name == info.dependence) {
				foundDependence = true;
				break;
			}
		}
		if (!foundDependence) return MissDependence;
	}
	if (!(*init)()) return InitFailed; //��ʼ��Mod
	info.call = call;
	mods.push_back(info);
	return Success;
}

Mod::ModLoader::ModCall Mod::ModLoader::loadMod(std::string filename)
{
	return LoadLibrary(filename.c_str());
}

void* Mod::ModLoader::getFunction(ModCall call, std::string functionName)
{
	return GetProcAddress((HMODULE)call, functionName.c_str());
}

void Mod::ModLoader::unloadMod(ModCall call)
{
	FreeLibrary((HMODULE)call);
}
