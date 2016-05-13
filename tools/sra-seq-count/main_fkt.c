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

#include "options.h"

#include <kapp/main.h>
#include <kapp/args.h>

#include <klib/rc.h>
#include <klib/out.h>
#include <klib/text.h>

#include <os-native.h>
#include <sysalloc.h>

#include <stdlib.h>
#include <string.h>

#define OPTION_ID_ATTR         	"id_attr"
#define OPTION_FEATURE_TYPE    	"feature_type"
#define OPTION_MODE            	"mode"

#define ALIAS_ID_ATTR          	"i"
#define ALIAS_FEATURE_TYPE     	"f"
#define ALIAS_MODE     			"m"

#define DEFAULT_ID_ATTR         "gene_id"
#define DEFAULT_FEATURE_TYPE    "exon"

static const char * id_attr_usage[] 		= { "id-attr (default gene_id)", NULL };
static const char * feature_type_usage[] 	= { "feature-type (default exon)", NULL };
static const char * mode_usage[] 			= { "output-mode (norm, debug)", NULL };

OptDef sra_seq_count_options[] =
{
    { OPTION_ID_ATTR, 		ALIAS_ID_ATTR,			NULL, id_attr_usage,		1, true, false },
    { OPTION_FEATURE_TYPE, 	ALIAS_FEATURE_TYPE, 	NULL, feature_type_usage, 	1, true, false },
    { OPTION_MODE, 			ALIAS_MODE, 			NULL, mode_usage, 			1, true, false }	
};

const char UsageDefaultName[] = "sra-seq-count";

rc_t CC UsageSummary ( const char * progname )
{
    return KOutMsg ( 	"\n"
						"Usage:\n"
						"  %s <sra-accession> <gtf-file> [options]\n"
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

    if ( rc )
        progname = fullpath = UsageDefaultName;

    UsageSummary ( progname );

    KOutMsg ( "Options:\n" );
    HelpOptionLine ( ALIAS_ID_ATTR,			OPTION_ID_ATTR,			NULL, 		id_attr_usage );
    HelpOptionLine ( ALIAS_FEATURE_TYPE, 	OPTION_FEATURE_TYPE, 	NULL, 		feature_type_usage );
    HelpOptionLine ( ALIAS_MODE, 			OPTION_MODE, 			NULL, 		mode_usage );

    KOutMsg ( "\n" );	
    HelpOptionsStandard ();
    HelpVersion ( fullpath, KAppVersion() );

    return rc;
}

static rc_t get_str_option( const Args * args, const char * option_name, const char ** dst, const char * default_value )
{
    uint32_t count;
    rc_t rc = ArgsOptionCount( args, option_name, &count );
    if ( ( rc == 0 )&&( count > 0 ) )
        rc = ArgsOptionValue( args, option_name, 0, (const void **)dst );
	else
		(*dst) = string_dup_measure ( default_value, NULL );
    return rc;
}


static rc_t gather_options( const Args * args, struct sra_seq_count_options * options )
{
	rc_t rc;

	memset ( options, 0, sizeof *options );
	rc = get_str_option( args, OPTION_ID_ATTR, &options->id_attrib, DEFAULT_ID_ATTR );
	if ( rc == 0 )
		rc = get_str_option( args, OPTION_FEATURE_TYPE, &options->feature_type, DEFAULT_FEATURE_TYPE );
	if ( rc == 0 )
	{
		const char * mode;
		rc = get_str_option( args, OPTION_MODE, &mode, "norm" );
		if ( rc == 0 )
		{
			if ( mode[ 0 ] == 'd' && mode[ 1 ] == 'e' && mode[ 2 ] == 'b' && mode[ 3 ] == 'u' &&
				 mode[ 4 ] == 'g' && mode[ 5 ] == 0 )
			{
				options -> output_mode = SSC_MODE_DEBUG;
			}
		}
	}
	
	if ( rc == 0 )
	{
		uint32_t count;
		rc = ArgsParamCount( args, &count );
		if ( rc == 0 )
		{
			if ( count == 2 )
			{
				rc = ArgsParamValue( args, 0, (const void **)&options->sra_accession );
				if ( rc == 0 )
					rc = ArgsParamValue( args, 1, (const void **)&options->gtf_file );
				if ( rc == 0 )
					options -> valid = true;
			}
			else
			{
				UsageSummary ( UsageDefaultName );
			}
		}
	}
	return rc;
}


static rc_t report_options( const struct sra_seq_count_options * options )
{
	rc_t rc = KOutMsg( "accession    : %s\n", options->sra_accession );
	if ( rc == 0 )
		rc =  KOutMsg( "gtf-file     : %s\n", options->gtf_file );
	if ( rc == 0 )
		rc =  KOutMsg( "id-attr      : %s\n", options->id_attrib );
	if ( rc == 0 )
		rc =  KOutMsg( "feature-type : %s\n", options->feature_type );
	if ( rc == 0 )
	{
		switch ( options->output_mode )
		{
			case SSC_MODE_NORMAL : rc =  KOutMsg( "output-mode  : mormal\n" ); break;
			case SSC_MODE_DEBUG  : rc =  KOutMsg( "output-mode  : debug\n" ); break;
			default              : rc =  KOutMsg( "output-mode  : unknown\n" ); break;
		}
	}
	return rc;
}


rc_t CC KMain ( int argc, char *argv [] )
{
    Args * args;
    rc_t rc = ArgsMakeAndHandle ( &args, argc, argv, 1,
								  sra_seq_count_options, 
								  ( sizeof sra_seq_count_options / sizeof sra_seq_count_options [ 0 ] ) );
    if ( rc == 0 )
    {
        struct sra_seq_count_options options;
		rc = gather_options( args, &options );
		if ( rc == 0 && options.valid )
		{
			rc = report_options( &options );
			if ( rc == 0 )
			{
				rc = matching( &options );	/* here we are calling into C++ */
			}
		}
        ArgsWhack ( args );
    }
    return rc;
}
