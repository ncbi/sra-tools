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

/*==============================================================================

  search/nucstrstr.h

    nucleotide k-mer searching facility
 */


#ifndef _h_search_nucstrstr_
#define _h_search_nucstrstr_

#ifndef _h_klib_defs_
#include <klib/defs.h>
#endif

#include <search/extern.h>

#ifdef __cplusplus
extern "C" {
#endif

/* set to 1 if buffer supplied to NucStrstrSearch is
   always at least 16 bytes beyond than the sequence */
#ifndef ENDLESS_BUFFER
#define ENDLESS_BUFFER 1
#endif

/* normally used for test only */
#ifndef ENABLE_AT_EXPR
#define ENABLE_AT_EXPR 0
#endif

/* positional returns make little sense with Boolean logic */
#ifndef ALLOW_POSITIONAL_OPERATOR_MIX
#define ALLOW_POSITIONAL_OPERATOR_MIX 1
#endif


/*--------------------------------------------------------------------------
 * NucStrstr
 *  prepared handle for nucleotide k-mer strstr
 */
typedef union NucStrstr NucStrstr;

/* NucStrstrMake
 *  prepares search by parsing expression query string
 *  returns error if conversion was not possible.
 *
 *  "nss" [ OUT ] - return parameter for one-time search handle
 *
 *  "positional" [ IN ] - if non-zero, build an expression tree
 *  to return found position rather than simply a Boolean found.
 *  see NucStrstrSearch. if ENABLE_AT_EXPR is defined as non-zero,
 *  the <position_expr> production can switch this flag to true
 *  for the immediately following fasta_expr from within the
 *  expression string itself. if ALLOW_POSITIONAL_OPERATOR_MIX
 *  is defined as non-zero, operators '!', '&&' and '||' will
 *  be allowed when generating positional results. they will make
 *  no sense otherwise.
 *
 *  "query" [ IN ] and "len" [ IN ] - query string expression, such that:
 *       expr           : <unary_expr>
 *                      | <unary_expr> <boolean_op> <expr>
 *       unary_expr     : <primary_expr>
 *                      | '!' <unary_expr>
 *       primary_expr   : <position_expr>
 *                      | '^' <position_expr>
 *                      | <position_expr> '$'
 *                      | '(' <expr> ')'
 *       position_expr  : <fasta_expr>
 *                      | '@' <fasta_expr>
 *       fasta_expr     : FASTA
 *                      | "'" FASTA "'"
 *                      | '"' FASTA '"'
 *       boolean_op     : '&', '|', '&&', '||'
 *
 *    where the '@' operator may be used to force the "positional"
 *    flag to be true - conditionally enabled.
 *
 *  return values:
 *    EINVAL - invalid parameter or invalid expression
 */
SEARCH_EXTERN int CC NucStrstrMake ( NucStrstr **nss, int positional,
    const char *query, unsigned int len );

/* NucStrstrWhack
 *  discard structure when no longer needed
 */
SEARCH_EXTERN void CC NucStrstrWhack ( NucStrstr *self );

/* NucStrstrSearch
 *  search buffer from starting position
 *
 *  "ncbi2na" [ IN ] - pointer to 2na data
 *
 *    N.B. - "ncbi2na" must be at least long enough to allow for
 *    full 16-byte loads at the tail of the search sequence:
 *
 *    - technically, the size requirement is:
 *     ( ( pos + len + 3 ) / 4 ) == bytes,
 *     ( ( bytes + 15 ) / 16 ) * 16 == min data size
 *
 *    - practically, if "ncbi2na" points to a data buffer
 *      that is at least 16 bytes longer than "bytes" above,
 *      then it is always sufficient, and sometimes excessive.
 *
 *  "pos" [ IN ] - starting base position for search,
 *  relative to "ncbi2na". may be >= 4.
 *
 *  "len" [ IN ] - the number of bases to include in
 *  the search, relative to "pos".
 *
 *  return values:
 *    0 if the pattern was not found
 *    1..N indicating starting position if "self" was
 *      prepared for positional return
 *   !0 if "self" was prepared normally by NucStrstrMake
 */
SEARCH_EXTERN int CC NucStrstrSearch ( const NucStrstr *self,
    const void *ncbi2na, unsigned int pos, unsigned int len,unsigned int* selflen );

#ifdef __cplusplus
}
#endif

#endif /* _h_search_nucstrstr_ */

