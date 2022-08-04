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

#include "srapath.h" /* FUNCTION_NAMES */
#include "helper.h"

#include <kapp/main.h> /* AsciiToU32 */
#include <klib/log.h>
#include <klib/text.h>
#include <klib/printf.h>
#include <klib/out.h>

rc_t ErrMsg( const char * fmt, ... )
{
    rc_t rc;
    char buffer[ 4096 ];
    size_t num_writ;

    va_list list;
    va_start( list, fmt );
    rc = string_vprintf( buffer, sizeof buffer, &num_writ, fmt, list );
    if ( rc == 0 )
        rc = pLogMsg( klogErr, "$(E)", "E=%s", buffer );
    va_end( list );
    return rc;
} 

rc_t CC ArgsOptionCount( const struct Args * self, const char * option_name, uint32_t * count );
rc_t CC ArgsOptionValue( const struct Args * self, const char * option_name, uint32_t iteration, const void ** value );

const char * get_str_option( const struct Args *args, const char *name, const char * dflt )
{
    const char* res = dflt;
    uint32_t count;
    rc_t rc = ArgsOptionCount( args, name, &count );
    if ( rc == 0 && count > 0 )
    {
        rc = ArgsOptionValue( args, name, 0, (const void**)&res );
        if ( rc != 0 ) res = dflt;
    }
    return res;
}

bool get_bool_option( const struct Args *args, const char *name )
{
    uint32_t count;
    rc_t rc = ArgsOptionCount( args, name, &count );
    return ( rc == 0 && count > 0 );
}


uint64_t get_uint64_t_option( const struct Args * args, const char *name, uint64_t dflt )
{
    uint64_t res = dflt;
    const char * s = get_str_option( args, name, NULL );
    if ( s != NULL )
    {
        size_t l = string_size( s );
        if ( l > 0 )
        {
            char * endptr;
            res = strtol( s, &endptr, 0 );
        }
    }
    return res;
}

uint32_t get_uint32_t_option( const struct Args * args, const char *name, uint32_t dflt )
{
    return ( get_uint64_t_option( args, name, dflt ) & 0xFFFFFFFF );
}

size_t get_size_t_option( const struct Args * args, const char *name, size_t dflt )
{
    size_t res = dflt;
    const char * s = get_str_option( args, name, NULL );
    if ( s != NULL )
    {
        size_t l = string_size( s );
        if ( l > 0 )
        {
            size_t multipl = 1;
            switch( s[ l - 1 ] )
            {
                case 'k' :
                case 'K' : multipl = 1024; break;
                case 'm' :
                case 'M' : multipl = 1024 * 1024; break;
                case 'g' :
                case 'G' : multipl = 1024 * 1024 * 1024; break;
            }
            
            if ( multipl > 1 )
            {
                char * src = string_dup( s, l - 1 );
                if ( src != NULL )
                {
                    char * endptr;
                    res = strtol( src, &endptr, 0 ) * multipl;
                    free( src );
                }
            }
            else
            {
                char * endptr;
                res = strtol( s, &endptr, 0 );
            }
        }
    }
    return res;
}


func_t get_func_t( const char * function )
{
    func_t res = ft_resolve;
    if ( function != NULL && function[ 0 ] != 0 )
    {
        String Function, F_names;
        StringInitCString( &Function, function );
        StringInitCString( &F_names, FUNCTION_NAMES );
        if ( 0 == StringCaseCompare ( &Function, &F_names ) )
            res = ft_names;
        else
        {
            String F_search;
            StringInitCString( &F_search, FUNCTION_SEARCH );
            if ( 0 == StringCaseCompare ( &Function, &F_search ) )
                res = ft_search;
        }
    }
    return res;
}

typedef enum evs{ evs_pre_txt, evs_maj, evs_mid_txt, evs_min, evs_post_txt } evs_t;

static evs_t txt_2_num( String * S, const char * src, evs_t next )
{
    if ( S->addr == NULL ) StringInit( S, src, 0, 0 );
    S->len++;
    S->size++;
    return next;
}

static evs_t num_2_txt( String * S, uint8_t * vers_part, evs_t next )
{
    uint64_t value = StringToU64( S, NULL );
    *vers_part = ( value & 0xFF );
    StringInit( S, NULL, 0, 0 );
    return next;
}

void extract_versionS( ver_maj_min * ver, const String * version_string )
{
    if ( ver != NULL && version_string != NULL )
    {
        evs_t status = evs_pre_txt;
        uint32_t idx = 0;
        String S;

        ver->major = 0;
        ver->minor = 0;
        StringInit( &S, NULL, 0, 0 );
        while( idx < version_string->len )
        {
            const char * src = &version_string->addr[ idx ];
            bool is_num = ( *src >= '0' && *src <= '9' );
            switch( status )
            {
                case evs_pre_txt  : if ( is_num ) status = txt_2_num( &S, src, evs_maj );
                                    break;
                                    
                case evs_maj      : if ( is_num )
                                        { S.len++; S.size++; }
                                    else
                                        status = num_2_txt( &S, &ver->major, evs_mid_txt );
                                    break;
                
                case evs_mid_txt  : if ( is_num ) status = txt_2_num( &S, src, evs_min );
                                    break;
                                    
                case evs_min      : if ( is_num )
                                        { S.len++; S.size++; }
                                    else
                                        status = num_2_txt( &S, &ver->minor, evs_post_txt );
                                    break;
                                    
                case evs_post_txt : break;
            }
            idx++;
        }
        if ( status == evs_maj )
            num_2_txt( &S, &ver->major, evs_maj );
        else if ( status == evs_min )
            num_2_txt( &S, &ver->minor, evs_min );
    }
}

void extract_version( ver_maj_min * ver, const char * version_string )
{
    String S;
    StringInitCString( &S, version_string );
    extract_versionS( ver, &S );
}

pns_ver_t get_ver_t_S( const String * version, pns_ver_t default_version )
{
    pns_ver_t res = default_version;
    if ( version != NULL && version->len > 0 )
    {
        String V_1_0;
        StringInitCString( &V_1_0, "1.0" );
        if ( 0 == StringCaseCompare( version, &V_1_0 ) )
            res = vt_1_0;
        else
        {
            String V_1_1;
            StringInitCString( &V_1_1, "1.1" );
            if ( 0 == StringCaseCompare( version, &V_1_1 ) )
                res = vt_1_1;
            else
            {
                String V_3_0;
                StringInitCString( &V_3_0, "3.0" );
                if ( 0 == StringCaseCompare( version, &V_3_0 ) )
                    res = vt_3_0;
            }
        }
    }
    return res;
}

pns_ver_t get_ver_t( const char * version, pns_ver_t default_version )
{
    pns_ver_t res = default_version;
    if ( version != NULL && version[ 0 ] != 0 )
    {
        String Sversion;
        if ( version[ 0 ] == '#' )
            StringInitCString( &Sversion, &version[ 1 ] );
        else
            StringInitCString( &Sversion, version );
        res = get_ver_t_S( &Sversion, default_version );
    }
    return res;
}


rc_t CC ArgsParamCount( const struct Args * self, uint32_t * count );
rc_t CC ArgsParamValue( const struct Args * self, uint32_t iteration, const void ** value );
rc_t CC ArgsOptionValue ( const struct Args * self, const char * option_name, uint32_t iteration, const void ** value );

rc_t foreach_arg( const struct Args * args,
                  rc_t ( * on_arg )( const char * arg, void * data ),
                  void * data )
{
    uint32_t count;
    rc_t rc = ArgsParamCount( args, &count );
    if ( rc != 0 )
        ErrMsg( "ArgsParamCount() -> %R", rc );
    else
    {
        uint32_t idx;
        for ( idx = 0; rc == 0 && idx < count; ++ idx )
        {
            const char * value;
            rc = ArgsParamValue( args, idx, ( const void ** )&value );
            if ( rc != 0 )
                ErrMsg( "ArgsParamValue( #%d) -> %R", idx, rc );
            else
                rc = on_arg( value, data );
        }
    }
    return rc;
}

rc_t args_to_ptrs( const struct Args * args, const char *** ptr )
{
    uint32_t count;
    rc_t rc = ArgsParamCount( args, &count );
    if ( rc != 0 )
        ErrMsg( "ArgsParamCount() -> %R", rc );
    else
    {
        const char ** x = calloc( count + 1, sizeof *x );
        if ( x == NULL )
        {
            rc = RC( rcExe, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
            ErrMsg( "calloc( %d ) -> %R", count * ( sizeof * x ), rc );
        }
        else
        {
            uint32_t idx;
            for ( idx = 0; rc == 0 && idx < count; ++idx )
            {
                rc = ArgsParamValue( args, idx, ( const void ** )&( x[ idx ] ) );
                if ( rc != 0 )
                    ErrMsg( "ArgsParamValue( #%d) -> %R", idx, rc );
            }
            if ( rc == 0 )
                *ptr = x;
            else
                free( ( void * ) x );
        }
    }
    return rc;

}

static void CC HandleAsciiToIntError ( const char *arg, void *ignore ) {
    rc_t * rc = ignore;

    assert ( rc );

    if ( arg == NULL )
        * rc = RC ( rcApp, rcNumeral, rcConverting, rcString, rcNull );
    else if ( arg [ 0 ] == 0 )
        * rc = RC ( rcApp, rcNumeral, rcConverting, rcString, rcEmpty );
    else
        * rc = RC ( rcApp, rcNumeral, rcConverting, rcString, rcInvalid );
 
    LOGERR ( klogErr, * rc,
        "'--" OPTION_PRJ "' value should be a positive number" );
}

rc_t options_to_nums ( const struct Args * args,
                       const char * name, uint32_t ** ptr )
{
    uint32_t count = 0;
    rc_t rc = ArgsOptionCount ( args, name, & count );
    if ( rc != 0 )
        ErrMsg( "ArgsOptionCount(%s) -> %R", name, rc );
    else {
        uint32_t * x = calloc ( count + 1, sizeof * x );
        if ( x == NULL ) {
            rc = RC ( rcExe, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
            ErrMsg ( "calloc( %d ) -> %R", count * ( sizeof * x ), rc );
        }
        else
        {
            uint32_t idx = 0;
            for ( idx = 0; rc == 0 && idx < count; ++ idx ) {
                const char * v = NULL;
                rc = ArgsOptionValue ( args, name, idx, ( const void ** ) & v );
                if ( rc != 0 )
                    ErrMsg( "ArgsOptionValue( #%d) -> %R", idx, rc );
                else
                    x [ idx ] = AsciiToU32 ( v, HandleAsciiToIntError, & rc );
            }
            if ( rc == 0 )
                * ptr = x;
            else
                free ( ( void * ) x );
        }
    }
    return rc;
}

rc_t foreach_option( const struct Args * args, const char * option_name,
                     rc_t ( * on_option )( const char * arg, void * data ),
                     void * data )
{
    uint32_t count;
    rc_t rc = ArgsOptionCount( args, option_name, &count );
    if ( rc != 0 )
        ErrMsg( "ArgsOptionCount() -> %R", rc );
    else
    {
        uint32_t idx;
        for ( idx = 0; rc == 0 && idx < count; ++ idx )
        {
            const char * value;
            rc = ArgsOptionValue( args, option_name, idx, ( const void ** )&value );
            if ( rc != 0 )
                ErrMsg( "ArgsOptionValue( #%d) -> %R", idx, rc );
            else
                rc = on_option( value, data );
        }
    }
    return rc;
}

static rc_t add_to_namelist( const char * arg, void * data )
{
    VNamelist * dst = data;
    return VNamelistAppend( dst, arg );
}


rc_t args_to_namelist( const struct Args * args, VNamelist ** dst )
{
    rc_t rc = VNamelistMake( dst, 10 );
    if ( rc != 0 )
        ErrMsg( "VNamelistMake() -> %R", rc );
    else
        rc = foreach_arg( args, add_to_namelist, *dst );
    return rc;
}

rc_t options_to_namelist( const struct Args * args, const char * option_name,
                          VNamelist ** dst )
{
    rc_t rc = VNamelistMake( dst, 10 );
    if ( rc != 0 )
        ErrMsg( "VNamelistMake() -> %R", rc );
    else
        rc = foreach_option( args, option_name, add_to_namelist, *dst );
    return rc;
}

rc_t options_to_ptrs( const struct Args * args, const char * option_name, const char *** ptr )
{
    uint32_t count;
    rc_t rc = ArgsOptionCount( args, option_name, &count );
    if ( rc != 0 )
        ErrMsg( "ArgsOptionCount() -> %R", rc );
    else
    {
        const char ** x = calloc( count + 1, sizeof *x );
        if ( x == NULL )
        {
            rc = RC( rcExe, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
            ErrMsg( "calloc( %d ) -> %R", count * ( sizeof * x ), rc );
        }
        else
        {
            uint32_t idx;
            for ( idx = 0; rc == 0 && idx < count; ++idx )
            {
                rc = ArgsOptionValue( args, option_name, idx, ( const void ** )&( x[ idx ] ) );
                if ( rc != 0 )
                    ErrMsg( "ArgsOptionValue( #%d) -> %R", idx, rc );
            }
            if ( rc == 0 )
                *ptr = x;
            else
                free( ( void * ) x );
        }
    }
    return rc;
}

rc_t foreach_item( const VNamelist * src, 
                   rc_t ( * on_item )( const char * item, void * data ),
                   void * data )
{
    uint32_t count;
    rc_t rc = VNameListCount( src, &count );
    if ( rc == 0 )
    {
        uint32_t idx;
        for ( idx = 0; idx < count && rc == 0; ++idx )
        {
            const char * value;
            rc = VNameListGet( src, idx, &value );
            if ( rc == 0 && value != NULL )
                rc = on_item( value, data );
        }
    }
    return rc;
}

rc_t foreach_token( const String * line,
                    token_handler_t token_handler,
                    char delim,
                    void * data )
{
    rc_t rc = 0;
    String token;
    uint32_t idx, id;
    
    token.addr = line->addr;
    token.len = token.size = 0;
    for ( idx = 0, id = 0; rc == 0 && idx < line->len; ++idx )
    {
        if ( line->addr[ idx ] == delim )
        {
            rc = token_handler( &token, id++, data );
            token.len = token.size = 0;
        }
        else
        {
            if ( token.len == 0 )
                token.addr = &line->addr[ idx ];
            token.len++;
            token.size++;
        }
    }
    if ( token.len > 0 )
        rc = token_handler( &token, id, data );
    return rc;
}