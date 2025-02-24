#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <algorithm>
#include <list>
#include <sstream>

namespace vdb {

extern "C" {

typedef uint32_t rc_t;
typedef uint32_t ver_t;
typedef struct timeout_t timeout_t;
typedef int64_t KTime_t;
typedef uint64_t bitsz_t;

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

/* ========================================================================================= */

typedef struct KNamelist KNamelist;

rc_t KNamelistAddRef( const KNamelist *self );
rc_t KNamelistRelease( const KNamelist *self );
rc_t KNamelistCount( const KNamelist *self, uint32_t *count );
rc_t KNamelistGet( const KNamelist *self, uint32_t idx, const char **name );
bool KNamelistContains( const KNamelist * self, const char * to_find );

/* ========================================================================================= */

/*
typedef struct VNamelist VNamelist;

rc_t VNamelistMake( VNamelist **names, const uint32_t alloc_blocksize );
rc_t VNamelistRelease( const VNamelist *self );
rc_t VNamelistToNamelist( VNamelist *self, KNamelist **cast );
rc_t VNamelistToConstNamelist( const VNamelist *self, const KNamelist **cast );
rc_t VNamelistAppend ( VNamelist *self, const char* src );
rc_t VNamelistAppendString( VNamelist *self, const String * src );
rc_t VNamelistRemove( VNamelist *self, const char* s );
rc_t VNamelistRemoveAll( VNamelist *self );
rc_t VNamelistRemoveIdx( VNamelist *self, uint32_t idx );
rc_t VNamelistIndexOf( VNamelist *self, const char* s, uint32_t *found );
rc_t VNameListCount( const VNamelist *self, uint32_t *count );
rc_t VNameListGet( const VNamelist *self, uint32_t idx, const char **name );
void VNamelistReorder( VNamelist *self, bool case_insensitive );
rc_t foreach_String_part( const String * src, const uint32_t delim,
        rc_t ( * f ) ( const String * part, void *data ), void * data );
rc_t foreach_Str_part( const char * src, const uint32_t delim,
        rc_t ( * f ) ( const String * part, void *data ), void * data );
rc_t VNamelistSplitString( VNamelist * list, const String * str, const uint32_t delim );
rc_t VNamelistSplitStr( VNamelist * list, const char * str, const uint32_t delim );
rc_t VNamelistFromKNamelist( VNamelist ** list, const KNamelist * src );
rc_t CopyVNamelist( VNamelist ** list, const VNamelist * src );
rc_t VNamelistFromString( VNamelist ** list, const String * str, const uint32_t delim );
rc_t VNamelistFromStr( VNamelist ** list, const char * str, const uint32_t delim );
rc_t VNamelistJoin( const VNamelist * list, const uint32_t delim, const String ** rslt );
rc_t VNamelistContainsString( const VNamelist * list, const String * item, int32_t * idx );
rc_t VNamelistContainsStr( const VNamelist * list, const char * item, int32_t * idx );
*/

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

typedef std::list< std::string > list_of_str;

class NameList {
    private :
        KNamelist * l;
    public :
        NameList( KNamelist * al ) : l( al ) {}
        ~NameList() { KNamelistRelease( l ); }

        void into( list_of_str& res ) {
            uint32_t count;
            rc_t rc = KNamelistCount( l, &count );
            for ( uint32_t i = 0; 0 == rc && i < count; i++ ) {
                const char * s = nullptr;
                rc = KNamelistGet( l, i, &s );
                if ( 0 == rc && nullptr != s ) {
                    res . push_back( std::string( s ) );
                }
            }
        }

        list_of_str to_list( void ) {
            list_of_str res;
            into( res );
            return res;
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
            char buf[ 8 ];
            for( uint32_t i = 0; i < 16; i++ ) {
                snprintf( buf, sizeof buf, "%02x", d[ i ] );
                res . append( buf );
            }
            return res;
        }
};

class ChecksumKind;
typedef std::shared_ptr< ChecksumKind > ChecksumKind_ptr;
class ChecksumKind {
    private :
        enum class Checksum_E { NONE, CRC32, MD5 };
        Checksum_E f_type;

        ChecksumKind( const std::string& p ) {
            std::string s;
            std::transform( p . begin(), p . end(), std::inserter( s, s . begin() ), ::tolower );
            if ( s == "crc32" ) { f_type = Checksum_E::CRC32; }
            else if ( s == "md5" ) { f_type = Checksum_E::MD5; }
            else { f_type = Checksum_E::NONE; }
        }

    public:
        static ChecksumKind_ptr make( const std::string& p ) {
            return ChecksumKind_ptr( new ChecksumKind( p ) );
        }

        std::string to_string( void ) const {
            switch( f_type ) {
                case Checksum_E::NONE  : return std::string( "NONE" ); break;
                case Checksum_E::CRC32 : return std::string( "CRC32" ); break;
                case Checksum_E::MD5   : return std::string( "MD5" ); break;
            }
            return std::string( "NONE" );
        }

        bool is_none( void ) const {
            return ( f_type == Checksum_E::NONE );
        }

        bool is_crc32( void ) const {
            return ( f_type == Checksum_E::CRC32 );
        }

        bool is_md5( void ) const {
            return ( f_type == Checksum_E::MD5 );
        }

        KCreateMode mode( KCreateMode in ) const {
            KCreateMode res( in );
            if ( is_md5() ) res |= kcmMD5;
            return res;
        }

        KCreateMode mode_mask( KCreateMode in ) const {
            KCreateMode res( in );
            if ( is_md5() ) res |= kcmMD5;
            return res;
        }

        friend auto operator<<( std::ostream& os, ChecksumKind_ptr o ) -> std::ostream& {
            os << o -> to_string();
            return os;
        }
}; // end of class ChecksumKind

extern "C" {

rc_t KProcMgrInit( void );
rc_t KProcMgrWhack( void );
rc_t KProcMgrMakeSingleton( KProcMgr ** mgr );
rc_t KProcMgrAddRef( const KProcMgr *self );
rc_t KProcMgrRelease( const KProcMgr *self );
rc_t KProcMgrAddCleanupTask( KProcMgr *self, KTaskTicket *ticket, KTask *task );
rc_t ProcMgrRemoveCleanupTask( KProcMgr *self, KTaskTicket const *ticket );
bool KProcMgrOnMainThread( void );
rc_t KProcMgrGetPID( const KProcMgr * self, uint32_t * pid );
rc_t KProcMgrGetHostName( const KProcMgr * self, char * buffer, std::size_t buffer_size );

};  // end of extern "C"

class ProcMgr;
typedef std::shared_ptr< ProcMgr > ProcMgrPtr;
class ProcMgr : public VDBObj {
    private :
        KProcMgr * mgr;

        ProcMgr( void ) : mgr( nullptr ) {
            if ( set_rc( KProcMgrInit() ) ) {
                set_rc( KProcMgrMakeSingleton( &mgr ) );
            }
        }

    public :
        static ProcMgrPtr make( void ) { return ProcMgrPtr( new ProcMgr ); }

        ~ProcMgr() { if ( nullptr != mgr ) { KProcMgrRelease( mgr ); } }

        bool on_main_thread( void ) const { return KProcMgrOnMainThread(); }

        uint32_t get_pid( void ) const {
            uint32_t res = 0;
            rc_t rc = KProcMgrGetPID( mgr, &res );
            return 0 == rc ? res : 0;
        }

        std::string get_host_name( void ) const {
            char buffer[ 512 ];
            rc_t rc = KProcMgrGetHostName( mgr, buffer, sizeof buffer );
            if ( 0 != rc ) { buffer[ 0 ] = 0; }
            return std::string( buffer );
        }

        std::string unique_id( void ) const {
            std::stringstream ss;
            ss << get_host_name() << "_" << get_pid();
            return ss . str();
        }
};

struct RowRange;
typedef std::list< RowRange > list_of_range;
struct RowRange {
    int64_t first;
    uint64_t count;

    RowRange() : first( 0 ), count( 0 ) {}
    RowRange( int64_t a_first, uint64_t a_count ) : first( a_first ), count( a_count ) {}
    RowRange( const RowRange& other ) : first( other.first ), count( other.count ) {}

    RowRange& operator=( const RowRange& other ) {
        if ( this != &other ) {
            first = other . first;
            count = other . count;
        }
        return *this;
    }

    bool empty( void ) const { return ( 0 == count ); }

    list_of_range split( uint32_t parts, uint32_t min_count ) const {
        list_of_range res;
        if ( count > 0 ) {
            if ( count < min_count ) {
                res. push_back( RowRange{ first, count } );
            } else {
                uint64_t part_size = ( count / parts );
                int64_t start = first;
                for ( uint32_t loop = 0; loop < parts - 1; ++loop ) {
                    res. push_back( RowRange{ start, part_size } );
                    start += part_size;
                }
                res. push_back( RowRange{ start, count - start + 1 } );
            }
        }
        return res;
    }

    friend auto operator<<( std::ostream& os, RowRange const& r ) -> std::ostream& {
        int64_t end = r . first + r . count - 1;
        return os << r . first << ".." << end << " (" << r . count << ")";
    }
};

struct RowRangeIter{
    const RowRange range;
    uint64_t offset;

    RowRangeIter( const RowRange& a_range ) : range( a_range ), offset( 0 ) { }

    bool empty( void ) const { return range . empty(); }
    bool done( void ) const { return offset >= range . count; }
    void next( void ) { offset++; }

    bool next( int64_t& row ) {
        if ( empty() || done() ) { return false; }
        row = offset++ + range . first;
        return true;
    }

    int64_t get_row( void ) const { return offset + range . first; }

    bool has_data( int64_t& row, bool inc = false) {
        bool res = !empty() && offset < range . count;
        if ( res ) {
            row = offset + range . first;
            if ( inc ) offset++;
        }
        return res;
    }

    friend auto operator<<( std::ostream& os, RowRangeIter const& it ) -> std::ostream& {
        return os << it . range << " | offset = " << it . offset;
    }
};

extern "C" {

typedef struct String String;
struct String {
    const char *addr;
    std::size_t size;
    uint32_t len;
};

#define StringInit( s, val, sz, length ) \
    ( void ) \
        ( ( s ) -> addr = ( val ), \
          ( s ) -> size = ( sz ), \
          ( s ) -> len = ( length ) )

#define StringInitCString( s, cstr ) \
    ( void ) \
        ( ( s ) -> len = string_measure \
          ( ( s ) -> addr = ( cstr ), & ( s ) -> size ) )

#define StringEqual( a, b ) \
    ( ( a ) -> size == ( b ) -> size && \
    memcmp ( ( a ) -> addr, ( b ) -> addr, ( a ) -> size ) == 0 )

#define StringCopyWChar_t( cpy, text, bytes ) \
    ( ( sizeof ( wchar_t ) == sizeof ( uint16_t ) ) ? \
      StringCopyUTF16 ( cpy, ( const uint16_t* ) ( text ), bytes ) : \
      StringCopyUTF32 ( cpy, ( const uint32_t* ) ( text ), bytes ) )

#define StringHash( s ) string_hash ( ( s ) -> addr, ( s ) -> size )

#define CONST_STRING( s, val ) StringInit ( s, val, sizeof val - 1, sizeof val - 1 )

#define StringSize( s ) ( s ) -> size

#define StringLength( s ) ( s ) -> len

rc_t StringCopy( const String **cpy, const String *str );
rc_t StringConcat( const String **cat, const String *a, const String *b );
String* StringSubstr( const String *str, String *sub, uint32_t idx, uint32_t len );
String* StringTrim( const String * str, String * trimmed );
rc_t StringHead( const String *str, uint32_t *ch );
rc_t StringPopHead( String *str, uint32_t *ch );
int StringCompare( const String *a, const String *b );
bool StringCaseEqual( const String *a, const String *b );
int StringCaseCompare( const String *a, const String *b );
int64_t StringOrder( const String *a, const String *b );
int64_t StringOrderNoNullCheck( const String *a, const String *b );
uint32_t StringMatch( String *match, const String *a, const String *b );
uint32_t StringMatchExtend( String *match, const String *a, const String *b );
rc_t StringCopyUTF16( const String **cpy, const uint16_t *text, std::size_t bytes );
rc_t StringCopyUTF32( const String **cpy, const uint32_t *text, std::size_t bytes );
void StringWhack ( const String* self );
int64_t StringToI64( const String * self, rc_t * optional_rc );
uint64_t StringToU64( const String * self, rc_t * optional_rc );

std::size_t string_size( const char *str );
uint32_t string_len( const char *str, std::size_t size );
uint32_t string_measure( const char *str, std::size_t *size );
std::size_t string_copy( char *dst, std::size_t dst_size, const char *src, std::size_t src_size );
std::size_t string_copy_measure( char *dst, std::size_t dst_size, const char *src );
char* string_dup( const char *str, std::size_t size );
char* string_dup_measure( const char *str, std::size_t *size );
std::size_t tolower_copy( char *dst,  std::size_t dst_size, const char *src, std::size_t src_size );
std::size_t toupper_copy( char *dst, std::size_t dst_size, const char *src, std::size_t src_size );
int string_cmp( const char *a, std::size_t asize, const char *b, std::size_t bsize, uint32_t max_chars );
int strcase_cmp( const char *a, std::size_t asize, const char *b, std::size_t bsize, uint32_t max_chars );
uint32_t string_match( const char *a, std::size_t asize,
        const char *b, std::size_t bsize, uint32_t max_chars, std::size_t *msize );
uint32_t strcase_match( const char *a, std::size_t asize,
        const char *b, std::size_t bsize, uint32_t max_chars, std::size_t *msize );
char* string_chr( const char *str, std::size_t size, uint32_t ch );
char* string_rchr( const char *str, std::size_t size, uint32_t ch );
uint32_t string_hash( const char *str, std::size_t size );
char* string_idx( const char *str, std::size_t size, uint32_t idx );
int64_t string_to_I64( const char * str, std::size_t size, rc_t * optional_rc );
uint64_t string_to_U64( const char * str, std::size_t size, rc_t * optional_rc );
int utf8_utf32( uint32_t *ch, const char *begin, const char *end );
int utf32_utf8( char *begin, char *end, uint32_t ch );
std::size_t utf16_string_size( const uint16_t *str );
uint32_t utf16_string_len( const uint16_t *str, std::size_t size );
uint32_t utf16_string_measure( const uint16_t *str, std::size_t *size );
std::size_t utf32_string_size( const uint32_t *str );
uint32_t utf32_string_len( const uint32_t *str, std::size_t size );
uint32_t utf32_string_measure( const uint32_t *str, std::size_t *size );
std::size_t wchar_string_size( const wchar_t *str );
uint32_t wchar_string_len( const wchar_t *str, std::size_t size );
uint32_t wchar_string_measure( const wchar_t *str, std::size_t *size );
uint32_t utf16_cvt_string_len( const uint16_t *src, std::size_t src_size, std::size_t *dst_size );
uint32_t utf16_cvt_string_measure( const uint16_t *src, std::size_t *src_size, std::size_t *dst_size );
std::size_t utf16_cvt_string_copy( char *dst, std::size_t dst_size, const uint16_t *src, std::size_t src_size );
uint32_t utf32_cvt_string_len( const uint32_t *src, std::size_t src_size, std::size_t *dst_size );
uint32_t utf32_cvt_string_measure( const uint32_t *src, std::size_t *src_size, std::size_t *dst_size );
std::size_t utf32_cvt_string_copy( char *dst, std::size_t dst_size, const uint32_t *src, std::size_t src_size );
uint32_t wchar_cvt_string_len( const wchar_t *src, std::size_t src_size, std::size_t *dst_size );
uint32_t wchar_cvt_string_measure( const wchar_t *src, std::size_t *src_size, std::size_t *dst_size );
std::size_t wchar_cvt_string_copy( char *dst, std::size_t dst_size, const wchar_t *src, std::size_t src_size );
std::size_t string_cvt_wchar_copy( wchar_t *dst, std::size_t dst_size, const char *src, std::size_t src_size );

int iso8859_utf32 ( const uint32_t map [ 128 ], uint32_t *ch, const char *begin, const char *end );
std::size_t iso8859_string_size ( const uint32_t map [ 128 ], const char *str );
uint32_t iso8859_string_len ( const uint32_t map [ 128 ], const char *str, std::size_t size );
uint32_t iso8859_string_measure ( const uint32_t map [ 128 ], const char *str, std::size_t *size );
uint32_t iso8859_cvt_string_len ( const uint32_t map [ 128 ], const char *src, std::size_t src_size, std::size_t *dst_size );
uint32_t iso8859_cvt_string_measure ( const uint32_t map [ 128 ], const char *src, std::size_t *src_size, std::size_t *dst_size );
std::size_t iso8859_cvt_string_copy ( const uint32_t map [ 128 ], char *dst, std::size_t dst_size, const char *src, std::size_t src_size );

rc_t KConfigMake( KConfig **cfg, KDirectory const * optional_search_base );
rc_t KConfigAddRef( const KConfig *self );
rc_t KConfigRelease( const KConfig *self );
rc_t KConfigLoadFile( KConfig * self, const char * path, KFile const * file );
rc_t KConfigCommit( KConfig *self );
rc_t KConfigRead( const KConfig *self, const char *path, std::size_t offset, char *buffer,
		std::size_t bsize, std::size_t *num_read, std::size_t *remaining );
rc_t KConfigReadBool( const KConfig* self, const char* path, bool* result );
rc_t KConfigWriteBool( KConfig *self, const char * path, bool value );
rc_t KConfigReadI64( const KConfig* self, const char* path, int64_t* result );
rc_t KConfigReadU64( const KConfig* self, const char* path, uint64_t* result );
rc_t KConfigReadF64( const KConfig* self, const char* path, double* result );
rc_t KConfigReadString( const KConfig* self, const char* path, String** result );
rc_t KConfigWriteString( KConfig *self, const char * path, const char * value );
rc_t KConfigWriteSString( KConfig *self, const char * path, String const * value );
rc_t KConfigPrint( const KConfig * self, int indent );
rc_t KConfigToFile( const KConfig * self, KFile * file );
void KConfigDisableUserSettings ( void );
void KConfigSetNgcFile(const char * path);

/* ========================================================================================= */

rc_t KConfigNodeAddRef( const KConfigNode *self );
rc_t KConfigNodeRelease( const KConfigNode *self );

rc_t KConfigNodeGetMgr( const KConfigNode * self, KConfig ** mgr );
rc_t KConfigOpenNodeRead ( const KConfig *self, const KConfigNode **node, const char *path, ... );
rc_t KConfigNodeOpenNodeRead ( const KConfigNode *self, const KConfigNode **node, const char *path, ... );
rc_t KConfigVOpenNodeRead( const KConfig *self, const KConfigNode **node, const char *path, va_list args );
rc_t KConfigNodeVOpenNodeRead( const KConfigNode *self, const KConfigNode **node, const char *path, va_list args );
rc_t KConfigOpenNodeUpdate( KConfig *self, KConfigNode **node, const char *path, ... );
rc_t KConfigNodeOpenNodeUpdate( KConfigNode *self, KConfigNode **node, const char *path, ... );
rc_t KConfigVOpenNodeUpdate( KConfig *self, KConfigNode **node, const char *path, va_list args );
rc_t KConfigNodeVOpenNodeUpdate( KConfigNode *self, KConfigNode **node, const char *path, va_list args );
rc_t KConfigNodeRead( const KConfigNode *self, std::size_t offset, char *buffer, std::size_t bsize,
        std::size_t *num_read, std::size_t *remaining );
rc_t KConfigNodeReadBool( const KConfigNode *self, bool* result );
rc_t KConfigNodeReadI64( const KConfigNode *self, int64_t* result );
rc_t KConfigNodeReadU64( const KConfigNode *self, uint64_t* result );
rc_t KConfigNodeReadF64( const KConfigNode *self, double* result );
rc_t KConfigNodeReadString( const KConfigNode *self, struct String** result );
#define KConfigNodeListChild KConfigNodeListChildren
rc_t KConfigNodeListChildren( const KConfigNode *self, KNamelist **names );
rc_t KConfigNodeWrite( KConfigNode *self, const char *buffer, std::size_t size );
rc_t KConfigNodeWriteBool( KConfigNode *self, bool state );
rc_t KConfigNodeAppend( KConfigNode *self, const char *buffer, std::size_t size );
rc_t KConfigNodeReadAttr( const KConfigNode *self, const char *name, char *buffer, std::size_t bsize, std::size_t *size );
rc_t KConfigNodeWriteAttr( KConfigNode *self, const char *name, const char *value );
rc_t KConfigNodeDropAll( KConfigNode *self );
rc_t KConfigNodeDropAttr( KConfigNode *self, const char *attr );
rc_t KConfigNodeDropChild( KConfigNode *self, const char *path, ... );
rc_t KConfigNodeVDropChild( KConfigNode *self, const char *path, va_list args );
rc_t KConfigNodeRenameAttr( KConfigNode *self, const char *from, const char *to );
rc_t KConfigNodeRenameChild( KConfigNode *self, const char *from, const char *to );

/* ========================================================================================= */

rc_t VSchemaAddRef( const VSchema *self );
rc_t VSchemaRelease( const VSchema *self );

rc_t VDBManagerMakeSchema(  VDBManager const *self, VSchema **schema );
rc_t VSchemaAddIncludePath( VSchema *self, const char *path, ... );
rc_t VSchemaVAddIncludePath( VSchema *self, const char *path, va_list args );
rc_t VSchemaAddIncludePaths( VSchema *self, std::size_t length, const char *paths );

rc_t VSchemaParseText( VSchema *self, const char *name, const char *text, std::size_t bytes );
rc_t VSchemaParseFile( VSchema *self, const char *name, ... );
rc_t VSchemaVParseFile( VSchema *self, const char *name, va_list args );

enum VSchemaDumpMode { sdmPrint, sdmCompact };
rc_t VSchemaDump( const VSchema *self, uint32_t mode, const char *decl,
        rc_t ( * flush ) ( void *dst, const void *buffer, std::size_t bsize ), void *dst );

rc_t VSchemaIncludeFiles( const VSchema *self, KNamelist const **list );

typedef struct VTypedecl VTypedecl;
struct VTypedecl {
    uint32_t type_id;
    uint32_t dim;
};
rc_t VSchemaResolveTypedecl( const VSchema *self, VTypedecl *resolved, const char *typedecl, ... );
rc_t VSchemaVResolveTypedecl( const VSchema *self, VTypedecl *resolved, const char *typedecl, va_list args );
rc_t VTypedeclToText( const VTypedecl *self, const VSchema *schema, char *buffer, std::size_t bsize );
bool VTypedeclToSupertype( const VTypedecl *self, const VSchema *schema, VTypedecl *cast );
bool VTypedeclToType( const VTypedecl *self, const VSchema *schema,  uint32_t ancestor,
        VTypedecl *cast, uint32_t *distance );
bool VTypedeclToTypedecl( const VTypedecl *self, const VSchema *schema, const VTypedecl *ancestor,
        VTypedecl *cast, uint32_t *distance );
bool VTypedeclCommonAncestor( const VTypedecl *self, const VSchema *schema,
        const VTypedecl *peer, VTypedecl *ancestor, uint32_t *distance );

enum { vtdBool = 1, vtdUint, vtdInt, vtdFloat, vtdAscii, vtdUnicode };
typedef struct VTypedesc VTypedesc;
struct VTypedesc {
	uint32_t intrinsic_bits;
	uint32_t intrinsic_dim;
	uint32_t domain;
};
uint32_t VTypedescSizeof( const VTypedesc *self );
rc_t VSchemaDescribeTypedecl( const VSchema *self, VTypedesc *desc, const VTypedecl *td );

};

struct SchemaPrinter {
        std::string s;

        static rc_t callback( void * obj, const void *buffer, std::size_t bsize ) {
            SchemaPrinter * printer = static_cast< SchemaPrinter *>( obj );
            std::string tmp( ( const char * ) buffer, bsize );
            printer -> s += tmp;
            return 0;
        }
};

class VSch;
typedef std::shared_ptr<VSch> VSchPtr;
class VSch : public VDBObj {
    private:
        VSchema * f_schema;

        VSch( const VDBManager * mgr ) { set_rc( VDBManagerMakeSchema( mgr, &f_schema ) ); }

    public:
        ~VSch() { VSchemaRelease( f_schema ); }

        static VSchPtr make( const VDBManager * mgr ) { return VSchPtr( new VSch( mgr ) ); }

        VSchema * get_schema( void ) const { return f_schema; }

        bool AddInclucePath( const std::string& path ) {
            return set_rc( VSchemaAddIncludePath( f_schema, "%s", path.c_str() ) );
        }

        bool ParseText( const std::string& schema_txt ) {
            return set_rc( VSchemaParseText( f_schema, nullptr,
                                      schema_txt . c_str(), schema_txt .length() ) );
        }

        bool ParseFile( const std::string& filename ) {
            return set_rc( VSchemaParseFile( f_schema, "%s", filename . c_str() ) );
        }

        rc_t print_callback( const void *buffer, std::size_t bsize ) {
            return 0;
        }

        std::string print( void ) {
            SchemaPrinter printer;
            rc_t rc = VSchemaDump( f_schema, sdmPrint, nullptr, SchemaPrinter::callback, &printer );
            if ( 0 == rc ) {
                return printer . s;
            }
            return std::string( "err" );
        }
};

extern "C" {

rc_t VTableCreateCursorRead( VTable const *self, const VCursor **curs );
rc_t VTableCreateCursorWrite( VTable *self, VCursor **curs, KCreateMode mode );
rc_t VTableCreateCachedCursorRead( VTable const *self, const VCursor **curs, std::size_t capacity );

rc_t VCursorAddRef( const VCursor *self );
rc_t VCursorRelease( const VCursor *self );

rc_t VViewCreateCursor( VView const *self, const VCursor **curs );

rc_t VCursorAddColumn( const VCursor *self, uint32_t *idx, const char *name, ... );
rc_t VCursorVAddColumn( const VCursor *self, uint32_t *idx, const char *name, va_list args );

rc_t VCursorGetColumnIdx( const VCursor *self, uint32_t *idx, const char *name, ... );
rc_t VCursorVGetColumnIdx( const VCursor *self, uint32_t *idx, const char *name, va_list args );

rc_t VCursorDatatype( const VCursor *self, uint32_t idx, VTypedecl *type, VTypedesc *desc );

rc_t VCursorIdRange( const VCursor *self, uint32_t idx, int64_t *first, uint64_t *count );

rc_t VCursorOpen( const VCursor *self );
rc_t VCursorRowId( const VCursor *self, int64_t *row_id );
rc_t VCursorSetRowId( const VCursor *self, int64_t row_id );
rc_t VCursorFindNextRowId( const VCursor *self, uint32_t idx, int64_t * next );
rc_t VCursorFindNextRowIdDirect( const VCursor *self, uint32_t idx, int64_t start_id, int64_t * next );
rc_t VCursorOpenRow( const VCursor *self );
rc_t VCursorCloseRow( const VCursor *self );
rc_t VCursorCommitRow( VCursor *self );
rc_t VCursorCommit( VCursor *self );

rc_t VCursorGetBlob( const VCursor *self,  VBlob const **blob, uint32_t col_idx );
rc_t VCursorGetBlobDirect( const VCursor *self, VBlob const **blob, int64_t row_id, uint32_t col_idx );

rc_t VCursorRead( const VCursor *self, uint32_t col_idx, uint32_t elem_bits,
        void *buffer, uint32_t blen, uint32_t *row_len );
rc_t VCursorReadDirect( const VCursor *self, int64_t row_id, uint32_t col_idx, uint32_t elem_bits,
        void *buffer, uint32_t blen, uint32_t *row_len );

rc_t VCursorReadBits( const VCursor *self, uint32_t col_idx, uint32_t elem_bits, uint32_t start,
        void *buffer, uint32_t boff, uint32_t blen, uint32_t *num_read, uint32_t *remaining );
rc_t VCursorReadBitsDirect( const VCursor *self, int64_t row_id, uint32_t col_idx, uint32_t elem_bits,
        uint32_t start, void *buffer, uint32_t boff, uint32_t blen, uint32_t *num_read, uint32_t *remaining );

rc_t VCursorCellData( const VCursor *self, uint32_t col_idx, uint32_t *elem_bits, const void **base,
        uint32_t *boff, uint32_t *row_len );
rc_t VCursorCellDataDirect( const VCursor *self, int64_t row_id, uint32_t col_idx, uint32_t *elem_bits,
        const void **base, uint32_t *boff, uint32_t *row_len );

rc_t VCursorDefault( VCursor *self, uint32_t col_idx, bitsz_t elem_bits,
                     const void *buffer, bitsz_t boff, uint64_t row_len );

rc_t VCursorWrite( VCursor *self, uint32_t col_idx, bitsz_t elem_bits,
                   const void *buffer, bitsz_t boff, uint64_t count );

rc_t VCursorDataPrefetch( const VCursor * self, const int64_t * row_ids, uint32_t col_idx,
        uint32_t num_rows, int64_t min_valid_row_id, int64_t max_valid_row_id, bool continue_on_error );

rc_t VBlobAddRef( const VBlob *self );
rc_t VBlobRelease( const VBlob *self );
rc_t VBlobIdRange( const VBlob *self, int64_t *first, uint64_t *count );
rc_t VBlobSize( const VBlob * self, std::size_t * bytes );

rc_t VBlobRead( const VBlob *self, int64_t row_id, uint32_t elem_bits, void *buffer,
        uint32_t blen, uint32_t *row_len );

rc_t VBlobReadBits( const VBlob *self, int64_t row_id, uint32_t elem_bits, uint32_t start,
        void *buffer, uint32_t boff, uint32_t blen, uint32_t *num_read, uint32_t *remaining );

rc_t VBlobCellData( const VBlob *self, int64_t row_id, uint32_t *elem_bits,
        const void **base, uint32_t *boff, uint32_t *row_len );

rc_t VViewAddRef( const VView *self );
rc_t VViewRelease( const VView *self );

rc_t VViewCreateCursor( VView const *self, const VCursor **curs );
uint32_t VViewParameterCount( VView const * self );
rc_t VViewGetParameter( VView const * self, uint32_t idx, const String ** name, bool * is_table );
rc_t VViewBindParameterTable( const VView * self, const String * param_name, const VTable * table );
rc_t VViewBindParameterView( const VView * self, const String * param_name, const VView * view );
rc_t VViewListCol( const VView *self, KNamelist **names );
rc_t VViewOpenSchema( const VView *self, VSchema const **schema );

}; // end of extern "C"

#define COL_ARRAY_COUNT 32
#define INVALID_COL 0xFFFFFFFF

class VCur;
typedef std::shared_ptr<VCur> VCurPtr;
class VCur : public VDBObj {
    private :
#ifdef VDB_WRITE
        VCursor * f_cur;
#else
        const VCursor * f_cur;
#endif
        uint32_t columns[ COL_ARRAY_COUNT ];
        uint32_t elem_bits, boff, row_len;
        const void *base;
        uint32_t f_next_col_id;

#ifdef VDB_WRITE
        VCur( VTable * tbl ) {
            set_rc( VTableCreateCursorWrite( tbl, &f_cur, kcmCreate ) );
            for ( int i = 0; i < COL_ARRAY_COUNT; ++i ) { columns[ i ] = INVALID_COL; }
            f_next_col_id = 0;
        }
#endif

        VCur( const VTable * tbl, std::size_t capacity = 0 ) {
            if ( 0 == capacity ) {
                set_rc( VTableCreateCursorRead( tbl, ( const VCursor ** )&f_cur ) );
            } else {
                set_rc( VTableCreateCachedCursorRead( tbl, ( const VCursor ** )&f_cur, capacity ) );
            }
            for ( int i = 0; i < COL_ARRAY_COUNT; ++i ) { columns[ i ] = INVALID_COL; }
            f_next_col_id = 0;
        }

        bool valid_id( uint32_t id ) { return ( id < COL_ARRAY_COUNT && INVALID_COL != columns[ id ] ); }

    public :
        ~VCur() { VCursorRelease( f_cur ); }

#ifdef VDB_WRITE
        static VCurPtr make( VTable * tbl ) { return VCurPtr( new VCur( tbl ) ); }
#endif

        uint32_t next_col_id( void ) const { return f_next_col_id; }
        void inc_next_col_id( void ) { f_next_col_id++; }

        static VCurPtr make( const VTable * tbl, std::size_t capacity = 0 ) {
            return VCurPtr( new VCur( tbl, capacity ) );
        }

        bool open( void ) { return set_rc( VCursorOpen( f_cur ) ); }

        int64_t get_rowid( void ) {
            int64_t res = 0;
            set_rc( VCursorRowId( f_cur, &res ) );
            return res;
        }

        bool set_rowid( int64_t row ) { return set_rc( VCursorSetRowId( f_cur, row ) ); }

        int64_t find_next_rowid( uint32_t col_idx ) {
            int64_t res = 0;
            set_rc( VCursorFindNextRowId( f_cur, col_idx, &res ) );
            return res;
        }

        int64_t find_next_rowid_from( uint32_t col_idx, int64_t start ) {
            int64_t res = 0;
            set_rc( VCursorFindNextRowIdDirect( f_cur, col_idx, start, &res ) );
            return res;
        }

        bool open_row( void ) { return set_rc( VCursorOpenRow( f_cur ) ); }
        bool close_row( void ) { return set_rc( VCursorCloseRow( f_cur ) ); }

        bool add_col( uint32_t id, const std::string& name ) {
            if ( id >= COL_ARRAY_COUNT ) return false;
            if ( name . empty() ) return false;
            if ( INVALID_COL != columns[ id ] ) return false;
            return set_rc( VCursorAddColumn( f_cur, &( columns[ id ] ), "%s", name.c_str() ) );
        }

        RowRange row_range( uint32_t id ) {
            RowRange res{ 0, 0 };
            if ( valid_id( id  ) ) {
                set_rc( VCursorIdRange( f_cur, columns[ id ], &( res . first ), &( res . count ) ) );
            }
            return res;
        }

/* ----------------------------------------------------------------------------------------------------- */

        bool CellData( uint32_t id, int64_t row, uint32_t *elem_bits,
                       const void **base, uint32_t *boff, uint32_t *row_len ) {
            if ( !valid_id( id ) ) return false;
            return set_rc( VCursorCellDataDirect( f_cur, row, columns[ id ], elem_bits, base, boff, row_len ) );
        }

        template <typename T> bool read( uint32_t id, int64_t row, T* v ) {
            bool res = CellData( id, row, &elem_bits, &base, &boff, &row_len );
            if ( res ) { *v = *( ( T* )base ); }
            return res;
        }

        template <typename T, typename A> bool read_arr( uint32_t id, int64_t row, A* arr ) {
            bool res = CellData( id, row, &elem_bits, &base, &boff, &row_len );
            if ( res ) {
                arr -> data = ( T* )base;
                arr -> len = row_len;
            }
            return res;
        }

/* ----------------------------------------------------------------------------------------------------- */

#ifdef VDB_WRITE
        bool SetDefault( uint32_t id, bitsz_t elem_bits, const void * data,
                         bitsz_t boff, uint64_t row_len ) {
            if ( !valid_id( id ) ) return false;
            return set_rc( VCursorDefault( f_cur, columns[ id ], elem_bits, data, boff, row_len ) );
        }

        bool WriteCell( uint32_t id, bitsz_t elem_bits, const void * data,
                        bitsz_t boff, uint64_t row_len ) {
            if ( !valid_id( id ) ) return false;
            rc_t rc = VCursorWrite( f_cur, columns[ id ], elem_bits, data, boff, row_len );
            return set_rc( rc );
        }

        template <typename T> bool write( uint32_t id, const T* v ) {
            uint32_t bitcount = sizeof( T ) * 8;
            return WriteCell( id, bitcount, ( void * )v, 0, 1 );
        }

        template <typename T, typename A> bool write_arr( uint32_t id, const A* arr ) {
            uint32_t bitcount = sizeof( T ) * 8;
            return WriteCell( id, bitcount, ( void * )( arr -> data ), 0, arr -> len );
        }

        bool write_u8( uint32_t id, uint8_t v ) { return write< uint8_t >( id, &v ); }
        bool write_i8( uint32_t id, int8_t v )  { return write< int8_t >( id, &v ); }
        bool write_u16( uint32_t id, uint16_t v ) { return write< uint16_t >( id, &v ); }
        bool write_i16( uint32_t id, int16_t v )  { return write< int16_t >( id, &v ); }
        bool write_u32( uint32_t id, uint32_t v ) { return write< uint32_t >( id, &v ); }
        bool write_i32( uint32_t id, int32_t v )  { return write< int32_t >( id, &v ); }
        bool write_u64( uint32_t id, uint64_t v ) { return write< uint64_t >( id, &v ); }
        bool write_i64( uint32_t id, int64_t v )  { return write< int64_t >( id, &v ); }
        bool write_f32( uint32_t id, float v ) { return write< float >( id, &v ); }
        bool write_f64( uint32_t id, double v )  { return write< double >( id, &v ); }

        bool write_u8arr( uint32_t id, const uint8_t * v, size_t len ) {
            uint8_arr a{ v, len };
            return write_arr< uint8_t, uint8_arr >( id, &a );
        }
        bool write_u8arr( uint32_t id, const uint8_arr_ptr v ) { return write_arr< uint8_t, uint8_arr >( id, v ); }

        bool write_i8arr( uint32_t id, const int8_t * v, size_t len ) {
            int8_arr a{ v, len }; return write_arr< int8_t, int8_arr >( id, &a );
        }
        bool write_i8arr( uint32_t id, const int8_arr_ptr v ) { return write_arr< int8_t, int8_arr >( id, v ); }

        bool write_u16arr( uint32_t id, uint16_t * v, size_t len ) {
            uint16_arr a{ v, len }; return write_arr< uint16_t, uint16_arr >( id, &a );
        }
        bool write_u16arr( uint32_t id, uint16_arr_ptr v ) { return write_arr< uint16_t, uint16_arr >( id, v ); }

        bool write_i16arr( uint32_t id, const int16_t * v, size_t len ) {
            int16_arr a{ v, len }; return write_arr< int16_t, int16_arr >( id, &a );
        }
        bool write_i16arr( uint32_t id, const int16_arr_ptr v ) { return write_arr< int16_t, int16_arr >( id, v ); }

        bool write_u32arr( uint32_t id, const uint32_t * v, size_t len ) {
            uint32_arr a{ v, len };  return write_arr< uint32_t, uint32_arr >( id, &a );
        }
        bool write_u32arr( uint32_t id, const uint32_arr_ptr v ) { return write_arr< uint32_t, uint32_arr >( id, v ); }

        bool write_i32arr( uint32_t id, const int32_t * v, size_t len ) {
            int32_arr a{ v, len }; return write_arr< int32_t, int32_arr >( id, &a );
        }
        bool write_i32arr( uint32_t id, const int32_arr_ptr v ) { return write_arr< int32_t, int32_arr >( id, v ); }

        bool write_u64arr( uint32_t id, const uint64_t * v, size_t len ) {
            uint64_arr a{ v, len }; return write_arr< uint64_t, uint64_arr >( id, &a );
        }
        bool write_u64arr( uint32_t id, const uint64_arr_ptr v ) { return write_arr< uint64_t, uint64_arr >( id, v ); }

        bool write_i64arr( uint32_t id, const int64_t * v, size_t len ) {
            int64_arr a{ v, len }; return write_arr< int64_t, int64_arr >( id, &a ); }
        bool write_i64arr( uint32_t id, const int64_arr_ptr v ) { return write_arr< int32_t, int64_arr >( id, v ); }

        bool write_f32arr( uint32_t id, const float * v, size_t len ) {
            f32_arr a{ v, len }; return write_arr< float, f32_arr >( id, &a );
        }
        bool write_f32arr( uint32_t id, const f32_arr_ptr v ) { return write_arr< float, f32_arr >( id, v ); }

        bool write_f64arr( uint32_t id, const double * v, size_t len ) {
            f64_arr a{ v, len }; return write_arr< double, f64_arr >( id, &a );
        }
        bool write_f64arr( uint32_t id, const f64_arr_ptr v ) { return write_arr< double, f64_arr >( id, v ); }

        bool WriteString( uint32_t id, const std::string& v ) {
            uint32_t cur_col_id = columns[ id ];
            uint32_t row_len = v . length();
            const void * ptr = ( const void * )( v . c_str() );
            rc_t rc = VCursorWrite( f_cur, cur_col_id, 8, ptr, 0, row_len );
            return ( 0 == rc );
        }

        template <typename T> bool write_dflt( uint32_t id, const T* v ) {
            uint32_t bitcount = sizeof( T ) * 8;
            return SetDefault( id, bitcount, ( void * )v, 0, 1 );
        }

        bool dflt_u8( uint32_t id, uint8_t v ) { return write_dflt< uint8_t >( id, &v ); }
        bool dflt_i8( uint32_t id, int8_t v )  { return write_dflt< int8_t >( id, &v ); }
        bool dflt_u16( uint32_t id, uint16_t v ) { return write_dflt< uint16_t >( id, &v ); }
        bool dlft_i16( uint32_t id, int16_t v )  { return write_dflt< int16_t >( id, &v ); }
        bool dflt_u32( uint32_t id, uint32_t v ) { return write_dflt< uint32_t >( id, &v ); }
        bool dlft_i32( uint32_t id, int32_t v )  { return write_dflt< int32_t >( id, &v ); }
        bool dflt_u64( uint32_t id, uint64_t v ) { return write_dflt< uint64_t >( id, &v ); }
        bool dflt_i64( uint32_t id, int64_t v )  { return write_dflt< int64_t >( id, &v ); }
        bool dlft_f32( uint32_t id, float v ) { return write_dflt< float >( id, &v ); }
        bool dlft_f64( uint32_t id, double v )  { return write_dflt< double >( id, &v ); }

        bool commit_row( void ) {
            return set_rc( VCursorCommitRow( f_cur ) );
        }

        bool commit( void ) {
            return set_rc( VCursorCommit( f_cur ) );
        }
#endif

}; // end of VCursor

extern "C" {

rc_t KMetadataAddRef ( const KMetadata *self );
rc_t KMetadataRelease ( const KMetadata *self );

typedef struct KDatabase KDatabase;
typedef struct KTable KTable;
typedef struct KColumn KColumn;

rc_t KDatabaseOpenMetadataRead( KDatabase const *self, const KMetadata **meta );
rc_t KTableOpenMetadataRead( KTable const *self, const KMetadata **meta );
rc_t KColumnOpenMetadataRead( KColumn const *self, const KMetadata **meta );

rc_t KDatabaseOpenMetadataUpdate( KDatabase *self, KMetadata **meta );
rc_t KTableOpenMetadataUpdate( KTable *self, KMetadata **meta );
rc_t KColumnOpenMetadataUpdate( KColumn *self, KMetadata **meta );

rc_t VTableOpenMetadataRead( const VTable *self, KMetadata const **meta );
rc_t VTableOpenMetadataUpdate( VTable *self, struct KMetadata **meta );

rc_t KMetadataVersion( const KMetadata *self, uint32_t *version );
rc_t KMetadataByteOrder( const KMetadata *self, bool *reversed );
rc_t KMetadataRevision( const KMetadata *self, uint32_t *revision );
rc_t KMetadataMaxRevision( const KMetadata *self, uint32_t *revision );

rc_t KMetadataCommit( KMetadata *self );
rc_t KMetadataFreeze( KMetadata *self );

rc_t KMetadataOpenRevision( const KMetadata *self, const KMetadata **meta, uint32_t revision );

rc_t KMetadataGetSequence( const KMetadata *self, const char *seq, int64_t *val );
rc_t KMetadataSetSequence( KMetadata *self, const char *seq, int64_t val );
rc_t KMetadataNextSequence( KMetadata *self, const char *seq, int64_t *val );

typedef struct KMDataNode KMDataNode;

rc_t KMDataNodeAddRef( const KMDataNode *self );
rc_t KMDataNodeRelease( const KMDataNode *self );

rc_t KMetadataOpenNodeRead( const KMetadata *self, const KMDataNode **node, const char *path, ... );
rc_t KMDataNodeOpenNodeRead( const KMDataNode *self, const KMDataNode **node, const char *path, ... );
rc_t KMetadataVOpenNodeRead( const KMetadata *self, const KMDataNode **node, const char *path, va_list args );
rc_t KMDataNodeVOpenNodeRead( const KMDataNode *self, const KMDataNode **node, const char *path, va_list args );

rc_t KMetadataOpenNodeUpdate( KMetadata *self, KMDataNode **node, const char *path, ... );
rc_t KMDataNodeOpenNodeUpdate( KMDataNode *self, KMDataNode **node, const char *path, ... );
rc_t KMetadataVOpenNodeUpdate( KMetadata *self, KMDataNode **node, const char *path, va_list args );
rc_t KMDataNodeVOpenNodeUpdate( KMDataNode *self, KMDataNode **node, const char *path, va_list args );

rc_t KMDataNodeByteOrder( const KMDataNode *self, bool *reversed );

rc_t KMDataNodeRead( const KMDataNode *self, size_t offset, void *buffer, size_t bsize,
                     size_t *num_read, size_t *remaining );
rc_t KMDataNodeWrite( KMDataNode *self, const void *buffer, size_t size );
rc_t KMDataNodeAppend( KMDataNode *self, const void *buffer, size_t size );

rc_t KMDataNodeReadB8( const KMDataNode *self, void *b8 );
rc_t KMDataNodeReadB16( const KMDataNode *self, void *b16 );
rc_t KMDataNodeReadB32( const KMDataNode *self, void *b32 );
rc_t KMDataNodeReadB64( const KMDataNode *self, void *b64 );
rc_t KMDataNodeReadB128( const KMDataNode *self, void *b128 );

rc_t KMDataNodeReadAsI16( const KMDataNode *self, int16_t *i );
rc_t KMDataNodeReadAsU16( const KMDataNode *self, uint16_t *u );
rc_t KMDataNodeReadAsI32( const KMDataNode *self, int32_t *i );
rc_t KMDataNodeReadAsU32( const KMDataNode *self, uint32_t *u );
rc_t KMDataNodeReadAsI64( const KMDataNode *self, int64_t *i );
rc_t KMDataNodeReadAsU64( const KMDataNode *self, uint64_t *u );
rc_t KMDataNodeReadAsF64( const KMDataNode *self, double *f );
rc_t KMDataNodeReadCString( const KMDataNode *self, char *buffer, size_t bsize, size_t *size );

rc_t KMDataNodeWriteB8( KMDataNode *self, const void *b8 );
rc_t KMDataNodeWriteB16( KMDataNode *self, const void *b16 );
rc_t KMDataNodeWriteB32( KMDataNode *self, const void *b32 );
rc_t KMDataNodeWriteB64( KMDataNode *self, const void *b64 );
rc_t KMDataNodeWriteB128( KMDataNode *self, const void *b128 );
rc_t KMDataNodeWriteCString( KMDataNode *self, const char *str );

rc_t KMDataNodeReadAttr( const KMDataNode *self, const char *name, char *buffer, size_t bsize, size_t *size );
rc_t KMDataNodeWriteAttr( KMDataNode *self, const char *name, const char *value );

rc_t KMDataNodeReadAttrAsI16( const KMDataNode *self, const char *attr, int16_t *i );
rc_t KMDataNodeReadAttrAsU16( const KMDataNode *self, const char *attr, uint16_t *u );
rc_t KMDataNodeReadAttrAsI32( const KMDataNode *self, const char *attr, int32_t *i );
rc_t KMDataNodeReadAttrAsU32( const KMDataNode *self, const char *attr, uint32_t *u );
rc_t KMDataNodeReadAttrAsI64( const KMDataNode *self, const char *attr, int64_t *i );
rc_t KMDataNodeReadAttrAsU64( const KMDataNode *self, const char *attr, uint64_t *u );
rc_t KMDataNodeReadAttrAsF64( const KMDataNode *self, const char *attr, double *f );

rc_t KMDataNodeDropAll( KMDataNode *self );
rc_t KMDataNodeDropAttr( KMDataNode *self, const char *attr );
rc_t KMDataNodeDropChild( KMDataNode *self, const char *path, ... );
rc_t KMDataNodeVDropChild( KMDataNode *self, const char *path, va_list args );

rc_t KMDataNodeRenameAttr( KMDataNode *self, const char *from, const char *to );
rc_t KMDataNodeRenameChild( KMDataNode *self, const char *from, const char *to );

rc_t KMDataNodeCopy( KMDataNode *self, KMDataNode const *source );
rc_t KMDataNodeCompare( const KMDataNode *self, KMDataNode const *other, bool *equal );
rc_t KTableMetaCopy( struct KTable *self, const struct KTable *src,
                     const char * path, bool src_node_has_to_exist );
rc_t KTableMetaCompare( const struct KTable *self, const struct KTable *other,
                        const char * path, bool * equal );

}; // end of extern "C"

class MetaNode;
typedef std::shared_ptr< MetaNode > MetaNodePtr;
class MetaNode : public VDBObj {
    private :
#ifdef VDB_WRITE
        KMDataNode * f_node;
#else
        const KMDataNode * f_node;
#endif

        MetaNode( const KMetadata * meta, const std::string& path ) {
            set_rc( KMetadataOpenNodeRead( meta, (const KMDataNode **)&f_node, "%s", path . c_str() ) );
        }

        MetaNode( const KMDataNode * parent, const std::string& path ) {
            set_rc( KMDataNodeOpenNodeRead( parent, (const KMDataNode **)&f_node, "%s", path . c_str() ) );
        }

#ifdef VDB_WRITE
        MetaNode( KMetadata * meta, const std::string& path ) {
            set_rc( KMetadataOpenNodeUpdate( meta, &f_node, "%s", path . c_str() ) );
        }

        MetaNode( KMDataNode * parent, const std::string& path ) {
            set_rc( KMDataNodeOpenNodeUpdate( parent, &f_node, "%s", path . c_str() ) );
        }

#endif

    public :
        ~MetaNode() { KMDataNodeRelease( f_node ); }

        static MetaNodePtr open( const KMetadata * meta, const std::string& path ) {
            return MetaNodePtr( new MetaNode( meta, path ) );
        }

        MetaNodePtr open( const std::string& path ) {
            return MetaNodePtr( new MetaNode( f_node, path ) );
        }

        bool read( int16_t& value ) const {
            rc_t rc = KMDataNodeReadAsI16( f_node, &value );
            return ( 0 == rc );
        }

        bool read( uint16_t& value ) const {
            rc_t rc = KMDataNodeReadAsU16( f_node, &value );
            return ( 0 == rc );
        }

        bool read( int32_t& value ) const {
            rc_t rc = KMDataNodeReadAsI32( f_node, &value );
            return ( 0 == rc );
        }

        bool read( uint32_t& value ) const {
            rc_t rc = KMDataNodeReadAsU32( f_node, &value );
            return ( 0 == rc );
        }

        bool read( int64_t& value ) const {
            rc_t rc = KMDataNodeReadAsI64( f_node, &value );
            return ( 0 == rc );
        }

        bool read( uint64_t& value ) const {
            rc_t rc = KMDataNodeReadAsU64( f_node, &value );
            return ( 0 == rc );
        }

        bool read( double value ) const {
            rc_t rc = KMDataNodeReadAsF64( f_node, &value );
            return ( 0 == rc );
        }

        bool read( std::string& value ) const {
            char buffer[ 512 ];
            size_t read;
            rc_t rc = KMDataNodeReadCString( f_node, buffer, sizeof buffer, &read );
            if ( 0 == rc ) {
                value = std::string( buffer, read );
                return true;
            }
            return false;
        }

#ifdef VDB_WRITE
        static MetaNodePtr open_update( KMetadata * meta, const std::string& path ) {
            return MetaNodePtr( new MetaNode( meta, path ) );
        }

        MetaNodePtr open_update( const std::string& path ) {
            return MetaNodePtr( new MetaNode( f_node, path ) );
        }

        bool write( int16_t value ) {
            rc_t rc = KMDataNodeWriteB16( f_node, &value );
            return ( 0 == rc );
        }

        bool write( uint16_t value ) {
            rc_t rc = KMDataNodeWriteB16( f_node, &value );
            return ( 0 == rc );
        }

        bool write( int32_t value ) {
            rc_t rc = KMDataNodeWriteB32( f_node, &value );
            return ( 0 == rc );
        }

        bool write( uint32_t value ) {
            rc_t rc = KMDataNodeWriteB32( f_node, &value );
            return ( 0 == rc );
        }

        bool write( int64_t value ) {
            rc_t rc = KMDataNodeWriteB64( f_node, &value );
            return ( 0 == rc );
        }

        bool write( uint64_t value ) {
            rc_t rc = KMDataNodeWriteB64( f_node, &value );
            return ( 0 == rc );
        }

        bool write( const std::string& value ) {
            rc_t rc = KMDataNodeWriteCString( f_node, value . c_str() );
            return ( 0 == rc );
        }
#endif

};

class Meta;
typedef std::shared_ptr< Meta > MetaPtr;
class Meta : public VDBObj {
    private :
#ifdef VDB_WRITE
        KMetadata * f_meta;
#else
        const KMetadata * f_meta;
#endif

        Meta( const VTable * tbl ) { set_rc( VTableOpenMetadataRead( tbl, ( const KMetadata ** )&f_meta ) ); }

#ifdef VDB_WRITE
        Meta( VTable * tbl ) { set_rc( VTableOpenMetadataUpdate( tbl, &f_meta ) ); }
#endif

    public:
        ~Meta() { KMetadataRelease( f_meta ); }

        static MetaPtr open( const VTable * tbl ) { return MetaPtr( new Meta( tbl ) ); }

        MetaNodePtr open_node( const std::string& path ) {
            return MetaNode::open( f_meta, path );
        }

#ifdef VDB_WRITE
        static MetaPtr open( VTable * tbl ) { return MetaPtr( new Meta( tbl ) ); }

        MetaNodePtr open_node_update( const std::string& path ) {
            return MetaNode::open_update( f_meta, path );
        }

        bool commit( void ) {
            return set_rc( KMetadataCommit( f_meta ) );
        }

        bool freeze( void ) {
            return set_rc( KMetadataFreeze( f_meta ) );
        }

#endif

        uint32_t get_version( void ) const {
            uint32_t version;
            rc_t rc = KMetadataVersion( f_meta, &version );
            if ( 0 != rc ) { version = 0; }
            return version;
        }

        bool get_byte_order( void ) const {
            bool res;
            rc_t rc = KMetadataByteOrder( f_meta, &res );
            if ( 0 != rc ) { res = false; }
            return res;
        }

        uint32_t get_revision( void ) const {
            uint32_t revision;
            rc_t rc = KMetadataRevision( f_meta, &revision );
            if ( 0 != rc ) { revision = 0; }
            return revision;
        }

        uint32_t get_max_revision( void ) const {
            uint32_t revision;
            rc_t rc = KMetadataMaxRevision( f_meta, &revision );
            if ( 0 != rc ) { revision = 0; }
            return revision;
        }

};

extern "C" {

rc_t VDatabaseOpenDBRead( const VDatabase *self, const VDatabase **db, const char *name, ... );
rc_t VDatabaseVOpenDBRead( const VDatabase *self, const VDatabase **db, const char *name, va_list args );

rc_t VDBManagerOpenTableRead( VDBManager const *self, const VTable **tbl,
        VSchema const *schema, const char *path, ... );
rc_t VDBManagerVOpenTableRead( VDBManager const *self, const VTable **tbl,
        VSchema const *schema, const char *path, va_list args );
rc_t VDBManagerOpenTableReadVPath( VDBManager const *self, const VTable **tbl,
        VSchema const *schema, struct VPath *path );

rc_t VDBManagerCreateTable( VDBManager *self, VTable **tbl,
        VSchema const *schema, const char *typespec,
        KCreateMode cmode, const char *path, ... );

rc_t VDatabaseOpenTableRead( VDatabase const *self, const VTable **tbl,
        const char *name, ... );
rc_t VDatabaseVOpenTableRead( VDatabase const *self, const VTable **tbl,
        const char *name, va_list args );

rc_t VDatabaseCreateTableByMask( VDatabase *self, VTable **tbl,
        const char *member, KCreateMode cmode, KCreateMode cmode_mask, const char *name, ... );
rc_t VDatabaseVCreateTableByMask( VDatabase *self, VTable **tbl,
        const char *member, KCreateMode cmode, KCreateMode cmode_mask, const char *name, va_list args );
/* the following function is DEPRECATED, it's left for backward compatibility only */
rc_t VDatabaseCreateTable( VDatabase *self, VTable **tbl,
        const char *member, KCreateMode cmode, const char *name, ... );
/* the following function is DEPRECATED, it's left for backward compatibility only */
rc_t VDatabaseVCreateTable( VDatabase *self, VTable **tbl,
        const char *member, KCreateMode cmode, const char *name, va_list args );

rc_t VTableAddRef( const VTable *self );
rc_t VTableRelease( const VTable *self );

rc_t VTableTypespec( const VTable *self, char *ts_buff, std::size_t ts_size );

rc_t VTableOpenIndexRead( const VTable *self, const KIndex **idx, const char *name, ... );
rc_t VTableVOpenIndexRead( const VTable *self, const KIndex **idx, const char *name, va_list args );

rc_t VTableListReadableColumns( const VTable *self, KNamelist **names );
rc_t VTableListCol( const VTable *self, KNamelist **names );

rc_t VTableListReadableDatatypes( const VTable *self, const char *col,
        uint32_t *dflt_idx, KNamelist **typedecls );

rc_t VTableColumnDatatypes( const VTable *self, const char *col,
        uint32_t *dflt_idx, KNamelist **typedecls );

rc_t VTableOpenSchema( const VTable *self, VSchema const **schema );

rc_t VTableIsEmpty( const VTable *self, bool * empty );

rc_t VTableGetQualityCapability( const VTable *self, bool *fullQuality, bool *synthQuality );

};  // end of extern "C"

class VTbl;
typedef std::shared_ptr<VTbl> VTblPtr;
class VTbl : public VDBObj {
    private :
#ifdef VDB_WRITE
        VTable * f_tbl;
#else
        const VTable * f_tbl;
#endif

        VTbl( const VDBManager * mgr, const std::string& acc ) {
            set_rc( VDBManagerOpenTableRead( mgr, ( const VTable** )&f_tbl, nullptr, "%s", acc.c_str() ) );
        }

        VTbl( const VDatabase * db, const std::string& name ) {
            set_rc( VDatabaseOpenTableRead( db, ( const VTable** )&f_tbl, "%s", name.c_str() ) );
        }

#ifdef VDB_WRITE
        VTbl( VDBManager * mgr, const VSchPtr schema,
              const std::string& schema_type, const std::string& dir_name,
              const ChecksumKind_ptr checksum
        ) {
            KCreateMode mode = checksum -> mode( kcmInit );
            set_rc( VDBManagerCreateTable( mgr, &f_tbl, schema -> get_schema(),
                    schema_type . c_str(), mode, "%s", dir_name . c_str() ) );
        }

        VTbl( VDatabase * db, const std::string& schema_member, const std::string& name,
              const ChecksumKind_ptr checksum ) {
            KCreateMode mode = checksum -> mode( kcmInit );
            set_rc( VDatabaseCreateTable( db, &f_tbl,
                    schema_member . c_str(), mode, "%s", name . c_str() ) );
        }
#endif

    public :
        ~VTbl() { VTableRelease( f_tbl ); }

        static VTblPtr open( const VDBManager * mgr, const std::string& acc ) {
            return VTblPtr( new VTbl( mgr, acc ) );
        }

        static VTblPtr open( const VDatabase * db, const std::string& name ) {
            return VTblPtr( new VTbl( db, name ) );
        }

        MetaPtr open_meta( void ) const { return Meta::open( f_tbl ); }

#ifdef VDB_WRITE
        static VTblPtr create( VDBManager * mgr, const VSchPtr schema,
                              const std::string& schema_type, const std::string& dir_name,
                              const ChecksumKind_ptr checksum ) {
            return VTblPtr( new VTbl( mgr, schema, schema_type, dir_name, checksum ) );
        }

        static VTblPtr create( VDBManager * mgr, const VSchPtr schema,
                              const char * schema_type, const char * dir_name,
                              const ChecksumKind_ptr checksum ) {
            return VTblPtr( new VTbl( mgr, schema, schema_type, dir_name, checksum ) );
        }

        static VTblPtr create( VDatabase * db, const std::string& schema_member,
                               const std::string& name, const ChecksumKind_ptr checksum ) {
            return VTblPtr( new VTbl( db, schema_member, name, checksum ) );
        }

        static VTblPtr create( VDatabase * db, const char * schema_member,
                               const char * name, const ChecksumKind_ptr checksum ) {
            return VTblPtr( new VTbl( db, schema_member, name, checksum ) );
        }

        MetaPtr open_meta_for_update( void ) const { return Meta::open( f_tbl ); }

#endif

        list_of_str columns( bool readable = true ) {
            KNamelist * t;
            if ( readable ) {
                set_rc( VTableListReadableColumns( f_tbl, &t ) );
            } else {
                set_rc( VTableListCol( f_tbl, &t  ) );
            }
            return into_list( t );
        }

        bool has_column( const std::string& col_name )  {
            auto available = columns();
            auto found = std::find( available.begin(), available.end(), col_name );
            return ( found != available . end() );
        }

        RowRange row_range( const std::string& col_name ) {
            auto cur = cursor();
            if ( *cur  ) {
                if ( cur -> add_col( 1, col_name ) ) {
                    if ( cur -> open() ) {
                        return cur -> row_range( 1 );
                    }
                }
            }
            return RowRange();
        }

        bool empty( void ) {
            bool b = true;
            set_rc( VTableIsEmpty( f_tbl, &b ) );
            return b;
        }

        std::string get_type_spec( void ) const {
            char buff[ 64 ];
            rc_t rc = VTableTypespec( f_tbl, buff, sizeof( buff ) );
            return 0 == rc ? std::string( buff ) : std::string( "" );
        }

        VCurPtr cursor( std::size_t capacity = 0 ) const {
            return VCur::make( f_tbl, capacity );
        }

#ifdef VDB_WRITE
        VCurPtr writable_cursor( void ) const { return VCur::make( f_tbl ); }
#endif
}; // end of VTbl


extern "C" {

rc_t VDatabaseAddRef( const VDatabase *self );
rc_t VDatabaseRelease( const VDatabase *self );

rc_t VDatabaseOpenSchema( const VDatabase *self, VSchema const **schema );
rc_t VDatabaseListTbl( const VDatabase *self, KNamelist **names );
rc_t VDatabaseListDB( const VDatabase *self, KNamelist **names );

rc_t VDatabaseTypespec( const VDatabase *self, char *ts_buff, std::size_t ts_size );
rc_t VDatabaseGetQualityCapability( const VDatabase *self, bool *fullQuality, bool *synthQuality );
rc_t VDatabaseOpenMetadataRead( const VDatabase *self, KMetadata const **meta );

rc_t VDBManagerOpenDBRead( VDBManager const *self, const VDatabase **db,
        VSchema const *schema, const char *path, ... );
rc_t VDBManagerVOpenDBRead( VDBManager const *self, const VDatabase **db,
        VSchema const *schema, const char *path, va_list args );
rc_t VDBManagerOpenDBReadVPath( VDBManager const *self, const VDatabase **db,
        VSchema const *schema, VPath *path );

rc_t VDBManagerCreateDB( VDBManager *self, VDatabase **db,
        VSchema const *schema, const char *typespec,
        KCreateMode cmode, const char *path, ... );

rc_t VDatabaseCreateDB( VDatabase *self, VDatabase **db,
        const char *member, KCreateMode cmode, const char *name, ... );

};  // end of extern "C"

class VDb;
typedef std::shared_ptr< VDb > VDbPtr;
class VDb : public VDBObj {
    private :

#ifdef VDB_WRITE
         VDatabase * f_db;
#else
        const VDatabase * f_db;
#endif

        VDb( const VDBManager * mgr, const std::string& acc ) {
            set_rc( VDBManagerOpenDBRead( mgr, ( const VDatabase** )&f_db, nullptr, "%s", acc.c_str() ) );
        }

        VDb( const VDatabase * db, const std::string& name ) {
            set_rc( VDatabaseOpenDBRead( db, ( const VDatabase** )&f_db, "%s", name.c_str() ) );
        }

#ifdef VDB_WRITE
        VDb( VDBManager * mgr, const VSchPtr schema,
             const std::string& schema_type, const std::string& dir_name, const ChecksumKind_ptr checksum ) {
            KCreateMode mode = checksum -> mode( kcmInit );
            set_rc( VDBManagerCreateDB( mgr, &f_db, schema -> get_schema(),
                    schema_type . c_str(), mode, "%s", dir_name . c_str() ) );
        }

        VDb( VDatabase * parent, const std::string& schema_member, const std::string& name,
             const ChecksumKind_ptr checksum ) {
            KCreateMode mode = checksum -> mode( kcmInit );
            set_rc( VDatabaseCreateDB( parent, &f_db,
                    schema_member . c_str(), mode, "%s", name . c_str() ) );
        }

#endif

    public :
        ~VDb() { VDatabaseRelease( f_db ); }

        static VDbPtr make( const VDBManager * mgr, const std::string& acc ) {
            return VDbPtr( new VDb( mgr, acc ) );
        }

#ifdef VDB_WRITE
        static VDbPtr create( VDBManager * mgr, const VSchPtr schema,
                const std::string& schema_type, const std::string& dir_name,
                const ChecksumKind_ptr checksum ) {
            return VDbPtr( new VDb( mgr, schema, schema_type, dir_name, checksum ) );
        }

        static VDbPtr create( VDBManager * mgr, const VSchPtr schema,
                const char * schema_type, const char * dir_name,
                const ChecksumKind_ptr checksum ) {
            return VDbPtr( new VDb( mgr, schema, schema_type, dir_name, checksum ) );
        }

        VDbPtr create_child( const std::string& schema_member, const std::string& name,
                             const ChecksumKind_ptr checksum ) {
            return VDbPtr( new VDb( f_db, schema_member, name, checksum ) );
        }

        VDbPtr create_child( const char * schema_member, const char * name,
                             const ChecksumKind_ptr checksum) {
            return VDbPtr( new VDb( f_db, schema_member, name, checksum ) );
        }

        VTblPtr create_tbl( const std::string& schema_member, const std::string& name,
                            const ChecksumKind_ptr checksum ) {
            return VTbl::create( f_db, schema_member, name, checksum );
        }

        VTblPtr create_tbl( const char * schema_member, const char * name,
                            const ChecksumKind_ptr checksum ) {
            return VTbl::create( f_db, schema_member, name, checksum );
        }

#endif

        list_of_str tables( void ) {
            KNamelist * t;
            set_rc( VDatabaseListTbl( f_db, &t ) );
            return into_list( t );
        }

        list_of_str dbs( void ) {
            KNamelist * t;
            set_rc( VDatabaseListDB( f_db, &t ) );
            return into_list( t );
        }

        VDbPtr open_db( const std::string& name ) const {
            return VDbPtr( new VDb( f_db, name ) );
        }

        VTblPtr open_tbl( const std::string& name ) const {
            return VTbl::open( f_db, name );
        }

        RowRange range( const std::string tbl_name, const std::string& col_name ) const {
            auto tbl = open_tbl( tbl_name );
            if ( *tbl ) { return tbl -> row_range( col_name ); }
            return RowRange();
        }
};

extern "C" {

enum {
    kptAny = 0,
    kptDatabase = kptLastDefined,
    kptTable,
    kptIndex,
    kptColumn,
    kptMetadata,
    kptPrereleaseTbl
};

rc_t VDBManagerAddRef( const VDBManager *self );
rc_t VDBManagerRelease( const VDBManager *self );
rc_t VDBManagerMakeRead( const VDBManager **mgr, KDirectory const *wd );
rc_t VDBManagerMakeUpdate( VDBManager **mgr, KDirectory *wd );
rc_t VDBManagerVersion( const VDBManager *self, uint32_t *version );

rc_t VDBManagerAddSchemaIncludePath( const VDBManager *self, const char *path, ... );
rc_t VDBManagerVAddSchemaIncludePath( const VDBManager *self, const char *path, va_list args );

rc_t VDBManagerGetObjVersion( const VDBManager *self, ver_t * version, const char *path );
rc_t VDBManagerGetObjModDate( const VDBManager *self, KTime_t * ts, const char *path );

int VDBManagerPathType( const VDBManager * self, const char *path, ... );
int VDBManagerVPathType( const VDBManager * self, const char *path, va_list args );

rc_t VDBManagerGetCacheRoot( const VDBManager * self, VPath const ** path );
rc_t VDBManagerSetCacheRoot( const VDBManager * self, VPath const * path );

rc_t VDBManagerDeleteCacheOlderThan( const VDBManager * self, uint32_t days );
rc_t VDBManagerPreferFullQuality( VDBManager * self );
rc_t VDBManagerPreferZeroQuality( VDBManager * self );

rc_t VDBManagerOpenView( VDBManager const * self, const VView **view,
        const VSchema * schema, const char * name );
rc_t VDatabaseOpenView( VDatabase const *self, const VView **view, const char *name );

}; // end of extern "C"

class VMgr;
typedef std::shared_ptr< VMgr > VMgrPtr;
class VMgr : public VDBObj {
    private :
#ifdef VDB_WRITE
        VDBManager * f_mgr;
#else
        const VDBManager * f_mgr;
#endif

#ifdef VDB_WRITE
        VMgr( KDirectory * dir ) {
            set_rc( VDBManagerMakeUpdate( &f_mgr, dir ) );
        }
#else
        VMgr( const KDirectory * dir ) {
            set_rc( VDBManagerMakeRead( &f_mgr, dir ) );
        }
#endif

    public :
        ~VMgr() { VDBManagerRelease( f_mgr ); }

#ifdef VDB_WRITE
        static VMgrPtr make( KDirectory * dir ) { return VMgrPtr( new VMgr( dir ) ); }
#else
        static VMgrPtr make( const KDirectory * dir ) { return VMgrPtr( new VMgr( dir ) ); }
#endif

        // open table or database for read
        VTblPtr open_tbl( const std::string& acc ) const { return VTbl::open( f_mgr, acc ); }
        VDbPtr open_db( const std::string& acc ) const { return VDb::make( f_mgr, acc ); }

        VSchPtr make_schema( void ) const { return VSch::make( f_mgr ); }

#ifdef VDB_WRITE
        VTblPtr create_tbl( const VSchPtr schema, const std::string& schema_type,
                            const std::string& dir_name, const ChecksumKind_ptr checksum ) const {
            return VTbl::create( f_mgr, schema, schema_type, dir_name, checksum );
        }

        VTblPtr create_tbl( const VSchPtr schema, const char * schema_type,
                            const char * dir_name, const ChecksumKind_ptr checksum ) const {
            return VTbl::create( f_mgr, schema, schema_type, dir_name, checksum );
        }

        VDbPtr create_db( const VSchPtr schema, const std::string& schema_type,
                          const std::string& dir_name, const ChecksumKind_ptr checksum ) const {
            return VDb::create( f_mgr, schema, schema_type, dir_name, checksum );
        }

        VDbPtr create_db( const VSchPtr schema, const char * schema_type,
                          const char * dir_name, const ChecksumKind_ptr checksum ) const {
            return VDb::create( f_mgr, schema, schema_type, dir_name, checksum );
        }

#endif

        uint32_t version( void ) const {
            uint32_t v = 0;
            rc_t rc = ( VDBManagerVersion( f_mgr, &v ) );
            return 0 == rc ? v : 0;
        }

        int PathType( const std::string& path ) const {
            return VDBManagerPathType( f_mgr, "%s", path.c_str() );
        }

        bool AddSchemaIncludePath( const std::string& path ) const {
            rc_t rc = VDBManagerAddSchemaIncludePath( f_mgr, "%s", path.c_str() );
            return ( 0 == rc );
        }

        uint32_t GetObjVersion( const std::string& path ) const {
            uint32_t v = 0;
            rc_t rc = VDBManagerGetObjVersion( f_mgr, &v, path.c_str() );
            return 0 == rc ? v : 0;
        }

        KTime_t GetObjModDate( const std::string& path ) const {
            KTime_t t = 0;
            rc_t rc = VDBManagerGetObjModDate( f_mgr, &t, path . c_str() );
            return 0 == rc ? t : 0;
        }
};

extern "C" {

rc_t KDirectoryAddRef_v1( const KDirectory *self );
#define KDirectoryAddRef KDirectoryAddRef_v1
rc_t KDirectoryRelease_v1( const KDirectory *self );
#define KDirectoryRelease KDirectoryRelease_v1

rc_t KDirectoryList_v1( const KDirectory *self, KNamelist **list,
        bool ( * f ) ( const KDirectory *dir, const char *name, void *data ),
        void *data, const char *path, ... );
#define KDirectoryList KDirectoryList_v1
rc_t KDirectoryVList( const KDirectory *self, KNamelist **list,
        bool ( * f ) ( const KDirectory *dir, const char *name, void *data ),
        void *data, const char *path, va_list args );

rc_t KDirectoryVisit_v1( const KDirectory *self, bool recur,
        rc_t ( * f ) ( const KDirectory *dir, uint32_t type, const char *name, void *data ),
        void *data, const char *path, ... );
#define KDirectoryVisit KDirectoryVisit_v1
rc_t KDirectoryVVisit( const KDirectory *self, bool recur,
        rc_t ( * f ) ( const KDirectory *dir, uint32_t type, const char *name, void *data ),
        void *data, const char *path, va_list args );

rc_t KDirectoryVisitUpdate_v1( KDirectory *self, bool recur,
        rc_t ( * f ) ( KDirectory *dir, uint32_t type, const char *name, void *data ),
        void *data, const char *path, ... );
#define KDirectoryVisitUpdate KDirectoryVisitUpdate_v1
rc_t KDirectoryVVisitUpdate( KDirectory *self, bool recur,
        rc_t ( * f ) ( KDirectory *dir, uint32_t type, const char *name, void *data ),
        void *data, const char *path, va_list args );

uint32_t KDirectoryPathType_v1( const KDirectory *self, const char *path, ... );
#define KDirectoryPathType KDirectoryPathType_v1
uint32_t KDirectoryVPathType( const KDirectory *self, const char *path, va_list args );

rc_t KDirectoryResolvePath_v1( const KDirectory *self, bool absolute,
		char *resolved, std::size_t rsize, const char *path, ... );
#define KDirectoryResolvePath KDirectoryResolvePath_v1

rc_t KDirectoryVResolvePath( const KDirectory *self, bool absolute,
        char *resolved, std::size_t rsize, const char *path, va_list args );

rc_t KDirectoryResolveAlias_v1( const KDirectory *self, bool absolute,
        char *resolved, std::size_t rsize, const char *alias, ... );
#define KDirectoryResolveAlias KDirectoryResolveAlias_v1
rc_t KDirectoryVResolveAlias( const KDirectory *self, bool absolute,
        char *resolved, std::size_t rsize, const char *alias, va_list args );

rc_t KDirectoryRename_v1( KDirectory *self, bool force, const char *from, const char *to );
#define KDirectoryRename KDirectoryRename_v1

rc_t KDirectoryRemove_v1( KDirectory *self, bool force, const char *path, ... );
#define KDirectoryRemove KDirectoryRemove_v1
rc_t KDirectoryVRemove( KDirectory *self, bool force, const char *path, va_list args );

rc_t KDirectoryClearDir_v1( KDirectory *self, bool force, const char *path, ... );
#define KDirectoryClearDir KDirectoryClearDir_v1
rc_t KDirectoryVClearDir( KDirectory *self, bool force, const char *path, va_list args );

rc_t KDirectoryAccess_v1( const KDirectory *self, uint32_t *access, const char *path, ... );
#define KDirectoryAccess KDirectoryAccess_v1
rc_t KDirectoryVAccess( const KDirectory *self, uint32_t *access, const char *path, va_list args );

rc_t KDirectorySetAccess_v1( KDirectory *self, bool recur, uint32_t access,
        uint32_t mask, const char *path, ... );
#define KDirectorySetAccess KDirectorySetAccess_v1
rc_t KDirectoryVSetAccess( KDirectory *self, bool recur, uint32_t access,
        uint32_t mask, const char *path, va_list args );

rc_t KDirectoryDate_v1( const KDirectory *self, KTime_t *date, const char *path, ... );
#define KDirectoryDate KDirectoryDate_v1
rc_t KDirectoryVDate( const KDirectory *self, KTime_t *date, const char *path, va_list args );

rc_t KDirectorySetDate_v1( KDirectory *self, bool recur, KTime_t date, const char *path, ... );
#define KDirectorySetDate KDirectorySetDate_v1
rc_t KDirectoryVSetDate( KDirectory *self, bool recur, KTime_t date, const char *path, va_list args );

rc_t KDirectoryCreateAlias_v1( KDirectory *self, uint32_t access, KCreateMode mode,
        const char *targ, const char *alias );
#define KDirectoryCreateAlias KDirectoryCreateAlias_v1

rc_t KDirectoryCreateLink_v1( KDirectory *self, uint32_t access, KCreateMode mode,
        const char *oldpath, const char *newpath );
#define KDirectoryCreateLink KDirectoryCreateLink_v1

rc_t KDirectoryOpenFileRead_v1( const KDirectory *self, KFile const **f, const char *path, ... );
#define KDirectoryOpenFileRead KDirectoryOpenFileRead_v1
rc_t KDirectoryVOpenFileRead( const KDirectory *self, KFile const **f, const char *path, va_list args );

rc_t KDirectoryOpenFileWrite_v1( KDirectory *self, KFile **f, bool update, const char *path, ... );
#define KDirectoryOpenFileWrite KDirectoryOpenFileWrite_v1
rc_t KDirectoryVOpenFileWrite( KDirectory *self, KFile **f, bool update, const char *path, va_list args );

rc_t KDirectoryOpenFileSharedWrite_v1( KDirectory *self, KFile **f, bool update, const char *path, ... );
#define KDirectoryOpenFileSharedWrite KDirectoryOpenFileSharedWrite_v1
rc_t KDirectoryVOpenFileSharedWrite( KDirectory *self, KFile **f, bool update, const char *path, va_list args );

rc_t KDirectoryCreateFile( KDirectory *self, KFile **f, bool update, uint32_t access,
        KCreateMode mode, const char *path, ... );
rc_t KDirectoryVCreateFile( KDirectory *self, KFile **f, bool update, uint32_t access,
        KCreateMode mode, const char *path, va_list args );

rc_t KDirectoryFileSize_v1( const KDirectory *self, uint64_t *size, const char *path, ... );
#define KDirectoryFileSize KDirectoryFileSize_v1
rc_t KDirectoryVFileSize( const KDirectory *self, uint64_t *size, const char *path, va_list args );

rc_t KDirectoryFilePhysicalSize_v1( const KDirectory *self, uint64_t *size, const char *path, ... );
#define KDirectoryFilePhysicalSize KDirectoryFilePhysicalSize_v1
rc_t KDirectoryVFilePhysicalSize ( const KDirectory *self, uint64_t *size, const char *path, va_list args );

rc_t KDirectorySetFileSize_v1( KDirectory *self, uint64_t size, const char *path, ... );
#define KDirectorySetFileSize KDirectorySetFileSize_v1
rc_t KDirectoryVSetFileSize ( KDirectory *self, uint64_t size, const char *path, va_list args );

rc_t KDirectoryFileLocator_v1( const KDirectory *self, uint64_t *locator, const char *path, ... );
#define KDirectoryFileLocator KDirectoryFileLocator_v1
rc_t KDirectoryVFileLocator ( const KDirectory *self, uint64_t *locator, const char *path, va_list args );

rc_t KDirectoryFileContiguous_v1( const KDirectory *self, bool *contiguous, const char *path, ... );
#define KDirectoryFileContiguous KDirectoryFileContiguous_v1
rc_t KDirectoryVFileContiguous( const KDirectory *self, bool *contiguous, const char *path, va_list args );

rc_t KDirectoryOpenDirRead_v1( const KDirectory *self, const KDirectory **sub, bool chroot, const char *path, ... );
#define KDirectoryOpenDirRead KDirectoryOpenDirRead_v1
rc_t KDirectoryVOpenDirRead( const KDirectory *self, const KDirectory **sub, bool chroot, const char *path, va_list args );

rc_t KDirectoryOpenDirUpdate_v1( KDirectory *self, KDirectory **sub, bool chroot, const char *path, ... );
#define KDirectoryOpenDirUpdate KDirectoryOpenDirUpdate_v1
rc_t KDirectoryVOpenDirUpdate( KDirectory *self, KDirectory **sub, bool chroot, const char *path, va_list args );

rc_t KDirectoryCreateDir_v1( KDirectory *self, uint32_t access, KCreateMode mode, const char *path, ... );
#define KDirectoryCreateDir KDirectoryCreateDir_v1
rc_t KDirectoryVCreateDir( KDirectory *self, uint32_t access, KCreateMode mode, const char *path, va_list args );

rc_t KDirectoryCopyPath_v1( const KDirectory *src_dir, KDirectory *dst_dir, const char *src_path, const char *dst_path );
#define KDirectoryCopyPath KDirectoryCopyPath_v1
rc_t KDirectoryCopyPaths_v1( const KDirectory * src_dir, KDirectory *dst_dir,
        bool recursive, const char *src, const char *dst );
#define KDirectoryCopyPaths KDirectoryCopyPaths_v1

rc_t KDirectoryCopy_v1( const KDirectory *src_dir, KDirectory *dst_dir, bool recursive, const char *src, const char *dst );
#define KDirectoryCopy KDirectoryCopy_v1

rc_t KDirectoryGetDiskFreeSpace_v1( const KDirectory * self, uint64_t * free_bytes_available, uint64_t * total_number_of_bytes );
#define KDirectoryGetDiskFreeSpace KDirectoryGetDiskFreeSpace_v1

rc_t KDirectoryNativeDir_v1( KDirectory **dir );
#define KDirectoryNativeDir KDirectoryNativeDir_v1

rc_t KFileAddRef( const KFile *self );
rc_t KFileRelease( const KFile *self );
rc_t KFileRandomAccess( const KFile *self );
uint32_t KFileType( const KFile *self );
rc_t KFileSize( const KFile *self, uint64_t *size );
rc_t KFileSetSize( KFile *self, uint64_t size );
rc_t KFileRead( const KFile *self, uint64_t pos, void *buffer, std::size_t bsize, std::size_t *num_read );
rc_t KFileTimedRead( const KFile *self, uint64_t pos,void *buffer, std::size_t bsize,
        std::size_t *num_read, timeout_t *tm );
rc_t KFileReadAll( const KFile *self, uint64_t pos, void *buffer, std::size_t bsize, std::size_t *num_read );
rc_t KFileTimedReadAll( const KFile *self, uint64_t pos, void *buffer, std::size_t bsize,
        std::size_t *num_read, timeout_t *tm );
rc_t KFileReadExactly( const KFile *self, uint64_t pos, void *buffer, std::size_t bytes );
rc_t KFileTimedReadExactly( const KFile *self, uint64_t pos, void *buffer, std::size_t bytes, timeout_t *tm );
rc_t KFileReadChunked( const KFile *self, uint64_t pos, KChunkReader * chunks,
        std::size_t bytes, std::size_t * num_read );
rc_t KFileTimedReadChunked( const KFile *self, uint64_t pos, KChunkReader * chunks,
        std::size_t bytes, std::size_t * num_read, timeout_t *tm );
rc_t KFileWrite( KFile *self, uint64_t pos, const void *buffer, std::size_t size, std::size_t *num_writ );
rc_t KFileTimedWrite( KFile *self, uint64_t pos, const void *buffer, std::size_t size,
        std::size_t *num_writ, timeout_t *tm );
rc_t KFileWriteAll( KFile *self, uint64_t pos, const void *buffer, std::size_t size, std::size_t *num_writ );
rc_t KFileTimedWriteAll( KFile *self, uint64_t pos, const void *buffer, std::size_t size,
        std::size_t *num_writ, struct timeout_t *tm );
rc_t KFileWriteExactly( KFile *self, uint64_t pos, const void *buffer, std::size_t bytes );
rc_t KFileTimedWriteExactly( KFile *self, uint64_t pos, const void *buffer, std::size_t bytes, timeout_t *tm );
rc_t KFileMakeStdIn( const KFile **std_in );
rc_t KFileMakeStdOut( KFile **std_out );
rc_t KFileMakeStdErr( KFile **std_err );

}; // end of extern "C"

class KDir;
typedef std::shared_ptr<KDir> KDirPtr;
class KDir : public VDBObj {
    private :
        KDirectory * f_dir;

        KDir() : f_dir( nullptr ) {
            set_rc( KDirectoryNativeDir( &f_dir ) );
        }

        KDir( const std::string& path ) : f_dir( nullptr ) {
            KDirectory * f_root;
            if ( set_rc( KDirectoryNativeDir( &f_root ) ) ) {
                if ( path . empty() ) {
                    f_dir = f_root;
                } else {
                    set_rc( KDirectoryOpenDirUpdate( f_root, &f_dir, true, "%s", path.c_str() ) );
                    KDirectoryRelease( f_root );
                }
            }
        }

    public :
        ~KDir() { KDirectoryRelease( f_dir ); }

        static KDirPtr make( void ) { return KDirPtr( new KDir() ); }
        static KDirPtr make( const std::string& path ) { return KDirPtr( new KDir( path ) ); }

        VMgrPtr make_mgr( void ) { return VMgr::make( f_dir ); }

        KDirectory * get_dir( void ) const { return f_dir; }
};

extern "C" {

typedef uint32_t VRemoteProtocols;
enum {
    /* version 1.1 protocols */
      eProtocolNone  = 0
    , eProtocolDefault = eProtocolNone
    , eProtocolHttp  = 1
    , eProtocolFasp  = 2

      /* version 1.2 protocols */
    , eProtocolHttps = 3

      /* version 3.0 protocols */
    , eProtocolFile  = 4
    , eProtocolS3    = 5 /* Amazon Simple Storage Service */
    , eProtocolGS    = 6 /* Google Cloud Storage */

      /* value 7 are available for future */

    , eProtocolLast
    , eProtocolMax   = eProtocolLast - 1
    , eProtocolMask  = 7

    , eProtocolMaxPref = 6

      /* macros for building multi-protocol constants
         ordered by preference from least to most significant bits */
#define VRemoteProtocolsMake2( p1, p2 )                                     \
      ( ( ( VRemoteProtocols ) ( p1 ) & eProtocolMask ) |                   \
        ( ( ( VRemoteProtocols ) ( p2 ) & eProtocolMask ) << ( 3 * 1 ) ) )

#define VRemoteProtocolsMake3( p1, p2, p3 )                                 \
      ( VRemoteProtocolsMake2 ( p1, p2 ) |                                  \
        ( ( ( VRemoteProtocols ) ( p3 ) & eProtocolMask ) << ( 3 * 2 ) ) )

#define VRemoteProtocolsMake4( p1, p2, p3, p4 )                                 \
      ( VRemoteProtocolsMake3 ( p1, p2, p3 ) |                                  \
        ( ( ( VRemoteProtocols ) ( p4 ) & eProtocolMask ) << ( 3 * 3 ) ) )

    , eProtocolFaspHttp         = VRemoteProtocolsMake2 ( eProtocolFasp,  eProtocolHttp  )
    , eProtocolHttpFasp         = VRemoteProtocolsMake2 ( eProtocolHttp,  eProtocolFasp  )
    , eProtocolHttpsHttp        = VRemoteProtocolsMake2 ( eProtocolHttps, eProtocolHttp  )
    , eProtocolHttpHttps        = VRemoteProtocolsMake2 ( eProtocolHttp,  eProtocolHttps )
    , eProtocolFaspHttps        = VRemoteProtocolsMake2 ( eProtocolFasp,  eProtocolHttps )
    , eProtocolHttpsFasp        = VRemoteProtocolsMake2 ( eProtocolHttps, eProtocolFasp  )
    , eProtocolFaspHttpHttps    = VRemoteProtocolsMake3 ( eProtocolFasp,  eProtocolHttp,  eProtocolHttps )
    , eProtocolHttpFaspHttps    = VRemoteProtocolsMake3 ( eProtocolHttp,  eProtocolFasp,  eProtocolHttps )
    , eProtocolFaspHttpsHttp    = VRemoteProtocolsMake3 ( eProtocolFasp,  eProtocolHttps, eProtocolHttp  )
    , eProtocolHttpHttpsFasp    = VRemoteProtocolsMake3 ( eProtocolHttp,  eProtocolHttps, eProtocolFasp  )
    , eProtocolHttpsFaspHttp    = VRemoteProtocolsMake3 ( eProtocolHttps, eProtocolFasp,  eProtocolHttp  )
    , eProtocolHttpsHttpFasp    = VRemoteProtocolsMake3 ( eProtocolHttps, eProtocolHttp,  eProtocolFasp  )
    , eProtocolFileFaspHttpHttps= VRemoteProtocolsMake4 ( eProtocolFile,  eProtocolFasp,  eProtocolHttp, eProtocolHttps  )
};

rc_t VResolverAddRef( const VResolver * self );
rc_t VResolverRelease( const VResolver * self );

VRemoteProtocols VRemoteProtocolsParse( String const * protos );
rc_t VResolverQuery( const VResolver * self, VRemoteProtocols protocols, VPath const * query,
		VPath const ** local, VPath const ** remote, VPath const ** cache );

// DEPRECATED :
rc_t VResolverLocal( const VResolver * self, VPath const * accession, VPath const ** path );
rc_t VResolverRemote( const VResolver * self, VRemoteProtocols protocols, VPath const * accession,
		VPath const ** path );
rc_t VResolverCache( const VResolver * self, VPath const * url, VPath const ** path, uint64_t file_size );

typedef uint32_t VResolverEnableState;
enum { vrUseConfig = 0, vrAlwaysEnable = 1, vrAlwaysDisable = 2 };
VResolverEnableState VResolverLocalEnable( const VResolver * self, VResolverEnableState enable );
VResolverEnableState VResolverRemoteEnable( const VResolver * self, VResolverEnableState enable );
VResolverEnableState VResolverCacheEnable( const VResolver * self, VResolverEnableState enable );

}; // end of extern "C"

extern "C" {

rc_t VFSManagerAddRef( const VFSManager *self );
rc_t VFSManagerRelease( const VFSManager *self );
rc_t VFSManagerMake ( VFSManager ** pmanager );

rc_t VFSManagerMakeResolver( VFSManager const * self, VResolver ** new_resolver, KConfig const * cfg );

//rc_t KRepositoryMakeResolver( KRepository const * self, VResolver ** new_resolver, KConfig const * cfg );

rc_t VFSManagerOpenFileRead ( const VFSManager *self, KFile const **f, const VPath * path );
rc_t VFSManagerOpenFileReadWithBlocksize ( const VFSManager *self, KFile const **f,
        const VPath * path, uint32_t blocksize, bool promote );
rc_t VFSManagerOpenDirectoryRead( const VFSManager *self, KDirectory const **d, const VPath * path );
rc_t VFSManagerOpenDirectoryReadDecrypt( const VFSManager *self, KDirectory const **d, const VPath * path );
rc_t VFSManagerOpenDirectoryReadDecryptRemote( const VFSManager *self, KDirectory const **d,
        const VPath * path, const VPath * cache );
rc_t VFSManagerOpenDirectoryUpdate( const VFSManager *self, KDirectory **d, const VPath * path );
rc_t VFSManagerOpenFileWrite( const VFSManager *self, KFile **f, bool update, const VPath * path);
rc_t VFSManagerCreateFile( const VFSManager *self,  KFile **f, bool update, uint32_t access,
        KCreateMode mode, const VPath * path );
rc_t VFSManagerRemove( const VFSManager *self, bool force, const VPath * path );
rc_t VFSManagerGetCWD( const VFSManager * self, KDirectory ** cwd );
rc_t VFSManagerGetResolver( const VFSManager * self, VResolver ** resolver );
//rc_t VFSManagerGetKNSMgr( const VFSManager * self, KNSManager ** kns );
rc_t VFSManagerGetKryptoPassword( const VFSManager * self, char * new_password, std::size_t max_size, std::size_t * size);
rc_t VFSManagerUpdateKryptoPassword( const VFSManager * self, const char * password,
        std::size_t size, char * pwd_dir, std::size_t pwd_dir_size );
rc_t VFSManagerResolveSpec( const VFSManager * self, const char * spec, VPath ** path_to_build,
        const KFile ** remote_file, const VPath ** local_cache, bool resolve_acc );
rc_t VFSManagerResolveSpecIntoDir( const VFSManager * self, const char * spec,
        const KDirectory ** dir, bool resolve_acc );

rc_t KConfigReadVPath( KConfig const* self, const char* path, VPath** result );
rc_t KConfigNodeReadVPath( KConfigNode const *self, VPath** result );

#define vfsmgr_rflag_no_acc_local ( 1 << 0 )
#define vfsmgr_rflag_no_acc_remote ( 1 << 1 )
#define vfsmgr_rflag_no_acc (vfsmgr_rflag_no_acc_local | vfsmgr_rflag_no_acc_remote )

rc_t VFSManagerResolvePath( const VFSManager * self, uint32_t flags,
        const VPath * in_path, VPath ** out_path);
rc_t VFSManagerResolvePathRelative( const VFSManager * self, uint32_t flags,
        const  VPath * base_path, const VPath * in_path, struct VPath ** out_path );

rc_t VFSManagerRegisterObject( VFSManager* self, uint32_t oid, const VPath* obj );
rc_t VFSManagerGetObjectId( const VFSManager* self, const VPath* obj, uint32_t* oid );
rc_t VFSManagerGetObject( const VFSManager* self, uint32_t oid, struct VPath** obj );
rc_t VFSManagerSetAdCaching( VFSManager* self, bool enabled );
bool VFSManagerCheckAd( const VFSManager * self, const VPath * inPath, const VPath ** outPath );
bool VFSManagerCheckEnvAndAd( const VFSManager * self, const VPath * inPath, const VPath ** outPath);
rc_t VFSManagerLogNamesServiceErrors(  VFSManager* self, bool enabled );
rc_t VFSManagerGetLogNamesServiceErrors( VFSManager * self, bool * enabled);

rc_t VFSManagerMakePath( VFSManager const * self, VPath ** new_path, const char * path_str, ... );
rc_t VFSManagerVMakePath( VFSManager const * self, VPath ** new_path, const char * path_fmt, va_list args );
rc_t VFSManagerMakeSysPath( VFSManager const * self, VPath ** new_path, const char * sys_path );
rc_t VFSManagerWMakeSysPath( VFSManager const * self, VPath ** new_path, const wchar_t * wide_sys_path );
rc_t VFSManagerMakeAccPath( VFSManager const * self, VPath ** new_path, const char * acc, ... );
rc_t VFSManagerVMakeAccPath( VFSManager const * self, VPath ** new_path, const char * fmt, va_list args );
rc_t VFSManagerMakeOidPath( VFSManager const * self, VPath ** new_path, uint32_t oid );
rc_t VFSManagerMakePathWithExtension( VFSManager const * self, VPath ** new_path,
        const VPath * orig, const char * extension );
rc_t VFSManagerExtractAccessionOrOID( VFSManager const * self, VPath ** acc_or_oid, const VPath * orig );

rc_t VPathAddRef( const VPath *self );
rc_t VPathRelease( const VPath *self );
bool VPathIsAccessionOrOID( const VPath * self );
bool VPathIsFSCompatible( const VPath * self );
bool VPathFromUri( const VPath * self );
rc_t VPathMarkHighReliability( VPath * self, bool high_reliability );
bool VPathIsHighlyReliable( const VPath * self );
rc_t VPathReadUri( const VPath * self, char * buffer, std::size_t buffer_size, std::size_t * num_read );
rc_t VPathReadScheme( const VPath * self, char * buffer, std::size_t buffer_size, std::size_t * num_read );
rc_t VPathReadAuth( const VPath * self, char * buffer, std::size_t buffer_size, std::size_t * num_read );
rc_t VPathReadHost( const VPath * self, char * buffer, std::size_t buffer_size, std::size_t * num_read );
rc_t VPathReadPortName( const VPath * self, char * buffer, std::size_t buffer_size, std::size_t * num_read );
rc_t VPathReadPath( const VPath * self, char * buffer, std::size_t buffer_size, std::size_t * num_read );
rc_t VPathReadSysPath( const VPath * self, char * buffer, std::size_t buffer_size, std::size_t * num_read );
rc_t VPathReadQuery( const VPath * self, char * buffer, std::size_t buffer_size, std::size_t * num_read );
rc_t VPathReadParam( const VPath * self, const char * param, char * buffer, std::size_t buffer_size, std::size_t * num_read );
rc_t VPathReadFragment( const VPath * self, char * buffer, std::size_t buffer_size, std::size_t * num_read );
rc_t VPathMakeUri( const VPath * self, struct String const ** uri );
rc_t VPathMakeSysPath( const VPath * self, struct String const ** sys_path );
rc_t VPathMakeString( const VPath * self, struct String const ** str );
rc_t VPathGetVdbcache(const VPath * self, const VPath ** vdbcache, bool * vdbcacheChecked );
rc_t VPathGetScheme( const VPath * self, struct String * str );
rc_t VPathGetAuth( const VPath * self, struct String * str );
rc_t VPathGetHost( const VPath * self, struct String * str );
rc_t VPathGetPortName( const VPath * self, struct String * str );
uint16_t VPathGetPortNum( const VPath * self );
rc_t VPathGetPath( const VPath * self, struct String * str );
rc_t VPathGetQuery( const VPath * self, struct String * str );
rc_t VPathGetParam( const VPath * self, const char * param, struct String * str );
rc_t VPathGetFragment( const VPath * self, struct String * str );
uint32_t VPathGetOid( const VPath * self );
rc_t VPathGetId( const VPath * self, struct String * str );
rc_t VPathGetTicket( const VPath * self, struct String * str );
rc_t VPathGetAcc( const VPath * self, struct String * str );
rc_t VPathGetService( const VPath * self, struct String * str );
rc_t VPathGetType( const VPath * self, struct String * str );
rc_t VPathGetObjectType( const VPath * self, struct String * str );
rc_t VPathGetName( const VPath * self, struct String * str );
rc_t VPathGetNameExt( const VPath * self, struct String * str );
rc_t VPathGetCeRequired( const VPath * self, bool * required );
rc_t VPathGetPayRequired( const VPath * self, bool * required );
KTime_t VPathGetModDate( const VPath * self );
uint64_t VPathGetSize( const VPath * self );
const uint8_t * VPathGetMd5( const VPath * self );
bool VPathGetProjectId( const VPath * self, uint32_t * projectId );
// VQuality VPathGetQuality( const VPath * self );

}; // end of extern "C"

class VPathObj;
typedef std::shared_ptr<VPathObj> VPathObjPtr;
class VPathObj {
    private :
        const VPath * f_path;

        VPathObj( const VPath * path ) : f_path( path ) {}

    public :
        static VPathObjPtr make( VFSManager * mgr, const std::string& s ) {
            VPath * p;
            rc_t rc = VFSManagerMakePath( mgr, &p, "%s", s . c_str() );
            if ( 0 == rc ) {
                return VPathObjPtr( new VPathObj( p ) );
            }
            return nullptr;
        }

        VPathObjPtr extract_acc_or_oid( VFSManager * mgr ) const {
            VPath * p;
            rc_t rc = VFSManagerExtractAccessionOrOID( mgr, &p, f_path );
            if ( 0 == rc ) {
                return VPathObjPtr( new VPathObj( p ) );
            }
            return nullptr;
        }

        std::string to_string( void ) const {
            std::string res;
            const String * S;
            rc_t rc = VPathMakeString( f_path, &S );
            if ( 0 == rc ) {
                res = std::string( S -> addr, S -> len );
                StringWhack( S );
            }
            return res;
        }

        ~VPathObj() { VPathRelease( f_path ); }
};

class VFSMgr;
typedef std::shared_ptr<VFSMgr> VFSMgrPtr;
class VFSMgr {
    private :
        VFSManager * f_mgr;

        VFSMgr ( void ) { VFSManagerMake( &f_mgr ); }

        std::string extract_acc_or_oid( const std::string& acc ) {
            std::string res;
            auto input_path = VPathObj::make( f_mgr, acc );
            if ( nullptr != input_path ) {
                auto output_path = input_path -> extract_acc_or_oid( f_mgr );
                if ( nullptr != output_path ) {
                    res = output_path -> to_string();
                }
            }
            return res;
        }

    public :
        static VFSMgrPtr make( void ) { return VFSMgrPtr( new VFSMgr ); }

        static std::string extract_acc( const std::string& acc ) {
            return make() -> extract_acc_or_oid( acc );
        }

        ~VFSMgr( ) { VFSManagerRelease( f_mgr ); }
};

class VDB_Column;
typedef std::shared_ptr< VDB_Column > VDB_Column_Ptr;
class VDB_Column {
    private :
        const std::string f_name;
        VCurPtr f_cur;
        uint32_t f_id;
        bool f_ok;

        VDB_Column( const std::string& name, VCurPtr cur )
            : f_name( name ), f_cur( cur ) {
                f_id = f_cur -> next_col_id( );
                f_ok = f_cur -> add_col( f_id, name . c_str() );
                if ( f_ok ) { f_cur -> inc_next_col_id(); }
        }

    public :
        static VDB_Column_Ptr make( const std::string& name, VCurPtr cur ) {
            return VDB_Column_Ptr( new VDB_Column( name, cur ) );
        }

        operator bool() const { return f_ok; }

        RowRange row_range( void ) const { return f_cur -> row_range( f_id ); }

        template <typename T> bool read( int64_t row, T* v ) {
            bool res = f_ok;
            if ( res ) { res = f_cur -> read< T >( f_id, row, v ); }
            return res;
        }

        template <typename T, typename A> bool read_arr( int64_t row, A* v ) {
            bool res = f_ok;
            if ( res ) { res = f_cur -> read_arr< T, A >( f_id, row, v ); }
            return res;
        }

        bool read_u8( int64_t row, uint8_t* v ) { return read< uint8_t >( row, v ); }
        bool read_u8_arr( int64_t row, uint8_arr * v ) { return read_arr< uint8_t, uint8_arr >( row, v ); }
        bool read_i8( int64_t row, int8_t* v ) { return read< int8_t >( row, v ); }
        bool read_i8_arr( int64_t row, int8_arr * v ) { return read_arr< int8_t, int8_arr >( row, v ); }
        bool read_string_view( int64_t row, std::string_view& v ) {
            char_arr a;
            bool res  = read_arr< char, char_arr >( row, &a );
            if ( res ) { v = std::string_view{ a . data, a . len }; }
            return res;
        }

        bool read_u16( int64_t row, uint16_t* v ) { return read< uint16_t >( row, v ); }
        bool read_u16_arr( int64_t row, uint16_arr * v ) { return read_arr< uint16_t, uint16_arr >( row, v ); }
        bool read_i16( int64_t row, int16_t* v ) { return read< int16_t >( row, v ); }
        bool read_i16_arr( int64_t row, int16_arr * v ) { return read_arr< int16_t, int16_arr >( row, v ); }

        bool read_u32( int64_t row, uint32_t* v ) { return read< uint32_t >( row, v ); }
        bool read_u32_arr( int64_t row, uint32_arr * v ) { return read_arr< uint32_t, uint32_arr >( row, v ); }
        bool read_i32( int64_t row, int32_t* v ) { return read< int32_t >( row, v ); }
        bool read_i32_arr( int64_t row, int32_arr * v ) { return read_arr< int32_t, int32_arr >( row, v ); }

        bool read_u64( int64_t row, uint64_t* v ) { return read< uint64_t >( row, v ); }
        bool read_u64_arr( int64_t row, uint64_arr * v ) { return read_arr< uint64_t, uint64_arr >( row, v ); }
        bool read_i64( int64_t row, int64_t* v ) { return read< int64_t >( row, v ); }
        bool read_i64_arr( int64_t row, int64_arr * v ) { return read_arr< int64_t, int64_arr >( row, v ); }

#ifdef VDB_WRITE
        // set a single default value
        template <typename T> bool write_dflt( const T* v ) {
            return f_cur -> write_dflt< T >( f_id, v );
        }

        bool dflt_u8( uint8_t v ) { return write_dflt< uint8_t >( &v ); }
        bool dflt_i8( int8_t v )  { return write_dflt< int8_t >( &v ); }
        bool dflt_u16( uint16_t v ) { return write_dflt< uint16_t >( &v ); }
        bool dlft_i16( int16_t v )  { return write_dflt< int16_t >( &v ); }
        bool dflt_u32( uint32_t v ) { return write_dflt< uint32_t >( &v ); }
        bool dlft_i32( int32_t v )  { return write_dflt< int32_t >( &v ); }
        bool dflt_u64( uint64_t v ) { return write_dflt< uint64_t >( &v ); }
        bool dflt_i64( int64_t v )  { return write_dflt< int64_t >( &v ); }
        bool dlft_f32( float v ) { return write_dflt< float >( &v ); }
        bool dlft_f64( double v )  { return write_dflt< double >( &v ); }


        // write a single value
        template <typename T> bool write( const T* v ) {
            return f_cur -> write< T >( f_id, v );
        }

        bool write_u8( uint8_t v ) { return write< uint8_t >( &v ); }
        bool write_i8( int8_t v )  { return write< int8_t >( &v ); }
        bool write_u16( uint16_t v ) { return write< uint16_t >( &v ); }
        bool write_i16( int16_t v )  { return write< int16_t >( &v ); }
        bool write_u32( uint32_t v ) { return write< uint32_t >( &v ); }
        bool write_i32( int32_t v )  { return write< int32_t >( &v ); }
        bool write_u64( uint64_t v ) { return write< uint64_t >( &v ); }
        bool write_i64( int64_t v )  { return write< int64_t >( &v ); }
        bool write_f32( float v ) { return write< float >( &v ); }
        bool write_f64( double v )  { return write< double >( &v ); }

        // write multiple values
        template <typename T, typename A> bool write_arr( const A* arr ) {
            return f_cur -> write_arr< T, A >( f_id, arr );
        }

        bool write_u8arr( const uint8_t * v, size_t len ) {
            uint8_arr a{ v, len }; return write_arr< uint8_t, uint8_arr >( &a );
        }
        bool write_u8arr( const uint8_arr_ptr v ) { return write_arr< uint8_t, uint8_arr >( v ); }

        bool write_i8arr( const int8_t * v, size_t len ) {
            int8_arr a{ v, len }; return write_arr< int8_t, int8_arr >( &a );
        }
        bool write_i8arr( const int8_arr_ptr v ) { return write_arr< int8_arr >( v ); }

        bool write_u16arr( uint16_t * v, size_t len ) {
            uint16_arr a{ v, len }; return write_arr< uint16_t, uint16_arr >( &a );
        }
        bool write_u16arr( uint16_arr_ptr v ) { return write_arr< uint16_t, uint16_arr >( v ); }

        bool write_i16arr( const int16_t * v, size_t len ) {
            int16_arr a{ v, len }; return write_arr< int16_t, int16_arr >( &a );
        }
        bool write_i16arr( const int16_arr_ptr v ) { return write_arr< int16_t, int16_arr >( v ); }

        bool write_u32arr( const uint32_t * v, size_t len ) {
            uint32_arr a{ v, len };  return write_arr< uint32_t, uint32_arr >( &a );
        }
        bool write_u32arr( const uint32_arr_ptr v ) { return write_arr< uint32_t, uint32_arr >( v ); }

        bool write_i32arr( const int32_t * v, size_t len ) {
            int32_arr a{ v, len }; return write_arr< int32_t, int32_arr >( &a );
        }
        bool write_i32arr( const int32_arr_ptr v ) { return write_arr< int32_t, int32_arr >( v ); }

        bool write_u64arr( const uint64_t * v, size_t len ) {
            uint64_arr a{ v, len }; return write_arr< uint64_t, uint64_arr >( &a );
        }
        bool write_u64arr( const uint64_arr_ptr v ) { return write_arr< uint64_t, uint64_arr >( v ); }

        bool write_i64arr( const int64_t * v, size_t len ) {
            int64_arr a{ v, len }; return write_arr< int64_t, int64_arr >( &a );
        }
        bool write_i64arr( const int64_arr_ptr v ) { return write_arr< int32_t, int64_arr >( v ); }

        bool write_f32arr( const float * v, size_t len ) {
            f32_arr a{ v, len }; return write_arr< float, f32_arr >( &a );
        }
        bool write_f32arr( const f32_arr_ptr v ) { return write_arr< float, f32_arr >( v ); }

        bool write_f64arr( const double * v, size_t len ) {
            f64_arr a{ v, len }; return write_arr< double, f64_arr >( &a );
        }
        bool write_f64arr( const f64_arr_ptr v ) { return write_arr< double, f64_arr >( v ); }

        bool write_string( const std::string& v ) {
            return f_cur -> WriteString( f_id, v );
        }

#endif

};

}; // end of namespace vdb
