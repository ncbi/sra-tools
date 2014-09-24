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

#ifndef _h_definitions_
#define _h_definitions_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _h_klib_defs_
#include <klib/defs.h>
#endif

#ifndef _h_klib_out_
#include <klib/out.h>
#endif

#ifndef _h_klib_rc_
#include <klib/rc.h>
#endif

#define DISP_RC(rc,err) (void)((rc == 0) ? 0 : LogErr( klogInt, rc, err ))

#define DISP_RC2(rc,err,succ) \
    (void)((rc != 0)? 0 : (succ) ? LOGMSG( klogInfo, succ ) : LogErr( klogInt, rc, err ))

#define DB_COPY_ENABLED 1

#define ALLOW_EXTERNAL_CONFIG 1

#define SRA_PF_UNDEF 0
#define SRA_PF_454 1
#define SRA_PF_ILLUMINA 2
#define SRA_PF_ABSOLID 3
#define SRA_PF_COMPLETE_GENOMICS 4
#define SRA_PF_HELICOS 5
#define SRA_PF_UNKNOWN 6

#define SRA_READ_FILTER_PASS 0
#define SRA_READ_FILTER_REJECT 1
#define SRA_READ_FILTER_CRITERIA 2
#define SRA_READ_FILTER_REDACTED 3

#define MD5_MODE_OFF 1
#define MD5_MODE_ON 2
#define MD5_MODE_AUTO 3

#define BLOB_CHECKSUM_OFF 1
#define BLOB_CHECKSUM_CRC32 2
#define BLOB_CHECKSUM_MD5 3
#define BLOB_CHECKSUM_AUTO 4

#define TYPESPEC_BUF_LEN 128

#define VDB_COPY_PREFIX "/VDBCOPY/"

#define READ_FILTER_COL_NAME_KEY "/VDBCOPY/READ_FILTER_COL_NAME"
#define READ_FILTER_COL_NAME "READ_FILTER"

#define REDACTABLE_LIST_KEY "/VDBCOPY/REDACTVALUE/TYPES"

#define REDACTABLE_TYPES_KEY "/VDBCOPY/REDACTABLE_TYPES"
#define REDACTABLE_TYPES "INSDC:dna:text,INSDC:4na:bin,INSDC:4na:packed,INSDC:2na:bin,INSDC:x2na:bin,INSDC:2na:packed,INSDC:color:text,INSDC:2cs:bin,INSDC:x2cs:bin,INSDC:2cs:packed,INSDC:quality:phred,INSDC:quality:log_odds,INSDC:quality:text:phred_33,INSDC:quality:text:phred_64,INSDC:quality:text:log_odds_64,INSDC:position:zero,INSDC:position:one,NCBI:isamp1,NCBI:fsamp1,NCBI:fsamp4,NCBI:SRA:pos16,NCBI:SRA:encoded_qual4"

#define DO_NOT_REDACT_KEY "/VDBCOPY/DO_NOT_REDACT"

#define META_IGNORE_NODES_KEY "/VDBCOPY/META/IGNORE"
#define META_IGNROE_NODES_DFLT "col,.seq,STATS"

#define TYPE_SCORE_PREFIX "/VDBCOPY/SCORE/"

#define LEGACY_SCHEMA_KEY "/schema"
#define LEGACY_TAB_KEY "/tab"
#define LEGACY_EXCLUDE_KEY "/dont_use"

#define REDACTVALUE_PREFIX "/VDBCOPY/REDACTVALUE/"
#define REDACTVALUE_VALUE_POSTFIX "/VALUE"
#define REDACTVALUE_LEN_POSTFIX "/LEN"

#ifdef __cplusplus
}
#endif

#endif
