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

#ifndef _h_progressbar_
#define _h_progressbar_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct progressbar progressbar;

/*--------------------------------------------------------------------------
 * make_progressbar
 *
 *  creates a progressbar with zero-values inside
 *  does not output anything
 */
rc_t make_progressbar( progressbar ** pb );


/*--------------------------------------------------------------------------
 * destroy_progressbar
 *
 *  destroy's the progressbar
 *  does not output anything
 */
rc_t destroy_progressbar( progressbar * pb );


/*--------------------------------------------------------------------------
 * update_progressbar
 *
 *  sets the progressbar to a specific percentage
 *  outputs only if the percentage has changed from the last call
 *  the precentage is in 1/10-th of a percent ( 21.6% = 216 )
 *  expects the percents in increasing order ( does not jump back )
 *  writes a growing bar made from '-'-chars every 2nd percent
 */
rc_t update_progressbar( progressbar * pb, const uint8_t fract_digits,
                         const uint16_t percent );


uint8_t calc_progressbar_digits( uint64_t count );

uint16_t percent_progressbar( const uint64_t total, const uint64_t progress,
                              const uint8_t digits );

#ifdef __cplusplus
}
#endif

#endif
