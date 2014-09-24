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

#include <klib/log.h>
#include <klib/rc.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>


/* create cursor
 *
 *  'create cursor ID on TBL [ for update ];'
 */
static
rc_t kqsh_create_cursor ( KSymTable *tbl, KTokenSource *src, KToken *t, uint32_t mode )
{
    rc_t rc = 0;

    void *obj;
    String name;
    bool for_write;
    uint32_t obj_type;
    KSymbol *using, *existing;

    /* looking for a cursor name */
    switch ( t -> id )
    {
    case eIdent:
    case obj_VCursor:
        name = t -> str;
        existing = t -> sym;
        break;
    default:
        return expected ( t, klogErr, "cursor name" );
    }

    /* get table */
    if ( next_token ( tbl, src, t ) -> id != kw_on )
        return expected ( t, klogErr, "on");

    switch ( next_token ( tbl, src, t ) -> id )
    {
    case obj_VTable:
        obj_type = obj_VCursor;
        using = t -> sym;
        break;
    default:
        return expected ( t, klogErr, "cursor capable table" );
    }

    /* look for update flag */
    for_write = false;
    if ( next_token ( tbl, src, t ) -> id == kw_for )
    {
        if ( next_token ( tbl, src, t ) -> id != kw_update )
            return expected ( t, klogErr, "update" );
        if ( read_only )
        {
            rc = RC ( rcExe, rcTable, rcOpening, rcMgr, rcReadonly );
            if ( interactive )
            {
                kqsh_printf ( "you are executing in read-only mode.\n"
                              "cannot create cursor for write.\n"
                              "you can relaunch kqsh with the '-u' option to enable updates.\n"
                    );
            }
            else
            {
                PLOGERR ( klogErr,  (klogErr, rc, "failed to create cursor '$(name)'",
                                     "name=%.*s", ( int ) name . size, name . addr ));
            }
            return rc;
        }

        for_write = true;
        next_token ( tbl, src, t );
    }

    /* now we're done parsing */
    if ( t -> id != eSemiColon )
        return expected ( t, klogErr, ";" );

    if ( existing != NULL ) switch ( mode )
    {
    case kcmOpen:
        if ( interactive )
        {
            kqsh_printf ( "opened cursor '%N' ( %p )\n"
                          , existing, existing -> u . obj );
        }
        return 0;
    case kcmInit:
        kqsh_whackobj ( & existing -> n, NULL );
        break;
    case kcmCreate:
        rc = RC ( rcExe, rcTable, rcOpening, rcCursor, rcExists );
        if ( interactive )
            kqsh_printf ( "cursor '%N' already exists.\n", existing );
        else
        {
            PLOGERR ( klogErr,  (klogErr, rc, "cursor '$(name)' could not be created",
                                 "name=%.*s", ( int ) existing -> name . size, existing -> name . addr ));
        }
        return rc;
    }

    /* create the cursor */
    switch ( using -> type )
    {
    case obj_VTable:
        rc = for_write ?
            _VTableCreateCursorWrite ( ( void* ) using -> u . obj, ( VCursor** ) & obj, kcmInsert ):
            _VTableCreateCursorRead ( using -> u . obj, ( const VCursor** ) & obj );
        break;
    }

    if ( rc == 0 )
    {
        KSymbol *sym;

        /* create symbol */
        rc = KSymTableCreateSymbol ( tbl, & sym, & name, obj_type, obj );
        if ( rc == 0 )
        {
            if ( interactive )
            {
                kqsh_printf ( "created cursor '%N' ( %p )\n"
                              , sym, sym -> u . obj );
            }
            return 0;
        }

        /* whack instance */
        switch ( obj_type )
        {
        case obj_VCursor:
            _VCursorRelease ( obj );
            break;
        }
    }

    PLOGERR ( klogInt,  (klogInt, rc,
              "failed to create cursor '$(name)' on $(targ)"
              , "name=%.*s,targ=%.*s"
              , ( int ) name . size, name . addr
                         , ( int ) using -> name . size, using -> name . addr ));

    return rc;
}

/* create table
 *
 *  'create table PATH [ as ID ] [ with schema ID[.ID] ] [ using ID ];'
 */
static
rc_t kqsh_create_table ( KSymTable *tbl, KTokenSource *src, KToken *t, uint32_t mode )
{
    rc_t rc = 0;
    /* bool have_as; */
    String path, name;

    KSymbol *schema;
    String schema_tbl;
    char schema_tbl_str [ 256 ];

    KSymbol *using, *existing;

    void *obj;
    uint32_t obj_type = 0;

    /* looking for a table path */
    if ( t -> id != eString )
        return expected ( t, klogErr, "table path string or name string" );
    StringSubstr ( & t -> str, & path, 1, t -> str . len - 2 );

    /* look for kqsh object id */
    if ( next_token ( tbl, src, t ) -> id != kw_as )
    {
        /* not renamed - take path leaf as kqsh variable name */
        const char *slash = string_rchr ( path . addr, path . size, '/' );
        if ( slash ++ == NULL )
            name = path;
        else
        {
            size_t size = path . size - ( slash - path . addr );
            StringInit ( & name, slash, size, string_len ( slash, size ) );
        }

        existing = KSymTableFind ( tbl, & name );
        /* have_as = false; */
    }

    /* require a name of some sort, allow redefine for the moment */
    else if ( next_token ( tbl, src, t ) -> id != eIdent && t -> sym == NULL )
        return expected ( t, klogErr, "table id" );
    else
    {
        name = t -> str;
        existing = t -> sym;
        next_token ( tbl, src, t );
        /* have_as = true; */
    }

    /* check to see if name is in use */
    if ( existing != NULL )
    {
        KMetadata *meta;

        /* use a kludgy test for being open for update */
        switch ( existing -> type )
        {
        case obj_KTable:
            rc = _KTableOpenMetadataUpdate ( ( void* ) existing -> u . obj, & meta );
            break;
        case obj_VTable:
            rc = _VTableOpenMetadataUpdate ( ( void* ) existing -> u . obj, & meta );
            break;
        default:
            return expected ( t, klogErr, "table id" );
        }

        _KMetadataRelease ( meta );
        if ( rc != 0 )
        {
            /* table is open, but is ( most likely ) read-only */
            if ( interactive )
                kqsh_printf ( "table '%N' is currently open for read", existing );
            else
            {
                rc = RC ( rcExe, rcTable, rcCreating, rcTable, rcBusy );
                PLOGERR ( klogErr,  (klogErr, rc, "cannot create table '$(name)'", "name=%.*s"
                                     , ( int ) name . size, name . addr ));
                return rc;
            }
        }
    }

    /* look for schema spec */
    schema = NULL;
    CONST_STRING ( & schema_tbl, "" );
    if ( t -> id == kw_with )
    {
        /* should see keyword 'schema' */
        if ( next_token ( tbl, src, t ) -> id == kw_schema )
            next_token ( tbl, src, t );

        /* accept any id, but look for schema object */
        if ( t -> id == obj_VSchema )
        {
            schema = t -> sym;
            if ( next_token ( tbl, src, t ) -> id == ePeriod )
            {
                if ( next_token ( tbl, src, t ) -> id != eIdent && t -> sym == NULL )
                    return expected ( t, klogErr, "schema table id" );
                schema_tbl = t -> str;
                next_token ( tbl, src, t );
            }
        }
        else if ( t -> id != eIdent && t -> sym == NULL )
            return expected ( t, klogErr, "schema table id" );
        else
        {
            schema_tbl = t -> str;
            next_token ( tbl, src, t );
        }
    }

    /* look for 'using' clause' */
    using = NULL;
    if ( t -> id == kw_using )
    {
        switch ( next_token ( tbl, src, t ) -> id )
        {
        case obj_KDBManager:
            /* if table exists but is not a KTable, it's an error */
            if ( existing != NULL ) switch ( existing -> type )
            {
            case obj_KTable:
                break;
            case obj_VTable:
                return expected ( t, klogErr, "vdb manager or database" );
            }
            /* both schema and tbl name require vdb+ manager */
            if ( schema != NULL && schema_tbl . len != 0 )
                return expected ( t, klogErr, "schema capable manager" );
            /* schema alone could be mgr with default name
               or db where table name collides with kqsh id */
            if ( schema != NULL )
                return expected ( t, klogErr, "schema capable manager or database" );
            /* no schema but tbl name requires database */
            if ( schema_tbl . len != 0 )
                return expected ( t, klogErr, "schema capable database" );

            /* this is the target object */
            using = t -> sym;
            break;

        case obj_VDBManager:
            /* if table exists but is not a VTable, it's an error */
            if ( existing != NULL ) switch ( existing -> type )
            {
            case obj_KTable:
                return expected ( t, klogErr, "kdb manager or database" );
            case obj_VTable:
                break;
            }
            /* neither schema nor tbl requires kdb mgr or db */
            if ( schema == NULL && schema_tbl . len == 0 )
                return expected ( t, klogErr, "kdb manager or database" );
            /* table name alone requires vdb+ database */
            if ( schema == NULL )
                return expected ( t, klogErr, "schema capable database" );

            /* schema alone implies default table
               schema plus table name is standard */
            using = t -> sym;
            break;

#if 0
        case obj_VDatabase:
            /* if table exists but is not a VTable, it's an error */
            if ( existing != NULL ) switch ( existing -> type )
            {
            case obj_KTable:
                return expected ( t, klogErr, "kdb manager or database" );
            case obj_VTable:
                break;
            }
            /* neither schema nor tbl requires kdb mgr or db */
            if ( schema == NULL && schema_tbl . len == 0 )
                return expected ( t, klogErr, "kdb manager or database" );
            /* schema alone is reinterpreted as table name */
            if ( schema_tbl . len == 0 )
            {
                schema_tbl = schema -> name;
                schema = NULL;
            }
            /* schema plus table name requires vdb+ manager */
            else if ( schema != NULL )
                return expected ( t, klogErr, "schema capable manager" );

            /* database with no schema but table name */
            using = t -> sym;
            break;
#endif
        default:
            if ( schema == NULL )
            {
                return expected ( t, klogErr, ( schema_tbl . len == 0 ) ?
                    "kdb manager or database" : "schema capable database" );
            }

            return expected ( t, klogErr, ( schema_tbl . len == 0 ) ?
                "schema capable manager or database" : "schema capable manager" );
        }

        next_token ( tbl, src, t );
    }

    /* expect target object */
    if ( using == NULL )
    {
        if ( schema == NULL )
        {
            return expected ( t, klogErr, ( schema_tbl . len == 0 ) ?
                "kdb manager or database" : "schema capable database" );
        }

        return expected ( t, klogErr, ( schema_tbl . len == 0 ) ?
            "schema capable manager or database" : "schema capable manager" );
    }

    /* close off statement */
    if ( t -> id != eSemiColon )
        return expected ( t, klogErr, ";" );

    /* if in read-only mode, this whole thing will fail */
    if ( read_only )
    {
        rc = RC ( rcExe, rcTable, rcOpening, rcMgr, rcReadonly );
        if ( interactive )
        {
            kqsh_printf ( "you are executing in read-only mode.\n"
                          "table creation is not supported.\n"
                          "you can relaunch kqsh with the '-u' option to enable updates.\n"
                );
        }
        else
        {
            PLOGERR ( klogErr,  (klogErr, rc, "failed to create table '$(path)' as '$(name)'",
                      "path=%.*s,name=%.*s"
                      , ( int ) path . size, path . addr
                                 , ( int ) name . size, name . addr ));
        }
        return rc;
    }

    /* prepare schema table string */
    if ( schema_tbl . size >= sizeof schema_tbl_str )
    {
        rc = RC ( rcExe, rcTable, rcCreating, rcString, rcExcessive );
        PLOGERR ( klogErr,  (klogErr, rc, "schema name length = $(len)", "len=%zu", schema_tbl . size ));
        return rc;
    }
    string_copy ( schema_tbl_str, sizeof schema_tbl_str,
                  schema_tbl . addr, schema_tbl . size );


    /* handle existing table */
    if ( existing != NULL )  switch ( mode )
    {
    case kcmOpen:
        if ( interactive )
        {
            kqsh_printf ( "opened table '%N' ( %p )\n"
                          , existing, existing -> u . obj );
        }
        return 0;
    case kcmInit:
        kqsh_whackobj ( & existing -> n, NULL );
        break;
    case kcmCreate:
        rc = RC ( rcExe, rcTable, rcCreating, rcTable, rcExists );
        if ( interactive )
            kqsh_printf ( "table '%N' is already open", existing );
        else
        {
            PLOGERR ( klogErr,  (klogErr, rc, "table '$(name)' could not be created",
                                 "name=%.*s", ( int ) existing -> name . size, existing -> name . addr ));
        }
        return rc;
    }

    /* dispatch */
    switch ( using -> type )
    {
    case obj_KDBManager:
        obj_type = obj_KTable;
        rc = _KDBManagerCreateTable ( ( void* ) using -> u . obj, ( KTable** ) & obj,
            mode | kcmParents,"%.*s", ( int ) path . size, path . addr );
        break;
    case obj_VDBManager:
        obj_type = obj_VTable;
        assert ( schema != NULL );
        rc = _VDBManagerCreateTable ( ( void* ) using -> u . obj, ( VTable** ) & obj,
            schema -> u . obj, schema_tbl_str [ 0 ] ? schema_tbl_str : NULL,
            mode | kcmParents,"%.*s", ( int ) path . size, path . addr );
        break;
    }

    if ( rc == 0 )
    {
        KSymbol *sym;

        /* create symbol */
        rc = KSymTableCreateSymbol ( tbl, & sym, & name, obj_type, obj );
        if ( rc == 0 )
        {
            if ( interactive )
            {
                kqsh_printf ( "created table '%N' ( %p )\n"
                              , sym, sym -> u . obj );
            }
            return 0;
        }

        /* whack instance */
        switch ( obj_type )
        {
        case obj_KTable:
            _KTableRelease ( obj );
            break;
        case obj_VTable:
            _VTableRelease ( obj );
            break;
        }
    }

    PLOGERR ( klogInt,  (klogInt, rc,
              "failed to create table '$(name)' using $(targ)"
              , "name=%.*s,targ=%.*s"
              , ( int ) name . size, name . addr
                         , ( int ) using -> name . size, using -> name . addr ));

    return rc;
}


/* create schema
 *
 *  'create schema [ as ] ID [ using MGR ];'
 */
static
rc_t kqsh_create_schema ( KSymTable *tbl, KTokenSource *src, KToken *t, uint32_t mode )
{
    rc_t rc = 0;
    String name;
    KSymbol *existing;
    const KSymbol *mgr;
    struct VSchema *schema;

    /* shouldn't be here, but don't choke on it */
    if ( t -> id == kw_as )
        next_token ( tbl, src, t );

    /* get schema object name */
    switch ( t -> id )
    {
    case eIdent:
    case obj_VSchema:
        name = t -> str;
        existing = t -> sym;
        break;
    default:
        return expected ( t, klogErr, "schema name" );
    }

    /* there is no implicit mgr yet */
    mgr = NULL;

    /* get using clause */
    if ( next_token ( tbl, src, t ) -> id == kw_using )
    {
        switch ( next_token ( tbl, src, t ) -> id )
        {
        case obj_VDBManager:
        case obj_SRAManager:
            mgr = t -> sym;
            break;
        default:
            return expected ( t, klogErr, "open vdb or sra manager" );
        }

        next_token ( tbl, src, t );
    }
    else if ( mgr == NULL )
    {
        return expected ( t, klogErr, "using" );
    }

    /* expect ';' */
    if ( t -> id != eSemiColon )
        return expected ( t, klogErr, ";" );

    /* if the schema already exists, then look at mode */
    if ( existing != NULL ) switch ( mode )
    {
    case kcmOpen:
        if ( interactive )
        {
            kqsh_printf ( "opened schema '%N' ( %p )\n"
                          , existing, existing -> u . obj );
        }
        return 0;
    case kcmInit:
        kqsh_whackobj ( & existing -> n, NULL );
        break;
    case kcmCreate:
        rc = RC ( rcExe, rcSchema, rcCreating, rcSchema, rcExists );
        if ( interactive )
            kqsh_printf ( "schema '%N' already exists", existing );
        else
        {
            PLOGERR ( klogErr,  (klogErr, rc, "schema '$(name)' could not be created",
                                 "name=%.*s", ( int ) existing -> name . size, existing -> name . addr ));
        }
        return rc;
    }

    /* now process the request */
    switch ( mgr -> type )
    {
    case obj_VDBManager:
        rc = _VDBManagerMakeSchema ( mgr -> u . obj, & schema );
        break;
    case obj_SRAManager:
        rc = _SRAMgrMakeSRASchema ( mgr -> u . obj, & schema );
        break;
    }

    if ( rc == 0 )
    {
        KSymbol *sym;

        /* create symbol */
        rc = KSymTableCreateSymbol ( tbl, & sym, & name, obj_VSchema, schema );
        if ( rc == 0 )
        {
            if ( interactive )
            {
                kqsh_printf ( "created schema '%N' ( %p )\n"
                              , sym, sym -> u . obj );
            }
            return 0;
        }

        /* whack instance */
        _VSchemaRelease ( schema );
    }

    PLOGERR ( klogInt,  (klogInt, rc,
              "failed to create schema '$(name)' using $(mgr)"
              , "name=%.*s,mgr=%.*s"
              , ( int ) name . size, name . addr
                         , ( int ) mgr -> name . size, mgr -> name . addr ));

    return rc;
}

/* create
 */
rc_t kqsh_create ( KSymTable *tbl, KTokenSource *src, KToken *t )
{
    uint32_t mode = kcmCreate;

    if ( t -> id == kw_or )
    {
        switch ( next_token ( tbl, src, t ) -> id )
        {
        case kw_replace:
            mode = kcmInit;
            break;
        case kw_open:
            mode = kcmOpen;
            break;
        default:
            return expected ( t, klogErr, "replace or open" );
        }

        next_token ( tbl, src, t );
    }

    switch ( t -> id )
    {
    case kw_cursor:
        return kqsh_create_cursor ( tbl, src, next_token ( tbl, src, t ), mode );
    case kw_schema:
        return kqsh_create_schema ( tbl, src, next_token ( tbl, src, t ), mode );
    case kw_table:
        return kqsh_create_table ( tbl, src, next_token ( tbl, src, t ), mode );
    }

    return expected ( t, klogErr, "cursor, schema or table" );
}
