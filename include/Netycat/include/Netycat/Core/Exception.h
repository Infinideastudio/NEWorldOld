#ifndef _NETYCAT_CORE_EXCEPTION_H_
#define _NETYCAT_CORE_EXCEPTION_H_


#include <exception>


namespace Netycat {
    
    namespace Core {
        
        class ExceptionAddress : public std::exception {
            
            public:
            
            const char* what();
            
        };
        
        class ExceptionSocket : public std::exception {
            
            public:
            
            const char* what();
            
        };
        
    }
    
}


#endif

