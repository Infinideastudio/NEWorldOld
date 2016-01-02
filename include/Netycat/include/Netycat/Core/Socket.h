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


#ifndef _NETYCAT_CORE_SOCKET_H_
#define _NETYCAT_CORE_SOCKET_H_


#include <stdint.h>

#include <Netycat/Core/Address.h>
#include <Netycat/Core/Buffer.h>
#include <Netycat/Core/BufferCondition.h>
#include <Netycat/Core/Endpoint.h>


namespace Netycat {
    
    namespace Core {
        
        class SocketBase {
            
            protected:
            
            int sock;
            Endpoint* endpoint;
            
            public:
            
            SocketBase() noexcept;
            SocketBase(const SocketBase& src) = delete;
            SocketBase(SocketBase&& src) noexcept;
            virtual ~SocketBase() noexcept;
            
            const Endpoint* getEndpoint();
            
            protected:
            
            bool nativeAccept(SocketBase& s);
            bool nativeBind(const Endpoint* end);
            bool nativeClose();
            bool nativeConnect(const Endpoint* end);
            bool nativeListen(int backlog);
            intptr_t nativeRecv(void* buffer, uintptr_t length,
                int flags);
            intptr_t nativeRecvfrom(void* buffer, uintptr_t length,
                int flags, Endpoint*& end);
            intptr_t nativeSend(const void* buffer, uintptr_t length,
                int flags);
            intptr_t nativeSendto(void* buffer, uintptr_t length,
                int flags, const Endpoint* end);
            bool nativeSetsockopt(int level, int optname,
                const void* optval, uintptr_t optlen);
            bool nativeSocket(int domain, int type, int protocol);
            
            public:
            
            void setBroadcast(bool val);
            
        };
        
        class SocketStream : public SocketBase {
            
            public:
            
            SocketStream() noexcept;
            SocketStream(const SocketStream& src) = delete;
            SocketStream(SocketStream&& src) noexcept;
            virtual ~SocketStream() noexcept;
            
        };
        
        class SocketTCP : public SocketStream {
            
            public:
            
            SocketTCP() noexcept;
            SocketTCP(const SocketTCP& src) = delete;
            SocketTCP(SocketTCP&& src) noexcept;
            ~SocketTCP() noexcept;
            
            void connect(const Endpoint* end);
            void listen(const Endpoint* end);
            void accept(SocketTCP& s);
            
            uintptr_t recv(void* data, uintptr_t len);
            
            void send(const void* data, uintptr_t len);
            
            void close();
            
        };
        
        
        class SocketDatagram : public SocketBase {
            
        };
        
        class SocketUDP : public SocketDatagram {
            
        };
        
        
        class SocketRaw : public SocketBase {
            
        };
        
        class SocketICMP : public SocketRaw {
            
        };
        
        
        using Socket = SocketTCP;
        
    }
    
}


#endif

