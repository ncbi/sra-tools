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

#include "dflt_defline.h"

#ifndef _h_klib_text_
#include <klib/text.h>
#endif

static const char * DSD_FASTQ_USE_NAME_RDID = "@$ac.$si/$ri $sn length=$rl";
static const char * DSD_FASTQ_SYN_NAME_RDID = "@$ac.$si/$ri $si length=$rl";
static const char * DSD_FASTA_USE_NAME_RDID = ">$ac.$si/$ri $sn length=$rl";
static const char * DSD_FASTA_SYN_NAME_RDID = ">$ac.$si/$ri $si length=$rl";

static const char * DSD_FASTQ_USE_NAME = "@$ac.$si $sn length=$rl";
static const char * DSD_FASTQ_SYN_NAME = "@$ac.$si $si length=$rl";
static const char * DSD_FASTA_USE_NAME = ">$ac.$si $sn length=$rl";
static const char * DSD_FASTA_SYN_NAME = ">$ac.$si $si length=$rl";

const char * dflt_seq_defline( bool use_name, bool use_read_id, bool fasta ) {
    if ( use_read_id ) {
        if ( fasta ) {
            return use_name ? DSD_FASTA_USE_NAME_RDID : DSD_FASTA_SYN_NAME_RDID;
        } else {
            return use_name ? DSD_FASTQ_USE_NAME_RDID : DSD_FASTQ_SYN_NAME_RDID;
        }
    } else {
        if ( fasta ) {
            return use_name ? DSD_FASTA_USE_NAME : DSD_FASTA_SYN_NAME;
        } else {
            return use_name ? DSD_FASTQ_USE_NAME : DSD_FASTQ_SYN_NAME;
        }
    }
    return NULL;
}

static const char * DQD_USE_NAME_RDID = "+$ac.$si/$ri $sn length=$rl";
static const char * DQD_SYN_NAME_RDID = "+$ac.$si/$ri $si length=$rl";

static const char * DQD_USE_NAME = "+$ac.$si $sn length=$rl";
static const char * DQD_SYN_NAME = "+$ac.$si $si length=$rl";

const char * dflt_qual_defline( bool use_name, bool use_read_id ) {
    if ( use_read_id ) {
        return use_name ? DQD_USE_NAME_RDID : DQD_SYN_NAME_RDID;
    } else {
        return use_name ? DQD_USE_NAME : DQD_SYN_NAME;
    }
    return NULL;
}

/* ------------------------------------------------------------------------------------------- */

static uint32_t var_in_line( const char * defline, const char * var ) {
    uint32_t res = 0;
    if ( NULL != defline && NULL != var ) {
        char * needle;
        do {
            needle = strstr( defline, var );
            if ( NULL != needle ) {
                res++;
                defline += string_size( var );
            }
        } while ( NULL != needle );
    }
    return res;
}

static bool var_requested( const char * defline, const char * var ) {
    if ( NULL == defline || NULL == var ) {
        return false;
    } else {
        return ( NULL != strstr( defline, var ) );
    }
}

bool spot_group_requested( const char * seq_defline, const char * qual_defline ) {
    return ( var_requested( seq_defline, "$sg" ) || var_requested( qual_defline, "$sg" ) );
}

/* ------------------------------------------------------------------------------------------- */

size_t estimate_defline_length( const defline_estimator_input_t * input ) {
    size_t res = 0;
    if ( NULL != input ) {
        if ( NULL != input -> defline ) {
            uint32_t n;
            
            res = string_size( input -> defline );

            /* if the accession is in the defline */
            n = var_in_line( input -> defline, "$ac" );
            if ( n > 0 ) {
                res -= n * 3;   /* take away the 3 chars '$ac' */
                res += n * string_size( input -> acc );
            }

            /* if the name is in the defline */
            n = var_in_line( input -> defline, "$sn" );
            if ( n > 0 ) {
                res -= n * 3;   /* take away the 3 chars '$sn' */
                res += n * input -> avg_name_len;
            }

            /* if the spot-group is in the defline */
            n = var_in_line( input -> defline, "$sg" );
            if ( n > 0 ) {
                res -= n * 3;   /* take away the 3 chars '$sg' */
                res += n * input -> avg_name_len;
            }

        }
    }
    return res;
}
