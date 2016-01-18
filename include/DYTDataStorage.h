/*
* DYTDataStorage is an open-source data storage system.
* Copyright 2016 DYT
* License: DYTDataStorage is distributed under the MIT License.
*/
#ifndef DYTDATASTORAGE_H
#define DYTDATASTORAGE_H
#include <map>
#include <string>
#include <fstream>
#include <exception>
#ifdef UNICODE
typedef std::wstring dds_string;
typedef wchar_t dds_char;
#ifndef TEXT
#define TEXT(quote) L##quote
#endif
#else
typedef std::string dds_string;
typedef char dds_char;
#ifndef TEXT
#define TEXT(quote) quote
#endif
#endif
#ifndef DDS_VERSION
#define DDS_VERSION ((int)0)
#endif
#ifndef have_declare_data_type_defined
#define have_declare_data_type_defined
#define declare_data_type(type,name,get_func_name,set_func_name) \
inline type get_func_name() const \
{ \
if (Pointer->Type!=TEXT(name)) \
throw new std::exception("Type mismatched!"); \
return *(type*)Pointer->Data; \
} \
inline void set_func_name(type data) \
{ \
if (Pointer->Type!=TEXT(name)) \
{ \
Pointer->Type=TEXT(name); \
delete[] Pointer->Data; \
Pointer->Length=sizeof(type); \
Pointer->Data=new char[Pointer->Length]; \
} \
*(type*)Pointer->Data=data; \
}
#endif
class DataItemInternal
{
public:
	dds_string Type;
	char* Data;
	int Length;
	int Count;
	inline DataItemInternal()
		:Count(1),Length(0),Type(TEXT("")),Data(new char[2])
	{
	}
	inline ~DataItemInternal()
	{
		delete[] Data;
	}
};
inline dds_string ReadString(std::ifstream& stream)
{
	int length;
	stream.read((char*)&length, sizeof(int));
	dds_char* data = new dds_char[length + 1];
	stream.read((char*)data, length*sizeof(dds_char));
	data[length] = 0;
	dds_string ret = data;
	delete[] data;
	return ret;
}
inline void WriteString(std::ofstream& stream, const dds_string& str)
{
	int length = (int)str.size();
	stream.write((char*)&length, sizeof(int));
	stream.write((char*)str.c_str(), length*sizeof(dds_char));
}
class DataItem
{
public:
	dds_string Name;
	inline DataItem()
	{
		Pointer = new DataItemInternal;
	}
	inline DataItem(const dds_string& name)
	{
		Pointer = new DataItemInternal;
		Name = name;
	}
	inline DataItem(const dds_string& name, const dds_string& type)
	{
		Pointer = new DataItemInternal;
		Name = name;
		Pointer->Type = type;
	}
	inline DataItem(const dds_string& name, const dds_string& type, const char* data, int length)
	{
		Pointer = new DataItemInternal;
		Name = name;
		Pointer->Type = type;
		Pointer->Length = length;
		Pointer->Data = new char[length + 2];
		memcpy(Pointer->Data, data, length);
		Pointer->Data[length + 1] = Pointer->Data[length] = 0;
	}
	inline DataItem(const DataItem& src)
	{
		Pointer = src.Pointer;
		Pointer->Count++;
		Name = src.Name;
	}
	inline DataItem& operator=(const DataItem& src)
	{
		Pointer = src.Pointer;
		Pointer->Count++;
		Name = src.Name;
		return *this;
	}
	inline ~DataItem()
	{
		Pointer->Count--;
		if (!Pointer->Count)
			delete Pointer;
	}
	inline DataItem(std::ifstream& stream)
	{
		Read(stream);
	}
	inline void Read(std::ifstream& stream)
	{
		Pointer = new DataItemInternal;
		Pointer->Count = 1;
		Name = ReadString(stream);
		Pointer->Type = ReadString(stream);
		stream.read((char*)&Pointer->Length, sizeof(int));
		Pointer->Data = new char[Pointer->Length + 2];
		stream.read(Pointer->Data, Pointer->Length);
		Pointer->Data[Pointer->Length + 1] = Pointer->Data[Pointer->Length] = 0;
	}
	inline void Write(std::ofstream& stream) const
	{
		WriteString(stream, Name);
		WriteString(stream, Pointer->Type);
		stream.write((char*)&Pointer->Length, sizeof(int));
		stream.write(Pointer->Data, Pointer->Length);
	}
	declare_data_type(char,"int8",GetInt8,SetInt8)
	declare_data_type(unsigned char,"uint8",GetUInt8,SetUInt8)
	declare_data_type(short,"int16",GetInt16,SetInt16)
	declare_data_type(unsigned short,"uint16",GetUInt16,SetUInt16)
	declare_data_type(int,"int32",GetInt32,SetInt32)
	declare_data_type(unsigned int,"uint32",GetUInt32,SetUInt32)
	declare_data_type(float,"float",GetFloat,SetFloat)
	declare_data_type(double,"double",GetDouble,SetDouble)
	declare_data_type(bool,"boolean",GetBoolean,SetBoolean)
	inline std::string GetString() const
	{
		if (Pointer->Type != "string")
			throw new std::exception("Type mismatched!");
		return std::string(Pointer->Data);
	}
	inline void SetString(const std::string& value)
	{
		if (Pointer->Type != "string")
		{
			Pointer->Type = TEXT("string");
			delete[] Pointer->Data;
			Pointer->Length = (int)value.size()*sizeof(char);
			Pointer->Data = new char[Pointer->Length + 2];
			Pointer->Data[Pointer->Length + 1] = Pointer->Data[Pointer->Length] = 0;
		}
		strcpy(Pointer->Data, value.c_str());
	}
	inline std::wstring GetWString() const
	{
		if (Pointer->Type != "wstring")
			throw new std::exception("Type mismatched!");
		return std::wstring((wchar_t*)Pointer->Data);
	}
	inline void SetWString(const std::wstring& value)
	{
		if (Pointer->Type != "string")
		{
			Pointer->Type = TEXT("string");
			delete[] Pointer->Data;
			Pointer->Length = (int)value.size()*sizeof(wchar_t);
			Pointer->Data = new char[Pointer->Length + 2];
			Pointer->Data[Pointer->Length + 1] = Pointer->Data[Pointer->Length] = 0;
		}
		wcscpy((wchar_t*)Pointer->Data, value.c_str());
	}
	inline char* GetRawData()
	{
		return Pointer->Data;
	}
	inline void SetRawData(const dds_string& type, const char* data, int length)
	{
		Pointer->Count--;
		if (!Pointer->Count)
			delete Pointer;
		DataItemInternal* tmp = new DataItemInternal;
		tmp->Type = type;
		tmp->Length = length;
		tmp->Data = new char[length + 2];
		memcpy(tmp->Data, data, length);
		tmp->Data[length + 1] = tmp->Data[length] = 0;
		tmp->Count = 1;
		Pointer = tmp;
	}
	inline int GetDataLength() const
	{
		return Pointer->Length;
	}
	inline dds_string GetDataType() const
	{
		return Pointer->Type;
	}
	inline void SetDataType(const dds_string& type)
	{
		Pointer->Type = type;
	}
protected:
	DataItemInternal* Pointer;
};
class DataNode
{
public:
	dds_string Name;
	inline DataNode()
	{
	}
	inline DataNode(const dds_string& name)
	{
		Name = name;
	}
	inline DataNode(std::ifstream& stream)
	{
		Read(stream);
	}
	inline void Read(std::ifstream& stream)
	{
		Name = ReadString(stream);
		int count;
		stream.read((char*)&count, sizeof(int));
		while (count--)
		{
			DataNode tmp(stream);
			if (SubNodes.count(tmp.Name) || Values.count(tmp.Name))
				throw new std::exception("Node name duplicated!");
			SubNodes[tmp.Name] = tmp;
		}
		stream.read((char*)&count, sizeof(int));
		while (count--)
		{
			DataItem tmp(stream);
			if (SubNodes.count(tmp.Name) || Values.count(tmp.Name))
				throw new std::exception("Item name duplicated!");
			Values[tmp.Name] = tmp;
		}
	}
	inline void Write(std::ofstream& stream) const
	{
		WriteString(stream, Name);
		int count = (int)SubNodes.size();
		stream.write((char*)&count, sizeof(int));
		for (std::map<dds_string, DataNode>::const_iterator it = SubNodes.begin(); it != SubNodes.end(); it++)
			it->second.Write(stream);
		count = (int)Values.size();
		stream.write((char*)&count, sizeof(int));
		for (std::map<dds_string, DataItem>::const_iterator it = Values.begin(); it != Values.end(); it++)
			it->second.Write(stream);
	}
	inline DataItem& GetValue(const dds_string& name)
	{
		int index = -1;
		DataNode* current = this;
		while (true)
		{
			int next = name.find(TEXT("."), index + 1);
			if (next == dds_string::npos)
			{
				dds_string itemname = name.substr(index + 1);
				if (!current->Values.count(itemname))
					current->Values[itemname] = DataItem(itemname);
				return current->Values[itemname];
			}
			else
			{
				dds_string nodename = name.substr(index + 1, next - index - 1);
				if (!current->SubNodes.count(nodename))
					current->SubNodes[nodename] = DataNode(nodename);
				current = &current->SubNodes[nodename];
				index = next;
			}
		}
	}
	inline DataNode& GetSubNode(const dds_string& name)
	{
		int index = -1;
		DataNode* current = this;
		while (true)
		{
			int next = name.find(TEXT("."), index + 1);
			if (next == dds_string::npos)
			{
				dds_string itemname = name.substr(index + 1);
				if (!current->SubNodes.count(itemname))
					current->SubNodes[itemname] = DataNode(itemname);
				return current->SubNodes[itemname];
			}
			else
			{
				dds_string nodename = name.substr(index + 1, next - index - 1);
				if (!current->SubNodes.count(nodename))
					current->SubNodes[nodename] = DataNode(nodename);
				current = &current->SubNodes[nodename];
				index = next;
			}
		}
	}
	inline bool ValueExists(const dds_string& name) const
	{
		int index = -1;
		DataNode* current = (DataNode*)this;
		while (true)
		{
			int next = name.find(TEXT("."), index + 1);
			if (next == dds_string::npos)
			{
				dds_string itemname = name.substr(index + 1);
				return current->Values.count(itemname) == 1;
			}
			else
			{
				dds_string nodename = name.substr(index + 1, next - index - 1);
				if (!current->SubNodes.count(nodename))
					return false;
				current = &current->SubNodes[nodename];
				index = next;
			}
		}
	}
	inline bool SubNodeExists(const dds_string& name) const
	{
		int index = -1;
		DataNode* current = (DataNode*)this;
		while (true)
		{
			int next = name.find(TEXT("."), index + 1);
			if (next == dds_string::npos)
			{
				dds_string itemname = name.substr(index + 1);
				return current->SubNodes.count(itemname) == 1;
			}
			else
			{
				dds_string nodename = name.substr(index + 1, next - index - 1);
				if (!current->SubNodes.count(nodename))
					return false;
				current = &current->SubNodes[nodename];
				index = next;
			}
		}
	}
	inline void RemoveSubNode(const dds_string& name)
	{
		int index = -1;
		DataNode* current = this;
		while (true)
		{
			int next = name.find(TEXT("."), index + 1);
			if (next == dds_string::npos)
			{
				dds_string nodename = name.substr(index + 1);
				if (!current->SubNodes.count(nodename))
					throw new std::exception("DataNode doesn't exist!");
				current->SubNodes.erase(nodename);
			}
			else
			{
				dds_string nodename = name.substr(index + 1, next - index - 1);
				if (!current->SubNodes.count(nodename))
					throw new std::exception("DataNode doesn't exist!");
				current = &current->SubNodes[nodename];
				index = next;
			}
		}
	}
	inline void RemoveValue(const dds_string& name)
	{
		int index = -1;
		DataNode* current = this;
		while (true)
		{
			int next = name.find(TEXT("."), index + 1);
			if (next == dds_string::npos)
			{
				dds_string valuename = name.substr(index + 1);
				if (!current->Values.count(valuename))
					throw new std::exception("Value doesn't exist!");
				current->Values.erase(valuename);
			}
			else
			{
				dds_string nodename = name.substr(index + 1, next - index - 1);
				if (!current->SubNodes.count(nodename))
					throw new std::exception("DataNode doesn't exist!");
				current = &current->SubNodes[nodename];
				index = next;
			}
		}
	}
protected:
	std::map<dds_string, DataNode> SubNodes;
	std::map<dds_string, DataItem> Values;
};
class DataFile
{
public:
	inline DataFile() 
	{
		Version = DDS_VERSION;
		Magic[0] = 'D';
		Magic[1] = 'D';
		Magic[2] = 'S';
		Magic[3] = '\0';
		RootNode.Name = TEXT("RootNode");
	}
	inline ~DataFile()
	{
		if (FileName != TEXT(""))
			Save();
	}
	inline DataFile(dds_string filename)
	{
		Read(std::ifstream(filename));
		FileName = filename;
	}
	inline void Read(std::ifstream stream)
	{
		if (!stream.is_open())
			throw new std::exception("Unable to read!");
		stream.read(Magic, sizeof(Magic));
		if (Magic[0] != 'D' || Magic[1] != 'D' || Magic[2] != 'S' || Magic[3] != '\0')
			throw new std::exception("Magic mismatched!");
		stream.read((char*)&Version, sizeof(int));
		if (Version != DDS_VERSION)
			throw new std::exception("Version mismatched!");
		RootNode.Read(stream);
	}
	inline void Write(std::ofstream stream)
	{
		if (!stream.is_open())
			throw new std::exception("Unable to write!");
		stream.write(Magic, sizeof(Magic));
		stream.write((char*)&Version, sizeof(int));
		RootNode.Write(stream);
	}
	inline void Save()
	{
		Write(std::ofstream(FileName));
	}
	inline void Save(dds_string filename)
	{
		Write(std::ofstream(filename));
		FileName = filename;
	}
	DataNode RootNode;
	inline int GetVersion()
	{
		return Version;
	}
protected:
	char Magic[4];
	int Version;
	dds_string FileName;
};
#endif
