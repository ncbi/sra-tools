#include <kdb/manager.h>
#include <kdb/database.h>
#include <kdb/table.h>
#include <kdb/column.h>
#include <kdb/namelist.h>
#include <klib/namelist.h>
#include <klib/rc.h>

#include <stdio.h>
#include <inttypes.h>

static int64_t blob_limit = 10;

static
uint32_t dump_col ( const KColumn *col, const char *dbname, const char *tblname, const char *colname )
{
    rc_t rc = 0;
    int64_t row, first;
    const KColumnBlob *blob;
    uint32_t count, num_blobs;

    for ( num_blobs = 0, row = 1; rc == 0 && num_blobs < blob_limit; ++ num_blobs, row = first + count )
    {
        rc = KColumnOpenBlobRead ( col, & blob, row );
        if ( rc == 0 )
        {
            rc = KColumnBlobIdRange ( blob, & first, & count );
            if ( rc != 0 )
                fprintf ( stderr, "failed to get row-range for blob containing row '%s.%s.%s.%" PRId64 "'\n", dbname, tblname, colname, row );
            else
            {
                size_t num_read, remaining;
                rc = KColumnBlobRead ( blob, 0, & first, 0, & num_read, & remaining );
                if ( rc != 0 )
                    fprintf ( stderr, "failed to get size of blob containing row '%s.%s.%s.%" PRId64 "'\n", dbname, tblname, colname, row );
                else
                {
                    printf ( "  %" PRId64 " .. %" PRId64 " ( %u rows ), %zu bytes\n", first, first + count - 1, count, remaining );
                }
            }

            KColumnBlobRelease ( blob );
        }
        else if ( GetRCState ( rc ) == rcNotFound )
            return num_blobs;
        else
        {
            fprintf ( stderr, "failed to open blob for row '%s.%s.%s.%" PRId64 "'\n", dbname, tblname, colname, row );
        }
    }

    for ( ; rc == 0; ++ num_blobs, row = first + count )
    {
        rc = KColumnOpenBlobRead ( col, & blob, row );
        if ( rc == 0 )
        {
            rc = KColumnBlobIdRange ( blob, & first, & count );
            KColumnBlobRelease ( blob );
        }
    }

    return num_blobs;
}

static
void dump_col_name ( const KTable *tbl, const char *dbname, const char *tblname, const char *colname )
{
    const KColumn *col;
    rc_t rc = KTableOpenColumnRead ( tbl, & col, "%s", colname );
    if ( rc != 0 )
        fprintf ( stderr, "failed to open column '%s.%s.%s'\n", dbname, tblname, colname );
    else
    {
        uint32_t num_blobs;
        printf ( "COLUMN '%s.%s.%s'\n", dbname, tblname, colname );
        num_blobs = dump_col ( col, dbname, tblname, colname );
        KColumnRelease ( col );
        if ( num_blobs > blob_limit )
            printf ( "  %u blobs in column\n", num_blobs );
    }
}

static
void dump_tbl ( const KTable *tbl, const char *tblname, int argc, char *argv [] )
{
    if ( argc >= 4 )
    {
        int i;
        for ( i = 3; i < argc; ++ i )
        {
            dump_col_name ( tbl, argv [ 1 ], tblname, argv [ i ] );
        }
    }
    else
    {
        KNamelist *names;
        rc_t rc = KTableListCol ( tbl, & names );
        if ( rc != 0 )
            fprintf ( stderr, "failed to list columns for tbl '%s.%s'\n", argv [ 1 ], tblname );
        else
        {
            uint32_t count;
            rc = KNamelistCount ( names, & count );
            if ( rc != 0 )
                fprintf ( stderr, "failed to count columns for tbl '%s.%s'\n", argv [ 1 ], tblname );
            else
            {
                uint32_t i;
                for ( i = 0; i < count; ++ i )
                {
                    const char *name;
                    rc = KNamelistGet ( names, i, & name );
                    if ( rc != 0 )
                        fprintf ( stderr, "failed to access column name [ %u ] for tbl '%s.%s'\n", i, argv [ 1 ], tblname );
                    else
                    {
                        dump_col_name ( tbl, argv [ 1 ], tblname, name );
                    }
                }
            }

            KNamelistRelease ( names );
        }
    }
}

static
void dump_tbl_name ( const KDatabase *db, const char *tblname, int argc, char *argv [] )
{
    const KTable *tbl;
    rc_t rc = KDatabaseOpenTableRead ( db, & tbl, "%s", tblname );
    if ( rc != 0 )
        fprintf ( stderr, "failed to open table '%s.%s'\n", argv [ 1 ], tblname );
    else
    {
        dump_tbl ( tbl, tblname, argc, argv );
        KTableRelease ( tbl );
    }
}

static
void dump_db ( const KDatabase *db, int argc, char *argv [] )
{
    if ( argc >= 3 )
        dump_tbl_name ( db, argv [ 2 ], argc, argv );
    else
    {
        KNamelist *names;
        rc_t rc = KDatabaseListTbl ( db, & names );
        if ( rc != 0 )
            fprintf ( stderr, "failed to list tables for db '%s'\n", argv [ 1 ] );
        else
        {
            uint32_t count;
            rc = KNamelistCount ( names, & count );
            if ( rc != 0 )
                fprintf ( stderr, "failed to count tables for db '%s'\n", argv [ 1 ] );
            else
            {
                uint32_t i;
                for ( i = 0; i < count; ++ i )
                {
                    const char *name;
                    rc = KNamelistGet ( names, i, & name );
                    if ( rc != 0 )
                        fprintf ( stderr, "failed to access table name [ %u ] for db '%s'\n", i, argv [ 1 ] );
                    else
                    {
                        dump_tbl_name ( db, name, argc, argv );
                    }
                }
            }

            KNamelistRelease ( names );
        }
    }
}

int main ( int argc, char *argv [] )
{
    if ( argc < 2 )
        printf ( "Usage: %s database [ table [ col ... ] ]\n", argv [ 0 ] );
    else
    {
        const KDBManager *mgr;
        rc_t rc = KDBManagerMakeRead ( & mgr, NULL );
        if ( rc == 0 )
        {
            const KDatabase *db;
            rc = KDBManagerOpenDBRead ( mgr, & db, "%s", argv [ 1 ] );
            if ( rc != 0 )
                fprintf ( stderr, "failed to open database '%s'\n", argv [ 1 ] );
            else
            {
                dump_db ( db, argc, argv );
                KDatabaseRelease ( db );
            }

            KDBManagerRelease ( mgr );
        }
    }

    return 0;
}
