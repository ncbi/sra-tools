#pragma once

#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string>
#include <fstream>
#include <iostream>

class MemoryMapped {
    public:
        enum CacheHint { Normal, SequentialScan, RandomAccess };
        enum MapRange { WholeFile = 0 };

        MemoryMapped() : _filesize( 0 ), _hint( Normal ), 
            _mappedBytes( 0 ), _file( 0 ), _mappedView ( NULL ) { }

    
        /// open file, mappedBytes = 0 maps the whole file
        MemoryMapped( const std::string& filename, size_t mappedBytes = WholeFile,
                    CacheHint hint = Normal ) : _filesize( 0 ), _hint( hint ),
            _mappedBytes( mappedBytes ), _file( 0 ), _mappedView ( NULL ) {
            open( filename.c_str(), mappedBytes, hint );
        }

        /// open file, mappedBytes = 0 maps the whole file
        MemoryMapped( const std::string& filename, size_t mappedBytes = WholeFile,
                      bool create_first = false, CacheHint hint = Normal ) : _filesize( 0 ), _hint( hint ),
            _mappedBytes( mappedBytes ), _file( 0 ), _mappedView ( NULL ) {
            bool made = true;
            if ( create_first ) {
                made = make_empty_file( filename, mappedBytes );
            }
            if ( made ) {
                open( filename.c_str(), mappedBytes, _hint );
            }
        }

        ~MemoryMapped() { close(); }

        bool open( const char * filename, size_t mappedBytes = WholeFile,
                CacheHint hint = Normal )  {
            if ( isValid() ) return false;
            _file       = 0;
            _filesize   = 0;
            _hint       = hint;
            _mappedView = NULL;

            _file = ::open( filename, O_RDWR | O_LARGEFILE );
            if ( _file == -1 ) { _file = 0; return false; }

            struct stat64 statInfo;
            if ( fstat64( _file, &statInfo ) < 0 ) { return false; }
            _filesize = statInfo.st_size;

            remap( 0, mappedBytes ); // initial mapping
            if ( !_mappedView ) { return false; }
            return true; // everything's fine
        }

        void close() {
            if ( _mappedView ) { ::munmap(_mappedView, _filesize); _mappedView = NULL; }
            if ( _file ) { ::close(_file); _file = 0; }
            _filesize = 0;
        }

        void * ptr( uint64_t ofs ) {
            uint8_t * p = ( uint8_t * )_mappedView;
            uint8_t * p_ofs = p + ofs;
            return p_ofs;
        }

        uint8_t * u8_ptr( uint64_t ofs ) { return (uint8_t *)ptr( ofs ); }
        uint16_t * u16_ptr( uint64_t ofs ) { return (uint16_t *)ptr( ofs * 2 ); }
        uint32_t * u32_ptr( uint64_t ofs ) { return (uint32_t *)ptr( ofs * 4 ); }
        uint64_t * u64_ptr( uint64_t ofs ) { return (uint64_t *)ptr( ofs * 8 ); }

        uint64_t& operator[]( uint64_t idx ) { return *u64_ptr( idx ); }
        
        std::string to_string( void ) { return std::string( ( const char * )u8_ptr( 0 ), mappedSize() ); }

        bool isValid() const { return _mappedView != NULL; }
        uint64_t size() const { return _filesize; }
        size_t mappedSize() const { return _mappedBytes; }

        /// replace mapping by a new one of the same file, offset MUST be a multiple of the page size
        bool remap( uint64_t offset, size_t mappedBytes ) {
            if ( !_file ) { return false; }
            if ( mappedBytes == WholeFile ) { mappedBytes = _filesize; }
            if ( _mappedView ) { ::munmap( _mappedView, _mappedBytes ); _mappedView = NULL; }
            if ( offset > _filesize ) { return false; }
            if ( offset + mappedBytes > _filesize ) { mappedBytes = size_t( _filesize - offset ); }
            _mappedView = ::mmap64( NULL, mappedBytes, PROT_READ|PROT_WRITE, MAP_SHARED, _file, offset );
            if ( _mappedView == MAP_FAILED ) { _mappedBytes = 0; _mappedView  = NULL; return false; }
            _mappedBytes = mappedBytes;

            // tweak performance
            int linuxHint = 0;
            switch ( _hint )
            {
                case Normal:         linuxHint = MADV_NORMAL;     break;
                case SequentialScan: linuxHint = MADV_SEQUENTIAL; break;
                case RandomAccess:   linuxHint = MADV_RANDOM;     break;
                default: break;
            }
            linuxHint |= MADV_WILLNEED; // assume that file will be accessed soon
            linuxHint |= MADV_HUGEPAGE; // assume that file will be large
            ::madvise( _mappedView, _mappedBytes, linuxHint );
            return true;
        }

    private:
        MemoryMapped( const MemoryMapped& ); /// don't copy object
        MemoryMapped& operator=( const MemoryMapped& ); /// don't copy object

        static int getpagesize() { return sysconf(_SC_PAGESIZE); } 

        bool make_empty_file( const std::string& filename, uint64_t file_size ) {
            try {
                std::ofstream ofs( filename, std::ios::binary | std::ios::out );
                ofs.seekp( file_size - 1 );
                ofs.write( "", 1 );
                return true;
            } catch ( ... ) {
                return false;
            }
        }

        uint64_t    _filesize; /// file size
        CacheHint   _hint; /// caching strategy
        size_t      _mappedBytes; /// mapped size
        typedef int   FileHandle; /// define handle
        FileHandle  _file; /// file handle
        void*       _mappedView; /// pointer to the file contents mapped into memory
};
