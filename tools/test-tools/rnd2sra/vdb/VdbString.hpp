#pragma once

#include <cstdint>

namespace vdb {

extern "C" {

typedef uint32_t rc_t;

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

}; // end of extern "C"

}; // end of namespace vdb
