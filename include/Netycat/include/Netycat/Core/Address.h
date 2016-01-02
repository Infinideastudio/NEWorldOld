/*******************************************************************************
    
    Copyright 2015 SuperSodaSea

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at
    
        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
    
********************************************************************************
    
*******************************************************************************/


#ifndef _NETYCAT_CORE_INETADDRESS_H_
#define _NETYCAT_CORE_INETADDRESS_H_


#include <stdint.h>
#include <string>


namespace Netycat {
    
    namespace Core {
        
        class Address {
            
            public:
            
            enum AddressType {
                
                IPv4,
                IPv6,
                
            };
            
            public:
            
            Address() noexcept;
            Address(const Address& src) noexcept;
            virtual ~Address() noexcept = 0;
            
            virtual AddressType getType() const noexcept = 0;
            
            virtual std::string toString() const noexcept = 0;
            
            public:
            
            static Address* getByName(const char *host);
            
        };
        
        class AddressIPv4 : public Address {
            
            private:
            
            uint8_t data[4];
            
            public:
            
            AddressIPv4() noexcept;
            AddressIPv4(uint8_t d1, uint8_t d2, uint8_t d3, uint8_t d4) noexcept;
            AddressIPv4(const AddressIPv4& src) noexcept;
            ~AddressIPv4() noexcept;
            
            Address::AddressType getType() const noexcept;
            
            std::string toString() const noexcept;
            
            uint8_t* getData() noexcept;
            const uint8_t* getData() const noexcept;
            
            void set(uint8_t d1, uint8_t d2, uint8_t d3, uint8_t d4) noexcept;
            
            public:
            
            static AddressIPv4* getAny() noexcept;
            static AddressIPv4* getLoopback() noexcept;
            static AddressIPv4* getBroadcast() noexcept;
            
        };
        
        class AddressIPv6 : public Address {
            
            private:
            
            uint8_t data[16];
            
            public:
            
            AddressIPv6() noexcept;
            AddressIPv6(uint8_t d1, uint8_t d2, uint8_t d3, uint8_t d4,
                uint8_t d5, uint8_t d6, uint8_t d7, uint8_t d8,
                uint8_t d9, uint8_t d10, uint8_t d11, uint8_t d12,
                uint8_t d13, uint8_t d14, uint8_t d15, uint8_t d16) noexcept;
            AddressIPv6(const AddressIPv6& src) noexcept;
            ~AddressIPv6() noexcept;
            
            Address::AddressType getType() const noexcept;
            
            std::string toString() const noexcept;
            
            uint8_t* getData() noexcept;
            const uint8_t* getData() const noexcept;
            
            void set(uint8_t d1, uint8_t d2, uint8_t d3, uint8_t d4,
                uint8_t d5, uint8_t d6, uint8_t d7, uint8_t d8,
                uint8_t d9, uint8_t d10, uint8_t d11, uint8_t d12,
                uint8_t d13, uint8_t d14, uint8_t d15, uint8_t d16) noexcept;
            
            public:
            
            static AddressIPv6* getAny() noexcept;
            static AddressIPv6* getLoopback() noexcept;
            
        };
        
    }
    
}


#endif

