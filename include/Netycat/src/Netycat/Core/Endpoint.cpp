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


#include <Netycat/Core/Address.h>
#include <Netycat/Core/Endpoint.h>


namespace Netycat {
    
    namespace Core {
        
        Endpoint::Endpoint() noexcept {
        	
		}
        
        Endpoint::Endpoint(const Endpoint& src) noexcept {
        	
		}
        
        Endpoint::~Endpoint() noexcept {
        	
		}
		
		
        Endpoint* Endpoint::getByAddress(const Address* addr) {
            
            if(addr == nullptr) {
                
                return nullptr;
                
            }
            
            switch(addr->getType()) {
                
                case Address::IPv4: {
                    
                    return new EndpointIPv4(*(AddressIPv4*)addr);
                    
                }
                case Address::IPv6: {
                    
                    return new EndpointIPv6(*(AddressIPv6*)addr);
                    
                }
                default: {
                    
                    return nullptr;
                    
                }
                
            }
            
        }
        
        Endpoint* Endpoint::getByType(const EndpointType type) {
            
            switch(type) {
                
                case IPv4: {
                    
                    return new EndpointIPv4();
                    
                }
                case IPv6: {
                    
                    return new EndpointIPv6();
                    
                }
                default: {
                    
                    return nullptr;
                    
                }
                
            }
            
        }
		
		
        EndpointIPv4::EndpointIPv4() noexcept :
			address(),
			port(0) {
			
		}
		
        EndpointIPv4::EndpointIPv4(const AddressIPv4& addr) noexcept :
			address(addr),
			port(0) {
			
		}
        
        EndpointIPv4::EndpointIPv4(const uint16_t port_) noexcept :
			address(),
			port(port_) {
			
		}
		
        EndpointIPv4::EndpointIPv4(const AddressIPv4& addr,
			const uint16_t port_) noexcept :
			address(addr),
			port(port_) {
			
		}
        
        EndpointIPv4::EndpointIPv4(const EndpointIPv4& src) noexcept :
			address(src.address),
			port(src.port) {
			
		}
        
        EndpointIPv4::~EndpointIPv4() noexcept {
			
		}
		
        
        Endpoint::EndpointType EndpointIPv4::getType() const noexcept {
			
			return Endpoint::IPv4;
			
		}
		
        
        std::string EndpointIPv4::toString() const noexcept {
            
            std::string res;
            
            res += address.toString();
            
            return res;
            
        }
		
		
        const AddressIPv4* EndpointIPv4::getAddress() const noexcept {
        	
        	return &address;
        	
		}
		
        uint16_t EndpointIPv4::getPort() const noexcept {
			
			return port;
			
		}
		
		
        void EndpointIPv4::setAddress(const AddressIPv4* addr) noexcept {
        	
        	address = *addr;
        	
		}
		
        void EndpointIPv4::setPort(const uint16_t port_) noexcept {
			
			port = port_;
			
		}
		
		
        EndpointIPv6::EndpointIPv6() noexcept :
			address(),
			port(0) {
			
		}
		
        EndpointIPv6::EndpointIPv6(const AddressIPv6& addr) noexcept :
			address(addr),
			port(0) {
			
		}
        
        EndpointIPv6::EndpointIPv6(const uint16_t port_) noexcept :
			address(),
			port(port_) {
			
		}
		
        EndpointIPv6::EndpointIPv6(const AddressIPv6& addr,
			const uint16_t port_) noexcept :
			address(addr),
			port(port_) {
			
		}
        
        EndpointIPv6::EndpointIPv6(const EndpointIPv6& src) noexcept :
			address(src.address),
			port(src.port) {
			
		}
        
        EndpointIPv6::~EndpointIPv6() noexcept {
			
		}
        
        
        Endpoint::EndpointType EndpointIPv6::getType() const noexcept {
			
			return Endpoint::IPv6;
			
		}
		
        
        std::string EndpointIPv6::toString() const noexcept {
            
            std::string res;
            
            res += address.toString();
            
            return res;
            
        }
		
		
        const AddressIPv6* EndpointIPv6::getAddress() const noexcept {
        	
        	return &address;
        	
		}
		
        uint16_t EndpointIPv6::getPort() const noexcept {
			
			return port;
			
		}
		
		
        void EndpointIPv6::setAddress(const AddressIPv6* addr) noexcept {
        	
        	address = *addr;
        	
		}
		
        void EndpointIPv6::setPort(const uint16_t port_) noexcept {
			
			port = port_;
			
		}
        
    }
    
}

