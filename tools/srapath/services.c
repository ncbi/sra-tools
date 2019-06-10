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
* ==============================================================================
*
*/


#include "srapath.h" /* OPTION_PARAM */
#include "services.h" /* names_request */

#include <klib/log.h> /* LOGERR */
#include <klib/out.h> /* OUTMSG */
#include <klib/time.h> /* KTimeIso8601 */
#include <vfs/path.h> /* VPath */
#include <vfs/services-priv.h> /* KServiceNamesExecuteExt */


#define DISP_RC(rc, err) (void)((rc == 0) ? 0 : LOGERR(klogErr, rc, err))

#define RELEASE(type, obj) do { rc_t rc2 = type##Release(obj); \
    if (rc2 != 0 && rc == 0) { rc = rc2; } obj = NULL; } while (false)


typedef struct { const VRemoteProtocols p; const char * const n; size_t s; }
TProtocol;


static const TProtocol PROTOCOLS [] = {
    { eProtocolHttps, "https", sizeof "https" - 1 },
    { eProtocolHttp , "http" , sizeof "http"  - 1 },
    { eProtocolFasp , "fasp" , sizeof "fasp"  - 1 },
    { eProtocolFile , "file" , sizeof "file"  - 1 },
    { eProtocolS3   , "s3"   , sizeof "s3"    - 1 },
    { eProtocolGS   , "gs"   , sizeof "gs"    - 1 },
};


static bool printCommon ( const VPath * path, const KSrvError * error ) {
    bool printed = false;

    String tmp = { NULL, 0, 0 };

    if ( path != NULL ) {
        const uint8_t * m = NULL;
        size_t s = 0;
        KTime_t t = 0;
        rc_t r2 = VPathGetId ( path, & tmp );
        OUTMSG ( ( " ObjectId=\"" ) );
        if ( r2 == 0 )
            OUTMSG ( ( "%S", & tmp ) );
        else
            OUTMSG ( ( "%R", r2 ) );
        OUTMSG ( ( "\"" ) );
        s = VPathGetSize ( path );
        if ( s != 0 )
            OUTMSG ( ( ", size=%zu", s ) );
        t = VPathGetModDate ( path );
        if ( t != 0 ) {
            char c [ 64 ];
            int s = KTimeIso8601 ( t, c, sizeof c );
            OUTMSG ( ( ", modified=" ) );
            if ( s != 0 )
                OUTMSG ( ( "\"%.*s\"", s, c ) );
        }
        m = VPathGetMd5 ( path );
        if ( m != NULL ) {
            int i = 0;
            OUTMSG ( ( ", md5=" ) );
            for ( i = 0; i < 16; ++i ) {
                OUTMSG ( ( "%02x", m [ i ] ) );
            }
        }

        printed = true;
    }

    else if ( error != NULL ) {
        rc_t r3;
        EObjectType type = eOT_undefined;
        uint32_t code = 0;
        rc_t r2 = KSrvErrorObject  ( error, & tmp, & type );
        OUTMSG ( ( " Error = { ObjectId=\"" ) );
        if ( r2 == 0 )
            OUTMSG ( ( "%S\"", & tmp ) );
        else
            OUTMSG ( ( "%R\"", r2 ) );
        r2 = KSrvErrorCode ( error, & code );
        if ( r2 == 0 )
            OUTMSG ( ( ", code=%u", code ) );
        else
            OUTMSG ( ( ", code=%R", r2 ) );
        r2 = KSrvErrorMessage ( error, & tmp );
        if ( r2 == 0 )
            OUTMSG ( ( ", message=\"%S\"", & tmp ) );
        else
            OUTMSG ( ( ", message=\"%R\"", r2 ) );
        r2 = KSrvErrorRc ( error, & r3 );
        if ( r2 == 0 )
            OUTMSG ( ( ", rc=%R }", r3 ) );
        else
            OUTMSG ( ( ", cannot get rc:%R }", r2 ) );

        printed = true;
    }

    return printed;
}


static
rc_t KSrvResponse_Print ( const KSrvResponse * self, bool cache, bool pPath )
{
    rc_t rc = 0;
    rc_t re = 0;
    uint32_t i = 0;
    uint32_t l = KSrvResponseLength  ( self );

    if ( ! pPath )
        OUTMSG ( ( "%u #\n\n", l ) );

    for ( i = 0; i < l; ++ i ) {
        bool printed = false;
        int j = 0;
        if ( ! pPath )
            OUTMSG ( ( "#%u {", i ) );
        for ( j = 0; j < sizeof PROTOCOLS / sizeof PROTOCOLS [ 0 ];
              ++ j )
        {
            const TProtocol * p = & PROTOCOLS [ j ];
            const VPath * path = NULL;
            const VPath * vdbcache = NULL;
            const KSrvError * error = NULL;
            rc_t r2 = KSrvResponseGetPath ( self,
                i, p -> p, & path, & vdbcache, & error );
            if ( r2 != 0 )
                PLOGERR ( klogErr, ( klogErr, r2, "Cannot get response path"
                    "($(i), $(s))", "i=%u,s=%s", i, p -> n ) );
            else {
                if ( ! ( pPath || printed ) )
                    printed = printCommon ( path, error );
                if ( error != NULL ) {
                    rc_t r = 0;
                    KSrvErrorRc ( error, & r );
                    assert ( r );
                    if ( re == 0 )
                        re = r;
                }
                if ( path != NULL ) {
                    const String * tmp = NULL;
                    r2 = VPathMakeString ( path, & tmp );
                    if ( ! pPath || r2 != 0 )
                        OUTMSG ( ( "\n\t%s: ", p -> n ) );
                    if ( r2 == 0 )
                        if ( pPath ) {
                            if ( printed )
                                OUTMSG ( ( "\n" ) );
                            OUTMSG ( ( "%S", tmp ) );
                            printed = true;
                        }
                        else
                            OUTMSG ( ( "path=\"%S\"", tmp ) );
                    else
                        OUTMSG ( ( "%R", r2 ) );
                    free ( ( void * ) tmp );
                }
                if ( vdbcache != NULL ) {
                    const String * tmp = NULL;
                    r2 = VPathMakeString ( vdbcache, & tmp );
                    if ( r2 == 0 )
                        if ( pPath )
                            OUTMSG ( ( "\n%S", tmp ) );
                        else
                            OUTMSG ( ( " vdbcache=\"%S\"", tmp ) );
                    else
                        OUTMSG ( ( "%R", r2 ) );
                    free ( ( void * ) tmp );
                }
            }
            RELEASE ( VPath, path );
            RELEASE ( VPath, vdbcache );
            if ( error != NULL ) {
                RELEASE ( KSrvError, error );
                break;
            }
        }

        if ( cache & ! pPath ) {
            const VPath * cache = NULL;
            rc_t r2 = KSrvResponseGetCache ( self, i, & cache );
            OUTMSG ( ( "\n     Cache=" ) );
            if ( r2 != 0 )
                OUTMSG ( ( "%R\n", r2 ) );
            else {
                const String * tmp = NULL;
                r2 = VPathMakeString ( cache, & tmp );
                if ( r2 == 0 )
                    OUTMSG ( ( "\"%S\"\n", tmp ) );
                else
                    OUTMSG ( ( "\"%R\"\n", r2 ) );
                free ( ( void * ) tmp );
            }
            RELEASE ( VPath, cache );
        }

        if ( ! pPath )
            OUTMSG ( ( " }" ) );
        OUTMSG ( ( "\n" ) );
    }

    if ( rc == 0 && re != 0 )
        rc = re;
    return rc;
}


static rc_t names_remote ( KService * service,
    VRemoteProtocols protocols, const request_params * request, bool path )
{
    rc_t rc = 0;

    const KSrvResponse * response = NULL;

    rc = KServiceNamesExecuteExt ( service, protocols,
        request -> names_url, request -> names_ver, & response );
    if ( rc != 0 )
        OUTMSG ( ( "Error: %R\n", rc ) );
    else
        rc = KSrvResponse_Print ( response, false, path );

    RELEASE ( KSrvResponse, response );

    return rc;
}


static rc_t names_remote_cache ( KService * service,
    VRemoteProtocols protocols, const request_params * request, bool path )
{
    rc_t rc = 0;

    const KSrvResponse * response = NULL;

    assert ( request );

    rc = KServiceNamesQueryExt ( service, protocols, request -> names_url,
                                 request -> names_ver, NULL, NULL, & response );
    if ( rc != 0 )
        OUTMSG ( ( "Error: %R\n", rc ) );
    else
        rc = KSrvResponse_Print ( response, true, path );

    RELEASE ( KSrvResponse, response );

    return rc;
}

static void json_print_nvp(  char const *const name
                           , char const *const value
                           , bool const comma)
{
    OUTMSG(("%.*s\"%s\": \"%s\""
            , comma ? 2 : 0, ", "
            , name
            , value));
}

static void json_print_nVp(  char const *const name
                           , String const *const value
                           , bool const comma)
{
    OUTMSG(("%.*s\"%s\": \"%S\""
            , comma ? 2 : 0, ", "
            , name
            , value));
}

static void json_print_nzp(  char const *const name
                           , size_t const value
                           , bool const comma)
{
    OUTMSG(("%.*s\"%s\": %zu"
            , comma ? 2 : 0, ", "
            , name
            , value));
}

static void json_print_named_url(  char const *const name
                                 , VPath const *const url
                                 , bool const comma)
{
    String const *tmp = NULL;
    rc_t rc = VPathMakeString(url, &tmp); assert(rc == 0);
    OUTMSG(("%.*s\"%s\": \"%S\""
            , comma ? 2 : 0, ", "
            , name
            , tmp));
    free((void *)tmp);
}

static unsigned json_print_named_urls_and_service(  char const *const name
                                                  , VPath const *const url
                                                  , unsigned const count
                                                  , bool const comma)
{
    if (url) {
        bool ce = false;
        bool pay = false;
        String service;
        String const *tmp = NULL;
        rc_t rc = VPathMakeString(url, &tmp); assert(rc == 0);

        memset(&service, 0, sizeof service);
        VPathGetService(url, &service);

        VPathGetCeRequired(url, &ce);
        VPathGetPayRequired(url, &pay);

        if (count == 0) {
            OUTMSG(("%.*s\"%s\": ["
                    , comma ? 2 : 0, ", "
                    , name));
        }
        OUTMSG(("%.*s{ ", count > 0 ? 2 : 0, ",\n"));
        json_print_nVp("path", tmp, false);
        json_print_nVp("service", &service, true);
        json_print_nvp("CE-Required", ce ? "true" : "false", true);
        json_print_nvp("Payment-Required", pay ? "true" : "false", true);
        OUTMSG((" }"));

        free((void *)tmp);
        return count + 1;
    }
    else if (count > 0) {
        /* close json array */
        OUTMSG(("]"));
    }
    return count;
}

static unsigned json_print_response_file(KSrvRespFile const *const file, unsigned count)
{
    if (count == 0) {
        OUTMSG(("[\n"));
    }
    if (file) {
        rc_t rc = 0;
        VPath const *path = NULL;
        char const *str = NULL;
        uint32_t id = 0;
        uint64_t sz = 0;
        KSrvRespFileIterator *iter = NULL;
        
        str = NULL; rc = KSrvRespFileGetAccOrId(file, &str, &id); assert(rc == 0);
        OUTMSG(("%.*s{", count == 0 ? 0 : 2, ",\n"));
        json_print_nvp("accession", str, false);
        count += 1;

        str = NULL; rc = KSrvRespFileGetType(file, &str);
        if (str && str[0])
            json_print_nvp("itemType", str, true);

        str = NULL; rc = KSrvRespFileGetClass(file, &str);
        if (rc == 0 && str && str[0])
            json_print_nvp("itemClass", str, true);
        
        rc = KSrvRespFileGetSize(file, &sz);
        if (rc == 0 && sz > 0)
            json_print_nzp("size", (size_t)sz, true);

        path = NULL; KSrvRespFileGetLocal(file, &path);
        if (path) {
            json_print_named_url("local", path, true);
            RELEASE(VPath, path);
        }
        
        path = NULL; KSrvRespFileGetCache(file, &path);
        if (path) {
            json_print_named_url("cache", path, true);
            RELEASE(VPath, path);
        }
        
        rc = KSrvRespFileMakeIterator(file, &iter);
        if (rc == 0 && iter) {
            unsigned rcount = 0;
            
            for ( ; ; ) {
                if ((rc = KSrvRespFileIteratorNextPath(iter, &path)) == 0) {
                    rcount = json_print_named_urls_and_service("remote", path, rcount, true);
                    if (path == NULL)
                        break;
                    RELEASE(VPath, path); path = NULL;
                }
            }
            RELEASE(KSrvRespFileIterator, iter);
        }
        OUTMSG(("}"));
    }
    else {
        /* last; close json array */
        OUTMSG(("\n]\n"));
    }
    return count;
}

static rc_t names_remote_json ( KService * const service
                               , VRemoteProtocols const protocols
                               , request_params const * const request
                               , bool const path )
{
    rc_t rc = 0;
    KSrvResponse const * response = NULL;
    
    rc = KServiceNamesQueryExt( service, protocols, request -> names_url,
        request -> names_ver, NULL, NULL, & response );
    if ( rc != 0 )
        OUTMSG ( ( "Error: %R\n", rc ) );
    else {
        unsigned const count = KSrvResponseLength(response);
        unsigned i;
        unsigned output_count = 0;
        
        for (i = 0; i < count; ++i) {
            KSrvRespObj const *obj = NULL;
            KSrvRespObjIterator *iter = NULL;
            
            rc = KSrvResponseGetObjByIdx(response, i, &obj);
            assert(rc == 0); /* if everything was done right, should not be fail-able */
            rc = KSrvRespObjMakeIterator(obj, &iter);
            assert(rc == 0); /* if everything was done right, should not be fail-able */
            
            for ( ; ; ) { /* iterate files */
                KSrvRespFile *file = NULL;
                
                rc = KSrvRespObjIteratorNextFile(iter, &file);
                assert(rc == 0); /* if everything was done right, should not be fail-able */
                
                if (file == NULL) /* done iterating */
                    break;
                output_count = json_print_response_file(file, output_count);
                RELEASE(KSrvRespFile, file);
            }
            RELEASE(KSrvRespObjIterator, iter);
            RELEASE(KSrvRespObj, obj);
        }
        output_count = json_print_response_file(NULL, output_count);
    }
    RELEASE ( KSrvResponse, response );
    
    return rc;
}


static rc_t KService_Make ( KService ** self, const request_params * request ) {
    rc_t rc = 0;

    assert ( request && request -> terms );

    rc = KServiceMake ( self );

    if ( rc == 0 ) {
        const char ** id = NULL;

        assert ( * self );

        for ( id = request -> terms; * id != NULL && rc == 0; ++ id)
            rc = KServiceAddId ( * self, * id );
    }

    return rc;
}


static
VRemoteProtocols parseProtocol ( const char * protocol, rc_t * prc )
{
    VRemoteProtocols protocols = 0;

    int n = 0;
    int j = 0;

    uint32_t sz = 0;

    assert ( protocol && prc );

    sz = string_measure ( protocol, NULL );

    while ( sz > 0 ) {
        bool found = false;
        int l = sz;

        char * c = string_chr ( protocol, sz, ',' );
        if ( c != NULL )
            l = c - protocol;

        for ( j = 0;
                j < sizeof PROTOCOLS / sizeof PROTOCOLS [ 0 ]; ++ j )
        {
            const TProtocol * p = & PROTOCOLS [ j ];
            uint32_t max_chars = p -> s > l ? p -> s : l;
            if (string_cmp ( protocol, l, p -> n, p -> s, max_chars ) == 0) {
                VRemoteProtocols next = p -> p << ( 3 * n );
                protocols = next + protocols;
                ++n;

                assert ( l >= p -> s );

                protocol += p -> s;
                sz -= p -> s;

                found = true;
                break;
            }
        }

        if ( ! found ) {
            * prc = RC ( rcExe, rcArgv, rcParsing, rcParam, rcInvalid );
            PLOGERR ( klogErr, ( klogErr, * prc, "Bad protocol '$(p)'",
                "p=%.*s", l, protocol ) );
            break;
        }

        if ( c != NULL ) {
            ++ protocol;
            -- sz;
        }
    }

    return protocols;
}

static rc_t names_request_1 (  const request_params * request
                             , bool path
                             , rc_t (*func)(  KService * service
                                            , VRemoteProtocols protocols
                                            , const request_params * request
                                            , bool path )
                             )
{
    rc_t rc = 0;
    
    KService * service = NULL;
    
    VRemoteProtocols protocols = 0;
    
    assert ( request && request -> terms );
    
    if ( sizeof PROTOCOLS / sizeof PROTOCOLS [ 0 ] != eProtocolMax ) {
        rc = RC ( rcExe, rcTable, rcValidating, rcTable, rcIncomplete );
        LOGERR ( klogFatal, rc, "Incomplete PROTOCOLS array" );
        assert ( sizeof PROTOCOLS / sizeof PROTOCOLS [ 0 ] == eProtocolMax );
        return rc;
    }
    
    if ( request -> params != NULL && * request -> params != NULL )
        LOGMSG ( klogWarn, "'--" OPTION_PARAM "' is ignored: "
                "it used just with '--" OPTION_RAW "' '" FUNCTION_NAMES "' and '"
                FUNCTION_SEARCH "' funtions") ;
    
    rc = KService_Make ( & service, request );
    
    if ( rc == 0 ) {
        if ( rc == 0 && request -> projects != NULL &&
            request -> projects [ 0 ] != 0 )
        {
            uint32_t * ptr = NULL;
            for ( ptr = request -> projects; * ptr != 0; ++ ptr ) {
                rc = KServiceAddProject ( service, * ptr );
                if ( rc != 0 ) {
                    PLOGERR ( klogErr, ( klogErr, rc,
                                        "Cannot add project '$(p)'", "p=%u", * ptr ) );
                    break;
                }
            }
        }
    }
    
    if ( rc == 0 && request -> proto != NULL )
        protocols = parseProtocol ( request -> proto, & rc );
    
    if ( rc == 0 ) {
        rc = func( service, protocols, request, path );
    }
    
    RELEASE ( KService, service );
    
    return rc;
}

rc_t names_request ( const request_params * request, bool cache, bool path )
{
    if (cache)
        return names_request_1(request, path, names_remote_cache);
    else
        return names_request_1(request, path, names_remote      );
}

rc_t names_request_json ( const request_params * request, bool cache, bool path )
{
    return names_request_1(request, path, names_remote_json);
}

static void KartItem_Print ( const KartItem * self ) {
    rc_t r2 = 0;
    const String * elem = NULL;

    OUTMSG ( ( "{ proj-id=\"" ) );
    r2 = KartItemProjId ( self, & elem);
    if ( r2 != 0 )
        OUTMSG ( ( "%R", r2 ) );
    else if ( elem == NULL )
        OUTMSG ( ( "NULL" ) );
    else
        OUTMSG ( ( "%S", elem ) );

    OUTMSG ( ( "\", obj-type=\"" ) );
    r2 = KartItemObjType ( self, & elem);
    if ( r2 != 0 )
        OUTMSG ( ( "%R", r2 ) );
    else if ( elem == NULL )
        OUTMSG ( ( "NULL" ) );
    else
        OUTMSG ( ( "%S", elem ) );

    OUTMSG ( ( "\", item-id=\"" ) );
    r2 = KartItemItemId ( self, & elem);
    if ( r2 != 0 )
        OUTMSG ( ( "%R", r2 ) );
    else if ( elem == NULL )
        OUTMSG ( ( "NULL" ) );
    else
        OUTMSG ( ( "%S", elem ) );

    OUTMSG ( ( "\", name=\"" ) );
    r2 = KartItemName ( self, & elem);
    if ( r2 != 0 )
        OUTMSG ( ( "%R", r2 ) );
    else if ( elem == NULL )
        OUTMSG ( ( "NULL" ) );
    else
        OUTMSG ( ( "%S", elem ) );

    OUTMSG ( ( "\", path=\"" ) );
    r2 = KartItemPath ( self, & elem);
    if ( r2 != 0 )
        OUTMSG ( ( "%R", r2 ) );
    else if ( elem == NULL )
        OUTMSG ( ( "NULL" ) );
    else
        OUTMSG ( ( "%S", elem ) );

    OUTMSG ( ( "\", size=\"" ) );
    r2 = KartItemSize ( self, & elem);
    if ( r2 != 0 )
        OUTMSG ( ( "%R", r2 ) );
    else if ( elem == NULL )
        OUTMSG ( ( "NULL" ) );
    else
        OUTMSG ( ( "%S", elem ) );

    OUTMSG ( ( "\", item-desc=\"" ) );
    r2 = KartItemItemDesc ( self, & elem);
    if ( r2 != 0 )
        OUTMSG ( ( "%R", r2 ) );
    else if ( elem == NULL )
        OUTMSG ( ( "NULL" ) );
    else
        OUTMSG ( ( "%S", elem ) );

    OUTMSG ( ( "\" }\n" ) );
}


rc_t search_request ( const request_params * request ) {
    KService * service = NULL;
    const struct Kart * response = NULL;

    rc_t rc = KService_Make ( & service, request );

    assert ( request );

    if ( request -> params != NULL && * request -> params != NULL )
        LOGMSG ( klogWarn, "'--" OPTION_PARAM "' is ignored: "
            "it used just with '--" OPTION_RAW "' '" FUNCTION_NAMES "' and '"
            FUNCTION_SEARCH "' funtions") ;

    if ( rc == 0 )
        rc = KServiceSearchExecuteExt ( service, request -> search_url,
                                        request -> search_ver, & response );

    if ( rc == 0 ) {
        const KartItem * item = NULL;
        while ( true ) {
            rc = KartMakeNextItem ( response, & item );
            if ( rc != 0 || item == NULL )
                break;

            KartItem_Print ( item );
            RELEASE ( KartItem, item );
        }
    }

    RELEASE ( Kart, response );
    RELEASE ( KService, service );

    return rc;
}
