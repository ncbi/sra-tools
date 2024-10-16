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

#ifndef _h_search_ref_variation_
#define _h_search_ref_variation_

#ifndef _h_search_extern_
#include <search/extern.h>
#endif

#ifndef _h_klib_defs_
#include <klib/defs.h>
#endif

#ifndef _h_insdc_insdc_
#include <insdc/insdc.h>
#endif

#ifndef REF_VAR_ALG
#define REF_VAR_ALG 1
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if REF_VAR_ALG
/*--------------------------------------------------------------------------
 * RefVarAlg
 *  while experimenting with algorithms...
 */
typedef uint32_t RefVarAlg;
enum
{
    refvarAlgSW = 1,
    refvarAlgRA
};

#endif

/*--------------------------------------------------------------------------
 * RefVariation
 */
typedef struct RefVariation RefVariation;


/* IUPACMake
 *  make a RefVarition object from IUPAC sequence data
 *
 *  "obj" [ OUT ] - return parameter for object
 *
 *  "ref" [ IN ] and "ref_len" [ IN ] - reference sequence
 *  against which the variation is expressed
 *
 *  "deletion_pos" [ IN ] and "deletion_len" [ IN, ZERO OKAY ] - location
 *  and length of deletion portion of variation, as expressed against "ref".
 *  these describe the portion of "ref" to be replaced.
 *
 *  "insertion" [ IN ] and "insertion_len" [ IN, ZERO OKAY ] - sequence to
 *  be inserted at "deletion_pos" as a substitution for deleted bases.
 *
 *  for a pure insertion, "deletion_len" should be zero and "insertion_len"
 *  should be non-zero. for a pure deletion, "deletion_len" should be non-zero
 *  and "insertion_len" should be zero. otherwise, the operation describes a
 *  substitution.
 */
SEARCH_EXTERN rc_t CC RefVariationIUPACMake ( RefVariation ** obj,
        INSDC_dna_text const* ref, size_t ref_len,
        size_t deletion_pos, size_t deletion_len,
        INSDC_dna_text const* insertion, size_t insertion_len
#if REF_VAR_ALG
        , RefVarAlg alg
#endif
    );


/* AddRef
 * Release
 *  accepts NULL for self
 */
SEARCH_EXTERN rc_t CC RefVariationAddRef ( RefVariation const* self );
SEARCH_EXTERN rc_t CC RefVariationRelease ( RefVariation const* self );


/* GetIUPACSearchQuery
 *  returns a minimum query string in IUPAC alphabet
 *  this string contains sufficient bounding bases to encompass
 *  all deletions and insertions
 *
 *  "query" [ OUT ] and "query_len" [ OUT, NULL OKAY ] - return parameter
 *  for query sequence.
 *
 * NB - "query" is owned by object, and cannot be freed by caller.
 *
 *  "query_start" [ OUT, NULL OKAY ] - return parameter for start of
 *  query sequence on ( original ) reference used to create object.
 */
SEARCH_EXTERN rc_t CC RefVariationGetIUPACSearchQuery ( RefVariation const* self,
    INSDC_dna_text const ** query, size_t * query_len, size_t * query_start );


/* GetSearchQueryLenOnRef
 *  returns projected length of query sequence on reference
 *
 *  "query_len_on_ref" [ OUT ] - projected length of query sequence on reference
 */
SEARCH_EXTERN rc_t CC RefVariationGetSearchQueryLenOnRef ( RefVariation const* self, size_t * query_len_on_ref );


/* GetAllele
 *
 *  "allele" [ OUT ] and "allele_len" [ OUT ] - return parameter
 *  for allele sequence
 *
 * NB - "allele" is owned by object, and cannot be freed by caller.
 *
 *  "allele_start" [ OUT, NULL OKAY ] - return parameter for start of
 *  allele sequence on ( original ) reference used to create object.
 */
SEARCH_EXTERN rc_t CC RefVariationGetAllele ( RefVariation const* self,
    INSDC_dna_text const ** allele, size_t * allele_len, size_t * allele_start );


/* GetAlleleLenOnRef
 *  returns projected length of allele on reference
 *
 *  "allele_len_on_ref" [ OUT ] - projected length of allele on reference
 */
SEARCH_EXTERN rc_t CC RefVariationGetAlleleLenOnRef ( RefVariation const* self, size_t * allele_len_on_ref );


#ifdef __cplusplus
}
#endif

#endif /* _h_search_ref_variation_ */
