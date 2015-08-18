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

#include "sra-seq-count.vers.h"

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
#define OPTION_REF_TRANS       	"translation"
#define OPTION_MAX_GENES       	"max-genes"
#define OPTION_FUNCTION       	"func"
#define OPTION_COMPARE       	"compare"
#define OPTION_REFERENCES      	"refs"
#define OPTION_MAPQ      		"mapq"

#define ALIAS_ID_ATTR          	"i"
#define ALIAS_FEATURE_TYPE     	"f"
#define ALIAS_MODE     			"m"
#define ALIAS_REF_TRANS       	"t"
#define ALIAS_MAX_GENES       	"x"
#define ALIAS_COMPARE       	"c"
#define ALIAS_REFERENCES       	"r"
#define ALIAS_MAPQ       		"q"

#define DEFAULT_ID_ATTR         "gene_id"
#define DEFAULT_FEATURE_TYPE    "exon"

static const char * id_attr_usage[] 		= { "id-attr (default gene_id)", NULL };
static const char * feature_type_usage[] 	= { "feature-type (default exon)", NULL };
static const char * mode_usage[] 			= { "output-mode (norm, debug)", NULL };
static const char * ref_trans_usage[] 		= { "translation of ref-names(file)", NULL };
static const char * max_genes_usage[] 		= { "max. number of genes", NULL };
static const char * func_usage[] 			= { "alternative functions (ref)", NULL };
static const char * compare_usage[] 		= { "compare-mode (1,2,...)", NULL };
static const char * references_usage[] 		= { "restrict to these references (chr3,chr5)", NULL };
static const char * mapq_usage[] 			= { "min. mapq-value", NULL };

OptDef sra_seq_count_options[] =
{
    { OPTION_ID_ATTR, 		ALIAS_ID_ATTR,			NULL, id_attr_usage,		1, true, false },
    { OPTION_FEATURE_TYPE, 	ALIAS_FEATURE_TYPE, 	NULL, feature_type_usage, 	1, true, false },
    { OPTION_MODE, 			ALIAS_MODE, 			NULL, mode_usage, 			1, true, false },
    { OPTION_REF_TRANS,		ALIAS_REF_TRANS, 		NULL, ref_trans_usage,		1, true, false },
    { OPTION_MAX_GENES,		ALIAS_MAX_GENES, 		NULL, max_genes_usage,		1, true, false },
    { OPTION_FUNCTION,		NULL, 					NULL, func_usage,			1, true, false },
    { OPTION_COMPARE,		ALIAS_COMPARE, 			NULL, compare_usage,		1, true, false },
    { OPTION_REFERENCES,	ALIAS_REFERENCES, 		NULL, references_usage,	1, true, false },
    { OPTION_MAPQ,			ALIAS_MAPQ, 			NULL, mapq_usage,			1, true, false }
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
    HelpOptionLine ( ALIAS_REF_TRANS,		OPTION_REF_TRANS,		NULL, 		ref_trans_usage );
    HelpOptionLine ( ALIAS_COMPARE,			OPTION_COMPARE,			NULL, 		compare_usage );
    HelpOptionLine ( ALIAS_REFERENCES,		OPTION_REFERENCES,		NULL, 		references_usage );
	
    KOutMsg ( "\n" );	
    HelpOptionsStandard ();
    HelpVersion ( fullpath, KAppVersion() );

    return rc;
}

ver_t CC KAppVersion ( void )
{
    return SRA_SEQ_COUNT_VERS;
}


static rc_t get_str_option( const Args * args, const char * option_name, const char ** dst, const char * default_value )
{
    uint32_t count;
    rc_t rc = ArgsOptionCount( args, option_name, &count );
    if ( ( rc == 0 )&&( count > 0 ) )
	{
		const void *v;
        rc = ArgsOptionValue( args, option_name, 0, &v );
		if ( rc == 0 ) *dst = ( const char * )v;
	}
	else
	{
		if ( default_value != NULL )
			(*dst) = string_dup_measure ( default_value, NULL );
		else
			(*dst) = NULL;
	}
    return rc;
}


static rc_t get_bool_option( const Args * args, const char * option_name, bool * dst )
{
    uint32_t count;
    rc_t rc = ArgsOptionCount( args, option_name, &count );
    *dst = ( ( rc == 0 )&&( count > 0 ) );
	return rc;
}


static rc_t get_int_option( const Args * args, const char * option_name, int * dst )
{
	const char * s_value = NULL;
	rc_t rc = get_str_option( args, option_name, &s_value, NULL );
    if ( rc == 0 && s_value != NULL )
		*dst = atoi( s_value );
	else
		*dst = 0;
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
		rc = get_str_option( args, OPTION_REF_TRANS, &options->ref_trans, NULL );
	if ( rc == 0 )
		rc = get_str_option( args, OPTION_REFERENCES, &options->refs, NULL );
	if ( rc == 0 )
		rc = get_int_option( args, OPTION_MAPQ, &options->mapq );
	if ( rc == 0 )
	{
		const char * s;
		rc = get_str_option( args, OPTION_MAX_GENES, &s, "0" );
		if ( rc ==  0 )
			options->max_genes = atoi( s );
	}
	if ( rc == 0 )
		rc = get_str_option( args, OPTION_FUNCTION, &options->function, NULL );
	if ( rc == 0 )
		rc = get_bool_option( args, OPTION_QUIET, &options->silent );
	
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
				const void * v;
				rc = ArgsParamValue( args, 0, &v );
				if ( rc == 0 )
				{
					options->sra_accession = ( const char * )v;
					rc = ArgsParamValue( args, 1, &v );
					if ( rc == 0 )
						options->gtf_file = ( const char * )v;
				}
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
	rc_t rc = KOutMsg( "\naccession    : %s\n", options->sra_accession );
	if ( rc == 0 )
		rc =  KOutMsg( "gtf-file     : %s\n", options->gtf_file );
	if ( rc == 0 )
		rc =  KOutMsg( "id-attr      : %s\n", options->id_attrib );
	if ( rc == 0 )
		rc =  KOutMsg( "feature-type : %s\n", options->feature_type );
	if ( rc == 0 )
		rc =  KOutMsg( "mapq         : %d\n", options->mapq );
	if ( rc == 0 )
	{
		if ( options->refs == NULL )
			rc =  KOutMsg( "references   : process all references found in the accession\n" );
		else
			rc =  KOutMsg( "references   : restrict to %s\n", options->refs );
	}
	if ( rc == 0 )
	{
		switch ( options->output_mode )
		{
			case SSC_MODE_NORMAL : rc =  KOutMsg( "output-mode  : mormal\n" ); break;
			case SSC_MODE_DEBUG  : rc =  KOutMsg( "output-mode  : debug\n" ); break;
			default              : rc =  KOutMsg( "output-mode  : unknown\n" ); break;
		}
	}
	if ( rc == 0 )
	{
		if ( options->ref_trans != NULL )
			rc =  KOutMsg( "translation  : %s\n", options->ref_trans );
		else
			rc =  KOutMsg( "translation  : none\n" );
	}
	if ( rc == 0 && options->max_genes > 0 )
		rc =  KOutMsg( "max genes    : %d\n", options->max_genes );
	
	return rc;
}


static rc_t main_function( const struct sra_seq_count_options * options )
{
	rc_t rc = 0;
	if ( !options->silent )
		rc = report_options( options );
	if ( rc == 0 )
		rc = perform_seq_counting_2( options );	/* here we are calling into C++ */
	return rc;
}


static rc_t report_refs_function( const struct sra_seq_count_options * options )
{
	list_gtf_refs( options );
	list_acc_refs( options );
	return 0;
}


static rc_t perform_test( const struct sra_seq_count_options * options )
{
	/* index_gtf( &options ); */
	token_tests( options );
	return 0;
}

static rc_t perform_sorted_test( const struct sra_seq_count_options * options )
{
	rc_t rc = KOutMsg( "testing gtf file and accession for being sorted\n" );
	
	if ( rc == 0 )
	{
		if ( is_gtf_sorted( options ) )
			rc = KOutMsg( "gtf file is sorted!\n" );
		else
			rc = KOutMsg( "gtf is not sorted!\n" );
	}
	
	if ( rc == 0 )
	{
		if ( is_acc_sorted( options ) )
			rc =  KOutMsg( "accession is sorted!\n" );
		else
			rc =  KOutMsg( "accession is not sorted!\n" );		
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
			if ( options.function == NULL )
				rc = main_function( &options );
			else
			{
				switch ( options.function[ 0 ] )
				{
					/* "ref" or "Ref" ... report reference-names... */
					case 'R' : 	;
					case 'r' : rc = report_refs_function( &options ); break;

					/* "test" or "Test" ... perform range tests... */
					case 'T' : ;
					case 't' : rc = perform_test( &options ); break;

					/* "sorted" or "Sorted" ... perform sorted test... */
					case 'S' : ;
					case 's' : rc = perform_sorted_test( &options ); break;
				}
			}
		}
        ArgsWhack ( args );
    }
    return rc;
}
