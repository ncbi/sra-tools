#pragma once

#include <iostream>
#include <cstring>
#include <memory>

#include "kfile_interface.hpp"

namespace custom_istream {

/* abstract base-class aka interface, for reading from:
    - std::string
    - vdb::KFile
    - vdb::KStream
*/
class src_interface;
typedef std::shared_ptr< src_interface > src_interface_ptr;
class src_interface {
    public :
        virtual size_t read( char * dst, size_t requested ) = 0;
};

// wraps a string and implements the virtual read-method
class string_src : public src_interface {
    private:
        const std::string f_content_str;
        const size_t f_size;
        size_t f_start;
        const char* f_buffer;

        // private Ctor
        string_src( const std::string& content )
            : f_content_str{ content },
              f_size{ content . length() },
              f_start{ 0 } {
            f_buffer = f_content_str . c_str();
        }

    public:
        // Factory
        static src_interface_ptr make( const std::string& content ) {
            return src_interface_ptr( new string_src( content ) );
        }

        size_t read( char * dst, size_t requested ) {
            if ( f_start < ( f_size - 1 ) ) {
                size_t left = f_size - f_start;
                size_t res = std::min( requested, left );
                const char * src = f_buffer + f_start;
                memcpy( dst, src, res );
                f_start += res;
                return res;
            }
            return -1;
        }
};

// wraps a KFile and implements the virtual read-method
// will not AddRef the given KFile, it will release it in its Dtor
class kfile_src : public src_interface {
    private:
        const vdb::KFile * f_file;
        uint64_t f_pos;

        // private Ctor
        kfile_src( const vdb::KFile * a_file )
            : f_file{ a_file }, f_pos{ 0 } { }

    public:
        // Dtor
        ~kfile_src( void ) { vdb::KFileRelease( f_file ); }

        // Factory
        static src_interface_ptr make( const vdb::KFile * a_file ) {
            return src_interface_ptr( new kfile_src( a_file ) );
        }

        size_t read( char * dst, size_t requested ) {
            size_t num_read = -1;
            vdb::rc_t rc = vdb::KFileRead( f_file, f_pos, dst, requested, &num_read );
            if ( 0 != rc ) {
                vdb::MsgFactory::throw_msg( "KFileRead() failed : ", rc );
            } else {
                f_pos += num_read;
            }
            return num_read;
        }
};

// wraps a KStream and implements the virtual read-method
// will not AddRef the given KStream, it will release it in its Dtor
class kstream_src : public src_interface {
    private:
        const vdb::KStream* f_stream;

        // private Ctor
        kstream_src( const vdb::KStream * a_stream ) : f_stream{ a_stream } { }

    public:
        // Dtor
        ~kstream_src( void ) { vdb::KStreamRelease( f_stream ); }

        // Factory
        static src_interface_ptr make( const vdb::KStream * a_stream ) {
            return src_interface_ptr( new kstream_src( a_stream ) );
        }

        size_t read( char * dst, size_t requested ) {
            size_t num_read = -1;
            vdb::rc_t rc = vdb::KStreamRead( f_stream, dst, requested, &num_read );
            if ( 0 != rc ) {
                vdb::MsgFactory::throw_msg( "KStreamRead() failed : ", rc );
            }
            return num_read;
        }
};


template < class CharT, class Traits = std::char_traits< CharT > >
    class custom_streambuf : public std::basic_streambuf< CharT, Traits > {

    using char_type = CharT;
    using int_type = typename Traits::int_type;
    using traits_type = Traits;

    private :
        const std::streamsize f_pbSize = 4;
        const std::streamsize f_bufSize;
        src_interface_ptr f_src;
        CharT * f_buffer;

        // the private worker to provide data...
        int fill_buffer( void ) {
            // calculate how many bytes to move for put-back-area
            char_type* nxtptr = std::basic_streambuf< CharT, Traits >::gptr();
            if ( nullptr == nxtptr ) { return Traits::eof(); }
            char_type* bckptr = std::basic_streambuf< CharT, Traits >::eback();
            if ( nullptr == bckptr ) { return Traits::eof(); }
            std::streamsize numPutbacks = std::min( nxtptr - bckptr, f_pbSize );

            // perform the move
            char_type* dst = f_buffer + ( (f_pbSize - numPutbacks ) * sizeof( char_type ) );
            char_type* src = nxtptr - numPutbacks * sizeof( char_type );
            size_t to_move = numPutbacks * sizeof( char_type );
            memcpy( dst, src, to_move );

            // get data from the source
            char_type * rd_dst = f_buffer + f_pbSize * sizeof( char_type );
            size_t to_read = f_bufSize - f_pbSize;
            int read = f_src -> read( rd_dst, to_read );

            if ( read <= 0 ) {
                char_type* begin_of_get_area = 0;
                char_type* next_read_position = 0;
                char_type* end_of_get_area = 0;
                std::basic_streambuf< CharT, Traits >::setg(
                    begin_of_get_area, next_read_position, end_of_get_area );
                read = -1;
            } else {
                char_type* begin_of_get_area = f_buffer + f_pbSize - numPutbacks;
                char_type* next_read_position = f_buffer + f_pbSize;
                char_type* end_of_get_area = f_buffer + f_pbSize + read;
                std::basic_streambuf< CharT, Traits >::setg(
                    begin_of_get_area, next_read_position, end_of_get_area );
            }
            return read;
        }

        // copy-ctor and assignment-operator made private and unreachable
        custom_streambuf( const custom_streambuf& );
        custom_streambuf & operator= ( const custom_streambuf& );

    protected :

        // virtual method of std::basic_streambuf
        // has to be overwritten
        int_type underflow() {
            // did the next-pointer reach the end-pointer?
            char_type* nxtptr  = std::basic_streambuf< CharT, Traits >::gptr();
            if ( nullptr == nxtptr ) { return Traits::eof(); }
            char_type* endptr = std::basic_streambuf< CharT, Traits >::egptr();
            if ( nullptr == endptr ) { return Traits::eof(); }
            if ( nxtptr < endptr ) {
                // No: we still have chars to give...
                return Traits::to_int_type( *nxtptr );
            }
            // Yes: we have to refill from source
            int read = fill_buffer();
            if ( read < 0 ) {
                return Traits::eof();
            } else {
                char_type* nxtptr = std::basic_streambuf< CharT, Traits >::gptr();
                if ( nullptr == nxtptr ) { return Traits::eof(); }
                return Traits::to_int_type( *nxtptr );
            }
        }

        // virtual method of std::basic_streambuf
        // has to be overwritten
        int_type pbackfail( int_type c ) {
            char_type* nxtptr = std::basic_streambuf< CharT, Traits >::gptr();
            if ( nullptr == nxtptr ) { return Traits::eof(); }
            char_type* bckptr = std::basic_streambuf< CharT, Traits >::eback();
            if ( nullptr == bckptr ) { return Traits::eof(); }
            if ( nxtptr != bckptr ) {
                std::basic_streambuf< CharT, Traits >::gbump( -1 );
                if ( ! Traits::eq_int_type( c, Traits::eof() ) ) {
                    char_type* nxtptr = std::basic_streambuf< CharT, Traits >::gptr();
                    if ( nullptr == nxtptr ) { return Traits::eof(); }
                    *nxtptr = Traits::to_char_type( c );
                }
                return Traits::not_eof( c );
            } else {
                return Traits::eof();
            }
        }

    public :
        // Ctor
        custom_streambuf( src_interface_ptr src, std::streamsize bufSize = 4096 )
            : f_bufSize{ 4096 },
              std::basic_streambuf<CharT, Traits>(),
              f_src{ src } {
                f_buffer = new CharT [ f_bufSize ];
                char_type* begin_of_get_area = f_buffer + f_pbSize;
                char_type* next_read_position = f_buffer + f_pbSize;
                char_type* end_of_get_area = f_buffer + f_pbSize;
                std::basic_streambuf< CharT, Traits >::setg(
                    begin_of_get_area, next_read_position, end_of_get_area );
        }

        // Dtor
        ~custom_streambuf( void ) {
            delete [] f_buffer;
        }

}; // end of class "custom_streambuf"

// a class derived from the istream and the custom_streambuf
class custom_istream : private custom_streambuf< char >, public std::istream {
    public:
        // Ctor - takes a src-buf-pointer to create
        explicit custom_istream( src_interface_ptr src )
            : custom_streambuf< char >( src ), std::istream( this ) {}

        // Factory - takes a string and creates a custom_istream
        static custom_istream make_from_string( const std::string& src ) {
            return custom_istream( string_src::make( src ) );
        }

        // Factory - takes a KFile and creates a custom_istream
        static custom_istream make_from_kfile( const vdb::KFile * src ) {
            return custom_istream( kfile_src::make( src ) );
        }

        // Factory - takes a KStream and creates a custom_istream
        static custom_istream make_from_kstream( const vdb::KStream * src ) {
            return custom_istream( kstream_src::make( src ) );
        }
};

}; // end of namespace "custom_istream"

namespace custom_istream_test {

class test {
    public:
        static size_t consume_line_by_line( std::istream& stream, bool show = false, size_t max = SIZE_MAX ) {
            size_t res = 0;
            std::string s;
            while( res < max && std::getline( stream, s ) ) {
                if ( show ) { std::cout << s << std::endl; }
                res++;
            }
            return res;
        }

        static size_t consume_numbers( std::istream& s, bool show = false ) {
            size_t res = 0;
            while ( s . good() ) {
                int n;
                s >> n;
                if ( show ) { std::cout << n << std::endl; }
                res++;
            }
            return res;
        }

        static bool test1( void ) {
            auto stream = custom_istream::custom_istream::make_from_string(
                "this is a very long \nlong long string" );
            return ( 2 == consume_line_by_line( stream ) );
        }

        static bool test2( void ) {
            auto stream = custom_istream::custom_istream::make_from_string( "100 200 300 400" );
            return ( 4 == consume_numbers( stream ) );
        }

        static bool test3( void ) {
            auto src = vdb::KFileFactory::make_from_path( "Makefile" );
            auto stream = custom_istream::custom_istream::make_from_kfile( src );
            bool res = ( consume_line_by_line( stream ) > 0 );
            return res;
        }

        static bool test4( void ) {
            bool res = false;
            const std::string url{ "https://sra-downloadb.be-md.ncbi.nlm.nih.gov/sos5/sra-pub-zq-11/SRR000/000/SRR000001/SRR000001.lite.1" };
            auto src = vdb::KFileFactory::make_from_vpath( url );
            if ( nullptr != src ) {
                auto stream = custom_istream::custom_istream::make_from_kfile( src );
                res = ( consume_line_by_line( stream, false, 100 ) > 0 );
            }
            return res;
        }

        static bool test5( void ) {
            bool res = false;
            const std::string uri{ "https://www.nih.gov" };
            auto src = vdb::KStreamFactory::make_from_uri( uri );
            if ( nullptr != src ) {
                auto stream = custom_istream::custom_istream::make_from_kstream( src );
                res = ( consume_line_by_line( stream, false, 20 ) > 0 );
            }
            return res;
        }

        static bool test6( void ) {
            try {
                const std::string uri{ "https://www.nih.gov/an_invalid_path" };
                auto src = vdb::KStreamFactory::make_from_uri( uri );
                if ( nullptr != src ) {
                    auto stream = custom_istream::custom_istream::make_from_kstream( src );
                    consume_line_by_line( stream, false, 20 );
                }
            } catch ( const std::runtime_error& ex ) {
                // we are expecting an exception here - because of the invalid path
                return true;
            }
            return false;
        }

        static bool run( void ) {
            try {
                bool res1 = test1();
                std::cout << "test1 : " << std::boolalpha << res1 << std::endl;

                bool res2 = test2();
                std::cout << "test2 : " << std::boolalpha << res2 << std::endl;

                bool res3 = test3();
                std::cout << "test3 : " << std::boolalpha << res3 << std::endl;

                bool res4 = test4();
                std::cout << "test4 : " << std::boolalpha << res4 << std::endl;

                bool res5 = test5();
                std::cout << "test5 : " << std::boolalpha << res5 << std::endl;

                bool res6 = test6();
                std::cout << "test6 : " << std::boolalpha << res6 << std::endl;

                return res1 && res2 && res3 && res4 && res5 && res6;
            } catch ( const std::runtime_error& ex ) {
                std::cout << ex . what() << std::endl;
            }
            return false;
        }
}; // end of class test

}; // end of namespace "custom_istream_test"
