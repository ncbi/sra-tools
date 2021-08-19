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

#ifndef _h_atomic32_
#define _h_atomic32_

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Make sure gcc doesn't try to be clever and move things around
 * on us. We need to use _exactly_ the address the user gave us,
 * not some alias that contains the same information.
 */
typedef struct atomic32_t atomic32_t;
struct atomic32_t
{
    volatile int counter;
};

/* int atomic32_read ( const atomic32_t *v ); */
#define atomic32_read( v ) \
    ( ( v ) -> counter )

/* void atomic32_set ( atomic32_t *v, int i ); */
#define atomic32_set( v, i ) \
    ( ( void ) ( ( ( v ) -> counter ) = ( i ) ) )

/* add to v -> counter and return the prior value */
static __inline__ int atomic32_read_and_add ( atomic32_t *v, int i )
{
    return __sync_fetch_and_add( & v -> counter, i );
}

/* if no read is needed, define the least expensive atomic add */
#define atomic32_add( v, i ) \
    atomic32_read_and_add ( v, i )

static __inline__ void atomic32_dec ( atomic32_t *v )
{   
    atomic32_add( v, -1 );
}

static __inline__ int atomic32_test_and_set ( atomic32_t *v, int newval, int oldval )
{
    return __sync_val_compare_and_swap( & v -> counter, oldval, newval ); //NB: newval/oldval switched around
}

static __inline__
int atomic32_read_and_add_gt ( atomic32_t *v, int i, int t )
{
	int val, val_intern;
	for ( val = atomic32_read ( v ); val > t; val = val_intern )
	{
		val_intern = atomic32_test_and_set ( v, val + i, val );
		if ( val_intern == val )
			break;
	}
	return val;
}

#ifdef __cplusplus
}
#endif

#endif /* _h_atomic32_ */

