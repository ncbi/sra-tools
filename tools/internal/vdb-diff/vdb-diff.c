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

#include <kapp/main.h>

#include <klib/rc.h>
#include <klib/out.h>
#include <klib/log.h>
#include <klib/num-gen.h>
#include <klib/progressbar.h>

#include <vdb/manager.h>
#include <vdb/schema.h>
#include <vdb/table.h>
#include <vdb/cursor.h>
#include <vdb/database.h>

#include <sra/sraschema.h>

#include "coldefs.h"
#include "namelist_tools.h"
#include "vdb-diff-context.h"
#include "row_by_row.h"
#include "col_by_col.h"

#include <stdlib.h>
#include <string.h>

static const char * rows_usage[] = { "set of rows to be comparend (default = all)", NULL };
static const char * columns_usage[] = { "set of columns to be compared (default = all)", NULL };
static const char * table_usage[] = { "name of table (in case of database ) to be compared", NULL };
static const char * progress_usage[] = { "show progress in percent", NULL };
static const char * maxerr_usage[] = { "max errors im comparing (default = 1)", NULL };
static const char * intersect_usage[] = { "intersect column-set from both runs", NULL };
static const char * exclude_usage[] = { "exclude these columns from comapring", NULL };
static const char * columnwise_usage[] = { "exclude these columns from comapring", NULL };

OptDef MyOptions[] =
{
    { OPTION_ROWS, 			ALIAS_ROWS, 		NULL, 	rows_usage, 		1, 	true, 	false },
	{ OPTION_COLUMNS, 		ALIAS_COLUMNS, 		NULL, 	columns_usage, 		1, 	true, 	false },
	{ OPTION_TABLE, 		ALIAS_TABLE, 		NULL, 	table_usage, 		1, 	true, 	false },
	{ OPTION_PROGRESS, 		ALIAS_PROGRESS,		NULL, 	progress_usage,		1, 	false, 	false },
	{ OPTION_MAXERR, 		ALIAS_MAXERR,		NULL, 	maxerr_usage,		1, 	true, 	false },
	{ OPTION_INTERSECT,		ALIAS_INTERSECT,	NULL, 	intersect_usage,	1, 	false, 	false },
	{ OPTION_EXCLUDE,		ALIAS_EXCLUDE,		NULL, 	exclude_usage,		1, 	true, 	false },
    { OPTION_COLUMNWISE,    ALIAS_COLUMNWISE,   NULL,   columnwise_usage,   1,  false,  false }
};

const char UsageDefaultName[] = "vdb-diff";

rc_t CC UsageSummary ( const char * progname )
{
    return KOutMsg (
        "\n"
        "Usage:\n"
        "  %s <src1_path> <src2_path> [options]\n"
        "\n", progname );
}

rc_t CC Usage ( const Args * args )
{
    const char * progname = UsageDefaultName;
    const char * fullpath = UsageDefaultName;
    rc_t rc;

    if ( args == NULL )
        rc = RC ( rcApp, rcArgv, rcAccessing, rcSelf, rcNull );
    else
        rc = ArgsProgram ( args, &fullpath, &progname );

    if ( rc != 0 )
        progname = fullpath = UsageDefaultName;

    UsageSummary ( progname );

    KOutMsg ( "Options:\n" );
    HelpOptionLine ( ALIAS_ROWS, 		OPTION_ROWS, 		"row-range",	rows_usage );
    HelpOptionLine ( ALIAS_COLUMNS, 	OPTION_COLUMNS, 	"column-set",	columns_usage );
	HelpOptionLine ( ALIAS_TABLE, 		OPTION_TABLE, 		"table-name",	table_usage );
	HelpOptionLine ( ALIAS_PROGRESS, 	OPTION_PROGRESS,	NULL,			progress_usage );
	HelpOptionLine ( ALIAS_MAXERR, 		OPTION_MAXERR,	    "max value",	maxerr_usage );
	HelpOptionLine ( ALIAS_INTERSECT, 	OPTION_INTERSECT,   NULL,			intersect_usage );
	HelpOptionLine ( ALIAS_EXCLUDE, 	OPTION_EXCLUDE,   	"column-set",	exclude_usage );
	HelpOptionLine ( ALIAS_COLUMNWISE, 	OPTION_COLUMNWISE, 	NULL,	        columnwise_usage );

    HelpOptionsStandard ();
    HelpVersion ( fullpath, KAppVersion() );

    return rc;
}

static rc_t perform_table_diff( const VTable * tab_1, const VTable * tab_2,
                                const struct diff_ctx * dctx, const char * tablename,
                                unsigned long int *diffs  )
{
    KNamelist * cols_1;
    rc_t rc = VTableListReadableColumns( tab_1, &cols_1 );
    if ( rc != 0 )
    {
        LOGERR ( klogInt, rc, "VTableListReadableColumns( acc #1 ) failed" );
    }
    else
    {
        KNamelist * cols_2;
        rc = VTableListReadableColumns( tab_2, &cols_2 );
        if ( rc != 0 )
        {
            LOGERR ( klogInt, rc, "VTableListReadableColumns( acc #2 ) failed" );
        }
        else
        {
            const KNamelist * cols_to_diff;

            if ( dctx -> columns == NULL )
                rc = compare_2_namelists( cols_1, cols_2, &cols_to_diff, dctx -> intersect );
            else
                rc = extract_columns_from_2_namelists( cols_1, cols_2, dctx -> columns, &cols_to_diff );

            if ( rc == 0 && dctx -> excluded != NULL )
            {
                const KNamelist * temp;
                rc = nlt_remove_strings_from_namelist( cols_to_diff, &temp, dctx -> excluded );
                KNamelistRelease( cols_to_diff );
                cols_to_diff = temp;
            }

            if ( rc == 0 )
            {
                col_defs * defs;
                rc = col_defs_init( &defs );
                if ( rc != 0 )
                {
                    LOGERR ( klogInt, rc, "col_defs_init() failed" );
                }
                else
                {
                    rc = col_defs_fill( defs, cols_to_diff );
                    if ( rc == 0 )
                    {
                        /* ******************************************* */
                        if ( dctx -> columnwise )
                            rc = cbc_diff_columns( defs, tab_1, tab_2, dctx, tablename, diffs );
                        else
                            rc = rbr_diff_columns( defs, tab_1, tab_2, dctx, diffs );
                        /* ******************************************* */
                    }
                    col_defs_destroy( defs );
                }
                KNamelistRelease( cols_to_diff );
            }

            KNamelistRelease( cols_2 );
        }
        KNamelistRelease( cols_1 );
	}
	return rc;
}


static rc_t perform_database_diff_on_this_table( const VDatabase * db_1, const VDatabase * db_2,
												 const struct diff_ctx * dctx, const char * table_name,
                                                 unsigned long int *diffs )
{
	/* we want to compare only the table wich name was given at the commandline */
	const VTable * tab_1;
	rc_t rc = VDatabaseOpenTableRead ( db_1, &tab_1, "%s", table_name );
	if ( rc != 0 )
	{
		PLOGERR( klogInt, ( klogInt, rc,
				 "VDatabaseOpenTableRead( [$(acc)].$(tab) ) failed",
				 "acc=%s,tab=%s", dctx -> src1, table_name ) );
	}
	else
	{
		const VTable * tab_2;
		rc = VDatabaseOpenTableRead ( db_2, &tab_2, "%s", table_name );
		if ( rc != 0 )
		{
			PLOGERR( klogInt, ( klogInt, rc,
					 "VDatabaseOpenTableRead( [$(acc)].$(tab) ) failed",
					 "acc=%s,tab=%s", dctx -> src2, table_name ) );
		}
		else
		{
			/* ******************************************* */
			rc = perform_table_diff( tab_1, tab_2, dctx, table_name, diffs );
			/* ******************************************* */
			VTableRelease( tab_2 );
		}
		VTableRelease( tab_1 );
	}
	return rc;
}


static rc_t perform_database_diff( const VDatabase * db_1, const VDatabase * db_2,
                                   const struct diff_ctx * dctx, unsigned long int *diffs )
{
	rc_t rc = 0;
	if ( dctx -> table != NULL )
	{
		/* we want to compare only the table wich name was given at the commandline */
		/* ************************************************************************** */
		rc = perform_database_diff_on_this_table( db_1, db_2, dctx, dctx -> table, diffs );
		/* ************************************************************************** */
	}
	else
	{
		/* we want to compare all tables of the 2 accession, if they have the same set of tables */
		KNamelist * table_set_1;
		rc = VDatabaseListTbl ( db_1, &table_set_1 );
		if ( rc != 0 )
		{
			LOGERR ( klogInt, rc, "VDatabaseListTbl( acc #1 ) failed" );
		}
		else
		{
			KNamelist * table_set_2;
			rc = VDatabaseListTbl ( db_2, &table_set_2 );
			if ( rc != 0 )
			{
				LOGERR ( klogInt, rc, "VDatabaseListTbl( acc #2 ) failed" );
			}
			else
			{
				if ( nlt_compare_namelists( table_set_1, table_set_2, NULL ) )
				{
					/* the set of tables match, walk the first set, and perform a diff on each table */
					uint32_t count;
					rc = KNamelistCount( table_set_1, &count );
					if ( rc != 0  )
					{
						LOGERR ( klogInt, rc, "KNamelistCount() failed" );
					}
					else
					{
						uint32_t idx;
						for ( idx = 0; idx < count && rc == 0; ++idx )
						{
							const char * table_name;
							rc = KNamelistGet( table_set_1, idx, &table_name );
							if ( rc != 0 )
							{
								LOGERR ( klogInt, rc, "KNamelistGet() failed" );
							}
							else
							{
								rc = KOutMsg( "\ncomparing table: %s\n", table_name );
								if ( rc == 0 )
								{
									/* ********************************************************************* */
									rc = perform_database_diff_on_this_table( db_1, db_2, dctx, table_name, diffs );
									/* ********************************************************************* */
								}
							}
						}
					}
				}
				else
				{
					LOGERR ( klogInt, rc, "the 2 databases have not the same set of tables" );
                    ( *diffs )++;
				}
				KNamelistRelease( table_set_2 );
			}
			KNamelistRelease( table_set_1 );
		}
	}
	return rc;
}


/***************************************************************************
    perform the actual diffing
***************************************************************************/
static rc_t perform_diff( const struct diff_ctx * dctx, unsigned long int * diffs )
{
    KDirectory * dir;
    rc_t rc = KDirectoryNativeDir( &dir );
	if ( rc != 0 )
	{
		LOGERR ( klogInt, rc, "KDirectoryNativeDir() failed" );
	}
	else
    {
		const VDBManager * vdb_mgr;
		rc = VDBManagerMakeRead ( &vdb_mgr, dir );
		if ( rc != 0 )
		{
			LOGERR ( klogInt, rc, "VDBManagerMakeRead() failed" );
		}
		else
		{
			VSchema * vdb_schema = NULL;
			rc = VDBManagerMakeSRASchema( vdb_mgr, &vdb_schema );
			if ( rc != 0 )
			{
				LOGERR ( klogInt, rc, "VDBManagerMakeSRASchema() failed" );
			}
			else
			{
				const VDatabase * db_1;
				/* try to open it as a database */
				rc = VDBManagerOpenDBRead ( vdb_mgr, &db_1, vdb_schema, "%s", dctx -> src1 );
				if ( rc == 0 )
				{
					const VDatabase * db_2;
					rc = VDBManagerOpenDBRead ( vdb_mgr, &db_2, vdb_schema, "%s", dctx -> src2 );
					if ( rc == 0 )
					{
						/* ******************************************** */
						rc = perform_database_diff( db_1, db_2, dctx, diffs );
						/* ******************************************** */
						VDatabaseRelease( db_2 );
					}
					else
					{
						LOGERR ( klogInt, rc, "accession #1 opened as database, but accession #2 cannot be opened as database" );
					}
					VDatabaseRelease( db_1 );
				}
				else
				{
					const VTable * tab_1;
					rc = VDBManagerOpenTableRead( vdb_mgr, &tab_1, vdb_schema, "%s", dctx -> src1 );
					if ( rc == 0 )
					{
						const VTable * tab_2;
						rc = VDBManagerOpenTableRead( vdb_mgr, &tab_2, vdb_schema, "%s", dctx -> src2 );
						if ( rc == 0 )
						{
							/* ******************************************** */
							rc = perform_table_diff( tab_1, tab_2, dctx, "SEQ", diffs );
							/* ******************************************** */
							VTableRelease( tab_2 );
						}
						else
						{
							LOGERR ( klogInt, rc, "accession #1 opened as table, but accession #2 cannot be opened as table" );
						}
						VTableRelease( tab_1 );
					}
					else
					{
						LOGERR ( klogInt, rc, "accession #1 cannot be opened as table or database" );
					}
				}
				VSchemaRelease( vdb_schema );
			}
			VDBManagerRelease( vdb_mgr );
		}
		KDirectoryRelease( dir );
	}
	return rc;
}


/***************************************************************************
    Main:
***************************************************************************/
MAIN_DECL( argc, argv )
{
    VDB_INITIALIZE( argc, argv, VDB_INIT_FAILED );

    unsigned long int diffs = 0;
    Args * args;

    SetUsage( Usage );
    SetUsageSummary( UsageSummary );

    rc_t rc = ArgsMakeAndHandle ( &args, argc, argv, 1,
                                  MyOptions, sizeof MyOptions / sizeof ( OptDef ) );
    if ( rc != 0 )
	{
		LOGERR ( klogInt, rc, "ArgsMakeAndHandle failed()" );
	}
    else
    {
        struct diff_ctx dctx;

        KLogHandlerSetStdErr();
        init_diff_ctx( &dctx );
        rc = gather_diff_ctx( &dctx, args );
        if ( rc == 0 )
        {
            rc = report_diff_ctx( &dctx );
			if ( rc == 0 )
			{
				/* ***************************** */
				rc = perform_diff( &dctx, &diffs );
				/* ***************************** */
			}
        }
        else
        {
            Usage ( args );
        }
        release_diff_ctx( &dctx );

        ArgsWhack ( args );
    }

    if ( diffs > 0 ) rc = RC( rcExe, rcNoTarg, rcComparing, rcRow, rcInconsistent );
    {
        bool b = true;
        while( b )
        {
            rc_t rc1;
            const char * filename;
            const char * funcname;
            uint32_t lineno;
            b = GetUnreadRCInfo ( &rc1, &filename, &funcname, &lineno );
        }
    }
    KOutMsg( "%lu differences discovered ( rc = %d )\n", diffs, rc );
    return VDB_TERMINATE( rc );
}
