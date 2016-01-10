#ifndef _NNN_H_
#define _NNN_H_


#include <iostream>
#include <map>
#include <stdint.h>
#include <string>


namespace NNN {

    namespace ByteOrder {

        // Convert from Little Endian to Native.
        // U16
        void convertLE2NativeU16(uint16_t* data, uintptr_t size);
        // U32
        void convertLE2NativeU32(uint32_t* data, uintptr_t size);
        // U64
        void convertLE2NativeU64(uint64_t* data, uintptr_t size);
        // S16
        void convertLE2NativeS16(int16_t* data, uintptr_t size);
        // S32
        void convertLE2NativeS32(int32_t* data, uintptr_t size);
        // S64
        void convertLE2NativeS64(int64_t* data, uintptr_t size);
        // F32
        void convertLE2NativeF32(float* data, uintptr_t size);
        // F64
        void convertLE2NativeF64(double* data, uintptr_t size);

        // Convert from Native to Little Endian.
        // U16
        void convertNative2LEU16(uint16_t* data, uintptr_t size);
        // U32
        void convertNative2LEU32(uint32_t* data, uintptr_t size);
        // U64
        void convertNative2LEU64(uint64_t* data, uintptr_t size);
        // S16
        void convertNative2LES16(int16_t* data, uintptr_t size);
        // S32
        void convertNative2LES32(int32_t* data, uintptr_t size);
        // S64
        void convertNative2LES64(int64_t* data, uintptr_t size);
        // F32
        void convertNative2LEF32(float* data, uintptr_t size);
        // F64
        void convertNative2LEF64(double* data, uintptr_t size);

    }

    struct Magic {

        uint8_t data[4];

        bool operator !=(const Magic &src) const;

    };

    struct Version {

        uint8_t data[4];

        bool operator >(const Version &src) const;

    };


    struct Information {

        Version version;
        uintptr_t depth;

    };


    class Node {

        public:

        Node();
        Node(const Node& src);
        virtual ~Node() = 0;

        virtual Node* clone() = 0;

        virtual bool read(std::istream& in, Information& info) = 0;
        virtual bool write(std::ostream& out, Information& info) = 0;

    };

    class NodePackage : public Node {

        private:

        std::map<std::string, Node*>* data;
        uint32_t size;

        public:

        NodePackage();
        NodePackage(const NodePackage& src);
        virtual ~NodePackage();

        Node* clone();

        bool read(std::istream& in, Information& info);
        bool write(std::ostream& out, Information& info);

        std::map<std::string, Node*>* getData();
        const std::map<std::string, Node*>* getData() const;
        uint32_t getSize() const;

        void set(std::map<std::string, Node*>* data_,
            const uint32_t size_);

        private:

        void cleanMap(std::map<std::string, Node*>* data_, uintptr_t size_);

    };

    class NodeString : public Node {

        private:

        std::string* data;
        uint32_t size;

        public:

        NodeString();
        NodeString(const NodeString& src);
        virtual ~NodeString();

        Node* clone();

        bool read(std::istream& in, Information& info);
        bool write(std::ostream& out, Information& info);

        std::string* getData();
        const std::string* getData() const;
        uint32_t getSize() const;

        void set(std::string* data_, uint32_t size_);

    };

    class NodeU8 : public Node {

        private:

        uint8_t* data;
        uint32_t size;

        public:

        NodeU8();
        NodeU8(const NodeU8& src);
        virtual ~NodeU8();

        Node* clone();

        bool read(std::istream& in, Information& info);
        bool write(std::ostream& out, Information& info);

        uint8_t* getData();
        const uint8_t* getData() const;
        uint32_t getSize() const;

        void set(uint8_t* data_, uint32_t size_);

    };

    class NodeU16 : public Node {

        private:

        uint16_t* data;
        uint32_t size;

        public:

        NodeU16();
        NodeU16(const NodeU16& src);
        virtual ~NodeU16();

        Node* clone();

        bool read(std::istream& in, Information& info);
        bool write(std::ostream& out, Information& info);

        uint16_t* getData();
        const uint16_t* getData() const;
        uint32_t getSize() const;

        void set(uint16_t* data_, uint32_t size_);

    };

    class NodeU32 : public Node {

        private:

        uint32_t* data;
        uint32_t size;

        public:

        NodeU32();
        NodeU32(const NodeU32& src);
        virtual ~NodeU32();

        Node* clone();

        bool read(std::istream& in, Information& info);
        bool write(std::ostream& out, Information& info);

        uint32_t* getData();
        const uint32_t* getData() const;
        uint32_t getSize() const;

        void set(uint32_t* data_, uint32_t size_);

    };

    class NodeU64 : public Node {

        private:

        uint64_t* data;
        uint32_t size;

        public:

        NodeU64();
        NodeU64(const NodeU64& src);
        virtual ~NodeU64();

        Node* clone();

        bool read(std::istream& in, Information& info);
        bool write(std::ostream& out, Information& info);

        uint64_t* getData();
        const uint64_t* getData() const;
        uint32_t getSize() const;

        void set(uint64_t* data_, uint32_t size_);

    };

    class NodeS8 : public Node {

        private:

        int8_t* data;
        uint32_t size;

        public:

        NodeS8();
        NodeS8(const NodeS8& src);
        virtual ~NodeS8();

        Node* clone();

        bool read(std::istream& in, Information& info);
        bool write(std::ostream& out, Information& info);

        int8_t* getData();
        const int8_t* getData() const;
        uint32_t getSize() const;

        void set(int8_t* data_, uint32_t size_);

    };

    class NodeS16 : public Node {

        private:

        int16_t* data;
        uint32_t size;

        public:

        NodeS16();
        NodeS16(const NodeS16& src);
        virtual ~NodeS16();

        Node* clone();

        bool read(std::istream& in, Information& info);
        bool write(std::ostream& out, Information& info);

        int16_t* getData();
        const int16_t* getData() const;
        uint32_t getSize() const;

        void set(int16_t* data_, uint32_t size_);

    };

    class NodeS32 : public Node {

        private:

        int32_t* data;
        uint32_t size;

        public:

        NodeS32();
        NodeS32(const NodeS32& src);
        virtual ~NodeS32();

        Node* clone();

        bool read(std::istream& in, Information& info);
        bool write(std::ostream& out, Information& info);

        int32_t* getData();
        const int32_t* getData() const;
        uint32_t getSize() const;

        void set(int32_t* data_, uint32_t size_);

    };

    class NodeS64 : public Node {

        private:

        int64_t* data;
        uint32_t size;

        public:

        NodeS64();
        NodeS64(const NodeS64& src);
        virtual ~NodeS64();

        Node* clone();

        bool read(std::istream& in, Information& info);
        bool write(std::ostream& out, Information& info);

        int64_t* getData();
        const int64_t* getData() const;
        uint32_t getSize() const;

        void set(int64_t* data_, uint32_t size_);

    };

    class NodeF32 : public Node {

        private:

        float* data;
        uint32_t size;

        public:

        NodeF32();
        NodeF32(const NodeF32& src);
        virtual ~NodeF32();

        Node* clone();

        bool read(std::istream& in, Information& info);
        bool write(std::ostream& out, Information& info);

        float* getData();
        const float* getData() const;
        uint32_t getSize() const;

        void set(float* data_, uint32_t size_);

    };

    class NodeF64 : public Node {

        private:

        double* data;
        uint32_t size;

        public:

        NodeF64();
        NodeF64(const NodeF64& src);
        virtual ~NodeF64();

        Node* clone();

        bool read(std::istream& in, Information& info);
        bool write(std::ostream& out, Information& info);

        double* getData();
        const double* getData() const;
        uint32_t getSize() const;

        void set(double* data_, uint32_t size_);

    };


    using NodeByte = NodeS8;
    using NodeShort = NodeS16;
    using NodeInt = NodeS32;
    using NodeLong = NodeS64;
    using NodeUByte = NodeU8;
    using NodeUShort = NodeU16;
    using NodeUInt = NodeU32;
    using NodeULong = NodeU64;
    using NodeFloat = NodeF32;
    using NodeDouble = NodeF64;


    bool read(std::istream& in, NodePackage& package);
    bool write(std::ostream& out, NodePackage& package);


    const Magic MAGIC = {0x4E, 0x4E, 0x4E, 0xFF};
    const Version VERSION = {0x00, 0x00, 0x01, 0x00};

}


#endif
