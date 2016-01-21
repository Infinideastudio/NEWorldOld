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
#include <vector>
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
class dds_exception :public std::exception
{
public:
	inline dds_exception(char const* const _Message)
	{
		message = new char[strlen(_Message) + 1];
		strcpy(message, _Message);
	}
	inline ~dds_exception()
	{
		delete[] message;
	}
	inline char const* what() const
	{
		return message;
	}
protected:
	char* message;
};
class DataItemInternal
{
public:
	dds_string Type;
	char* Data;
	int Length;
	int Count;
	inline DataItemInternal()
		:Count(1), Data(new char[2]), Length(0)
	{
		Data[1] = Data[0] = 0;
	}
	inline DataItemInternal(const dds_string& type)
		:Count(1), Type(type)
	{
		int size = 0;
		if (Type == "boolean" || Type == "int8" || Type == "sint8")
			size = 1;
		if (Type == "int16" || Type == "uint16")
			size = 2;
		if (Type == "int32" || Type == "uint32" || Type == "float")
			size = 4;
		if (Type == "int64" || Type == "uint64" || Type == "double")
			size = 8;
		Data = new char[size + 2];
		Data[size + 1] = Data[size] = 0;
		Length = size;
	}
	inline DataItemInternal(const dds_string& type, const char* data, int length)
		:Count(1), Type(type), Length(length)
	{
		Data = new char[length + 2];
		Data[length + 1] = Data[length] = 0;
		memcpy(Data, data, length);
	}
	inline ~DataItemInternal()
	{
		delete[] Data;
	}
};
class HelperFunctions
{
public:
	inline static dds_string ReadString(std::ifstream& stream)
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
	inline static void WriteString(std::ofstream& stream, const dds_string& str)
	{
		int length = (int)str.size();
		stream.write((char*)&length, sizeof(int));
		stream.write((char*)str.c_str(), length*sizeof(dds_char));
	}
	inline static std::vector<dds_string> SplitString(const dds_string& str)
	{
		std::vector<dds_string> ret;
		int index = 0;
		while (true)
		{
			int next = str.find(TEXT("."), index);
			if (next == dds_string::npos)
			{
				ret.push_back(str.substr(index));
				return ret;
			}
			else
			{
				ret.push_back(str.substr(index, next - index));
				index = next + 1;
			}
		}
	}
	inline static dds_string GetLast(const dds_string& str)
	{
		return str.substr(str.find_last_of(TEXT(".")) + 1);
	}
};
typedef class DataItemBoolean
{
public:
	bool Value;
	inline DataItemBoolean(DataItemInternal* data)
	{
		ptr = data;
		if (ptr->Type != TEXT("boolean"))
			throw new dds_exception("Type mismatched!");
		Value = *(bool*)ptr->Data;
	}
	inline void Commit()
	{
		if (ptr->Type != TEXT("boolean"))
			throw new dds_exception("Type mismatched!");
		*(bool*)ptr->Data = Value;
	}
	inline ~DataItemBoolean()
	{
		Commit();
	}
	inline void SetValue(bool value)
	{
		Value = value;
	}
protected:
	DataItemInternal* ptr;
}DataItemBool;
typedef class DataItemInt8
{
public:
	char Value;
	inline DataItemInt8(DataItemInternal* data)
	{
		ptr = data;
		if (ptr->Type != TEXT("int8"))
			throw new dds_exception("Type mismatched!");
		Value = *(char*)ptr->Data;
	}
	inline void Commit()
	{
		if (ptr->Type != TEXT("int8"))
			throw new dds_exception("Type mismatched!");
		*(char*)ptr->Data = Value;
	}
	inline ~DataItemInt8()
	{
		Commit();
	}
	inline void SetValue(char value)
	{
		Value = value;
	}
protected:
	DataItemInternal* ptr;
}DataItemChar;
typedef class DataItemSInt8
{
public:
	signed char Value;
	inline DataItemSInt8(DataItemInternal* data)
	{
		ptr = data;
		if (ptr->Type != TEXT("sint8"))
			throw new dds_exception("Type mismatched!");
		Value = *(signed char*)ptr->Data;
	}
	inline void Commit()
	{
		if (ptr->Type != TEXT("sint8"))
			throw new dds_exception("Type mismatched!");
		*(signed char*)ptr->Data = Value;
	}
	inline ~DataItemSInt8()
	{
		Commit();
	}
	inline void SetValue(signed char value)
	{
		Value = value;
	}
protected:
	DataItemInternal* ptr;
}DataItemSignedChar;
typedef class DataItemInt16
{
public:
	short Value;
	inline DataItemInt16(DataItemInternal* data)
	{
		ptr = data;
		if (ptr->Type != TEXT("int16"))
			throw new dds_exception("Type mismatched!");
		Value = *(short*)ptr->Data;
	}
	inline void Commit()
	{
		if (ptr->Type != TEXT("int16"))
			throw new dds_exception("Type mismatched!");
		*(short*)ptr->Data = Value;
	}
	inline ~DataItemInt16()
	{
		Commit();
	}
	inline void SetValue(short value)
	{
		Value = value;
	}
protected:
	DataItemInternal* ptr;
}DataItemShort;
typedef class DataItemUInt16
{
public:
	unsigned short Value;
	inline DataItemUInt16(DataItemInternal* data)
	{
		ptr = data;
		if (ptr->Type != TEXT("uint16"))
			throw new dds_exception("Type mismatched!");
		Value = *(unsigned short*)ptr->Data;
	}
	inline void Commit()
	{
		if (ptr->Type != TEXT("uint16"))
			throw new dds_exception("Type mismatched!");
		*(unsigned short*)ptr->Data = Value;
	}
	inline ~DataItemUInt16()
	{
		Commit();
	}
	inline void SetValue(unsigned short value)
	{
		Value = value;
	}
protected:
	DataItemInternal* ptr;
}DataItemUnsignedShort;
typedef class DataItemInt32
{
public:
	int Value;
	inline DataItemInt32(DataItemInternal* data)
	{
		ptr = data;
		if (ptr->Type != TEXT("int32"))
			throw new dds_exception("Type mismatched!");
		Value = *(int*)ptr->Data;
	}
	inline void Commit()
	{
		if (ptr->Type != TEXT("int32"))
			throw new dds_exception("Type mismatched!");
		*(int*)ptr->Data = Value;
	}
	inline ~DataItemInt32()
	{
		Commit();
	}
	inline void SetValue(int value)
	{
		Value = value;
	}
protected:
	DataItemInternal* ptr;
}DataItemInteger;
typedef class DataItemUInt32
{
public:
	unsigned int Value;
	inline DataItemUInt32(DataItemInternal* data)
	{
		ptr = data;
		if (ptr->Type != TEXT("uint32"))
			throw new dds_exception("Type mismatched!");
		Value = *(unsigned int*)ptr->Data;
	}
	inline void Commit()
	{
		if (ptr->Type != TEXT("uint32"))
			throw new dds_exception("Type mismatched!");
		*(unsigned int*)ptr->Data = Value;
	}
	inline ~DataItemUInt32()
	{
		Commit();
	}
	inline void SetValue(unsigned int value)
	{
		Value = value;
	}
protected:
	DataItemInternal* ptr;
}DataItemUnsignedInteger;
typedef class DataItemInt64
{
public:
	long long Value;
	inline DataItemInt64(DataItemInternal* data)
	{
		ptr = data;
		if (ptr->Type != TEXT("int64"))
			throw new dds_exception("Type mismatched!");
		Value = *(long long*)ptr->Data;
	}
	inline void Commit()
	{
		if (ptr->Type != TEXT("int64"))
			throw new dds_exception("Type mismatched!");
		*(long long*)ptr->Data = Value;
	}
	inline ~DataItemInt64()
	{
		Commit();
	}
	inline void SetValue(long long value)
	{
		Value = value;
	}
protected:
	DataItemInternal* ptr;
}DataItemLongLong;
typedef class DataItemUInt64
{
public:
	unsigned long long Value;
	inline DataItemUInt64(DataItemInternal* data)
	{
		ptr = data;
		if (ptr->Type != TEXT("uint64"))
			throw new dds_exception("Type mismatched!");
		Value = *(unsigned long long*)ptr->Data;
	}
	inline void Commit()
	{
		if (ptr->Type != TEXT("uint64"))
			throw new dds_exception("Type mismatched!");
		*(unsigned long long*)ptr->Data = Value;
	}
	inline ~DataItemUInt64()
	{
		Commit();
	}
	inline void SetValue(unsigned long long value)
	{
		Value = value;
	}
protected:
	DataItemInternal* ptr;
}DataItemUnsignedLongLong;
class DataItemFloat
{
public:
	float Value;
	inline DataItemFloat(DataItemInternal* data)
	{
		ptr = data;
		if (ptr->Type != TEXT("float"))
			throw new dds_exception("Type mismatched!");
		Value = *(float*)ptr->Data;
	}
	inline void Commit()
	{
		if (ptr->Type != TEXT("float"))
			throw new dds_exception("Type mismatched!");
		*(float*)ptr->Data = Value;
	}
	inline ~DataItemFloat()
	{
		Commit();
	}
	inline void SetValue(float value)
	{
		Value = value;
	}
protected:
	DataItemInternal* ptr;
};
class DataItemDouble
{
public:
	double Value;
	inline DataItemDouble(DataItemInternal* data)
	{
		ptr = data;
		if (ptr->Type != TEXT("double"))
			throw new dds_exception("Type mismatched!");
		Value = *(double*)ptr->Data;
	}
	inline void Commit()
	{
		if (ptr->Type != TEXT("double"))
			throw new dds_exception("Type mismatched!");
		*(double*)ptr->Data = Value;
	}
	inline ~DataItemDouble()
	{
		Commit();
	}
	inline void SetValue(double value)
	{
		Value = value;
	}
protected:
	DataItemInternal* ptr;
};
class DataItemString
{
public:
	std::string Value;
	inline DataItemString(DataItemInternal* data)
	{
		ptr = data;
		if (ptr->Type != TEXT("string"))
			throw new dds_exception("Type mismatched!");
		Value = ptr->Data;
	}
	inline void Commit()
	{
		if (ptr->Type != TEXT("string"))
			throw new dds_exception("Type mismatched!");
		if ((int)Value.size()*(int)sizeof(char) != ptr->Length)
		{
			delete[] ptr->Data;
			ptr->Length = (int)Value.size()*sizeof(char);
			ptr->Data = new char[(int)Value.size()*sizeof(char) + 2];
			ptr->Data[(int)Value.size()*sizeof(char) + 1] = ptr->Data[(int)Value.size()*sizeof(char)] = 0;
		}
		strcpy(ptr->Data, Value.c_str());
	}
	inline ~DataItemString()
	{
		Commit();
	}
	inline void SetValue(const std::string& value)
	{
		Value = value;
	}
protected:
	DataItemInternal* ptr;
};
class DataItemWString
{
public:
	std::wstring Value;
	inline DataItemWString(DataItemInternal* data)
	{
		ptr = data;
		if (ptr->Type != TEXT("wstring"))
			throw new dds_exception("Type mismatched!");
		Value = (wchar_t*)ptr->Data;
	}
	inline void Commit()
	{
		if (ptr->Type != TEXT("wstring"))
			throw new dds_exception("Type mismatched!");
		if ((int)Value.size()*(int)sizeof(wchar_t) != ptr->Length)
		{
			delete[] ptr->Data;
			ptr->Length = (int)Value.size()*sizeof(wchar_t);
			ptr->Data = new char[(int)Value.size()*sizeof(wchar_t) + 2];
			ptr->Data[(int)Value.size()*sizeof(wchar_t) + 1] = ptr->Data[(int)Value.size()*sizeof(wchar_t)] = 0;
		}
		wcscpy((wchar_t*)ptr->Data, Value.c_str());
	}
	inline ~DataItemWString()
	{
		Commit();
	}
	inline void SetValue(const std::wstring& value)
	{
		Value = value;
	}
protected:
	DataItemInternal* ptr;
};
class DataItem
{
public:
	dds_string Name;
	inline DataItem()
	{
		ptr = new DataItemInternal;
	}
	inline DataItem(const dds_string& name)
	{
		ptr = new DataItemInternal;
		Name = name;
	}
	inline DataItem(const dds_string& name, const dds_string& type)
	{
		ptr = new DataItemInternal(type);
		Name = name;
	}
	inline DataItem(const dds_string& name, const dds_string& type, const char* data, int length)
	{
		ptr = new DataItemInternal(type, data, length);
		Name = name;
	}
	inline DataItem(const DataItem& src)
	{
		ptr = src.ptr;
		ptr->Count++;
		Name = src.Name;
	}
	inline DataItem& operator=(const DataItem& src)
	{
		ptr = src.ptr;
		ptr->Count++;
		Name = src.Name;
		return *this;
	}
	inline ~DataItem()
	{
		ptr->Count--;
		if (!ptr->Count)
			delete ptr;
	}
	inline DataItem(std::ifstream& stream)
	{
		Read(stream);
	}
	inline void Read(std::ifstream& stream)
	{
		ptr = new DataItemInternal;
		ptr->Count = 1;
		Name = HelperFunctions::ReadString(stream);
		ptr->Type = HelperFunctions::ReadString(stream);
		stream.read((char*)&ptr->Length, sizeof(int));
		ptr->Data = new char[ptr->Length + 2];
		stream.read(ptr->Data, ptr->Length);
		ptr->Data[ptr->Length + 1] = ptr->Data[ptr->Length] = 0;
	}
	inline void Write(std::ofstream& stream) const
	{
		HelperFunctions::WriteString(stream, Name);
		HelperFunctions::WriteString(stream, ptr->Type);
		stream.write((char*)&ptr->Length, sizeof(int));
		stream.write(ptr->Data, ptr->Length);
	}
	inline DataItemBoolean ToBoolean()
	{
		return DataItemBoolean(ptr);
	}
	inline DataItemInt8 ToInt8()
	{
		return DataItemInt8(ptr);
	}
	inline DataItemSInt8 ToUInt8()
	{
		return DataItemSInt8(ptr);
	}
	inline DataItemInt16 ToInt16()
	{
		return DataItemInt16(ptr);
	}
	inline DataItemUInt16 ToUInt16()
	{
		return DataItemUInt16(ptr);
	}
	inline DataItemInt32 ToInt32()
	{
		return DataItemInt32(ptr);
	}
	inline DataItemUInt32 ToUInt32()
	{
		return DataItemUInt32(ptr);
	}
	inline DataItemInt64 ToInt64()
	{
		return DataItemInt64(ptr);
	}
	inline DataItemUInt64 ToUInt64()
	{
		return DataItemUInt64(ptr);
	}
	inline DataItemFloat ToFloat()
	{
		return DataItemFloat(ptr);
	}
	inline DataItemDouble ToDouble()
	{
		return DataItemDouble(ptr);
	}
	inline DataItemString ToString()
	{
		return DataItemString(ptr);
	}
	inline DataItemWString ToWString()
	{
		return DataItemWString(ptr);
	}
	inline char* GetRawData()
	{
		return ptr->Data;
	}
	inline void SetRawData(const dds_string& type, const char* data, int length)
	{
		ptr->Count--;
		if (!ptr->Count)
			delete ptr;
		DataItemInternal* tmp = new DataItemInternal;
		tmp->Type = type;
		tmp->Length = length;
		tmp->Data = new char[length + 2];
		memcpy(tmp->Data, data, length);
		tmp->Data[length + 1] = tmp->Data[length] = 0;
		tmp->Count = 1;
		ptr = tmp;
	}
	inline int GetDataLength() const
	{
		return ptr->Length;
	}
	inline dds_string GetDataType() const
	{
		return ptr->Type;
	}
	inline void SetDataType(const dds_string& type)
	{
		ptr->Type = type;
	}
protected:
	DataItemInternal* ptr;
};
class DataTree
{
public:
	dds_string Name;
	inline DataTree()
	{
	}
	inline DataTree(const dds_string& name)
	{
		Name = name;
	}
	inline DataTree(std::ifstream& stream)
	{
		Read(stream);
	}
	inline void Read(std::ifstream& stream)
	{
		Name = HelperFunctions::ReadString(stream);
		int count;
		stream.read((char*)&count, sizeof(int));
		while (count--)
		{
			DataTree tmp(stream);
			if (SubNodes.count(tmp.Name) || Values.count(tmp.Name))
				throw new dds_exception("Node name duplicated!");
			SubNodes[tmp.Name] = tmp;
		}
		stream.read((char*)&count, sizeof(int));
		while (count--)
		{
			DataItem tmp(stream);
			if (SubNodes.count(tmp.Name) || Values.count(tmp.Name))
				throw new dds_exception("Item name duplicated!");
			Values[tmp.Name] = tmp;
		}
	}
	inline void Write(std::ofstream& stream) const
	{
		HelperFunctions::WriteString(stream, Name);
		int count = (int)SubNodes.size();
		stream.write((char*)&count, sizeof(int));
		for (std::map<dds_string, DataTree>::const_iterator it = SubNodes.begin(); it != SubNodes.end(); it++)
			it->second.Write(stream);
		count = (int)Values.size();
		stream.write((char*)&count, sizeof(int));
		for (std::map<dds_string, DataItem>::const_iterator it = Values.begin(); it != Values.end(); it++)
			it->second.Write(stream);
	}
	inline DataItem& CreateValue(const dds_string& name, const dds_string type)
	{
		DataTree* ret = GetSubNodeInternal(name, false);
		dds_string last = HelperFunctions::GetLast(name);
		if (ret->Values.count(last))
			throw new dds_exception("Value already exists!");
		ret->Values[last] = DataItem(last, type);
		return ret->Values[last];
	}
	inline DataTree& CreateSubNode(const dds_string& name)
	{
		DataTree* ret = GetSubNodeInternal(name, false);
		dds_string last = HelperFunctions::GetLast(name);
		if (ret->SubNodes.count(last))
			throw new dds_exception("Value already exists!");
		ret->SubNodes[last] = DataTree(last);
		return ret->SubNodes[last];
	}
	inline DataItem& GetValue(const dds_string& name)
	{
		DataTree* ret = GetSubNodeInternal(name, true);
		dds_string last = HelperFunctions::GetLast(name);
		if (!ret->Values.count(last))
			throw new dds_exception("Value doesn't exist!");
		return ret->Values[last];
	}
	inline DataTree& GetSubNode(const dds_string& name)
	{
		DataTree* ret = GetSubNodeInternal(name, true);
		dds_string last = HelperFunctions::GetLast(name);
		if (!ret->SubNodes.count(last))
			throw new dds_exception("DataNode doesn't exist!");
		return ret->SubNodes[last];
	}
	inline bool ValueExists(const dds_string& name) const
	{
		int index = -1;
		DataTree* current = (DataTree*)this;
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
		DataTree* current = (DataTree*)this;
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
		DataTree* current = this;
		while (true)
		{
			int next = name.find(TEXT("."), index + 1);
			if (next == dds_string::npos)
			{
				dds_string nodename = name.substr(index + 1);
				if (!current->SubNodes.count(nodename))
					throw new dds_exception("DataTree doesn't exist!");
				current->SubNodes.erase(nodename);
			}
			else
			{
				dds_string nodename = name.substr(index + 1, next - index - 1);
				if (!current->SubNodes.count(nodename))
					throw new dds_exception("DataTree doesn't exist!");
				current = &current->SubNodes[nodename];
				index = next;
			}
		}
	}
	inline void RemoveValue(const dds_string& name)
	{
		int index = -1;
		DataTree* current = this;
		while (true)
		{
			int next = name.find(TEXT("."), index + 1);
			if (next == dds_string::npos)
			{
				dds_string valuename = name.substr(index + 1);
				if (!current->Values.count(valuename))
					throw new dds_exception("Value doesn't exist!");
				current->Values.erase(valuename);
			}
			else
			{
				dds_string nodename = name.substr(index + 1, next - index - 1);
				if (!current->SubNodes.count(nodename))
					throw new dds_exception("DataTree doesn't exist!");
				current = &current->SubNodes[nodename];
				index = next;
			}
		}
	}
protected:
	std::map<dds_string, DataTree> SubNodes;
	std::map<dds_string, DataItem> Values;
	inline DataTree* GetSubNodeInternal(const dds_string& name, bool throw_exception)
	{
		std::vector<dds_string> items = HelperFunctions::SplitString(name);
		DataTree* ret = this;
		for (int i = 0; i < (int)items.size() - 1; i++)
		{
			dds_string& tmp = items[i];
			if (!ret->SubNodes.count(tmp))
			{
				if (throw_exception)
					throw new dds_exception("DataNode doesn't exist!");
				else
					ret->SubNodes[tmp] = DataTree(tmp);
			}
			ret = &ret->SubNodes[tmp];
		}
		return ret;
	}
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
			throw new dds_exception("Unable to read!");
		stream.read(Magic, sizeof(Magic));
		if (Magic[0] != 'D' || Magic[1] != 'D' || Magic[2] != 'S' || Magic[3] != '\0')
			throw new dds_exception("Magic mismatched!");
		stream.read((char*)&Version, sizeof(int));
		if (Version != DDS_VERSION)
			throw new dds_exception("Version mismatched!");
		RootNode.Read(stream);
	}
	inline void Write(std::ofstream stream)
	{
		if (!stream.is_open())
			throw new dds_exception("Unable to write!");
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
	DataTree RootNode;
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
