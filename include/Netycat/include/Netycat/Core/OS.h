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


#ifndef _NETYCAT_CORE_OS_H_
#define _NETYCAT_CORE_OS_H_


#if defined(_WIN32)
    #define NETYCAT_OS_WINDOWS
#elif defined(__linux) || defined(_inux)
    #define NETYCAT_OS_LINUX
#else
    #error Sorry, but Netycat do not support your operation system now...
#endif


#endif

