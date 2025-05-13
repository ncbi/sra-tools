#pragma once

#include <cstdint>
#include <memory>

#include "NameList.hpp"

namespace vdb {

extern "C" {

typedef uint32_t rc_t;
typedef uint32_t ver_t;
typedef struct timeout_t timeout_t;
typedef int64_t KTime_t;
typedef uint32_t bitsz_t;

struct uint8_arr { const uint8_t * data; size_t len; };
typedef uint8_arr* uint8_arr_ptr;

struct int8_arr { const int8_t * data; size_t len; };
typedef int8_arr* int8_arr_ptr;

struct uint16_arr { const uint16_t * data; size_t len; };
typedef uint16_arr* uint16_arr_ptr;

struct int16_arr { const int16_t * data; size_t len; };
typedef int16_arr* int16_arr_ptr;

struct uint32_arr { const uint32_t * data; size_t len; };
typedef uint32_arr* uint32_arr_ptr;

struct int32_arr { const int32_t * data; size_t len; };
typedef int32_arr* int32_arr_ptr;

struct uint64_arr { const uint64_t * data; size_t len; };
typedef uint64_arr* uint64_arr_ptr;

struct int64_arr { const int64_t * data; size_t len; };
typedef int64_arr* int64_arr_ptr;

struct f32_arr { const float * data; size_t len; };
typedef f32_arr* f32_arr_ptr;

struct f64_arr { const double * data; size_t len; };
typedef f64_arr* f64_arr_ptr;

struct char_arr{ const char * data; size_t len; };
typedef char_arr* char_arr_ptr;

#define VersionGetMajor( self ) ( ( self ) >> 24 )
#define VersionGetMinor( self ) ( ( ( self ) >> 16 ) & 0xFF )
#define VersionGetRelease( self ) ( ( self ) & 0xFFFF )

const char * GetRCFilename( void );
const char * GetRCFunction( void );
uint32_t GetRCLineno( void );
rc_t SetRCFileFuncLine( rc_t rc, const char *filename, const char *funcname, uint32_t lineno );
bool GetUnreadRCInfo( rc_t *rc, const char **filename, const char **funcname, uint32_t *lineno );

rc_t string_printf( char *dst, size_t bsize, size_t *num_writ, const char *fmt, ... );

typedef struct KFile KFile;
typedef struct KDirectory KDirectory;
typedef struct VResolver VResolver;
typedef struct VPath VPath;
typedef struct KChunkReader KChunkReader;
typedef struct KProcMgr KProcMgr;
typedef struct KTask KTask;
typedef struct KTaskTicket KTaskTicket;
typedef struct KConfig KConfig;
typedef struct KConfigNode KConfigNode;

typedef struct KMetadata KMetadata;
typedef struct VSchema VSchema;
typedef struct VTypedecl VTypedecl;
typedef struct VTypedesc VTypedesc;
typedef struct KIndex KIndex;
typedef struct VBlob VBlob;
typedef struct VView VView;
typedef struct VCursor VCursor;
typedef struct VTable VTable;
typedef struct VDatabase VDatabase;
typedef struct VFSManager VFSManager;

typedef struct VDBManager VDBManager;

typedef uint32_t KPathType;
enum {
    kptFirstDefined = 0,
    kptNotFound = kptFirstDefined,
    kptBadPath,
    kptFile,
    kptDir,
    kptCharDev,
    kptBlockDev,
    kptFIFO,
    kptZombieFile,
    kptFakeRoot,
    kptDataset,
    kptDatatype,
    kptLastDefined,
    kptAlias = 128
};

typedef uint32_t KCreateMode;
enum {
    kcmOpen,
    kcmInit,
    kcmCreate,
    kcmSharedAppend,
    kcmValueMask = 15,
    kcmMD5     = ( 1 << 6 ),
    kcmParents = ( 1 << 7 ),
    kcmBitMask = ( 1 << 8 ) - kcmValueMask - 1
};

typedef struct MD5State MD5State;
struct MD5State {
    uint32_t count [ 2 ];
    uint32_t abcd [ 4 ];
    uint8_t buf [ 64 ];
};
void MD5StateInit( MD5State *md5 );
void MD5StateAppend( MD5State *md5, const void *data, size_t size );
void MD5StateFinish( MD5State *md5, uint8_t digest [ 16 ] );

}; // end of extern "C"

const uint8_t RD_FILTER_PASS = 0;
const uint8_t RD_TYPE_TECHNICAL = 0;
const uint8_t RD_TYPE_BIOLOGICAL = 1;

class VDB {
    public :
        static void UnreadRC( void ) {
            rc_t rc;
            const char * filename;
            const char * funcname;
            uint32_t lineno;
            while( GetUnreadRCInfo( &rc, &filename, &funcname, &lineno ) ) { ; }
        }
};

class VDBObj {
    private :
        rc_t rc;

    public :
        VDBObj() : rc( 0 ) {}

        operator bool() const { return ( 0 == rc ); }

        std::string error( void ) const {
            if ( 0 == rc ) { return std::string( "OK"); }
            char buffer[ 512 ];
            size_t num_writ;
            rc_t rc2 = string_printf( buffer, sizeof buffer, &num_writ, "%R", rc );
            if ( 0 == rc2 ) {
                return std::string( buffer );
            }
            return std::string( "failed to print error" );
        }

        bool set_rc( rc_t a_rc ) { rc = a_rc; return ( 0 == rc ); }

        list_of_str into_list( KNamelist * t ) const {
            list_of_str res;
            if ( 0 == rc ) {
                NameList l( t );
                l . into( res );
            }
            return res;
        }
};

class MD5 {
    private :
        MD5State f_state;

    public :
        MD5( void ) { MD5StateInit( &f_state ); }

        void append( const void * data, size_t size ) {
            MD5StateAppend( &f_state, data, size );
        }

        void finish( uint8_t digest[ 16 ] ) {
            MD5StateFinish( &f_state, digest );
        }

        std::string digest( void ) {
            uint8_t d[ 16 ];
            finish( d );
            std::string res;
            res.reserve(33);
            for( auto i = 0; i < 16; i++ ) {
                res.append(1, "0123456789abcdef"[d[i] >> 4]);
                res.append(1, "0123456789abcdef"[d[i] & 0xF]);
            }
            return res;
        }
};

};
