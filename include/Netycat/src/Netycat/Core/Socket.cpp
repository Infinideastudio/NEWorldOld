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


#include <stdint.h>
#include <string.h>
#include <utility>

#include <Netycat/Core/Include.h>

#include <Netycat/Core/Address.h>
#include <Netycat/Core/Buffer.h>
#include <Netycat/Core/BufferCondition.h>
#include <Netycat/Core/Endpoint.h>
#include <Netycat/Core/Exception.h>
#include <Netycat/Core/Socket.h>


namespace Netycat {
    
    namespace Core {
        
        SocketBase::SocketBase() noexcept :
            sock(-1),
            endpoint(nullptr) {
            
        }
        
        SocketBase::SocketBase(SocketBase&& src) noexcept :
            sock(src.sock),
            endpoint(src.endpoint) {
            
            src.sock = -1;
            src.endpoint = nullptr;
            
        }
        
        SocketBase::~SocketBase() noexcept {
            
            if(sock != -1) {
                
                #if defined(NETYCAT_OS_WINDOWS)
                    ::closesocket(sock);
                #elif defined(NETYCAT_OS_LINUX)
                    ::close(sock);
                #endif
                
            }
            
            if(endpoint != nullptr) {
                
                delete endpoint;
                
            }
            
        }
        
        
        const Endpoint* SocketBase::getEndpoint() {
            
            return endpoint;
            
        }
        
        
        bool SocketBase::nativeAccept(SocketBase& s) {
            
            if(sock == -1) {
                
                return false;
                
            }
            
            if(s.sock != -1) {
                
                return false;
                
            }
            
            struct sockaddr_storage saddr_;
            
            #if defined(NETYCAT_OS_WINDOWS)
                int slen = sizeof(saddr_);
            #elif defined(NETYCAT_OS_LINUX)
                unsigned int slen = sizeof(saddr_);
            #endif
            
            if((s.sock = ::accept(sock, (struct sockaddr*)&saddr_, &slen))
                == -1) {
                
                return false;
                
            }
            
            switch(saddr_.ss_family) {
                
                case AF_INET: {
                	
                    struct sockaddr_in& saddr = (struct sockaddr_in&)saddr_;
                    
                    uint32_t addr = ntohl(saddr.sin_addr.s_addr);
                    
                    uint8_t d1 = (addr & 0xFF000000) >> 24;
                    uint8_t d2 = (addr & 0x00FF0000) >> 16;
                    uint8_t d3 = (addr & 0x0000FF00) >> 8;
                    uint8_t d4 = (addr & 0x000000FF);
                    
                    AddressIPv4 address(d1, d2, d3, d4);
                    s.endpoint = new EndpointIPv4(address,
                        ntohs(saddr.sin_port));
                    
            		return true;
            		
            	}
				case AF_INET6: {
					
                    struct sockaddr_in6& saddr = (struct sockaddr_in6&)saddr_;
                    
                    uint8_t d1 = saddr.sin6_addr.s6_addr[0];
                    uint8_t d2 = saddr.sin6_addr.s6_addr[1];
                    uint8_t d3 = saddr.sin6_addr.s6_addr[2];
                    uint8_t d4 = saddr.sin6_addr.s6_addr[3];
                    uint8_t d5 = saddr.sin6_addr.s6_addr[4];
                    uint8_t d6 = saddr.sin6_addr.s6_addr[5];
                    uint8_t d7 = saddr.sin6_addr.s6_addr[6];
                    uint8_t d8 = saddr.sin6_addr.s6_addr[7];
                    uint8_t d9 = saddr.sin6_addr.s6_addr[8];
                    uint8_t d10 = saddr.sin6_addr.s6_addr[9];
                    uint8_t d11 = saddr.sin6_addr.s6_addr[10];
                    uint8_t d12 = saddr.sin6_addr.s6_addr[11];
                    uint8_t d13 = saddr.sin6_addr.s6_addr[12];
                    uint8_t d14 = saddr.sin6_addr.s6_addr[13];
                    uint8_t d15 = saddr.sin6_addr.s6_addr[14];
                    uint8_t d16 = saddr.sin6_addr.s6_addr[15];
                    
                    AddressIPv6 address(
                        d1, d2, d3, d4, d5, d6, d7, d8,
                        d9, d10, d11, d12, d13, d14, d15, d16);
                    s.endpoint = new EndpointIPv6(address,
                        ntohs(saddr.sin6_port));
                    
                    return true;
					
				}
            	default: {
            		
                    return false;
					
				}
            	
            }
            
        }
        
        bool SocketBase::nativeBind(const Endpoint* end) {
            
            if(sock == -1) {
                
                return false;
                
            }
            
            if(end == nullptr) {
                
                return false;
                
            }
            
            switch(end->getType()) {
                
                case Endpoint::IPv4: {
                    
                    const EndpointIPv4* endIPv4 = (const EndpointIPv4*)end;
                    const AddressIPv4* addrIPv4 = endIPv4->getAddress();
                    const uint8_t* dataIPv4 = addrIPv4->getData();
                    
                    struct sockaddr_in saddr;
                    ::memset(&saddr, 0, sizeof(saddr));
                    saddr.sin_family = AF_INET;
                    saddr.sin_port = ::htons(endIPv4->getPort());
                    saddr.sin_addr.s_addr = htonl(
                        (dataIPv4[0] << 24) | (dataIPv4[1] << 16) |
                        (dataIPv4[2] << 8) | dataIPv4[3]);
                    
                    if(::bind(sock, (struct sockaddr*)&saddr, sizeof(saddr))
                        != 0) {
                        
                        return false;
                        
                    }
                    
                    endpoint = new EndpointIPv4(*endIPv4);
                    
                    return true;
                    
                }
                case Endpoint::IPv6: {
                    
                    const EndpointIPv6* endIPv6 = (const EndpointIPv6*)end;
                    const AddressIPv6* addrIPv6 = endIPv6->getAddress();
                    const uint8_t* dataIPv6 = addrIPv6->getData();
                    
                    struct sockaddr_in6 saddr;
                    ::memset(&saddr, 0, sizeof(saddr));
                    saddr.sin6_family = AF_INET6;
                    saddr.sin6_port = ::htons(endIPv6->getPort());
                    saddr.sin6_addr.s6_addr[0] = dataIPv6[0];
                    saddr.sin6_addr.s6_addr[1] = dataIPv6[1];
                    saddr.sin6_addr.s6_addr[2] = dataIPv6[2];
                    saddr.sin6_addr.s6_addr[3] = dataIPv6[3];
                    saddr.sin6_addr.s6_addr[4] = dataIPv6[4];
                    saddr.sin6_addr.s6_addr[5] = dataIPv6[5];
                    saddr.sin6_addr.s6_addr[6] = dataIPv6[6];
                    saddr.sin6_addr.s6_addr[7] = dataIPv6[7];
                    saddr.sin6_addr.s6_addr[8] = dataIPv6[8];
                    saddr.sin6_addr.s6_addr[9] = dataIPv6[9];
                    saddr.sin6_addr.s6_addr[10] = dataIPv6[10];
                    saddr.sin6_addr.s6_addr[11] = dataIPv6[11];
                    saddr.sin6_addr.s6_addr[12] = dataIPv6[12];
                    saddr.sin6_addr.s6_addr[13] = dataIPv6[13];
                    saddr.sin6_addr.s6_addr[14] = dataIPv6[14];
                    saddr.sin6_addr.s6_addr[15] = dataIPv6[15];
                    
                    if(::bind(sock, (struct sockaddr*)&saddr, sizeof(saddr))
                        != 0) {
                        
                        return false;
                        
                    }
                    
                    endpoint = new EndpointIPv6(*endIPv6);
                    
                    return true;
                    
                }
                default: {
                    
                    return false;
                    
                }
                
            }
            
        }
        
        bool SocketBase::nativeClose() {
            
            if(sock != -1) {
                
                #if defined(NETYCAT_OS_WINDOWS)
                    ::closesocket(sock);
                #elif defined(NETYCAT_OS_LINUX)
                    ::close(sock);
                #endif
                
                sock = -1;
                delete endpoint;
                endpoint = nullptr;
                
                return true;
                
            } else {
                
                return false;
                
            }
            
        }
        
        bool SocketBase::nativeConnect(const Endpoint* end) {
            
            if(sock == -1) {
                
                return false;
                
            }
            
            if(end == nullptr) {
                
                return false;
                
            }
            
            switch(end->getType()) {
                
                case Endpoint::IPv4: {
                    
                    const EndpointIPv4* endIPv4 = (const EndpointIPv4*)end;
                    const AddressIPv4* addrIPv4 = endIPv4->getAddress();
                    const uint8_t* dataIPv4 = addrIPv4->getData();
                    
                    struct sockaddr_in saddr;
                    ::memset(&saddr, 0, sizeof(saddr));
                    saddr.sin_family = AF_INET;
                    saddr.sin_port = ::htons(endIPv4->getPort());
                    saddr.sin_addr.s_addr = htonl(
                        (dataIPv4[0] << 24) | (dataIPv4[1] << 16) |
                        (dataIPv4[2] << 8) | dataIPv4[3]);
                    
                    if(::connect(sock, (struct sockaddr*)&saddr, sizeof(saddr))
                        != 0) {
                        
                        return false;
                        
                    }
                    
                    endpoint = new EndpointIPv4(*endIPv4);
                    
                    return true;
                    
                }
                case Endpoint::IPv6: {
                    
                    const EndpointIPv6* endIPv6 = (const EndpointIPv6*)end;
                    const AddressIPv6* addrIPv6 = endIPv6->getAddress();
                    const uint8_t* dataIPv6 = addrIPv6->getData();
                    
                    struct sockaddr_in6 saddr;
                    ::memset(&saddr, 0, sizeof(saddr));
                    saddr.sin6_family = AF_INET6;
                    saddr.sin6_port = ::htons(endIPv6->getPort());
                    saddr.sin6_addr.s6_addr[0] = dataIPv6[0];
                    saddr.sin6_addr.s6_addr[1] = dataIPv6[1];
                    saddr.sin6_addr.s6_addr[2] = dataIPv6[2];
                    saddr.sin6_addr.s6_addr[3] = dataIPv6[3];
                    saddr.sin6_addr.s6_addr[4] = dataIPv6[4];
                    saddr.sin6_addr.s6_addr[5] = dataIPv6[5];
                    saddr.sin6_addr.s6_addr[6] = dataIPv6[6];
                    saddr.sin6_addr.s6_addr[7] = dataIPv6[7];
                    saddr.sin6_addr.s6_addr[8] = dataIPv6[8];
                    saddr.sin6_addr.s6_addr[9] = dataIPv6[9];
                    saddr.sin6_addr.s6_addr[10] = dataIPv6[10];
                    saddr.sin6_addr.s6_addr[11] = dataIPv6[11];
                    saddr.sin6_addr.s6_addr[12] = dataIPv6[12];
                    saddr.sin6_addr.s6_addr[13] = dataIPv6[13];
                    saddr.sin6_addr.s6_addr[14] = dataIPv6[14];
                    saddr.sin6_addr.s6_addr[15] = dataIPv6[15];
                    
                    if(::connect(sock, (struct sockaddr*)&saddr, sizeof(saddr))
                        != 0) {
                        
                        return false;
                        
                    }
                    
                    endpoint = new EndpointIPv6(*endIPv6);
                    
                    return true;
                    
                }
                default: {
                    
                    return false;
                    
                }
                
            }
            
        }
        
        bool SocketBase::nativeListen(int backlog) {
            
            if(sock == -1) {
                
                return false;
                
            }
            
            if(::listen(sock, backlog) == 0) {
                
                return true;
                
            } else {
                
                return false;
                
            }
            
        }
        
        intptr_t SocketBase::nativeRecv(void* buffer, uintptr_t length,
            int flags) {
            
            if(sock == -1) {
                
                return -1;
                
            }
            
            if(buffer == nullptr) {
                
                return -1;
                
            }
            
            #if defined(NETYCAT_OS_WINDOWS)
                intptr_t ret = ::recv(sock, (char*)buffer, length, flags);
            #elif defined(NETYCAT_OS_LINUX)
                intptr_t ret = ::recv(sock, buffer, length, flags);
            #endif
            
            if(ret < 0) {
                
                return -1;
                
            }
            
            if(ret == 0) {
                
                return 0;
                
            }
            
            return ret;
            
        }
        
        intptr_t SocketBase::nativeRecvfrom(void* buffer, uintptr_t length,
            int flags, Endpoint*& end) {
            
            if(sock == -1) {
                
                return -1;
                
            }
            
            if(buffer == nullptr) {
                
                return -1;
                
            }
            
            struct sockaddr_storage saddr_;
            
            #if defined(NETYCAT_OS_WINDOWS)
                int slen = sizeof(saddr_);
                intptr_t ret = ::recvfrom(sock, (char*)buffer, length,
                    flags, (sockaddr*)&saddr_, &slen);
            #elif defined(NETYCAT_OS_LINUX)
                unsigned int slen = sizeof(saddr_);
                intptr_t ret = ::recvfrom(sock, buffer, length,
                    flags, (sockaddr*)&saddr_, &slen);
            #endif
            
            if(ret < 0) {
                
                return -1;
                
            }
            
            if(ret == 0) {
                
                return 0;
                
            }
            
            switch(saddr_.ss_family) {
                
                case AF_INET: {
                	
                    struct sockaddr_in& saddr = (struct sockaddr_in&)saddr_;
                    
                    uint32_t addr_ = ntohl(saddr.sin_addr.s_addr);
                    
                    uint8_t d1 = (addr_ & 0xFF000000) >> 24;
                    uint8_t d2 = (addr_ & 0x00FF0000) >> 16;
                    uint8_t d3 = (addr_ & 0x0000FF00) >> 8;
                    uint8_t d4 = (addr_ & 0x000000FF);
                    
                    AddressIPv4 address(d1, d2, d3, d4);
                    end = new EndpointIPv4(address,
                        ntohs(saddr.sin_port));
                    
                    return ret;
            		
            	}
				case AF_INET6: {
					
                    struct sockaddr_in6& saddr = (struct sockaddr_in6&)saddr_;
                    
                    uint8_t d1 = saddr.sin6_addr.s6_addr[0];
                    uint8_t d2 = saddr.sin6_addr.s6_addr[1];
                    uint8_t d3 = saddr.sin6_addr.s6_addr[2];
                    uint8_t d4 = saddr.sin6_addr.s6_addr[3];
                    uint8_t d5 = saddr.sin6_addr.s6_addr[4];
                    uint8_t d6 = saddr.sin6_addr.s6_addr[5];
                    uint8_t d7 = saddr.sin6_addr.s6_addr[6];
                    uint8_t d8 = saddr.sin6_addr.s6_addr[7];
                    uint8_t d9 = saddr.sin6_addr.s6_addr[8];
                    uint8_t d10 = saddr.sin6_addr.s6_addr[9];
                    uint8_t d11 = saddr.sin6_addr.s6_addr[10];
                    uint8_t d12 = saddr.sin6_addr.s6_addr[11];
                    uint8_t d13 = saddr.sin6_addr.s6_addr[12];
                    uint8_t d14 = saddr.sin6_addr.s6_addr[13];
                    uint8_t d15 = saddr.sin6_addr.s6_addr[14];
                    uint8_t d16 = saddr.sin6_addr.s6_addr[15];
                    
                    AddressIPv6 address(
                        d1, d2, d3, d4, d5, d6, d7, d8,
                        d9, d10, d11, d12, d13, d14, d15, d16);
                    end = new EndpointIPv6(address,
                        ntohs(saddr.sin6_port));
                    
                    return ret;
					
				}
            	default: {
            		
                    return -1;
					
				}
            	
            }
            
        }
        
        intptr_t SocketBase::nativeSend(const void* buffer, uintptr_t length,
            int flags) {
            
            if(sock == -1) {
                
                return -1;
                
            }
            
            if(buffer == nullptr) {
                
                return -1;
                
            }
            
            #if defined(NETYCAT_OS_WINDOWS)
                intptr_t ret = ::send(sock, (char*)buffer, length, flags);
            #elif defined(NETYCAT_OS_LINUX)
                intptr_t ret = ::send(sock, buffer, length, flags);
            #endif
            
            if(ret < 0) {
                
                return -1;
                
            }
            
            return ret;
            
        }
        
        intptr_t SocketBase::nativeSendto(void* buffer, uintptr_t length,
            int flags, const Endpoint* end) {
            
            if(sock == -1) {
                
                return -1;
                
            }
            
            if(buffer == nullptr) {
                
                return -1;
                
            }
            
            switch(end->getType()) {
                
                case Endpoint::IPv4: {
                    
                    const EndpointIPv4* endIPv4 = (const EndpointIPv4*)end;
                    const AddressIPv4* addrIPv4 = endIPv4->getAddress();
                    const uint8_t* dataIPv4 = addrIPv4->getData();
                    
                    struct sockaddr_in saddr;
                    ::memset(&saddr, 0, sizeof(saddr));
                    saddr.sin_family = AF_INET;
                    saddr.sin_port = ::htons(endIPv4->getPort());
                    saddr.sin_addr.s_addr = htonl(
                        (dataIPv4[0] << 24) | (dataIPv4[1] << 16) |
                        (dataIPv4[2] << 8) | dataIPv4[3]);
                    
                    intptr_t ret = ::sendto(sock, (char*)buffer, length,
                        flags, (sockaddr*)&saddr, sizeof(saddr));
                    
                    if(ret < 0) {
                        
                        return -1;
                        
                    }
                    
                    if(ret == 0) {
                        
                        return 0;
                        
                    }
                    
                    return ret;
                    
                }
                case Endpoint::IPv6: {
                    
                    const EndpointIPv6* endIPv6 = (const EndpointIPv6*)end;
                    const AddressIPv6* addrIPv6 = endIPv6->getAddress();
                    const uint8_t* dataIPv6 = addrIPv6->getData();
                    
                    struct sockaddr_in6 saddr;
                    ::memset(&saddr, 0, sizeof(saddr));
                    saddr.sin6_family = AF_INET6;
                    saddr.sin6_port = ::htons(endIPv6->getPort());
                    saddr.sin6_addr.s6_addr[0] = dataIPv6[0];
                    saddr.sin6_addr.s6_addr[1] = dataIPv6[1];
                    saddr.sin6_addr.s6_addr[2] = dataIPv6[2];
                    saddr.sin6_addr.s6_addr[3] = dataIPv6[3];
                    saddr.sin6_addr.s6_addr[4] = dataIPv6[4];
                    saddr.sin6_addr.s6_addr[5] = dataIPv6[5];
                    saddr.sin6_addr.s6_addr[6] = dataIPv6[6];
                    saddr.sin6_addr.s6_addr[7] = dataIPv6[7];
                    saddr.sin6_addr.s6_addr[8] = dataIPv6[8];
                    saddr.sin6_addr.s6_addr[9] = dataIPv6[9];
                    saddr.sin6_addr.s6_addr[10] = dataIPv6[10];
                    saddr.sin6_addr.s6_addr[11] = dataIPv6[11];
                    saddr.sin6_addr.s6_addr[12] = dataIPv6[12];
                    saddr.sin6_addr.s6_addr[13] = dataIPv6[13];
                    saddr.sin6_addr.s6_addr[14] = dataIPv6[14];
                    saddr.sin6_addr.s6_addr[15] = dataIPv6[15];
                    
                    intptr_t ret = ::sendto(sock, (char*)buffer, length,
                        flags, (sockaddr*)&saddr, sizeof(saddr));
                    
                    if(ret < 0) {
                        
                        return -1;
                        
                    }
                    
                    if(ret == 0) {
                        
                        return 0;
                        
                    }
                    
                    return ret;
                    
                }
                default: {
                    
                    return -1;
                    
                }
                
            }
            
        }
        
        bool SocketBase::nativeSetsockopt(int level, int optname,
            const void* optval, uintptr_t optlen) {
            
            if(sock != -1) {
                
                return false;
                
            }
            
            #if defined(NETYCAT_OS_WINDOWS)
                int ret = ::setsockopt(sock, level, optname,
                    (const char*)optval, optlen);
            #elif defined(NETYCAT_OS_LINUX)
                int ret = ::setsockopt(sock, level, optname,
                    optval, optlen);
            #endif
            
            if(ret != 0) {
                
                return false;
                
            }
            
            return true;
            
        }
        
        bool SocketBase::nativeSocket(int domain, int type, int protocol) {
            
            if(sock != -1) {
                
                return false;
                
            }
            
            sock = ::socket(domain, type, protocol);
            if(sock != -1) {
                
                return true;
                
            } else {
                
                return false;
                
            }
            
        }
        
        
        void SocketBase::setBroadcast(bool val) {
            
            int optval = val;
            uintptr_t optlen = sizeof(optval);
            
            nativeSetsockopt(SOL_SOCKET, SO_BROADCAST, &optval, optlen);
            
        }
        
        
        SocketStream::SocketStream() noexcept {
            
        }
        
        SocketStream::SocketStream(SocketStream&& src) noexcept :
            SocketBase(std::move(src)) {
            
        }
        
        SocketStream::~SocketStream() noexcept {
            
        }
        
        
        SocketTCP::SocketTCP() noexcept {
            
        }
        
        SocketTCP::SocketTCP(SocketTCP&& src) noexcept :
            SocketStream(std::move(src)) {
            
        }
        
        SocketTCP::~SocketTCP() noexcept {
            
        }
        
        
        void SocketTCP::connect(const Endpoint* end) {
            
            if(sock != -1) {
                
                throw ExceptionSocket();
                
            }
            
            if(end == nullptr) {
                
                throw ExceptionSocket();
                
            }
            
            switch(end->getType()) {
                
                case Endpoint::IPv4: {
                    
                    if(!nativeSocket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) {
                        
                        throw ExceptionSocket();
                        
                    }
                    
                    break;
                    
                }
                case Endpoint::IPv6: {
                    
                    if(!nativeSocket(PF_INET6, SOCK_STREAM, IPPROTO_TCP)) {
                        
                        throw ExceptionSocket();
                        
                    }
                    
                    break;
                    
                }
                default: {
                    
                    throw ExceptionSocket();
                    
                }
                
            }
                    
            if(!nativeConnect(end)) {
                
                nativeClose();
                throw ExceptionSocket();
                
            }
            
        }
        
        void SocketTCP::listen(const Endpoint* end) {
            
            if(sock != -1) {
                
                throw ExceptionSocket();
                
            }
            
            if(end == nullptr) {
                
                throw ExceptionSocket();
                
            }
            
            switch(end->getType()) {
                
                case Endpoint::IPv4: {
                    
                    if(!nativeSocket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) {
                        
                        throw ExceptionSocket();
                        
                    }
                    
                    break;
                    
                }
                case Endpoint::IPv6: {
                    
                    if(!nativeSocket(PF_INET6, SOCK_STREAM, IPPROTO_TCP)) {
                        
                        throw ExceptionSocket();
                        
                    }
                    
                    break;
                    
                }
                default: {
                    
                    throw ExceptionSocket();
                    
                }
                
            }
                    
            if(!nativeBind(end)) {
                
                nativeClose();
                throw ExceptionSocket();
                
            }
            
            if(!nativeListen(8)) {
                
                nativeClose();
                throw ExceptionSocket();
                
            }
            
        }
        
        void SocketTCP::accept(SocketTCP& s) {
            
            if(!nativeAccept(s)) {
                
                throw ExceptionSocket();
                
            }
            
        }
        
        
        uintptr_t SocketTCP::recv(void* data, uintptr_t len) {
            
            uintptr_t recvlen = 0;
            
            while(recvlen < len) {
                
                int ret;
                ret = nativeRecv((uint8_t*)data + recvlen, len - recvlen, 0);
                if(ret == -1) {
                    
                    throw ExceptionSocket();
                    
                }
                if(ret == 0) {
                    
                    throw ExceptionSocket();
                    
                }
                
                recvlen += ret;
                
            }
            
            return recvlen;
            
        }
        
        
        void SocketTCP::send(const void* data, uintptr_t len) {
            
            uintptr_t sendlen = 0;
            
            while(sendlen < len) {
                
                int ret;
                ret = nativeSend((uint8_t*)data + sendlen, len - sendlen, 0);
                if(ret == -1) {
                    
                    throw ExceptionSocket();
                    
                }
                
                sendlen += ret;
                
            }
            
        }
        
        
        void SocketTCP::close() {
            
            if(!nativeClose()) {
                
                throw ExceptionSocket();
                
            }
            
        }
        
    }
    
}

