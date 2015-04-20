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

#ifndef _h_cigar_
#define _h_cigar_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cigar_opt
{
	char op;
	uint32_t count;
	struct cigar_opt * next;
} cigar_opt;

cigar_opt * parse_cigar( const char * cigar );
void free_cigar( cigar_opt * cigar );
uint32_t calc_reflen( const cigar_opt * cigar );
uint32_t calc_readlen( const cigar_opt * cigar );
uint32_t calc_inslen( const cigar_opt * cigar );
char * to_string( const cigar_opt * cigar );
cigar_opt * merge_match_and_mismatch( const cigar_opt * cigar );
char * produce_read( const cigar_opt * cigar, const char * ref_bases, const char * ins_bases );

#ifdef __cplusplus
}
#endif

#endif
