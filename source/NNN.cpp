#include <iostream>
#include <stdint.h>
#include <string.h>
#include <typeinfo>
#include <utility>

#include "NNN.h"


namespace NNN {

    namespace ByteOrder {

        void convertLE2NativeU16(uint16_t* data, uintptr_t size) {

            for(uint32_t i = 0; i < size; i++) {

                uint8_t* data_ = (uint8_t*)(data + i);
                data[i] = (uint16_t)data_[0] | ((uint16_t)data_[1] << 8);

            }

        }

        void convertLE2NativeU32(uint32_t* data, uintptr_t size) {

            for(uint32_t i = 0; i < size; i++) {

                uint8_t* data_ = (uint8_t*)(data + i);
                data[i] = (uint32_t)data_[0] | ((uint32_t)data_[1] << 8)
                    | ((uint32_t)data_[2] << 16) | ((uint32_t)data_[3] << 24);

            }

        }

        void convertLE2NativeU64(uint64_t* data, uintptr_t size) {

            for(uint32_t i = 0; i < size; i++) {

                uint8_t* data_ = (uint8_t*)(data + i);
                data[i] = (uint64_t)data_[0] | ((uint64_t)data_[1] << 8)
                    | ((uint64_t)data_[2] << 16) | ((uint64_t)data_[3] << 24)
                    | ((uint64_t)data_[4] << 32) | ((uint64_t)data_[5] << 40)
                    | ((uint64_t)data_[6] << 48) | ((uint64_t)data_[7] << 56);

            }

        }

        void convertLE2NativeS16(int16_t* data, uintptr_t size) {

            convertLE2NativeU16((uint16_t*)data, size);

        }

        void convertLE2NativeS32(int32_t* data, uintptr_t size) {

            convertLE2NativeU32((uint32_t*)data, size);

        }

        void convertLE2NativeS64(int64_t* data, uintptr_t size) {

            convertLE2NativeU64((uint64_t*)data, size);

        }

        void convertLE2NativeF32(float* data, uintptr_t size) {

            convertLE2NativeU32((uint32_t*)data, size);

        }

        void convertLE2NativeF64(double* data, uintptr_t size) {

            convertLE2NativeU64((uint64_t*)data, size);

        }


        void convertNative2LEU16(uint16_t* data, uintptr_t size) {

            for(uint32_t i = 0; i < size; i++) {

                uint16_t d = data[i];
                uint8_t* data_ = (uint8_t*)(data + i);
                data_[0] = d & 0xFF;
                data_[1] = (d >> 8) & 0xFF;

            }

        }

        void convertNative2LEU32(uint32_t* data, uintptr_t size) {

            for(uint32_t i = 0; i < size; i++) {

                uint32_t d = data[i];
                uint8_t* data_ = (uint8_t*)(data + i);
                data_[0] = d & 0xFF;
                data_[1] = (d >> 8) & 0xFF;
                data_[2] = (d >> 16) & 0xFF;
                data_[3] = (d >> 24) & 0xFF;

            }

        }

        void convertNative2LEU64(uint64_t* data, uintptr_t size) {

            for(uint32_t i = 0; i < size; i++) {

                uint64_t d = data[i];
                uint8_t* data_ = (uint8_t*)(data + i);
                data_[0] = d & 0xFF;
                data_[1] = (d >> 8) & 0xFF;
                data_[2] = (d >> 16) & 0xFF;
                data_[3] = (d >> 24) & 0xFF;
                data_[4] = (d >> 32) & 0xFF;
                data_[5] = (d >> 40) & 0xFF;
                data_[6] = (d >> 48) & 0xFF;
                data_[7] = (d >> 56) & 0xFF;

            }

        }

        void convertNative2LES16(int16_t* data, uintptr_t size) {

            convertNative2LEU16((uint16_t*)data, size);

        }

        void convertNative2LES32(int32_t* data, uintptr_t size) {

            convertNative2LEU32((uint32_t*)data, size);

        }

        void convertNative2LES64(int64_t* data, uintptr_t size) {

            convertNative2LEU64((uint64_t*)data, size);

        }

        void convertNative2LEF32(float* data, uintptr_t size) {

            convertNative2LEU32((uint32_t*)data, size);

        }

        void convertNative2LEF64(double* data, uintptr_t size) {

            convertNative2LEU64((uint64_t*)data, size);

        }

    }


    bool Magic::operator !=(const Magic &src) const {

        return (data[0] != src.data[0])
            || (data[1] != src.data[1])
            || (data[2] != src.data[2])
            || (data[3] != src.data[3]);

    }


    bool Version::operator >(const Version &src) const {

        return (data[0] > src.data[0])
            || ((data[0] == src.data[0]) && (data[1] > src.data[1]))
            || ((data[1] == src.data[1]) && (data[2] > src.data[2]))
            || ((data[2] == src.data[2]) && (data[3] > src.data[3]));

    }


    Node::Node() {

    }

    Node::Node(const Node& src) {

    }

    Node::~Node() {

    }


    NodePackage::NodePackage() :
        data(NULL),
        size(0) {

    }

    NodePackage::NodePackage(const NodePackage& src) :
        data(NULL),
        size(0) {

        if(src.data != NULL) {

            size = src.size;
            data = new std::map<std::string, Node*>[size];
            for(uint32_t i = 0; i < size; i++) {

                data[i] = src.data[i];
                for(std::map<std::string, Node*>::iterator it = data[i].begin();
                it != data[i].end();
                    ++it) {

                    if(it->second != NULL) {

                        it->second = it->second->clone();

                    }

                }

            }

        }

    }

    NodePackage::~NodePackage() {

        if(data != NULL) {

            cleanMap(data, size);
            delete[] data;

        }

    }

    Node* NodePackage::clone() {

        return new NodePackage(*this);

    }

    bool NodePackage::read(std::istream& in, Information& info) {

        uint8_t flags;
        in.read((char*)&flags, 1);
        if(!in) {

            return false;

        }

        uint32_t size_;
        in.read((char*)&size_, 4);
        if(!in) {

            return false;

        }
        ByteOrder::convertLE2NativeU32(&size_, 1);

        info.depth++;

        std::map<std::string, Node*>* data_;
        data_ = new std::map<std::string, Node*>[size_];
        for(uint32_t i = 0; i < size_; i++) {

            if(info.depth >= 128) {

                return false;

            }

            while(true) {

                uint8_t type;
                in.read((char*)&type, 1);
                if(!in) {

                    cleanMap(data_, i + 1);
                    delete[] data_;

                    info.depth--;

                    return false;

                }

                if(type == 0x00) {

                    break;

                }

                Node* node;

                switch(type) {

                    case 0x01:
                    {

                        node = (Node*)new NodePackage();
                        break;

                    }
                    case 0x02:
                    {

                        node = (Node*)new NodeString();
                        break;

                    }
                    case 0x03:
                    {

                        node = (Node*)new NodeU8();
                        break;

                    }
                    case 0x04:
                    {

                        node = (Node*)new NodeU16();
                        break;

                    }
                    case 0x05:
                    {

                        node = (Node*)new NodeU32();
                        break;

                    }
                    case 0x06:
                    {

                        node = (Node*)new NodeU64();
                        break;

                    }
                    case 0x07:
                    {

                        node = (Node*)new NodeS8();
                        break;

                    }
                    case 0x08:
                    {

                        node = (Node*)new NodeS16();
                        break;

                    }
                    case 0x09:
                    {

                        node = (Node*)new NodeS32();
                        break;

                    }
                    case 0x0A:
                    {

                        node = (Node*)new NodeS64();
                        break;

                    }
                    case 0x0B:
                    {

                        node = (Node*)new NodeF32();
                        break;

                    }
                    case 0x0C:
                    {

                        node = (Node*)new NodeF64();
                        break;

                    }
                    default:
                    {

                        cleanMap(data_, i + 1);
                        delete[] data_;

                        info.depth--;

                        return false;

                    }

                }

                uint16_t namelen;
                in.read((char*)&namelen, 2);
                if(!in) {

                    cleanMap(data_, i + 1);
                    delete[] data_;

                    info.depth--;

                    return false;

                }

                std::string name;
                if(namelen != 0) {

                    char* namedata = new char[namelen];
                    in.read(namedata, namelen);
                    if(!in) {

                        delete[] namedata;

                        cleanMap(data_, i + 1);
                        delete[] data_;

                        info.depth--;

                        return false;

                    }
                    name.append(namedata, namelen);
                    delete[] namedata;

                }

                if(!node->read(in, info)) {

                    cleanMap(data_, i + 1);
                    delete[] data_;

                    info.depth--;

                    return false;

                }

                if(!data_[i].insert(
                    std::pair<std::string, Node*>(name, node)).second) {

                    cleanMap(data_, i + 1);
                    delete[] data_;

                    info.depth--;

                }

            }

        }

        info.depth--;

        if(data != NULL) {

            delete[] data;

        }

        data = data_;
        size = size_;

        return true;

    }

    bool NodePackage::write(std::ostream& out, Information& info) {

        if(info.depth >= 128) {

            return false;

        }

        uint8_t flags = 0x00;
        out.write((char*)&flags, 1);
        if(!out) {

            return false;

        }

        uint32_t size_ = data == NULL ? 0 : size;
        ByteOrder::convertNative2LEU32(&size_, 1);
        out.write((char*)&size_, 4);
        if(!out) {

            return false;

        }
        ByteOrder::convertLE2NativeU32(&size_, 1);

        if(size_ != 0) {

            info.depth++;

            for(uint32_t i = 0; i < size_; i++) {

                for(std::map<std::string, Node*>::iterator it = data[i].begin();
                it != data[i].end();
                    ++it) {

                    if(it->second == NULL) {

                        info.depth--;

                        return false;

                    }

                    uint8_t type;
                    const std::type_info& typeinfo = typeid(*(it->second));
                    if(typeinfo == typeid(NodePackage)) {

                        type = 0x01;

                    } else if(typeinfo == typeid(NodeString)) {

                        type = 0x02;

                    } else if(typeinfo == typeid(NodeU8)) {

                        type = 0x03;

                    } else if(typeinfo == typeid(NodeU16)) {

                        type = 0x04;

                    } else if(typeinfo == typeid(NodeU32)) {

                        type = 0x05;

                    } else if(typeinfo == typeid(NodeU64)) {

                        type = 0x06;

                    } else if(typeinfo == typeid(NodeS8)) {

                        type = 0x07;

                    } else if(typeinfo == typeid(NodeS16)) {

                        type = 0x08;

                    } else if(typeinfo == typeid(NodeS32)) {

                        type = 0x09;

                    } else if(typeinfo == typeid(NodeS64)) {

                        type = 0x0A;

                    } else if(typeinfo == typeid(NodeF32)) {

                        type = 0x0B;

                    } else if(typeinfo == typeid(NodeF64)) {

                        type = 0x0C;

                    } else {

                        info.depth--;

                        return false;

                    }
                    out.write((char*)&type, 1);
                    if(!out) {

                        info.depth--;

                        return false;

                    }

                    if(it->first.length() > 0xFFFF) {

                        info.depth--;

                        return false;

                    }

                    uint16_t namelen = (uint16_t)it->first.length();
                    ByteOrder::convertNative2LEU16(&namelen, 1);
                    out.write((char*)&namelen, 2);
                    if(!out) {

                        info.depth--;

                        return false;

                    }

                    out.write(it->first.data(), it->first.length());
                    if(!out) {

                        info.depth--;

                        return false;

                    }

                    if(!it->second->write(out, info)) {

                        info.depth--;

                        return false;

                    }

                }

                uint8_t end = 0x00;
                out.write((char*)&end, 1);
                if(!out) {

                    info.depth--;

                    return false;

                }

            }

            info.depth--;

        }

        return true;


    }

    std::map<std::string, Node*>* NodePackage::getData() {

        return data;

    }

    const std::map<std::string, Node*>* NodePackage::getData() const {

        return data;

    }

    uint32_t NodePackage::getSize() const {

        return size;

    }

    void NodePackage::set(
        std::map<std::string, Node*>* data_,
        uint32_t size_) {

        std::map<std::string, Node*>* data__ = NULL;

        if(size_ != 0) {

            data__ = new std::map<std::string, Node*>[size_];
            for(uint32_t i = 0; i < size_; i++) {

                data__[i] = data_[i];

            }

        }

        if(data != NULL) {

            delete[] data;

        }

        data = data__;
        size = size_;

    }

    void NodePackage::cleanMap(std::map<std::string, Node*>* data_,
        uintptr_t size_) {

        for(uint32_t i = 0; i < size_; i++) {

            for(std::map<std::string, Node*>::iterator it = data_[i].begin();
            it != data_[i].end();
                ++it) {

                if(it->second != NULL) {

                    delete it->second;

                }

            }

        }

    }


    NodeString::NodeString() :
        data(NULL),
        size(0) {

    }

    NodeString::NodeString(const NodeString& src) :
        data(NULL),
        size(0) {

        if(src.data != NULL) {

            size = src.size;
            data = new std::string[size];
            for(uintptr_t i = 0; i < size; i++) {

                data[i] = src.data[i];

            }

        }

    }

    NodeString::~NodeString() {

        if(data != NULL) {

            delete[] data;

        }

    }

    Node* NodeString::clone() {

        return new NodeString(*this);

    }

    bool NodeString::read(std::istream& in, Information& info) {

        uint8_t flags;
        in.read((char*)&flags, 1);
        if(!in) {

            return false;

        }

        uint32_t size_;
        in.read((char*)&size_, 4);
        if(!in) {

            return false;

        }
        ByteOrder::convertLE2NativeU32(&size_, 1);

        std::string* data_;
        data_ = new std::string[size_];
        for(uint32_t i = 0; i < size_; i++) {

            uint16_t slen;
            in.read((char*)&slen, 2);
            if(!in) {

                delete[] data_;

                return false;

            }

            if(slen != 0) {

                char* sdata = new char[slen];
                in.read(sdata, slen);
                if(!in) {

                    delete[] sdata;

                    delete[] data_;

                    return false;

                }
                data_[i].append(sdata, slen);
                delete[] sdata;

            }

        }

        if(data != NULL) {

            delete[] data;

        }

        data = data_;
        size = size_;

        return true;

    }

    bool NodeString::write(std::ostream& out, Information& info) {

        uint8_t flags = 0x00;
        out.write((char*)&flags, 1);
        if(!out) {

            return false;

        }

        uint32_t size_ = data == NULL ? 0 : size;
        ByteOrder::convertNative2LEU32(&size_, 1);
        out.write((char*)&size_, 4);
        if(!out) {

            return false;

        }
        ByteOrder::convertLE2NativeU32(&size_, 1);

        if(size_ != 0) {

            for(uint32_t i = 0; i < size_; i++) {

                uint16_t slen = (uint16_t)data[i].length();
                ByteOrder::convertNative2LEU16(&slen, 1);
                out.write((char*)&slen, 2);
                if(!out) {

                    return false;

                }

                out.write(data[i].data(), data[i].length());
                if(!out) {

                    return false;

                }

            }

        }

        return true;


    }

    std::string* NodeString::getData() {

        return data;

    }

    const std::string* NodeString::getData() const {

        return data;

    }

    uint32_t NodeString::getSize() const {

        return size;

    }

    void NodeString::set(std::string* data_, uint32_t size_) {

        std::string* data__ = NULL;

        if(size_ != 0) {

            data__ = new std::string[size_];
            for(uintptr_t i = 0; i < size; i++) {

                data[i] = data_[i];

            }

        }

        if(data != NULL) {

            delete[] data;

        }

        data = data__;
        size = size_;

    }


    NodeU8::NodeU8() :
        data(NULL),
        size(0) {

    }

    NodeU8::NodeU8(const NodeU8& src) :
        data(NULL),
        size(0) {

        if(src.data != NULL) {

            size = src.size;
            data = new uint8_t[size];
            ::memcpy(data, src.data, size * 1);

        }

    }

    NodeU8::~NodeU8() {

        if(data != NULL) {

            delete[] data;

        }

    }

    Node* NodeU8::clone() {

        return new NodeU8(*this);

    }

    bool NodeU8::read(std::istream& in, Information& info) {

        uint8_t flags;
        in.read((char*)&flags, 1);
        if(!in) {

            return false;

        }

        uint32_t size_;
        in.read((char*)&size_, 4);
        if(!in) {

            return false;

        }
        ByteOrder::convertLE2NativeU32(&size_, 1);

        uint8_t* data_;
        data_ = new uint8_t[size_];
        in.read((char*)data_, size_ * 1);
        if(!in) {

            delete[] data_;

            return false;

        }

        if(data != NULL) {

            delete[] data;

        }

        data = data_;
        size = size_;

        return true;

    }

    bool NodeU8::write(std::ostream& out, Information& info) {

        uint8_t flags = 0x00;
        out.write((char*)&flags, 1);
        if(!out) {

            return false;

        }

        uint32_t size_ = data == NULL ? 0 : size;
        ByteOrder::convertNative2LEU32(&size_, 1);
        out.write((char*)&size_, 4);
        if(!out) {

            return false;

        }
        ByteOrder::convertLE2NativeU32(&size_, 1);

        if(size_ != 0) {

            out.write((char*)data, size_ * 1);
            if(!out) {

                return false;

            }

        }

        return true;


    }

    uint8_t* NodeU8::getData() {

        return data;

    }

    const uint8_t* NodeU8::getData() const {

        return data;

    }

    uint32_t NodeU8::getSize() const {

        return size;

    }

    void NodeU8::set(uint8_t* data_, uint32_t size_) {

        uint8_t* data__ = NULL;

        if(size_ != 0) {

            data__ = new uint8_t[size_];
            ::memcpy(data__, data_, size_ * 1);

        }

        if(data != NULL) {

            delete[] data;

        }

        data = data__;
        size = size_;

    }


    NodeU16::NodeU16() :
        data(NULL),
        size(0) {

    }

    NodeU16::NodeU16(const NodeU16& src) :
        data(NULL),
        size(0) {

        if(src.data != NULL) {

            size = src.size;
            data = new uint16_t[size];
            ::memcpy(data, src.data, size * 2);

        }

    }

    NodeU16::~NodeU16() {

        if(data != NULL) {

            delete[] data;

        }

    }

    Node* NodeU16::clone() {

        return new NodeU16(*this);

    }

    bool NodeU16::read(std::istream& in, Information& info) {

        uint8_t flags;
        in.read((char*)&flags, 1);
        if(!in) {

            return false;

        }

        uint32_t size_;
        in.read((char*)&size_, 4);
        if(!in) {

            return false;

        }
        ByteOrder::convertLE2NativeU32(&size_, 1);

        uint16_t* data_ = new uint16_t[size_];
        in.read((char*)data_, size_ * 2);
        if(!in) {

            delete[] data_;

            return false;

        }
        ByteOrder::convertLE2NativeU16(data_, size_);

        if(data != NULL) {

            delete[] data;

        }

        data = data_;
        size = size_;

        return true;

    }

    bool NodeU16::write(std::ostream& out, Information& info) {

        uint8_t flags = 0x00;
        out.write((char*)&flags, 1);
        if(!out) {

            return false;

        }

        uint32_t size_ = data == NULL ? 0 : size;
        ByteOrder::convertNative2LEU32(&size_, 1);
        out.write((char*)&size_, 4);
        if(!out) {

            return false;

        }
        ByteOrder::convertLE2NativeU32(&size_, 1);

        if(size_ != 0) {

            ByteOrder::convertNative2LEU16(data, size);
            out.write((char*)data, size * 2);
            ByteOrder::convertLE2NativeU16(data, size);
            if(!out) {

                return false;

            }

        }

        return true;


    }

    uint16_t* NodeU16::getData() {

        return data;

    }

    const uint16_t* NodeU16::getData() const {

        return data;

    }

    uint32_t NodeU16::getSize() const {

        return size;

    }

    void NodeU16::set(uint16_t* data_, uint32_t size_) {

        uint16_t* data__ = NULL;

        if(size_ != 0) {

            data__ = new uint16_t[size_];
            ::memcpy(data__, data_, size_ * 2);

        }

        if(data != NULL) {

            delete[] data;

        }

        data = data__;
        size = size_;

    }


    NodeU32::NodeU32() :
        data(NULL),
        size(0) {

    }

    NodeU32::NodeU32(const NodeU32& src) :
        data(NULL),
        size(0) {

        if(src.data != NULL) {

            size = src.size;
            data = new uint32_t[size];
            ::memcpy(data, src.data, size * 4);

        }

    }

    NodeU32::~NodeU32() {

        if(data != NULL) {

            delete[] data;

        }

    }

    Node* NodeU32::clone() {

        return new NodeU32(*this);

    }

    bool NodeU32::read(std::istream& in, Information& info) {

        uint8_t flags;
        in.read((char*)&flags, 1);
        if(!in) {

            return false;

        }

        uint32_t size_;
        in.read((char*)&size_, 4);
        if(!in) {

            return false;

        }
        ByteOrder::convertLE2NativeU32(&size_, 1);

        uint32_t* data_ = new uint32_t[size_];
        in.read((char*)data_, size_ * 4);
        if(!in) {

            delete[] data_;

            return false;

        }
        ByteOrder::convertLE2NativeU32(data_, size_);

        if(data != NULL) {

            delete[] data;

        }

        data = data_;
        size = size_;

        return true;

    }

    bool NodeU32::write(std::ostream& out, Information& info) {

        uint8_t flags = 0x00;
        out.write((char*)&flags, 1);
        if(!out) {

            return false;

        }

        uint32_t size_ = data == NULL ? 0 : size;
        ByteOrder::convertNative2LEU32(&size_, 1);
        out.write((char*)&size_, 4);
        if(!out) {

            return false;

        }
        ByteOrder::convertLE2NativeU32(&size_, 1);

        if(size_ != 0) {

            ByteOrder::convertNative2LEU32(data, size);
            out.write((char*)data, size * 4);
            ByteOrder::convertLE2NativeU32(data, size);
            if(!out) {

                return false;

            }

        }

        return true;


    }

    uint32_t* NodeU32::getData() {

        return data;

    }

    const uint32_t* NodeU32::getData() const {

        return data;

    }

    uint32_t NodeU32::getSize() const {

        return size;

    }

    void NodeU32::set(uint32_t* data_, uint32_t size_) {

        uint32_t* data__ = NULL;

        if(size_ != 0) {

            data__ = new uint32_t[size_];
            ::memcpy(data__, data_, size_ * 4);

        }

        if(data != NULL) {

            delete[] data;

        }

        data = data__;
        size = size_;

    }


    NodeU64::NodeU64() :
        data(NULL),
        size(0) {

    }

    NodeU64::NodeU64(const NodeU64& src) :
        data(NULL),
        size(0) {

        if(src.data != NULL) {

            size = src.size;
            data = new uint64_t[size];
            ::memcpy(data, src.data, size * 8);

        }

    }

    NodeU64::~NodeU64() {

        if(data != NULL) {

            delete[] data;

        }

    }

    Node* NodeU64::clone() {

        return new NodeU64(*this);

    }

    bool NodeU64::read(std::istream& in, Information& info) {

        uint8_t flags;
        in.read((char*)&flags, 1);
        if(!in) {

            return false;

        }

        uint32_t size_;
        in.read((char*)&size_, 4);
        if(!in) {

            return false;

        }
        ByteOrder::convertLE2NativeU32(&size_, 1);

        uint64_t* data_ = new uint64_t[size_];
        in.read((char*)data_, size_ * 8);
        if(!in) {

            delete[] data_;

            return false;

        }
        ByteOrder::convertLE2NativeU64(data_, size_);

        if(data != NULL) {

            delete[] data;

        }

        data = data_;
        size = size_;

        return true;

    }

    bool NodeU64::write(std::ostream& out, Information& info) {

        uint8_t flags = 0x00;
        out.write((char*)&flags, 1);
        if(!out) {

            return false;

        }

        uint32_t size_ = data == NULL ? 0 : size;
        ByteOrder::convertNative2LEU32(&size_, 1);
        out.write((char*)&size_, 4);
        if(!out) {

            return false;

        }
        ByteOrder::convertLE2NativeU32(&size_, 1);

        if(size_ != 0) {

            ByteOrder::convertNative2LEU64(data, size);
            out.write((char*)data, size * 8);
            ByteOrder::convertLE2NativeU64(data, size);
            if(!out) {

                return false;

            }

        }

        return true;


    }

    uint64_t* NodeU64::getData() {

        return data;

    }

    const uint64_t* NodeU64::getData() const {

        return data;

    }

    uint32_t NodeU64::getSize() const {

        return size;

    }

    void NodeU64::set(uint64_t* data_, uint32_t size_) {

        uint64_t* data__ = NULL;

        if(size_ != 0) {

            data__ = new uint64_t[size_];
            ::memcpy(data__, data_, size_ * 8);

        }

        if(data != NULL) {

            delete[] data;

        }

        data = data__;
        size = size_;

    }


    NodeS8::NodeS8() :
        data(NULL),
        size(0) {

    }

    NodeS8::NodeS8(const NodeS8& src) :
        data(NULL),
        size(0) {

        if(src.data != NULL) {

            size = src.size;
            data = new int8_t[size];
            ::memcpy(data, src.data, size * 1);

        }

    }

    NodeS8::~NodeS8() {

        if(data != NULL) {

            delete[] data;

        }

    }

    Node* NodeS8::clone() {

        return new NodeS8(*this);

    }

    bool NodeS8::read(std::istream& in, Information& info) {

        uint8_t flags;
        in.read((char*)&flags, 1);
        if(!in) {

            return false;

        }

        uint32_t size_;
        in.read((char*)&size_, 4);
        if(!in) {

            return false;

        }
        ByteOrder::convertLE2NativeU32(&size_, 1);

        int8_t* data_;
        data_ = new int8_t[size_];
        in.read((char*)data_, size_ * 1);
        if(!in) {

            delete[] data_;

            return false;

        }

        if(data != NULL) {

            delete[] data;

        }

        data = data_;
        size = size_;

        return true;

    }

    bool NodeS8::write(std::ostream& out, Information& info) {

        uint8_t flags = 0x00;
        out.write((char*)&flags, 1);
        if(!out) {

            return false;

        }

        uint32_t size_ = data == NULL ? 0 : size;
        ByteOrder::convertNative2LEU32(&size_, 1);
        out.write((char*)&size_, 4);
        if(!out) {

            return false;

        }
        ByteOrder::convertLE2NativeU32(&size_, 1);

        if(size_ != 0) {

            out.write((char*)data, size_ * 1);
            if(!out) {

                return false;

            }

        }

        return true;


    }

    int8_t* NodeS8::getData() {

        return data;

    }

    const int8_t* NodeS8::getData() const {

        return data;

    }

    uint32_t NodeS8::getSize() const {

        return size;

    }

    void NodeS8::set(int8_t* data_, uint32_t size_) {

        int8_t* data__ = NULL;

        if(size_ != 0) {

            data__ = new int8_t[size_];
            ::memcpy(data__, data_, size_ * 1);

        }

        if(data != NULL) {

            delete[] data;

        }

        data = data__;
        size = size_;

    }


    NodeS16::NodeS16() :
        data(NULL),
        size(0) {

    }

    NodeS16::NodeS16(const NodeS16& src) :
        data(NULL),
        size(0) {

        if(src.data != NULL) {

            size = src.size;
            data = new int16_t[size];
            ::memcpy(data, src.data, size * 2);

        }

    }

    NodeS16::~NodeS16() {

        if(data != NULL) {

            delete[] data;

        }

    }

    Node* NodeS16::clone() {

        return new NodeS16(*this);

    }

    bool NodeS16::read(std::istream& in, Information& info) {

        uint8_t flags;
        in.read((char*)&flags, 1);
        if(!in) {

            return false;

        }

        uint32_t size_;
        in.read((char*)&size_, 4);
        if(!in) {

            return false;

        }
        ByteOrder::convertLE2NativeU32(&size_, 1);

        int16_t* data_ = new int16_t[size_];
        in.read((char*)data_, size_ * 2);
        if(!in) {

            delete[] data_;

            return false;

        }
        ByteOrder::convertLE2NativeS16(data_, size_);

        if(data != NULL) {

            delete[] data;

        }

        data = data_;
        size = size_;

        return true;

    }

    bool NodeS16::write(std::ostream& out, Information& info) {

        uint8_t flags = 0x00;
        out.write((char*)&flags, 1);
        if(!out) {

            return false;

        }

        uint32_t size_ = data == NULL ? 0 : size;
        ByteOrder::convertNative2LEU32(&size_, 1);
        out.write((char*)&size_, 4);
        if(!out) {

            return false;

        }
        ByteOrder::convertLE2NativeU32(&size_, 1);

        if(size_ != 0) {

            ByteOrder::convertNative2LES16(data, size);
            out.write((char*)data, size * 2);
            ByteOrder::convertLE2NativeS16(data, size);
            if(!out) {

                return false;

            }

        }

        return true;


    }

    int16_t* NodeS16::getData() {

        return data;

    }

    const int16_t* NodeS16::getData() const {

        return data;

    }

    uint32_t NodeS16::getSize() const {

        return size;

    }

    void NodeS16::set(int16_t* data_, uint32_t size_) {

        int16_t* data__ = NULL;

        if(size_ != 0) {

            data__ = new int16_t[size_];
            ::memcpy(data__, data_, size_ * 2);

        }

        if(data != NULL) {

            delete[] data;

        }

        data = data__;
        size = size_;

    }


    NodeS32::NodeS32() :
        data(NULL),
        size(0) {

    }

    NodeS32::NodeS32(const NodeS32& src) :
        data(NULL),
        size(0) {

        if(src.data != NULL) {

            size = src.size;
            data = new int32_t[size];
            ::memcpy(data, src.data, size * 4);

        }

    }

    NodeS32::~NodeS32() {

        if(data != NULL) {

            delete[] data;

        }

    }

    Node* NodeS32::clone() {

        return new NodeS32(*this);

    }

    bool NodeS32::read(std::istream& in, Information& info) {

        uint8_t flags;
        in.read((char*)&flags, 1);
        if(!in) {

            return false;

        }

        uint32_t size_;
        in.read((char*)&size_, 4);
        if(!in) {

            return false;

        }
        ByteOrder::convertLE2NativeU32(&size_, 1);

        int32_t* data_ = new int32_t[size_];
        in.read((char*)data_, size_ * 4);
        if(!in) {

            delete[] data_;

            return false;

        }
        ByteOrder::convertLE2NativeS32(data_, size_);

        if(data != NULL) {

            delete[] data;

        }

        data = data_;
        size = size_;

        return true;

    }

    bool NodeS32::write(std::ostream& out, Information& info) {

        uint8_t flags = 0x00;
        out.write((char*)&flags, 1);
        if(!out) {

            return false;

        }

        uint32_t size_ = data == NULL ? 0 : size;
        ByteOrder::convertNative2LEU32(&size_, 1);
        out.write((char*)&size_, 4);
        if(!out) {

            return false;

        }
        ByteOrder::convertLE2NativeU32(&size_, 1);

        if(size_ != 0) {

            ByteOrder::convertNative2LES32(data, size);
            out.write((char*)data, size * 4);
            ByteOrder::convertLE2NativeS32(data, size);
            if(!out) {

                return false;

            }

        }

        return true;


    }

    int32_t* NodeS32::getData() {

        return data;

    }

    const int32_t* NodeS32::getData() const {

        return data;

    }

    uint32_t NodeS32::getSize() const {

        return size;

    }

    void NodeS32::set(int32_t* data_, uint32_t size_) {

        int32_t* data__ = NULL;

        if(size_ != 0) {

            data__ = new int32_t[size_];
            ::memcpy(data__, data_, size_ * 4);

        }

        if(data != NULL) {

            delete[] data;

        }

        data = data__;
        size = size_;

    }


    NodeS64::NodeS64() :
        data(NULL),
        size(0) {

    }

    NodeS64::NodeS64(const NodeS64& src) :
        data(NULL),
        size(0) {

        if(src.data != NULL) {

            size = src.size;
            data = new int64_t[size];
            ::memcpy(data, src.data, size * 8);

        }

    }

    NodeS64::~NodeS64() {

        if(data != NULL) {

            delete[] data;

        }

    }

    Node* NodeS64::clone() {

        return new NodeS64(*this);

    }

    bool NodeS64::read(std::istream& in, Information& info) {

        uint8_t flags;
        in.read((char*)&flags, 1);
        if(!in) {

            return false;

        }

        uint32_t size_;
        in.read((char*)&size_, 4);
        if(!in) {

            return false;

        }
        ByteOrder::convertLE2NativeU32(&size_, 1);

        int64_t* data_ = new int64_t[size_];
        in.read((char*)data_, size_ * 8);
        if(!in) {

            delete[] data_;

            return false;

        }
        ByteOrder::convertLE2NativeS64(data_, size_);

        if(data != NULL) {

            delete[] data;

        }

        data = data_;
        size = size_;

        return true;

    }

    bool NodeS64::write(std::ostream& out, Information& info) {

        uint8_t flags = 0x00;
        out.write((char*)&flags, 1);
        if(!out) {

            return false;

        }

        uint32_t size_ = data == NULL ? 0 : size;
        ByteOrder::convertNative2LEU32(&size_, 1);
        out.write((char*)&size_, 4);
        if(!out) {

            return false;

        }
        ByteOrder::convertLE2NativeU32(&size_, 1);

        if(size_ != 0) {

            ByteOrder::convertNative2LES64(data, size);
            out.write((char*)data, size * 8);
            ByteOrder::convertLE2NativeS64(data, size);
            if(!out) {

                return false;

            }

        }

        return true;


    }

    int64_t* NodeS64::getData() {

        return data;

    }

    const int64_t* NodeS64::getData() const {

        return data;

    }

    uint32_t NodeS64::getSize() const {

        return size;

    }

    void NodeS64::set(int64_t* data_, uint32_t size_) {

        int64_t* data__ = NULL;

        if(size_ != 0) {

            data__ = new int64_t[size_];
            ::memcpy(data__, data_, size_ * 8);

        }

        if(data != NULL) {

            delete[] data;

        }

        data = data__;
        size = size_;

    }


    NodeF32::NodeF32() :
        data(NULL),
        size(0) {

    }

    NodeF32::NodeF32(const NodeF32& src) :
        data(NULL),
        size(0) {

        if(src.data != NULL) {

            size = src.size;
            data = new float[size];
            ::memcpy(data, src.data, size * 4);

        }

    }

    NodeF32::~NodeF32() {

        if(data != NULL) {

            delete[] data;

        }

    }

    Node* NodeF32::clone() {

        return new NodeF32(*this);

    }

    bool NodeF32::read(std::istream& in, Information& info) {

        uint8_t flags;
        in.read((char*)&flags, 1);
        if(!in) {

            return false;

        }

        uint32_t size_;
        in.read((char*)&size_, 4);
        if(!in) {

            return false;

        }
        ByteOrder::convertLE2NativeU32(&size_, 1);

        float* data_ = new float[size_];
        in.read((char*)data_, size_ * 4);
        if(!in) {

            delete[] data_;

            return false;

        }
        ByteOrder::convertLE2NativeF32(data_, size_);

        if(data != NULL) {

            delete[] data;

        }

        data = data_;
        size = size_;

        return true;

    }

    bool NodeF32::write(std::ostream& out, Information& info) {

        uint8_t flags = 0x00;
        out.write((char*)&flags, 1);
        if(!out) {

            return false;

        }

        uint32_t size_ = data == NULL ? 0 : size;
        ByteOrder::convertNative2LEU32(&size_, 1);
        out.write((char*)&size_, 4);
        if(!out) {

            return false;

        }
        ByteOrder::convertLE2NativeU32(&size_, 1);

        if(size_ != 0) {

            ByteOrder::convertNative2LEF32(data, size);
            out.write((char*)data, size * 4);
            ByteOrder::convertLE2NativeF32(data, size);
            if(!out) {

                return false;

            }

        }

        return true;


    }

    float* NodeF32::getData() {

        return data;

    }

    const float* NodeF32::getData() const {

        return data;

    }

    uint32_t NodeF32::getSize() const {

        return size;

    }

    void NodeF32::set(float* data_, uint32_t size_) {

        float* data__ = NULL;

        if(size_ != 0) {

            data__ = new float[size_];
            ::memcpy(data__, data_, size_ * 4);

        }

        if(data != NULL) {

            delete[] data;

        }

        data = data__;
        size = size_;

    }


    NodeF64::NodeF64() :
        data(NULL),
        size(0) {

    }

    NodeF64::NodeF64(const NodeF64& src) :
        data(NULL),
        size(0) {

        if(src.data != NULL) {

            size = src.size;
            data = new double[size];
            ::memcpy(data, src.data, size * 8);

        }

    }

    NodeF64::~NodeF64() {

        if(data != NULL) {

            delete[] data;

        }

    }

    Node* NodeF64::clone() {

        return new NodeF64(*this);

    }

    bool NodeF64::read(std::istream& in, Information& info) {

        uint8_t flags;
        in.read((char*)&flags, 1);
        if(!in) {

            return false;

        }

        uint32_t size_;
        in.read((char*)&size_, 4);
        if(!in) {

            return false;

        }
        ByteOrder::convertLE2NativeU32(&size_, 1);

        double* data_ = new double[size_];
        in.read((char*)data_, size_ * 8);
        if(!in) {

            delete[] data_;

            return false;

        }
        ByteOrder::convertLE2NativeF64(data_, size_);

        if(data != NULL) {

            delete[] data;

        }

        data = data_;
        size = size_;

        return true;

    }

    bool NodeF64::write(std::ostream& out, Information& info) {

        uint8_t flags = 0x00;
        out.write((char*)&flags, 1);
        if(!out) {

            return false;

        }

        uint32_t size_ = data == NULL ? 0 : size;
        ByteOrder::convertNative2LEU32(&size_, 1);
        out.write((char*)&size_, 4);
        if(!out) {

            return false;

        }
        ByteOrder::convertLE2NativeU32(&size_, 1);

        if(size_ != 0) {

            ByteOrder::convertNative2LEF64(data, size);
            out.write((char*)data, size * 8);
            ByteOrder::convertLE2NativeF64(data, size);
            if(!out) {

                return false;

            }

        }

        return true;


    }

    double* NodeF64::getData() {

        return data;

    }

    const double* NodeF64::getData() const {

        return data;

    }

    uint32_t NodeF64::getSize() const {

        return size;

    }

    void NodeF64::set(double* data_, uint32_t size_) {

        double* data__ = NULL;

        if(size_ != 0) {

            data__ = new double[size_];
            ::memcpy(data__, data_, size_ * 8);

        }

        if(data != NULL) {

            delete[] data;

        }

        data = data__;
        size = size_;

    }


    bool read(std::istream& in, NodePackage& package) {

        Magic magic;
        in.read((char*)magic.data, 4);
        if(!in) {

            return false;

        }
        if(magic != MAGIC) {

            return false;

        }

        Version version;
        in.read((char*)version.data, 4);
        if(!in) {

            return false;

        }
        if(version > VERSION) {

            return false;

        }

        Information info;
        info.version = version;
        info.depth = 0;
        if(!package.read(in, info)) {

            return false;

        }

        return true;

    }

    bool write(std::ostream& out, NodePackage& package) {

        out.write((char*)MAGIC.data, 4);
        if(!out) {

            return false;

        }

        out.write((char*)VERSION.data, 4);
        if(!out) {

            return false;

        }

        Information info;
        info.version = VERSION;
        info.depth = 0;
        if(!package.write(out, info)) {

            return false;

        }

        return true;

    }

}
