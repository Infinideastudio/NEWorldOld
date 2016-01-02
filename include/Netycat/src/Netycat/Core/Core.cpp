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


#include <Netycat/Core/Include.h>

#include <Netycat/Core/Core.h>


namespace Netycat {
    
    namespace Core {
        
        bool startup() {

            #if defined(NETYCAT_OS_WINDOWS)
                WSADATA wsaData;
                if(::WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
                    
                    return false;
                    
                }
                if(wsaData.wVersion != MAKEWORD(2, 2)) {
                    
                    ::WSACleanup();
                    return false;
                    
                }
            #endif
            
            return true;
            
        }
        
        bool cleanup() {
            
            #if defined(NETYCAT_OS_WINDOWS)
                ::WSACleanup();
            #endif
            
            return true;
            
        }
        
    }
    
}

