#pragma once

#include <memory>
#include <iostream>
#include <fstream>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include "../vdb/VdbObj.hpp"
#include "values.hpp"

using namespace std;

namespace sra_convert {

class fd_reader;
typedef std::shared_ptr< fd_reader > fd_reader_ptr;
class fd_reader {
    protected :
        enum class State_E : uint8_t { DONE = 0, WOULD_BLOCK = 1, ERR = 2, OK = 3 };
        int f_fd;
        State_E f_status;
        int f_bytes_read;
        char f_buffer[ 4096 ];

        // >>>>> Ctor <<<<<
        fd_reader( int fd ) : f_fd( fd ), f_status( State_E::OK ) {
            // set the pipe file descriptor to none-blocking
            fcntl( fd, F_SETFL, fcntl( fd, F_GETFL ) | O_NONBLOCK );
        }

        virtual void write_buffer( void ) {}

        State_E read_buffer( void ) {
            State_E res;
            f_bytes_read = read( f_fd, f_buffer, sizeof( f_buffer ) );
            if ( 0 == f_bytes_read ) {
                res = State_E::DONE;
            } else if ( f_bytes_read > 0 ) {
                res = State_E::OK;
            } else {
                if ( errno == EAGAIN ) {
                    res = State_E::WOULD_BLOCK;
                } else {
                    res = State_E::ERR;
                }
            }
            return res;
        }

    public :
        static fd_reader_ptr make( int fd ) { return fd_reader_ptr( new fd_reader( fd ) ); }

        void show_status( void ) const {
            switch( f_status ) {
                case State_E::DONE : cout << "DONE\n"; break;
                case State_E::WOULD_BLOCK : cout << "WOULD_BLOCK\n"; break;
                case State_E::ERR : cout << "ERR\n"; break;
                case State_E::OK : cout << "OK\n"; break;
            }
        }

        bool would_block( void ) const { return f_status == State_E::WOULD_BLOCK; }

        State_E read_loop( void ) {
            if ( f_status == State_E::DONE || f_status == State_E::ERR ) {
                return f_status;
            }
            if ( f_status == State_E::WOULD_BLOCK ) {
                f_status = State_E::OK;
            }
            while ( f_status == State_E::OK ) {
                f_status = read_buffer();
                if ( f_status == State_E::OK ) { write_buffer(); }
            }
            return f_status;
        }
};

class fd_to_null_reader : public fd_reader {
    protected :
        // >>>>> Ctor <<<<<
        fd_to_null_reader( int fd ) : fd_reader( fd ) {}

        void write_buffer( void ) override { }

    public :
        static fd_reader_ptr make( int fd ) {
            return fd_reader_ptr( new fd_to_null_reader( fd ) );
        }
};

class fd_to_stdout_reader : public fd_reader {
    protected :
        // >>>>> Ctor <<<<<
        fd_to_stdout_reader( int fd ) : fd_reader( fd ) {}

        void write_buffer( void ) override {
            string s( f_buffer, f_bytes_read );
            cout << s;
        }

    public :
        static fd_reader_ptr make( int fd ) {
            return fd_reader_ptr( new fd_to_stdout_reader( fd ) );
        }
};

class fd_to_stderr_reader : public fd_reader {
    protected :
        // >>>>> Ctor <<<<<
        fd_to_stderr_reader( int fd ) : fd_reader( fd ) {}

        void write_buffer( void ) override {
            string s( f_buffer, f_bytes_read );
            cerr << s;
        }

    public :
        static fd_reader_ptr make( int fd ) {
            return fd_reader_ptr( new fd_to_stderr_reader( fd ) );
        }
};

class fd_to_md5_reader : public fd_reader {
    protected :
        const string f_filename;
        vdb::MD5 f_md5;

        // >>>>> Ctor <<<<<
        fd_to_md5_reader( int fd, const string& filename ) : fd_reader( fd ), f_filename( filename ) { }

        void write_buffer( void ) override {
            f_md5 . append( f_buffer, f_bytes_read );
        }

    public :
        // >>>>> Dtor <<<<<
        ~fd_to_md5_reader() {
            ofstream out( f_filename );
            out << f_md5 . digest();
            out . close();
        }

        static fd_reader_ptr make( int fd, const string& filename ) {
            return fd_reader_ptr( new fd_to_md5_reader( fd, filename ) );
        }
};

class fd_to_md5_value_reader : public fd_reader {
    protected :
        const string f_value_name;
        KV_Map_Ptr f_values;
        vdb::MD5 f_md5;

        // >>>>> Ctor <<<<<
        fd_to_md5_value_reader( int fd, const string& value_name, KV_Map_Ptr values )
            : fd_reader( fd ), f_value_name( value_name ), f_values( values ) { }

        void write_buffer( void ) override {
            f_md5 . append( f_buffer, f_bytes_read );
        }

    public :
        // >>>>> Dtor <<<<<
        ~fd_to_md5_value_reader() {
            f_values -> insert( f_value_name, f_md5 . digest() );
        }

        static fd_reader_ptr make( int fd, const string& value_name, KV_Map_Ptr values ) {
            return fd_reader_ptr( new fd_to_md5_value_reader( fd, value_name, values ) );
        }
};

class fd_to_file_reader : public fd_reader {
    protected :
        ofstream f_out;

        // >>>>> Ctor <<<<<
        fd_to_file_reader( int fd, const string& filename ) : fd_reader( fd ), f_out( filename ) { }

        void write_buffer( void ) override {
            string s( f_buffer, f_bytes_read );
            f_out << s;
        }

    public :
        // >>>>> Dtor <<<<<
        ~fd_to_file_reader() { f_out.close(); }

        static fd_reader_ptr make( int fd, const string& filename ) {
            return fd_reader_ptr( new fd_to_file_reader( fd, filename ) );
        }
};

class fd_to_value_reader : public fd_reader {
    protected :
        string f_value_name;
        KV_Map_Ptr f_values;
        string f_content;

        // >>>>> Ctor <<<<<
        fd_to_value_reader( int fd, const string& value_name, KV_Map_Ptr values )
            : fd_reader( fd ), f_value_name( value_name ), f_values( values ) { }

        void write_buffer( void ) override {
            if ( f_bytes_read > 0 ) {
                f_content . append( f_buffer, f_bytes_read );
            }
        }

    public :
        // >>>>> Dtor <<<<<
        ~fd_to_value_reader() {
            if ( f_bytes_read > 0 ) {
                f_content . append( f_buffer, f_bytes_read );
            }
            f_values -> insert( f_value_name, f_content );
        }

        static fd_reader_ptr make( int fd, const string& value_name, KV_Map_Ptr values ) {
            return fd_reader_ptr( new fd_to_value_reader( fd, value_name, values ) );
        }
};

}
