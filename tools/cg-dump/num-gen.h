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

#ifndef _h_num_gen_
#define _h_num_gen_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _h_klib_vector_
#include <klib/vector.h>
#endif

#ifndef _h_klib_rc_
#include <klib/rc.h>
#endif

#ifndef _h_klib_text_
#include <klib/text.h>
#endif


/*--------------------------------------------------------------------------
 * A NUMBER GENERATOR
 * 
 *  input : string, for instance "3,6,8,12,44-49"
 *  ouptut: sequence of integers, for instance 3,6,8,12,44,45,46,47,48,49
 */


/*--------------------------------------------------------------------------
 * opaque number-generator and it's iterator
 */
typedef struct num_gen num_gen;
typedef struct num_gen_iter num_gen_iter;


/*--------------------------------------------------------------------------
 * num_gen_make
 *
 *  creates a empty number-generator
 *  or creates a number-generator and parses the string
 *  or creates and presets it with a range
 */
rc_t num_gen_make( num_gen** self );
rc_t num_gen_make_from_str( num_gen** self, const char *src );
rc_t num_gen_make_from_range( num_gen** self, 
                              const int64_t first, const uint64_t count );


/*--------------------------------------------------------------------------
 * num_gen_destroy
 *
 *  destroys a number-generator
 */
rc_t num_gen_destroy( num_gen* self );


/*--------------------------------------------------------------------------
 * num_gen_clear
 *
 *  resets a number-generator, to be empty just like after num_gen_make()
 */
rc_t num_gen_clear( num_gen* self );


/*--------------------------------------------------------------------------
 * num_gen_parse
 *
 *  parses a given string in this form: "3,6,8,12,44-49"
 *  does not clear the number-generator before parsing
 *  eventual overlaps with the previous content are consolidated
 */
rc_t num_gen_parse( num_gen* self, const char* src );


/*--------------------------------------------------------------------------
 * num_gen_add
 *
 *  inserts the given interval into the number-generator
 *
 *  num_gen_add( *g, 10, 30 )
 *  is equivalent to:
 *  num_gen_parse( *g, "10-39" );
 *
 *  eventual overlaps with the previous content are consolidated 
 */
rc_t num_gen_add( num_gen* self, const uint64_t first, const uint64_t count );


/*--------------------------------------------------------------------------
 * num_gen_trim
 *
 *  checks if the content of the number-generator is inside the given interval
 *  removes or shortens internal nodes if necessary
 */
rc_t num_gen_trim( num_gen* self, const int64_t first, const uint64_t count );


/*--------------------------------------------------------------------------
 * num_gen_empty
 *
 *  checks if the generator has no ranges defined
 */
bool num_gen_empty( const num_gen* self );


/*--------------------------------------------------------------------------
 * num_gen_as_string
 *
 *  allocates a string that contains the generator as text
 *  *s = "1-5,20,24-25"
 *  caller has to free the string
 */
rc_t num_gen_as_string( const num_gen* self, char **s );


/*--------------------------------------------------------------------------
 * num_gen_debug
 *
 *  allocates a string that contains the internal intervals as text
 *  *s = "[s:1 c:5][s:20 c:1][s:24 c:2]"
 *  [s...start-value c:count]
 *  caller has to free the string 
 */
rc_t num_gen_debug( const num_gen* self, char **s );


/*--------------------------------------------------------------------------
 * num_gen_contains_value
 *
 *  checks if the generator contains the given value
 */
rc_t num_gen_contains_value( const num_gen* self, const uint64_t value );


/*--------------------------------------------------------------------------
 * num_gen_range_check
 *
 *  if the generator is empty --> set it to the given range
 *  if it is not empty ---------> trim it to the given range
 */
rc_t num_gen_range_check( num_gen* self, 
                          const int64_t first, const uint64_t count );

/*--------------------------------------------------------------------------
 * num_gen_iterator_make
 *
 *  creates a iterator from the number-generator
 *  the iterator contains a constant copy of the number-ranges
 *  after this call it is safe to destroy or change the number-generator
 *  returns an error-code if the number-generator was empty,
 *  and *iter will be NULL
 */
rc_t num_gen_iterator_make( const num_gen* self, const num_gen_iter **iter );


/*--------------------------------------------------------------------------
 * num_gen_iterator_destroy
 *
 *  destroys the iterator
 */
rc_t num_gen_iterator_destroy( const num_gen_iter *self );


/*--------------------------------------------------------------------------
 * num_gen_iterator_count
 *
 *  returns how many values the iterator contains
 */
rc_t num_gen_iterator_count( const num_gen_iter* self, uint64_t* count );


/*--------------------------------------------------------------------------
 * num_gen_iterator_next
 *
 *  pulls the next value out of the iterator...
 *  returns an error-code if the iterator has no more values
 */
rc_t num_gen_iterator_next( const num_gen_iter* self, uint64_t* value );


/*--------------------------------------------------------------------------
 * num_gen_iterator_percent
 *
 *  return in value the percentage of the iterator...
 *  depending on fract-digits the percentage will be:
 *      fract_digits = 0 ... full percent's
 *      fract_digits = 1 ... 1/10-th of a percent
 *      fract_digits = 2 ... 1/100-th of a percent
 */
rc_t num_gen_iterator_percent( const num_gen_iter* self,
                               const uint8_t fract_digits,
                               uint32_t* value );


#ifdef __cplusplus
}
#endif

#endif
