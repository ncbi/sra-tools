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

#include "kqsh-priv.h"
#include "kqsh-tok.h"
#include "kqsh-kdb.h"
#include "kqsh-vdb.h"
#include "kqsh-sra.h"

#include <klib/symbol.h>
#include <klib/symtab.h>
#include <klib/token.h>

#include <kdb/manager.h>
#include <vdb/manager.h>

#include <kfs/directory.h>
#include <kfs/dyload.h>
#include <klib/log.h>
#include <klib/vector.h>
#include <klib/rc.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* library names on disk */
#define LIBSTR2( str ) # str
#define LIBSTR( str ) LIBSTR2 ( str )

#if defined LIBKDB

#define LIBKDBSTR LIBSTR ( LIBKDB )
#define LIBWKDBSTR LIBSTR ( LIBWKDB )
#define LIBVDBSTR LIBSTR ( LIBVDB )
#define LIBWVDBSTR LIBSTR ( LIBWVDB )
#define LIBSRADBSTR LIBSTR ( LIBSRADB )
#define LIBWSRADBSTR LIBSTR ( LIBWSRADB )

#elif defined LIBNCBI_VDB

#define LIBKDBSTR LIBSTR ( LIBNCBI_VDB )
#define LIBWKDBSTR LIBSTR ( LIBNCBI_WVDB )
#define LIBVDBSTR LIBSTR ( LIBNCBI_VDB )
#define LIBWVDBSTR LIBSTR ( LIBNCBI_WVDB )
#define LIBSRADBSTR LIBSTR ( LIBNCBI_VDB )
#define LIBWSRADBSTR LIBSTR ( LIBNCBI_WVDB )

#else
#error need a library name
#endif

/* dynamic loader */
static KDyld *dl;


/* init_libpath
 *  initialize the library path
 */
rc_t kqsh_init_libpath ( void )
{
    return KDyldMake ( & dl );
}


/* whack_libpath
 */
void kqsh_whack_libpath ( void )
{
    KDyldRelease ( dl );
    dl = NULL;
}

/* update_libpath
 */
rc_t kqsh_update_libpath ( const char *path )
{
    const char *end;
    for ( end = strchr ( path, ':' ); end != NULL; end = strchr ( path = end + 1, ':' ) )
    {
        if ( path < end )
            KDyldAddSearchPath ( dl, "%.*s", ( int ) ( end - path ), path );
    }

    if ( path [ 0 ] != 0 )
        KDyldAddSearchPath ( dl, path );

    return 0;
}


/* system_libpath
 */
rc_t kqsh_system_libpath ( void )
{
    const char *LD_LIBRARY_PATH = getenv ( "LD_LIBRARY_PATH" );

    if ( LD_LIBRARY_PATH == NULL )
        return 0;

    return kqsh_update_libpath ( LD_LIBRARY_PATH );
}

/* load_lib
 *  loads the library
 *  fetches function to make manager
 */
static
rc_t kqsh_load_lib ( KDylib **lib, const char *libname, char *path, size_t path_size )
{
    rc_t rc = KDyldLoadLib ( dl, lib, libname );
    if ( rc == 0 )
    {
        /* return path to caller */
        rc = KDylibFullPath ( * lib, path, path_size );
        if ( rc != 0 )
            path [ 0 ] = 0;

        return 0;
    }

    return rc;
}

static
rc_t kqsh_import_sym ( const KDylib *lib, fptr_t *addr, const char *name )
{
    KSymAddr *sym;
    rc_t rc = KDylibSymbol ( lib, & sym, name );
    if ( rc == 0 )
    {
        KSymAddrAsFunc ( sym, addr );
        KSymAddrRelease ( sym );
    }
    return rc;
}

#if 0
static
void kqsh_set_lib_log_handler ( const KDylib *lib )
{
    const KWrtHandler *handler = KLogHandlerGet ();
    rc_t ( CC * klogLibHandlerSet ) ( KWrtWriter writer, void *data );
    rc_t rc = kqsh_import_sym ( lib, ( fptr_t* ) & klogLibHandlerSet, "KLogLibHandlerSet" );
    if ( rc == 0 )
    {
        rc_t ( CC * klogLibFmtHandlerSetDefault ) ( void );
        rc = kqsh_import_sym ( lib, ( fptr_t* ) & klogLibFmtHandlerSetDefault, "KLogLibFmtHandlerSetDefault" );
        if ( rc == 0 )
        {
            ( * klogLibHandlerSet ) ( handler -> writer, handler -> data );
            ( * klogLibFmtHandlerSetDefault ) ();
        }
    }
}
#endif

static
rc_t kqsh_import_lib ( kqsh_libdata *libdata, const char *libname )
{
    /* load the library */
    char path [ 4096 ];
    rc_t rc = kqsh_load_lib ( & libdata -> lib, libname, path, sizeof path );
    if ( rc != 0 )
        PLOGERR ( klogErr,  (klogErr, rc, "failed to load library '$(lib)'", "lib=%s", libname ));
    else
    {
        int i;
        bool done_update = read_only;
        fptr_t *vt = libdata -> cvt;
        const char *msg, **msgv = & libdata -> msg [ 1 ];
#if 0
        /* tell library to log to stderr */
        kqsh_set_lib_log_handler ( libdata -> lib );
#endif
        /* import first symbol according to operating mode */
        msg = msgv [ read_only ? -1 : 0 ];
        rc = kqsh_import_sym ( libdata -> lib, & vt [ 0 ], msg );

        /* import remaining symbols */
        for ( i = 1; rc == 0; ++ i )
        {
            /* detect end of list */
            msg = msgv [ i ];
            if ( msgv [ i ] == NULL )
            {
                /* exit if both const and update sides done */
                if ( done_update )
                {
                    if ( interactive )
                        kqsh_printf ( "loaded library '%s'\n", path );
                    return 0;
                }

                /* switch to update side */
                vt = libdata -> wvt;
                msgv = libdata -> wmsg;
                done_update = true;
                i = -1;
                continue;
            }

            /* import symbol */
            rc = kqsh_import_sym ( libdata -> lib, & vt [ i ], msg );
        }

        /* failed to find symbol */
        PLOGERR ( klogInt,  (klogInt, rc,
                  "failed to resolve symbol $(msg) in library '$(path)'"
                             , "msg=%s,path=%s", msg, path ));
    }

    return rc;
}

/* load
 *  expects "t" to describe a library class, i.e.
 *    'open <lib> mgr...'
 *            ^
 */
rc_t kqsh_load ( struct KSymTable *tbl, KTokenSource *src, KToken *t )
{
    rc_t rc = 0;
    void *mgr;
    String name;
    uint32_t type;
    const char *libtype;
    KSymbol *existing;

    /* for loading library */
    struct { kqsh_libdata *data; const char *ro, *rw; } libs [ 3 ] =
    {
        { & kdb_data, LIBKDBSTR, LIBWKDBSTR },
        { & vdb_data, LIBVDBSTR, LIBWVDBSTR },
        { & sra_data, LIBSRADBSTR, LIBWSRADBSTR }
    };

    uint32_t lib_idx;

    /* select which library */
    switch ( t -> id )
    {
    case kw_kdb:
        CONST_STRING ( & name, "kmgr" );
        libtype = "kdb";
        lib_idx = 0;
        type = obj_KDBManager;
        break;
    case kw_vdb:
        CONST_STRING ( & name, "vmgr" );
        libtype = "vdb";
        lib_idx = 1;
        type = obj_VDBManager;
        break;
    case kw_sra:
        CONST_STRING ( & name, "sramgr" );
        libtype = "sra";
        lib_idx = 2;
        type = obj_SRAManager;
        break;
    default:
        return expected ( t, klogErr, "kdb, vdb or sra" );
    }

    /* treat 'manager' as optional - it's understood */
    if ( next_token ( tbl, src, t ) -> id == kw_manager )
        next_token ( tbl, src, t );

    /* capture optional manager name */
    switch ( t -> id )
    {
    case kw_as:
        switch ( next_token ( tbl, src, t ) -> id )
        {
        case eIdent:
            break;

        case rsrv_kmgr:
        case obj_KDBManager:
            if ( type == obj_KDBManager )
                break;
            return expected ( t, klogErr, "identifier" );

        case rsrv_vmgr:
        case obj_VDBManager:
            if ( type == obj_VDBManager )
                break;

        case rsrv_sramgr:
        case obj_SRAManager:
            if ( type == obj_SRAManager )
                break;

        default:
            return expected ( t, klogErr, "identifier" );
        }

        name = t -> str;
        existing = t -> sym;

        next_token ( tbl, src, t );
        break;

    default:
        existing = KSymTableFind ( tbl, & name );
    }

    /* process existing */
    if ( existing != NULL && existing -> type >= obj_first )
    {
        kqsh_printf ( "%s manager '%N' ( %p ) is already open.\n",
            libtype, existing, existing -> u . obj );
        libs [ lib_idx ] . data -> dflt = existing;
        return 0;
    }

    /* load library if required */
    if ( libs [ lib_idx ] . data -> lib == NULL )
    {
        uint32_t i;
        for ( i = 0; i <= lib_idx; ++ i )
        {
            rc = kqsh_import_lib ( libs [ i ] . data,
                read_only ? libs [ i ] .  ro : libs [ i ] . rw );
            if ( rc != 0 )
                return rc;
        }
    }

    /* create mgr object */
    switch ( type )
    {
    case obj_KDBManager:
        rc = _KDBManagerMake ( ( struct KDBManager** ) & mgr, NULL );
        break;
    case obj_VDBManager:
        rc = _VDBManagerMake ( ( struct VDBManager** ) & mgr, NULL );
        break;
    case obj_SRAManager:
        rc = _SRAMgrMake ( ( struct SRAMgr** ) & mgr, NULL );
        break;
    }

    if ( rc == 0 )
    {
        KSymbol *sym;

        /* create symbol */
        rc = KSymTableCreateSymbol ( tbl, & sym, & name, type, mgr );
        if ( rc == 0 )
        {
            libs [ lib_idx ] . data -> dflt = sym;
            if ( interactive )
            {
                kqsh_printf ( "opened %s manager '%N' ( %p ) for %s\n"
                              , libtype
                              , sym, sym -> u . obj
                              , read_only ? "read" : "update" );
            }
            return 0;
        }

        /* whack instance */
        switch ( type )
        {
        case obj_KDBManager:
            _KDBManagerRelease ( ( const void* ) mgr );
            break;
        case obj_VDBManager:
            _VDBManagerRelease ( ( const void* ) mgr );
            break;
        case obj_SRAManager:
            _SRAMgrRelease ( ( const void* ) mgr );
            break;
        }
    }

    PLOGERR ( klogInt,  (klogInt, rc,
              "failed to open $(type) manager '$(name)' for $(mode)"
              , "type=%s,name=%.*s,mode=%s"
              , libtype
              , ( int ) name . size, name . addr
                         , read_only ? "read" : "update" ));

    return rc;
}
