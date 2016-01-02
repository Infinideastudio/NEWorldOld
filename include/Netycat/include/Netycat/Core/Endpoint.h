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


#ifndef _NETYCAT_CORE_ENDPOINT_H_
#define _NETYCAT_CORE_ENDPOINT_H_


#include <stdint.h>
#include <string>

#include <Netycat/Core/Address.h>


namespace Netycat {
    
    namespace Core {
        
        class Endpoint {
            
            public:
            
            enum EndpointType {
                
                IPv4,
                IPv6,
                
            };
            
            public:
            
            Endpoint() noexcept;
            Endpoint(const Endpoint& src) noexcept;
            virtual ~Endpoint() noexcept = 0;
            
            virtual EndpointType getType() const noexcept = 0;
            
            virtual std::string toString() const noexcept = 0;
        	
        	public:
        	
        	static Endpoint* getByAddress(const Address* addr);
        	static Endpoint* getByType(const EndpointType type);
            
        };
        
        class EndpointIPv4 : public Endpoint {
        	
        	private:
        	
        	AddressIPv4 address;
        	uint16_t port;
        	
        	public:
        	
            EndpointIPv4() noexcept;
            EndpointIPv4(const AddressIPv4& addr) noexcept;
            EndpointIPv4(const uint16_t port_) noexcept;
            EndpointIPv4(const AddressIPv4& addr,
                const uint16_t port_) noexcept;
            EndpointIPv4(const EndpointIPv4& src) noexcept;
            ~EndpointIPv4() noexcept;
            
            Endpoint::EndpointType getType() const noexcept;
            
            std::string toString() const noexcept;
        	
        	const AddressIPv4* getAddress() const noexcept;
        	uint16_t getPort() const noexcept;
        	
        	void setAddress(const AddressIPv4* addr) noexcept;
        	void setPort(const uint16_t port_) noexcept;
        	
		};
        
        class EndpointIPv6 : public Endpoint {
        	
        	private:
        	
        	AddressIPv6 address;
        	uint16_t port;
        	
        	public:
        	
            EndpointIPv6() noexcept;
            EndpointIPv6(const AddressIPv6& addr) noexcept;
            EndpointIPv6(const uint16_t port_) noexcept;
            EndpointIPv6(const AddressIPv6& addr,
                const uint16_t port_) noexcept;
            EndpointIPv6(const EndpointIPv6& src) noexcept;
            ~EndpointIPv6() noexcept;
            
            Endpoint::EndpointType getType() const noexcept;
            
            std::string toString() const noexcept;
        	
        	const AddressIPv6* getAddress() const noexcept;
        	uint16_t getPort() const noexcept;
        	
        	void setAddress(const AddressIPv6* addr) noexcept;
        	void setPort(const uint16_t port_) noexcept;
        	
		};
        
    }
    
}


#endif

