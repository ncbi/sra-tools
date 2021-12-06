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

#include <stddef.h>

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
