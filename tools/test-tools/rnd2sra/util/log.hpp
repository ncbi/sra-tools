#pragma once

#include <mutex>
#include <memory>
#include <sstream>
#include <stdarg.h>
#include <stdio.h>

using namespace std;

namespace sra_convert {
    
class Log;
typedef shared_ptr< Log > LogPtr;
class Log {
    private :
        mutex m;

    public :
        Log() {}

        void write( const string s, ... ) {
            std::lock_guard<std::mutex> lk( m );
            va_list args;
            va_start( args, s );
            vfprintf( stderr, s . c_str(), args );
            va_end( args );
        }

        void write( const stringstream& s ) { write( s . str() ); }

        void write( const char *s, ... ) { 
            std::lock_guard<std::mutex> lk( m );
            va_list args;
            va_start( args, s );
            vfprintf( stderr, s, args );
            va_end( args );
        }

        static LogPtr make( void ) { return LogPtr( new Log() ); }
};

} // namespace sra_convert
