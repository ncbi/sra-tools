/* ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnologmsgy Information
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
#include "allele_dict.h"
#include "lmdb.h"
#include "common.h"

#include <klib/text.h>
#include <klib/log.h>
#include <klib/printf.h>
#include <klib/out.h>

typedef struct Allele_Lookup
{
    MDB_env * env;
    MDB_dbi dbi;
    bool dbi_open;
} Allele_Lookup;


rc_t allele_lookup_release( struct Allele_Lookup * al )
{
    rc_t rc = 0;
    if ( al == NULL )
        rc = RC( rcApp, rcNoTarg, rcReleasing, rcParam, rcNull );
    else
    {
        if ( al->dbi_open ) mdb_dbi_close( al->env, al->dbi );
        if ( al->env != NULL ) mdb_env_close( al->env );
        free( ( void * ) al );
    }
    return rc;
}


static int initialize_lmdb( struct Allele_Lookup * o, const char * path )
{
    int res = mdb_env_create( &o->env );
    if ( res != MDB_SUCCESS )
    {
        const char * e = mdb_strerror( res );
        log_err( "allele_lookup.allele_lookup_make().mdb_env_create() failed: %s", e );
    }
    else
    {
        res = mdb_env_set_maxdbs( o->env, 4 );
        if ( res != MDB_SUCCESS )
        {
            const char * e = mdb_strerror( res );
            log_err( "allele_lookup.allele_lookup_make().mdb_env_set_maxdbs( 4 ) failed: %s", e );
        }
        else
        {
            res = mdb_env_open( o->env, path, MDB_NOSUBDIR | MDB_RDONLY, 0664 );
            if ( res != MDB_SUCCESS )
            {
                const char * e = mdb_strerror( res );
                log_err( "allele_lookup.allele_lookup_make().mdb_env_open( '%s' ) failed: %s", path, e );
            }
            else
            {
                MDB_txn * txn;
                res = mdb_txn_begin( o->env, NULL, MDB_RDONLY, &txn );
                if ( res != MDB_SUCCESS )
                {
                    const char * e = mdb_strerror( res );
                    log_err( "allele_lookup.allele_lookup_make().mdb_txn_begin() failed: %s", e );
                }
                else
                {
                    res = mdb_dbi_open( txn, "#ALLELEREG", 0, &o->dbi );
                    if ( res != MDB_SUCCESS )
                    {
                        const char * e = mdb_strerror( res );
                        log_err( "allele_lookup.allele_lookup_make().mdb_dbi_open() failed: %s", e );
                        mdb_txn_abort( txn );
                    }
                    else
                    {
                        res = mdb_txn_commit( txn );
                        if ( res != MDB_SUCCESS )
                        {
                            const char * e = mdb_strerror( res );
                            log_err( "allele_lookup.allele_lookup_make().mdb_txn_commit() failed: %s", e );
                        }
                        else
                            o->dbi_open = true;
                    }
                }
            }
        }
    }
    return res;
}

rc_t allele_lookup_make( struct Allele_Lookup ** al, const char * path )
{
    rc_t rc = 0;
    if ( al == NULL || path == NULL )
    {
        rc = RC( rcApp, rcNoTarg, rcAllocating, rcParam, rcNull );
        log_err( "allele_lookup.allele_lookup_make() given a NULL-ptr" );
    }
    else
    {
        Allele_Lookup * o = calloc( 1, sizeof *o );
        *al = NULL;
        if ( o == NULL )
        {
            rc = RC( rcApp, rcNoTarg, rcAllocating, rcMemory, rcExhausted );
            log_err( "allele_lookup.allele_lookup_make() memory exhausted" );
        }
        else
        {
            if ( initialize_lmdb( o, path ) == MDB_SUCCESS )
                o->dbi_open = true;
            else
                rc = RC ( rcApp, rcArgv, rcConstructing, rcParam, rcInvalid );
        }
        
        if ( rc == 0 )
            *al = o;
        else
            allele_lookup_release( o );
    }
    return rc;
}


bool allele_lookup_perform( const struct Allele_Lookup * al, const String * key, uint64_t * values )
{
    bool res = false;
    if ( al == NULL || key == NULL || values == NULL )
        log_err( "allele_lookup.allele_lookup_perform() given a NULL-ptr" );
    else if ( !al->dbi_open )
        log_err( "allele_lookup.allele_lookup_perform() database is not open" );
    else
    {
        MDB_txn * txn;
        int err = mdb_txn_begin( al->env, NULL, MDB_RDONLY, &txn );
        if ( err != MDB_SUCCESS )
        {
            const char * e = mdb_strerror( err );
            log_err( "allele_lookup.allele_lookup_perform().mdb_txn_begin() failed: %s", e );
        }
        else
        {
            MDB_val k, v;
            
            k.mv_data = ( void * )key->addr;
            k.mv_size = key->len;
            
            res = ( mdb_get( txn, al->dbi, &k, &v ) == MDB_SUCCESS );
            if ( res )
            {
                uint64_t * p = v.mv_data;

                values[ 0 ] = *p;
                values[ 1 ] = *( p + 1 );
            }
            
            mdb_txn_abort( txn );
        }
    }
    return res;
}


/* --------------------------------------------------------------------------------------------------- */
typedef struct Lookup_Cursor
{
    MDB_txn * txn;
    MDB_cursor * mc;
} Lookup_Cursor;


rc_t lookup_cursor_release( struct Lookup_Cursor * curs )
{
    rc_t rc = 0;
    if ( curs == NULL )
        rc = RC( rcApp, rcNoTarg, rcReleasing, rcParam, rcNull );
    else
    {
        mdb_cursor_close( curs->mc );
        mdb_txn_abort( curs->txn );
        free( ( void * ) curs );
    }
    return rc;
}


rc_t lookup_cursor_make( const struct Allele_Lookup * al, struct Lookup_Cursor ** curs )
{
    rc_t rc = 0;
    if ( al == NULL || curs == NULL )
    {
        rc = RC( rcApp, rcNoTarg, rcAllocating, rcParam, rcNull );
        log_err( "allele_lookup.lookup_cursor_make() given a NULL-ptr" );
    }
    else
    {
        Lookup_Cursor * o = calloc( 1, sizeof *o );
        *curs = NULL;
        if ( o == NULL )
        {
            rc = RC( rcApp, rcNoTarg, rcAllocating, rcMemory, rcExhausted );
            log_err( "allele_lookup.lookup_cursor_make() memory exhausted" );
        }
        else
        {
            int res = mdb_txn_begin( al->env, NULL, MDB_RDONLY, &o->txn );
            if ( res != MDB_SUCCESS )
            {
                rc = RC ( rcApp, rcArgv, rcConstructing, rcParam, rcInvalid );
                const char * e = mdb_strerror( res );
                log_err( "allele_lookup.lookup_cursor_make().mdb_txn_begin() failed: %s", e );
            }
            else
            {
                res = mdb_cursor_open( o->txn, al->dbi, &o->mc );
                if ( res != MDB_SUCCESS )
                {
                    rc = RC ( rcApp, rcArgv, rcConstructing, rcParam, rcInvalid );
                    const char * e = mdb_strerror( res );
                    log_err( "allele_lookup.lookup_cursor_make().mdb_cursor_open() failed: %s", e );
                }
            }
        }

        if ( rc == 0 )
            *curs = o;
        else
            lookup_cursor_release( o );
    }
    return rc;
}


bool lookup_cursor_next( struct Lookup_Cursor * curs, String * key, uint64_t * values )
{
    bool res = false;
    if ( curs == NULL || values == NULL )
    {
        log_err( "allele_lookup.lookup_cursor_next() given a NULL-ptr" );
    }
    else
    {
        MDB_val k, v;
        int err = mdb_cursor_get( curs->mc, &k, &v, MDB_NEXT );
        if ( err != MDB_SUCCESS )
        {
            if ( err != MDB_NOTFOUND )
            {
                const char * e = mdb_strerror( res );
                log_err( "allele_lookup.lookup_cursor_make().mdb_txn_begin() failed: %s", e );
            }
        }
        else
        {
            uint64_t * p = v.mv_data;

            values[ 0 ] = *p;
            values[ 1 ] = *( p + 1 );
            
            key->addr = k.mv_data;
            key->len = key->size = k.mv_size;

            res = true;
        }
        
    }
    return res;
}
