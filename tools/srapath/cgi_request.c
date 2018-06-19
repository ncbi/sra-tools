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

#include "cgi_request.h"
#include "helper.h"
#include "repository-mgr.h" /* KRepositoryMgr_DownloadTicket */
#include "line_iter.h"

#include <klib/debug.h> /* DBGMSG */
#include <klib/log.h> /* PLOGERR */
#include <klib/text.h>
#include <klib/namelist.h>
#include <klib/printf.h>
#include <klib/vector.h>
#include <klib/out.h>
#include <kns/manager.h>
#include <kns/http.h>
#include <kns/stream.h>

#include <string.h>

typedef struct cgi_request
{
    char * url;
    KNSManager * kns_mgr;
    VNamelist * params;
} cgi_request;


#define RELEASE(type, obj) do { rc_t rc2 = type##Release(obj); \
    if (rc2 && !rc) { rc = rc2; } obj = NULL; } while (false)


void release_cgi_request( struct cgi_request * request )
{
    if ( request != NULL )
    {
        if ( request->kns_mgr != NULL )
            KNSManagerRelease( request->kns_mgr );
        if ( request->params != NULL )
            VNamelistRelease( request->params );
        if ( request->url != NULL )
            free( ( void * ) request->url );
        free( ( void * ) request );
    }
}


rc_t make_cgi_request( struct cgi_request ** request, const char * url )
{
    rc_t rc = 0;
    cgi_request * r = calloc( 1, sizeof * r );
    if ( r == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        ErrMsg( "calloc( %d ) -> %R", ( sizeof * r ), rc );
    }
    else
    {
        r->url = string_dup_measure( url, NULL );
        if ( r->url == NULL )
        {
            rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
            ErrMsg( "string_dup_measure( '%s' ) -> %R", url, rc );
        }
        else
        {
            DBGMSG ( DBG_APP, DBG_FLAG ( DBG_APP_1 ), ( "%s\n", r -> url ) );
            rc = VNamelistMake( &r->params, 10 );
            if ( rc != 0 )
                ErrMsg( "VNamelistMake() -> %R", rc );
            else
            {
                rc = KNSManagerMake( &r->kns_mgr );
                if ( rc != 0 )
                    ErrMsg( "KNSManagerMake() -> %R", rc );
                else
                    *request = r;
            }
        }
        if ( rc != 0 )
            release_cgi_request( r );
    }
    return rc;
}


rc_t add_cgi_request_param( struct cgi_request * request, const char * fmt, ... )
{
    rc_t rc;
    if ( request == NULL || fmt == NULL )
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcNull );
    else
    {
        va_list args;
        char buffer[ 4096 ];
        size_t num_writ;
        
        va_start ( args, fmt );
        rc = string_vprintf( buffer, sizeof buffer, &num_writ, fmt, args );
        if ( rc != 0 )
            ErrMsg( "string_vprintf( '%s' ) -> %R", fmt, rc );
        else
        {
            DBGMSG ( DBG_APP, DBG_FLAG ( DBG_APP_1 ), ( "\t%s\n", buffer ) );
            rc = VNamelistAppend( request->params, buffer );
            if ( rc != 0 )
                ErrMsg( "VNamelistAppend( '%s' ) -> %R", buffer, rc );
        }   
        va_end ( args );
    }
    return rc;
}


typedef struct add_ctx
{
    struct cgi_request * request;
    const char * fmt;
} add_ctx;

static rc_t add_item( const char * item, void * data )
{
    add_ctx * actx = data;
    return add_cgi_request_param( actx->request, actx->fmt, item );
}

rc_t add_cgi_request_params( struct cgi_request * request, const char * fmt, const VNamelist * src )
{
    rc_t rc;
    if ( request == NULL || fmt == NULL || src == NULL )
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcNull );
    else
    {
        add_ctx actx = { request, fmt };
        rc = foreach_item( src, add_item, &actx );
    }
    return rc;
}

rc_t clear_cgi_request_params( struct cgi_request * request )
{
    rc_t rc;
    if ( request == NULL )
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcNull );
    else
    {
        if ( request->params != NULL )
        {
            rc = VNamelistRelease( request->params );
            if ( rc == 0 )
                rc = VNamelistMake( &request->params, 10 );
        }
    }
    return rc;
}

char * get_url_of_cgi_request( struct cgi_request * request )
{
    if ( request != NULL )
        return request->url;
    return NULL;
}

rc_t perform_cgi_request( struct cgi_request * request,
                          struct KStream ** reply_stream,
                          uint32_t * rslt_code )
{
    rc_t rc;
    if ( request == NULL || reply_stream == NULL || rslt_code == NULL )
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcNull );
    else
    {
        ver_t http_vers = 0x01010000;
        KClientHttpRequest * kns_req;

        *reply_stream = NULL;
        rc = KNSManagerMakeClientRequest( request->kns_mgr, &kns_req, http_vers, NULL, "%s", request->url );
        if ( rc != 0 )
            ErrMsg( "KNSManagerMakeClientRequest( '%s' ) -> %R", request->url, rc );
        else
        {
            uint32_t count;
            rc = VNameListCount( request->params, &count );
            if ( rc != 0 )
                ErrMsg( "VNameListCount() -> %R", rc );
            else
            {
                uint32_t idx;
                for ( idx = 0; idx < count && rc == 0; ++idx )
                {
                    const char * s;
                    rc = VNameListGet( request->params, idx, &s );
                    if ( rc != 0 )
                        ErrMsg( "VNameListGet( #%d ) -> %R", idx, rc );
                    else
                    {
                        rc = KClientHttpRequestAddPostParam( kns_req, "%s", s );
                        if ( rc != 0 )
                            ErrMsg( "KClientHttpRequestAddPostParam( '%s' ) -> %R", s, rc );
                    }
                }
                if ( rc == 0 )
                {
                    KClientHttpResult * kns_rslt;
                    rc = KClientHttpRequestPOST( kns_req, &kns_rslt );
                    if ( rc != 0 )
                        ErrMsg( "KClientHttpRequestPOST() -> %R", rc );
                    else
                    {
                        rc = KClientHttpResultStatus( kns_rslt, rslt_code, NULL, 0, NULL );
                        if ( rc != 0 )
                            ErrMsg( "KClientHttpResultStatus() -> %R", rc );
                        else
                        {
                            rc = KClientHttpResultGetInputStream( kns_rslt, reply_stream );
                            if ( rc != 0 )
                                ErrMsg( "KClientHttpResultGetInputStream() -> %R", rc );
                        }
                        KClientHttpResultRelease( kns_rslt );
                    }
                }
            }
            KClientHttpRequestRelease( kns_req );
        }
    }
    return rc;
}


/* ------------------------------------------------------------------------ */

static void CC free_reply_obj( void * obj, void * data )
{
    if ( obj != NULL )
    {
        reply_obj * o = obj;
        if ( o->prj_id != NULL ) StringWhack( o->prj_id );        
        if ( o->obj_type != NULL ) StringWhack( o->obj_type );
        if ( o->id != NULL ) StringWhack( o->id );
        if ( o->name != NULL ) StringWhack( o->name );
        if ( o->date != NULL ) StringWhack( o->date );
        if ( o->checksum != NULL ) StringWhack( o->checksum );
        if ( o->ticket != NULL ) StringWhack( o->ticket );
        if ( o->path != NULL ) StringWhack( o->path );
        if ( o->message != NULL ) StringWhack( o->message );
        free( obj );
    }
}

static rc_t clone_reply_obj( const reply_obj * src, reply_obj ** dst )
{
    rc_t rc = 0;
    reply_obj * o = calloc( 1, sizeof * o );
    if ( o == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        ErrMsg( "calloc( %d ) -> %R", ( sizeof * o ), rc );
    }
    else
    {
        if ( src->prj_id != NULL ) rc = StringCopy( &o->prj_id, src->prj_id );    
        if ( rc == 0 && src->obj_type != NULL ) rc = StringCopy( &o->obj_type, src->obj_type );
        if ( rc == 0 && src->id != NULL ) rc = StringCopy( &o->id, src->id );
        if ( rc == 0 && src->name != NULL ) rc = StringCopy( &o->name, src->name );
        if ( rc == 0 && src->date != NULL ) rc = StringCopy( &o->date, src->date );
        if ( rc == 0 && src->checksum != NULL ) rc = StringCopy( &o->checksum, src->checksum );
        if ( rc == 0 && src->ticket != NULL ) rc = StringCopy( &o->ticket, src->ticket );
        if ( rc == 0 && src->path != NULL ) rc = StringCopy( &o->path, src->path );
        if ( rc == 0 && src->message != NULL ) rc = StringCopy( &o->message, src->message );
        
        o->version = src->version;
        o->size = src->size;
        o->code = src->code;
        
        if ( rc == 0 )
            *dst = o;
        else
            free_reply_obj( o, NULL );
    }
    return rc;
}


typedef struct reply_obj_list
{
    Vector v;
} reply_obj_list;


void release_reply_obj_list( struct reply_obj_list * list )
{
    VectorWhack( &list->v, free_reply_obj, NULL );
}

static rc_t make_reply_obj_list( struct reply_obj_list ** list )
{
    rc_t rc = 0;
    reply_obj_list * l = calloc( 1, sizeof * l );
    if ( l == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        ErrMsg( "calloc( %d ) -> %R", ( sizeof * l ), rc );
    }
    else
    {
        VectorInit( &l->v, 0, 5 );
        *list = l;
    }
    return rc;
}

static rc_t add_clone_to_reply_obj_list( reply_obj_list * list, const reply_obj * src )
{
    reply_obj * clone;
    rc_t rc = clone_reply_obj( src, &clone );
    if ( rc == 0 )
        rc = VectorAppend( &list->v, NULL, clone );
    return rc;
}

static rc_t add_reply_to_list_cb( const reply_obj * obj, void * data )
{
    reply_obj_list * l = data;
    return add_clone_to_reply_obj_list( l, obj );
}

typedef struct reply_obj_ctx
{
    on_reply_obj_t on_obj;
    void * data;
    rc_t rc;
} reply_obj_ctx;

static void CC reply_obj_cb( void * item, void * data )
{
    reply_obj_ctx * rctx = data;
    if ( rctx->rc == 0 )
        rctx->rc = rctx->on_obj( item, rctx->data );
}

rc_t foreach_reply_obj( struct reply_obj_list * list,
                        on_reply_obj_t on_obj,
                        void * data )
{
    reply_obj_ctx rctx = { on_obj, data, 0 };
    VectorForEach( &list->v, false, reply_obj_cb, &rctx );
    return rctx.rc;
}


static int64_t find_obj_cmp( const void *key, const void *n )
{
    const String * id = key;
    const reply_obj * obj = n;
    return StringCompare( id, obj->id );
}

static reply_obj * find_obj( struct reply_obj_list * list, const String * id )
{
    return VectorFind( &list->v, id, NULL, find_obj_cmp );
}

typedef struct reply_parse_ctx
{
    on_reply_obj_t on_obj;
    void * data;
    ver_maj_min version;
} reply_parse_ctx;


/* ------------------------------------------------------------------------ */

enum request_type { request_type_names, request_type_search };

static const char * names_cgi_url = "https://www.ncbi.nlm.nih.gov/Traces/names/names.fcgi";
static const char * search_cgi_url = "https://www.ncbi.nlm.nih.gov/Traces/names/search.cgi";

static const char * validate_url( const char * src, enum request_type request_type )
{
    const char * res = src;
    if ( src == NULL )
    {
        switch( request_type )
        {
            case request_type_names  : res = names_cgi_url; break;
            case request_type_search : res = search_cgi_url; break;
        }
    }
    return res;
}


static const char * names_cgi_ver = "3.0";
static const char * search_cgi_ver = "1.0";

static const char * validate_ver( const char * src, enum request_type request_type )
{
    const char * res = src;
    if ( src == NULL )
    {
        switch( request_type )
        {
            case request_type_names  : res = names_cgi_ver; break;
            case request_type_search : res = search_cgi_ver; break;
        }
    }
    return res;
}


static void validate_request_params( const request_params * src, request_params * dst )
{
    dst->names_url      = validate_url( src->names_url,  request_type_names );
    dst->names_ver      = validate_ver( src->names_ver,  request_type_names );
    dst->search_url     = validate_url( src->search_url, request_type_search );
    dst->search_ver     = validate_ver( src->search_ver, request_type_search );

    dst->proto          = src->proto;
    dst->params         = src->params;
    dst->projects       = src->projects;
    dst->terms          = src->terms;
    dst->buffer_size    = src->buffer_size;
    dst->timeout_ms     = src->timeout_ms;
}

/* ------------------------------------------------------------------------ */


rc_t raw_names_request( const request_params * request,
                        on_string_t on_line,
                        uint32_t * rslt_code,
                        void * data )
{
    rc_t rc;
    cgi_request * req;
    request_params validated_request;

    validate_request_params( request, &validated_request );
    rc = make_cgi_request( &req, validated_request.names_url );
    if ( rc == 0 )
    {
        int i = 0;
        const char ** ptr = validated_request.terms;
        rc = add_cgi_request_param( req, "version=%s", validated_request.names_ver );
        for ( i = 0; rc == 0 && *ptr != NULL; ++ i, ++ ptr )
        {
            if ( validated_request . names_ver [ 0 ] < '3' )
                rc = add_cgi_request_param( req, "acc=%s", *ptr );
            else
                rc = add_cgi_request_param( req, "object=%d||%s", i, *ptr );
        }
        rc = add_cgi_request_param ( req, "accept-proto=%s",
                                     validated_request . proto );
        ptr = validated_request.params;
        while ( rc == 0 && *ptr != NULL )
        {
            rc = add_cgi_request_param( req, "%s", *ptr );
            ptr++;
        }
        if ( rc == 0 && validated_request . projects != NULL &&
             validated_request . projects [ 0 ] != 0 )
        {
            const KRepositoryMgr * mgr = NULL;
            uint32_t * ptr = NULL;
            rc = KRepositoryMgr_Make ( & mgr );
            for ( ptr = validated_request . projects;
                  rc == 0 && * ptr != 0; ++ ptr )
            {
                char buffer [ 64 ] = "";
                size_t ticket_size = 0;
                rc = KRepositoryMgr_DownloadTicket ( mgr, * ptr,
                    buffer, sizeof buffer, & ticket_size );
                if ( rc != 0 )
                    PLOGERR ( klogErr, ( klogErr, rc,
                        "Cannot add project '$(p)'", "p=%u", * ptr ) );
                else
                    rc = add_cgi_request_param ( req,
                        "tic=%.*s", ( int ) ticket_size, buffer );
            }
            RELEASE ( KRepositoryMgr, mgr );
        }
        if ( rc == 0 )
        {
            struct KStream * stream;
            rc = perform_cgi_request( req, &stream, rslt_code );
            if ( rc == 0 )
            {
                rc = stream_line_read( stream, on_line,
                    validated_request.buffer_size, validated_request.timeout_ms, data );
                KStreamRelease( stream );
            }
        }
        release_cgi_request( req );
    }
    return rc;
}


static rc_t names_token_v_1_0( const String * token, uint32_t id, void * data )
{
    rc_t rc = 0;
    if ( token->len > 0 )
    {
        reply_obj * obj = data;
        switch( id )
        {
            case 0 : rc = StringCopy( &obj->id, token ); break;
            case 1 : rc = StringCopy( &obj->ticket, token ); break;
            case 2 : rc = StringCopy( &obj->path, token ); break;
            case 3 : obj->code = StringToU64( token, &rc ); break;
            case 4 : rc = StringCopy( &obj->message, token ); break;
        }
    }
    return rc;
}

static rc_t names_token_v_1_1( const String * token, uint32_t id, void * data )
{
    rc_t rc = 0;
    if ( token->len > 0 )
    {
        reply_obj * obj = data;
        switch( id )
        {
            case 0 : rc = StringCopy( &obj->id, token ); break;
            case 1 : /* ??? */ break;
            case 2 : /* ??? */ break;
            case 3 : obj->size = StringToU64( token, &rc ); break;
            case 4 : rc = StringCopy( &obj->date, token ); break;
            case 5 : rc = StringCopy( &obj->checksum, token ); break;
            case 6 : rc = StringCopy( &obj->ticket, token ); break;
            case 7 : rc = StringCopy( &obj->path, token ); break;
            case 8 : obj->code = StringToU64( token, &rc ); break;
            case 9 : rc = StringCopy( &obj->message, token ); break;
        }
    }
    return rc;
}

static rc_t names_token_v_3_0( const String * token, uint32_t id, void * data )
{
    rc_t rc = 0;
    if ( token->len > 0 )
    {
        reply_obj * obj = data;
        switch( id )
        {
            case 0 : rc = StringCopy( &obj->obj_type, token ); break;
            case 1 : rc = StringCopy( &obj->id, token ); break;
            case 2 : obj->size = StringToU64( token, &rc ); break;
            case 3 : rc = StringCopy( &obj->date, token ); break;
            case 4 : rc = StringCopy( &obj->checksum, token ); break;
            case 5 : rc = StringCopy( &obj->ticket, token ); break;
            case 6 : rc = StringCopy( &obj->path, token ); break;
            case 7 : obj->code = StringToU64( token, &rc ); break;
            case 8 : rc = StringCopy( &obj->message, token ); break;
        }
    }
    return rc;
}

static rc_t on_names_line( const String * line, void * data )
{
    rc_t rc = 0;
    reply_parse_ctx * rctx = data;
    if ( line != NULL && line->len > 0 )
    {
        if ( line->addr[ 0 ] == '#' ) /* helper.c */
        {
            extract_versionS( &rctx->version, line );
        }
        else
        {
            reply_obj obj;
            memset( &obj, 0, sizeof obj );
            
            if ( rctx->version.major == 1 )
            {
                switch( rctx->version.minor )
                {
                    case 0 : rc = foreach_token( line, names_token_v_1_0, '|', &obj ); break;
                    case 1 : rc = foreach_token( line, names_token_v_1_1, '|', &obj ); break;
                }
            }
            else if ( rctx->version.major == 3 )
                rc = foreach_token( line, names_token_v_3_0, '|', &obj );

            obj.version = rctx->version;
            if ( rc == 0 )
                rc = rctx->on_obj( &obj, rctx->data );
        }
    }
    return rc;
}


rc_t parsed_names_request( const request_params * request,
                           on_reply_obj_t on_obj,
                           uint32_t * rslt_code,
                           void * data )
{
    reply_parse_ctx rctx = { on_obj, data, { 0, 0 } };
    return raw_names_request( request, on_names_line, rslt_code, &rctx );
}

rc_t names_request_to_list( const request_params * request,
                            uint32_t * rslt_code,
                            struct reply_obj_list ** list )
{
    reply_obj_list * l;
    rc_t rc = make_reply_obj_list( &l );
    if ( rc == 0 )
    {
        rc = parsed_names_request( request, add_reply_to_list_cb, rslt_code, l );
        if ( rc == 0 )
            *list = l;
        else
            release_reply_obj_list( l );
    }
    return rc;
}


/* ------------------------------------------------------------------------ */


rc_t raw_search_request( const request_params * request,
                        on_string_t on_line,
                        uint32_t * rslt_code,
                        void * data )
{
    rc_t rc;
    cgi_request * req;
    request_params validated_request;

    validate_request_params( request, &validated_request );
    rc = make_cgi_request( &req, validated_request.search_url );
    if ( rc == 0 )
    {
        const char ** ptr = validated_request.terms;
        rc = add_cgi_request_param( req, "version=%s", validated_request.search_ver );
        while ( rc == 0 && *ptr != NULL )
        {
            rc = add_cgi_request_param( req, "term=%s", *ptr );
            ptr++;
        }
        ptr = validated_request.params;
        while ( rc == 0 && *ptr != NULL )
        {
            rc = add_cgi_request_param( req, "%s", *ptr );
            ptr++;
        }
        if ( rc == 0 )
        {
            struct KStream * stream;
            rc = perform_cgi_request( req, &stream, rslt_code );
            if ( rc == 0 )
            {
                rc = stream_line_read( stream, on_line,
                    validated_request.buffer_size, validated_request.timeout_ms, data );
                KStreamRelease( stream );
            }
        }
        release_cgi_request( req );
    }
    return rc;
}

static rc_t search_token_v_2_0( const String * token, uint32_t id, void * data )
{
    rc_t rc = 0;
    if ( token->len > 0 )
    {
        reply_obj * obj = data;
        switch( id )
        {
            case 0 : rc = StringCopy( &obj->prj_id, token ); break;
            case 1 : rc = StringCopy( &obj->obj_type, token ); break;
            case 2 : rc = StringCopy( &obj->id, token ); break;
            case 3 : rc = StringCopy( &obj->name, token ); break;
            case 4 : rc = StringCopy( &obj->path, token ); break;
            case 5 : obj->size = StringToU64( token, &rc ); break;
            case 6 : rc = StringCopy( &obj->message, token ); break;
        }
    }
    return rc;
}


static rc_t on_search_line( const String * line, void * data )
{
    rc_t rc = 0;
    reply_parse_ctx * rctx = data;
    if ( line != NULL && line->len > 0 )
    {
        switch( line->addr[ 0 ] )
        {
            case '#' : extract_versionS( &rctx->version, line ); break;
            case '$' : break;
            default  : {
                            reply_obj obj;
                            memset( &obj, 0, sizeof obj );
                            if ( rctx->version.major == 2 )
                            {
                                if ( rctx->version.minor == 0 )
                                    rc = foreach_token( line, search_token_v_2_0, '|', &obj );
                            }
                            obj.version = rctx->version;
                            if ( rc == 0 )
                                rc = rctx->on_obj( &obj, rctx->data );
                        }
                        break;
        }
    }
    return rc;
}


rc_t parsed_search_request( const request_params * request,
                            on_reply_obj_t on_obj,
                            uint32_t * rslt_code,
                            void * data )
{
    reply_parse_ctx rctx = { on_obj, data, { 0, 0 } };
    return raw_search_request( request, on_search_line, rslt_code, &rctx );
}


static rc_t on_obj_for_count( const reply_obj * obj, void * data )
{
    if ( obj != NULL && data != NULL )
    {
        uint32_t * count = data;
        if ( obj->id != NULL ) *count += 1;
    }
    return 0;
}


typedef struct obj_add_ptr
{
    const char ** ptr;
    uint32_t idx;
} obj_add_ptr;

static rc_t on_obj_for_add_ptr( const reply_obj * obj, void * data )
{
    if ( obj != NULL && data != NULL )
    {
        obj_add_ptr * o = data;
        if ( obj->id != NULL )
        {
            o->ptr[ o->idx ] = obj->id->addr;
            o->idx++;
        }
    }
    return 0;
}

static rc_t reply_obj_list_2_ptrs( struct reply_obj_list * list, const char *** ptr )
{
    uint32_t count = 0;
    rc_t rc = foreach_reply_obj( list, on_obj_for_count, &count );
    *ptr = NULL;
    if ( rc == 0 )
    {
        if ( count > 0 )
        {
            const char ** x = calloc( count + 1, sizeof *x );
            if ( x == NULL )
            {
                rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
                ErrMsg( "calloc( %d ) -> %R", count * ( sizeof * x ), rc );
            }
            else
            {
                obj_add_ptr o = { x, 0 };
                rc = foreach_reply_obj( list, on_obj_for_add_ptr, &o );
                if ( rc == 0 )
                    *ptr = x;
                else
                    free( ( void * ) x );
            }
        }
    }
    return rc;
}


static rc_t on_enter_path( const reply_obj * obj, void * data )
{
    rc_t rc = 0;
    if ( obj != NULL && data != NULL )
    {
        reply_obj_list * list = data;
        reply_obj * found = find_obj( list, obj->id );
        if ( found != NULL )
        {
            if ( found->path != NULL ) StringWhack( found->path );
            rc = StringCopy( &found->path, obj->path );
        }
    }
    return rc;
}


static rc_t perform_sub_request( const request_params * request,
                                 uint32_t * rslt_code,
                                 struct reply_obj_list * list )
{
    rc_t rc;
    request_params sub_request;
    
    sub_request.names_url   = request->names_url;
    sub_request.search_url  = request->search_url;
    sub_request.names_ver   = request->names_ver;
    sub_request.search_ver  = request->search_ver;
    sub_request.params      = request->params;
    sub_request.terms       = NULL;
    sub_request.buffer_size = request->buffer_size;
    sub_request.timeout_ms  = request->timeout_ms;

    rc = reply_obj_list_2_ptrs( list, &sub_request.terms );
    if ( rc == 0 && sub_request.terms != NULL )
    {
        struct reply_obj_list * sub_list;
        rc = names_request_to_list( &sub_request, rslt_code, &sub_list );
        if ( rc == 0 )
        {
            /* walk the sub-list and enter the paths found into the list */
            rc = foreach_reply_obj( sub_list, on_enter_path, list );
            release_reply_obj_list( sub_list );
        }
        free( ( void * )sub_request.terms );
    }
    return rc;
}


rc_t search_request_to_list( const request_params * request,
                            uint32_t * rslt_code,
                            struct reply_obj_list ** list,
                            bool resolve_path )
{
    reply_obj_list * l;
    rc_t rc = make_reply_obj_list( &l );
    if ( rc == 0 )
    {
        rc = parsed_search_request( request, add_reply_to_list_cb, rslt_code, l );
        if ( rc == 0 )
        {
            if ( resolve_path )
            {
                rc = perform_sub_request( request, rslt_code, l );
                if ( rc == 0 )
                    *list = l;
                else
                    release_reply_obj_list( l );
            }
            else
                *list = l;
        }
        else
            release_reply_obj_list( l );
    }
    return rc;
}
