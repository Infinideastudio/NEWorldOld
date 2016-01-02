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


#ifndef _NETYCAT_CORE_INCLUDE_H_
#define _NETYCAT_CORE_INCLUDE_H_


#include <Netycat/Core/OS.h>


#if defined(NETYCAT_OS_WINDOWS)
    #include <mswsock.h>
    #include <winsock2.h>
    #include <ws2tcpip.h>
#elif defined(NETYCAT_OS_LINUX)
    #include <netdb.h>
    #include <netinet/in.h>
    #include <sys/socket.h>
    #include <sys/types.h>
    #include <unistd.h>
#endif


#endif

