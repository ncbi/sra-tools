/*===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's official duties as a United States Government employee and
*  thus cannot be copyrighted.  This software/database is freely available
*  to the public for use. The National Library of Medicine and the U.S.
*  Government have not placed any restriction on its use or reproduction.
*
*  Although all reasonable efforts have been taken to ensure the accuracy
*  and reliability of the software and data, the NLM and the U.S.
*  Government do not and cannot warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* ===========================================================================
*
*/

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
            : std::basic_streambuf<CharT, Traits>(),
              f_bufSize{ 4096 },
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
