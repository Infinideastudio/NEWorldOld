#include <exception>

#include <Netycat/Core/Exception.h>


namespace Netycat {
    
    namespace Core {
        
        const char* ExceptionAddress::what() {
            
            return "ExceptionAddress";
            
        }
        
        const char* ExceptionSocket::what() {
            
            return "ExceptionSocket";
            
        }
        
    }
    
}
