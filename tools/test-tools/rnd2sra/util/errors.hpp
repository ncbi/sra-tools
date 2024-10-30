#pragma once

#include <mutex>
#include <memory>

using namespace std;

namespace sra_convert {
    
class Errors;
typedef shared_ptr< Errors > ErrorsPtr;
class Errors {
    private :
        uint32_t value;
        mutex m;
        
    public :
        Errors() : value( 0 ) {}
        
        void inc( void ) {
            std::lock_guard<std::mutex> lk( m );
            value++;
        }
        
        uint32_t get( void ) {
            std::lock_guard<std::mutex> lk( m );
            return value;
        }

        static ErrorsPtr make( void ) { return ErrorsPtr( new Errors() ); }
};

} // namespace sra_convert
