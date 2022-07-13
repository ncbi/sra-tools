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

typedef struct DbPair StdDbPair;
#define DBPAIR_IMPL StdDbPair

#include "db-pair.h"
#include "csra-pair.h"
#include "tbl-pair.h"
#include "dir-pair.h"
#include "meta-pair.h"
#include "sra-sort.h"
#include "ctx.h"
#include "caps.h"
#include "except.h"
#include "status.h"
#include "mem.h"

#include <vdb/database.h>
#include <vdb/table.h>
#include <vdb/vdb-priv.h>
#include <kdb/database.h>
#include <kdb/meta.h>
#include <kdb/kdb-priv.h>
#include <kfs/directory.h>
#include <klib/printf.h>
#include <klib/text.h>
#include <klib/rc.h>

#include <string.h>


FILE_ENTRY ( db-pair );


/*--------------------------------------------------------------------------
 * StdDbPair
 *  generic database object pair
 */

static
void StdDbPairWhack ( StdDbPair *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );
    DbPairDestroy ( self, ctx );
    MemFree ( ctx, self, sizeof * self );
}

static
void StdDbPairExplode ( StdDbPair *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );
}

static
const char *StdDbPairGetTblMember ( const StdDbPair *self, const ctx_t *ctx, const VTable *src, const char *name )
{
    return name;
}

static DbPair_vt StdDbPair_vt =
{
    StdDbPairWhack,
    StdDbPairExplode,
    StdDbPairGetTblMember
};

static
DbPair *StdDbPairMake ( const ctx_t *ctx,
    const VDatabase *src, VDatabase *dst, const char *name, const char *opt_full_spec )
{
    FUNC_ENTRY ( ctx );

    StdDbPair *db;

    TRY ( db = MemAlloc ( ctx, sizeof * db, false ) )
    {
        TRY ( DbPairInit ( db, ctx, & StdDbPair_vt, src, dst, name, opt_full_spec ) )
        {
            return db;
        }

        MemFree ( ctx, db, sizeof * db );
    }

    return NULL;
}

/*--------------------------------------------------------------------------
 * DbPair
 *  interface code
 */


/* Make
 *  makes an object based upon open source and destination VDatabase objects
 */
static
bool is_csra_db ( const VDatabase *src, const ctx_t *ctx )
{
    /* TBD - hack'o-matic */
    return true;
}

DbPair *DbPairMake ( const ctx_t *ctx,
    const VDatabase *src, VDatabase *dst, const char *name )
{
    FUNC_ENTRY ( ctx );

    /* intercept certain types of databases - right now cSRA */
    if ( is_csra_db ( src, ctx ) )
        return cSRAPairMake ( ctx, src, dst, name );

    return StdDbPairMake ( ctx, src, dst, name, NULL );
}

DbPair *DbPairMakeStdDbPair ( DbPair *self, const ctx_t *ctx,
    const VDatabase *src, VDatabase *dst, const char *name )
{
    FUNC_ENTRY ( ctx );
    return StdDbPairMake ( ctx, src, dst, name, self -> full_spec );
}


/* Release
 */
void DbPairRelease ( DbPair *self, const ctx_t *ctx )
{
    rc_t rc;
    FUNC_ENTRY ( ctx );

    if ( self != NULL )
    {
        switch ( KRefcountDrop ( & self -> refcount, "DbPair" ) )
        {
        case krefOkay:
            break;
        case krefWhack:
            ( * self -> vt -> whack ) ( ( void* ) self, ctx );
            break;
        case krefZero:
        case krefLimit:
        case krefNegative:
            rc = RC ( rcExe, rcDatabase, rcDestroying, rcRefcount, rcInvalid );
            INTERNAL_ERROR ( rc, "failed to release DbPair" );
        }
    }
}

/* Duplicate
 */
DbPair *DbPairDuplicate ( DbPair *self, const ctx_t *ctx )
{
    rc_t rc;
    FUNC_ENTRY ( ctx );

    if ( self != NULL )
    {
        switch ( KRefcountAdd ( & self -> refcount, "DbPair" ) )
        {
        case krefOkay:
        case krefWhack:
        case krefZero:
            break;
        case krefLimit:
        case krefNegative:
            rc = RC ( rcExe, rcDatabase, rcAttaching, rcRefcount, rcInvalid );
            INTERNAL_ERROR ( rc, "failed to duplicate DbPair" );
            return NULL;
        }
    }

    return ( DbPair* ) self;
}


/* Copy
 */
static
bool CC DbPairCopyDbPair ( void *item, void *data )
{
    const ctx_t *ctx = ( const void* ) data;
    FUNC_ENTRY ( ctx );
    TRY ( DbPairCopy ( item, ctx ) )
    {
        return false;
    }
    return true;
}

static
bool CC DbPairCopyTablePair ( void *item, void *data )
{
    const ctx_t *ctx = ( const void* ) data;
    FUNC_ENTRY ( ctx );
    TRY ( TablePairCopy ( item, ctx ) )
    {
        return false;
    }
    return true;
}

static
bool CC DbPairCopyDirPair ( void *item, void *data )
{
    const ctx_t *ctx = ( const void* ) data;
    FUNC_ENTRY ( ctx );
    TRY ( DirPairCopy ( item, ctx ) )
    {
        return false;
    }
    return true;
}

void DbPairCopy ( DbPair *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    STATUS ( 2, "copying database '%s'", self -> full_spec );

    /* copy metadata */
    MetaPairCopy ( self -> meta, ctx, self -> full_spec, self -> exclude_meta );

    /* copy all child databases */
    if ( ! FAILED () && VectorLength ( & self -> dbs ) != 0 )
    {
        STATUS ( 2, "copying '%s' sub-databases", self -> full_spec );
        VectorDoUntil ( & self -> dbs, false, DbPairCopyDbPair, ( void* ) ctx );
    }

    /* now we should be able to copy each table */
    if ( ! FAILED () && VectorLength ( & self -> tbls ) != 0 )
    {
        STATUS ( 2, "copying '%s' tables", self -> full_spec );
        VectorDoUntil ( & self -> tbls, false, DbPairCopyTablePair, ( void* ) ctx );
    }

    /* copy any directories - extra */
    if ( ! FAILED () && VectorLength ( & self -> dirs ) != 0 )
    {
        STATUS ( 2, "copying '%s' directories", self -> full_spec );
        VectorDoUntil ( & self -> dirs, false, DbPairCopyDirPair, ( void* ) ctx );
    }
}


/* Explode
 *  probably a bad name, but it is intended to mean
 *  register all enclosed tables and databases
 *  and create a proper pair of KMetadata
 */
static
MetaPair *DbPairExplodeMetaPair ( DbPair *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    rc_t rc;
    MetaPair *meta = NULL;
    const KMetadata *smeta;

    rc = VDatabaseOpenMetadataRead ( self -> sdb, & smeta );
    if ( rc != 0 )
        ERROR ( rc, "VDatabaseOpenMetadataRead failed on 'src.%s'", self -> full_spec );
    else
    {
        KMetadata *dmeta;
        rc = VDatabaseOpenMetadataUpdate ( self -> ddb, & dmeta );
        if ( rc != 0 )
            ERROR ( rc, "VDatabaseOpenMetadataUpdate failed on 'dst.%s'", self -> full_spec );
        else
        {
            meta = MetaPairMake ( ctx, smeta, dmeta, self -> full_spec );

            KMetadataRelease ( dmeta );
        }
        
        KMetadataRelease ( smeta );
    }

    return meta;
}

static
void DbPairExplode ( DbPair *self, const ctx_t *ctx );

static
bool CC DbPairExplodeDbPair ( void *item, void *data )
{
    const ctx_t *ctx = ( const void* ) data;
    FUNC_ENTRY ( ctx );
    TRY ( DbPairExplode ( item, ctx ) )
    {
        return false;
    }
    return true;
}

static
bool CC DbPairPreExplodeTablePair ( void *item, void *data )
{
    const ctx_t *ctx = ( const void* ) data;
    FUNC_ENTRY ( ctx );
    TRY ( TablePairPreExplode ( ( ( TablePair* ) item ), ctx ) )
    {
        return false;
    }
    return true;
}

#if 0
static
void DbPairDefaultExplodeDB ( DbPair *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    rc_t rc;
    KNamelist *names;

    rc = VDatabaseListDB ( self -> sdb, & names );
    if ( rc != 0 )
        ERROR ( rc, "VDatabaseListDB failed listing '%s' databases", self -> full_spec );
    else
    {
        uint32_t count;
        rc = KNamelistCount ( names, & count );
        if ( rc != 0 )
            ERROR ( rc, "KNamelistCount failed listing '%s' databases", self -> full_spec );
        else if ( count > 0 )
        {
            uint32_t i;
            for ( i = 0; ! FAILED () && i < count; ++ i )
            {
                DbPair *db;

                const char *name;
                rc = KNamelistGet ( names, i, & name );
                if ( rc != 0 )
                {
                    ERROR ( rc, "KNamelistGet ( %u ) failed listing '%s' databases", i, self -> full_spec );
                    break;
                }

                if ( self -> exclude_dbs != NULL )
                {
                    uint32_t j;
                    for ( j = 0; self -> exclude_dbs [ j ] != NULL; ++ j )
                    {
                        if ( strcmp ( name, self -> exclude_dbs [ j ] ) == 0 )
                        {
                            name = NULL;
                            break;
                        }
                    }
                    if ( name == NULL )
                        continue;
                }

                TRY ( db = DbPairMakeDbPair ( self, ctx, name, name, true, DbPairMakeStdDbPair ) )
                {
                    ON_FAIL ( DbPairAddDbPair ( self, ctx, db ) )
                    {
                        DbPairRelease ( db, ctx );
                    }
                }
            }
        }

        KNamelistRelease ( names );
    }
}
#endif

static
void DbPairDefaultExplodeTbl ( DbPair *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    rc_t rc;
    KNamelist *names;

    rc = VDatabaseListTbl ( self -> sdb, & names );
    if ( rc != 0 )
        ERROR ( rc, "VDatabaseListTbl failed listing '%s' tables", self -> full_spec );
    else
    {
        uint32_t count;
        rc = KNamelistCount ( names, & count );
        if ( rc != 0 )
            ERROR ( rc, "KNamelistCount failed listing '%s' tables", self -> full_spec );
        else if ( count > 0 )
        {
            uint32_t i;
            for ( i = 0; ! FAILED () && i < count; ++ i )
            {
                TablePair *tbl;

                const char *name;
                rc = KNamelistGet ( names, i, & name );
                if ( rc != 0 )
                {
                    ERROR ( rc, "KNamelistGet ( %u ) failed listing '%s' tables", i, self -> full_spec );
                    break;
                }

                if ( self -> exclude_tbls != NULL )
                {
                    uint32_t j;
                    for ( j = 0; self -> exclude_tbls [ j ] != NULL; ++ j )
                    {
                        if ( strcmp ( name, self -> exclude_tbls [ j ] ) == 0 )
                        {
                            name = NULL;
                            break;
                        }
                    }
                    if ( name == NULL )
                        continue;
                }

                TRY ( tbl = DbPairMakeTablePair ( self, ctx, name, name, true, false, DbPairMakeStdTblPair ) )
                {
                    ON_FAIL ( DbPairAddTablePair ( self, ctx, tbl ) )
                    {
                        TablePairRelease ( tbl, ctx );
                    }
                }
            }
        }

        KNamelistRelease ( names );
    }
}

static
void DbPairDefaultExplode ( DbPair *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

#if 0
    TRY ( DbPairDefaultExplodeDB ( self, ctx ) )
#endif
    {
        DbPairDefaultExplodeTbl ( self, ctx );
    }
}

static
void DbPairExplode ( DbPair *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    STATUS ( 2, "exploding database '%s'", self -> full_spec );

    /* first, ask subclass to perform explicit explode */
    if ( self == NULL )
    {
        rc_t rc = RC ( rcExe, rcDatabase, rcAccessing, rcSelf, rcNull );
        INTERNAL_ERROR ( rc, "bad DbPair" );
    }
    else
    {
        TRY ( ( * self -> vt -> explode ) ( self, ctx ) )
        {
            /* next, perform default self-explode for all dbs, tbls not excluded */
            TRY ( DbPairDefaultExplode ( self, ctx ) )
            {
                /* now create metadata pair */
                if ( self -> meta == NULL )
                    self -> meta = DbPairExplodeMetaPair ( self, ctx );
                if ( ! FAILED () )
                {
                    /* now explode all child dbs, tbls */
                    TRY ( VectorDoUntil ( & self -> dbs, false, DbPairExplodeDbPair, ( void* ) ctx ) )
                    {
                        VectorDoUntil ( & self -> tbls, false, DbPairPreExplodeTablePair, ( void* ) ctx );
                    }
                }
            }
        }
    }
}


/* Run
 */
void DbPairRun ( DbPair *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    const Tool *tp = ctx -> caps -> tool;

    STATUS ( 1, "input database is '%s'", tp -> src_path );
    STATUS ( 1, "output database is '%s'", tp -> dst_path );

    /* the first thing to do is to explode the database
       to create sub-dbs, sub-tables, and metadata */
    TRY ( DbPairExplode ( self, ctx ) )
    {
        /* now copy */
        DbPairCopy ( self, ctx );
    }

    if ( FAILED () )
        STATUS ( 1, "failed processing database '%s'", self -> full_spec );
    else
        STATUS ( 1, "finished processing database '%s'", self -> full_spec );
}


/* MakeDbPair
 */
DbPair *DbPairMakeDbPair ( DbPair *self, const ctx_t *ctx, const char *member, const char *name, bool required,
    DbPair* ( * make ) ( DbPair *self, const ctx_t *ctx, const VDatabase *src, VDatabase *dst, const char *name ) )
{
    FUNC_ENTRY ( ctx );

    rc_t rc;
    const VDatabase *src;
    DbPair *db = NULL;
                
    STATUS ( 4, "creating db pair '%s.%s'", self -> full_spec, name );
    rc = VDatabaseOpenDBRead ( self -> sdb, & src, "%s", name );
    if ( rc != 0 )
    {
        if ( required )
            ERROR ( rc, "VDatabaseOpenDBRead: failed to open db '%s.%s'", self -> full_spec, name );
        else
            STATUS ( 4, "optional db '%s.%s' was not opened", self -> full_spec, name );
    }
    else
    {
#if 0
        if ( member == NULL )
            member = DbPairGetTblMember ( self, ctx, src, name );
#endif

        if ( ! FAILED () )
        {
            VDatabase *dst;
            const Tool *tp = ctx -> caps -> tool;
            rc = VDatabaseCreateDB ( self -> ddb, & dst, member, kcmOpen | ( tp -> db . cmode & kcmMD5 ), "%s", name );
            if ( rc != 0 )
                ERROR ( rc, "VDatabaseCreateDB: failed to create %s db '%s.%s'", member, self -> full_spec, name );
            else
            {
                db = ( * make ) ( self, ctx, src, dst, name );

                VDatabaseRelease ( dst );
            }
        }

        VDatabaseRelease ( src );
    }

    return db;
}


/* MakeTablePair
 */
TablePair *DbPairMakeTablePair ( DbPair *self, const ctx_t *ctx, const char *member, const char *name, bool required, bool reorder,
    TablePair* ( * make ) ( DbPair *self, const ctx_t *ctx, const VTable *src, VTable *dst, const char *name, bool reorder ) )
{
    FUNC_ENTRY ( ctx );

    rc_t rc;
    const VTable *src;
    TablePair *tbl = NULL;
                
    STATUS ( 4, "creating table pair '%s.%s'", self -> full_spec, name );
    rc = VDatabaseOpenTableRead ( self -> sdb, & src, "%s", name );
    if ( rc != 0 )
    {
        if ( required )
            ERROR ( rc, "VDatabaseOpenTableRead: failed to open table '%s.%s'", self -> full_spec, name );
        else
            STATUS ( 4, "optional table '%s.%s' was not opened", self -> full_spec, name );
    }
    else
    {
        if ( member == NULL )
            member = DbPairGetTblMember ( self, ctx, src, name );

        if ( ! FAILED () )
        {
            VTable *dst;
            const Tool *tp = ctx -> caps -> tool;
            rc = VDatabaseCreateTable ( self -> ddb, & dst, member, kcmOpen | ( tp -> db . cmode & kcmMD5 ), "%s", name );
            if ( rc != 0 )
                ERROR ( rc, "VDatabaseCreateTable: failed to create %s table '%s.%s'", member, self -> full_spec, name );
            else
            {
                tbl = ( * make ) ( self, ctx, src, dst, name, reorder );

                VTableRelease ( dst );
            }
        }

        VTableRelease ( src );
    }

    return tbl;
}


/* AddDbPair
 *  called from implementations
 */
void DbPairAddDbPair ( DbPair *self, const ctx_t *ctx, DbPair *db )
{
    FUNC_ENTRY ( ctx );

    rc_t rc;

    STATUS ( 4, "adding db pair '%s'", db -> full_spec );
    rc = VectorAppend ( & self -> dbs, NULL, ( const void* ) db );
    if ( rc != 0 )
        SYSTEM_ERROR ( rc, "failed to add db pair '%s'", db -> full_spec );
}


/* AddTablePair
 *  called from implementations
 */
void DbPairAddTablePair ( DbPair *self, const ctx_t *ctx, TablePair *tbl )
{
    if ( tbl != NULL )
    {
        FUNC_ENTRY ( ctx );

        rc_t rc;

        STATUS ( 4, "adding table pair '%s'", tbl -> full_spec );
        rc = VectorAppend ( & self -> tbls, NULL, ( const void* ) tbl );
        if ( rc != 0 )
            SYSTEM_ERROR ( rc, "failed to add table pair '%s'", tbl -> full_spec );
    }
}


/* MakeDirPair
 */
DirPair *DbPairMakeDirPair ( DbPair *self, const ctx_t *ctx, const char *name, bool required,
    DirPair* ( * make ) ( DbPair *self, const ctx_t *ctx, const KDirectory *src, KDirectory *dst, const char *name ) )
{
    FUNC_ENTRY ( ctx );

    rc_t rc;
    DirPair *dir = NULL;
    const KDatabase *sdb;
                
    STATUS ( 4, "creating directory pair '%s./%s'", self -> full_spec, name );

    rc = VDatabaseOpenKDatabaseRead ( self -> sdb, & sdb );
    if ( rc != 0 )
        INTERNAL_ERROR ( rc, "VDatabaseOpenKDatabaseRead: failed to access KDatabase 'src.%s'", self -> full_spec );
    else
    {
        KDatabase *ddb;
        rc = VDatabaseOpenKDatabaseUpdate ( self -> ddb, & ddb );
        if ( rc != 0 )
            INTERNAL_ERROR ( rc, "VDatabaseOpenKDatabaseUpdate: failed to access KDatabase 'dst.%s'", self -> full_spec );
        else
        {
            const KDirectory *sdir;
            rc = KDatabaseOpenDirectoryRead ( sdb, & sdir );
            if ( rc != 0 )
                INTERNAL_ERROR ( rc, "KDatabaseOpenDirectoryRead: failed to access KDirectory 'src.%s'", self -> full_spec );
            else
            {
                KDirectory *ddir;
                rc = KDatabaseOpenDirectoryUpdate ( ddb, & ddir );
                if ( rc != 0 )
                    INTERNAL_ERROR ( rc, "KDatabaseOpenDirectoryUpdate: failed to access KDirectory 'dst.%s'", self -> full_spec );
                else
                {
                    switch ( KDirectoryPathType ( sdir, "%s", name ) )
                    {
                    case kptNotFound:
                        if ( required )
                        {
                            rc = RC ( rcExe, rcDirectory, rcOpening, rcPath, rcNotFound );
                            ERROR ( rc, "required directory 'src.%s./%s' does not exist", self -> full_spec, name );
                        }
                        break;

                    case kptBadPath:
                        rc = RC ( rcExe, rcDirectory, rcOpening, rcPath, rcInvalid );
                        INTERNAL_ERROR ( rc, "directory path 'src.%s./%s' is invalid", self -> full_spec, name );
                        break;

                    case kptDir:
                    case kptDir | kptAlias:
                    {
                        dir = ( * make ) ( self, ctx, sdir, ddir, name );
                        break;
                    }

                    default:
                        rc = RC ( rcExe, rcDirectory, rcOpening, rcPath, rcIncorrect );
                        INTERNAL_ERROR ( rc, "directory path 'src.%s./%s' is not a directory", self -> full_spec, name );
                    }

                    KDirectoryRelease ( ddir );
                }

                KDirectoryRelease ( sdir );
            }

            KDatabaseRelease ( ddb );
        }

        KDatabaseRelease ( sdb );
    }

    return dir;
}


/* AddDirPair
 *  called from implementations
 */
void DbPairAddDirPair ( DbPair *self, const ctx_t *ctx, struct DirPair *dir )
{
    if ( dir != NULL )
    {
        FUNC_ENTRY ( ctx );

        rc_t rc;

        STATUS ( 4, "adding directory pair '%s'", dir -> full_spec );
        rc = VectorAppend ( & self -> dirs, NULL, ( const void* ) dir );
        if ( rc != 0 )
            SYSTEM_ERROR ( rc, "failed to add directory pair '%s'", dir -> full_spec );
    }
}


/* Init
 */
void DbPairInit ( DbPair *self, const ctx_t *ctx, const DbPair_vt *vt,
    const VDatabase *src, VDatabase *dst, const char *name, const char *opt_full_spec )
{
    FUNC_ENTRY ( ctx );

    rc_t rc;

    if ( self == NULL )
    {
        rc = RC ( rcExe, rcDatabase, rcConstructing, rcSelf, rcNull );
        INTERNAL_ERROR ( rc, "bad DbPair" );
        return;
    }

    memset ( self, 0, sizeof * self );
    self -> vt = vt;
    VectorInit ( & self -> dbs, 0, 4 );
    VectorInit ( & self -> tbls, 0, 8 );
    VectorInit ( & self -> dirs, 0, 8 );

    if ( opt_full_spec == NULL )
        opt_full_spec = "";

    rc = VDatabaseAddRef ( self -> sdb = src );
    if ( rc != 0 )
        ERROR ( rc, "failed to duplicate db 'src.%s%s%s'", opt_full_spec, opt_full_spec [ 0 ] ? "." : "", name );
    else
    {
        rc = VDatabaseAddRef ( self -> ddb = dst );
        if ( rc != 0 )
            ERROR ( rc, "failed to duplicate db 'dst.%s%s%s'", opt_full_spec, opt_full_spec [ 0 ] ? "." : "", name );
        else
        {
            char *full_spec;

            self -> full_spec_size = string_size ( opt_full_spec ) + string_size ( name );
            if ( opt_full_spec [ 0 ] != 0  )
                ++ self -> full_spec_size;

            TRY ( full_spec = MemAlloc ( ctx, self -> full_spec_size + 1, false ) )
            {
                if ( opt_full_spec [ 0 ] != 0 )
                    rc = string_printf ( full_spec, self -> full_spec_size + 1, NULL, "%s.%s", opt_full_spec, name );
                else
                    strcpy ( full_spec, name );
                if ( rc != 0 )
                    ABORT ( rc, "miscalculated string size" );
                else
                {
                    static const char *no_exclude [] = { NULL };

                    self -> full_spec = full_spec;
                    self -> name = full_spec;
                    if ( opt_full_spec [ 0 ] != 0 )
                        self -> name += string_size ( opt_full_spec ) + 1;

                    self -> exclude_dbs = no_exclude;
                    self -> exclude_tbls = no_exclude;
                    self -> exclude_meta = no_exclude;

                    KRefcountInit ( & self -> refcount, 1, "DbPair", "init", name );
                    return;
                }
            }

            VDatabaseRelease ( self -> ddb );
            self -> ddb = NULL;
        }

        VDatabaseRelease ( self -> sdb );
        self -> sdb = NULL;
    }
}

/* Destroy
 *  destroys superclass items
 */
static
void CC DbPairReleaseDbPair ( void *item, void *data )
{
    const ctx_t *ctx = ( const void* ) data;
    FUNC_ENTRY ( ctx );
    DbPairRelease ( item, ctx );
}

static
void CC DbPairReleaseTablePair ( void *item, void *data )
{
    const ctx_t *ctx = ( const void* ) data;
    FUNC_ENTRY ( ctx );
    TablePairRelease ( item, ctx );
}

static
void CC DbPairReleaseDirPair ( void *item, void *data )
{
    const ctx_t *ctx = ( const void* ) data;
    FUNC_ENTRY ( ctx );
    DirPairRelease ( item, ctx );
}

void DbPairDestroy ( DbPair *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    rc_t rc;

    VectorWhack ( & self -> dbs, DbPairReleaseDbPair, ( void* ) ctx );
    VectorWhack ( & self -> tbls, DbPairReleaseTablePair, ( void* ) ctx );
    VectorWhack ( & self -> dirs, DbPairReleaseDirPair, ( void* ) ctx );

    MetaPairRelease ( self -> meta, ctx );

    rc = VDatabaseRelease ( self -> ddb );
    if ( rc != 0 )
        WARN ( "VDatabaseRelease failed on 'dst.%s'", self -> full_spec );
    VDatabaseRelease ( self -> sdb );

    MemFree ( ctx, ( void* ) self -> full_spec, self -> full_spec_size + 1 );

    memset ( self, 0, sizeof * self );
}
