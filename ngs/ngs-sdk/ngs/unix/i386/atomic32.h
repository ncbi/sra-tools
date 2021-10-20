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
    int rtn, sum;
    __asm__ __volatile__
    (
        "mov (%2), %0;"
    "1:"
        "mov %3, %1;"
        "add %0, %1;"
    "lock;"
        "cmpxchg %1, (%2);"
        "jne 1b"
        : "=&a" ( rtn ), "=&r" ( sum )
        : "r" ( & v -> counter ), "r" ( i )
    );
    return rtn;
}

/* if no read is needed, define the least expensive atomic add */
#define atomic32_add( v, i ) \
    atomic32_read_and_add ( v, i )

/* add to v -> counter and return the result */
static __inline__ int atomic32_add_and_read ( atomic32_t *v, int i )
{
    int rtn, cmp;
    __asm__ __volatile__
    (
        "mov (%2), %0;"
    "1:"
        "mov %3, %1;"
        "add %0, %1;"
    "lock;"
        "cmpxchg %1,(%2);"
        "jne 1b;"
        : "=&a" ( cmp ), "=&r" ( rtn )
        : "r" ( & v -> counter ), "r" ( i )
    );
    return rtn;
}

/* just don't try to find out what the result was */
static __inline__ void atomic32_inc ( atomic32_t *v )
{
    __asm__ __volatile__
    (
    "lock;"
        "incl %0"
        : "=m" ( v -> counter )
        : "m" ( v -> counter )
    );
}

static __inline__ void atomic32_dec ( atomic32_t *v )
{
    __asm__ __volatile__
    (
    "lock;"
        "decl %0"
        : "=m" ( v -> counter )
        : "m" ( v -> counter )
    );
}

/* decrement by one and test result for 0 */
static __inline__ int atomic32_dec_and_test ( atomic32_t *v )
{
    unsigned char c;
    __asm__ __volatile__
    (
    "lock;"
        "decl %1;"
        "sete %0"
        : "=r" ( c ), "=m" ( v -> counter )
        : "m" ( v -> counter )
    );
    return c;
}

/* when atomic32_dec_and_test uses predecrement, you want
   postincrement to this function. so it isn't very useful */
static __inline__ int atomic32_inc_and_test ( atomic32_t *v )
{
    unsigned char c;
    __asm__ __volatile__
    (
    "lock;"
        "incl %1;"
        "sete %0"
        : "=r" ( c ), "=m" ( v -> counter )
        : "m" ( v -> counter )
    );
    return c;
}

/* HERE's useful */
#define atomic32_test_and_inc( v ) \
    ( atomic32_read_and_add ( v, 1 ) == 0 )

static __inline__ int atomic32_test_and_set ( atomic32_t *v, int s, int t )
{
    int rtn;
    __asm__ __volatile__
    (
    "lock;"
        "cmpxchg %2, (%1)"
        : "=a" ( rtn )
        : "r" ( & v -> counter ), "r" ( s ), "a" ( t )
    );
    return rtn;
}

/* conditional modifications */
static __inline__
int atomic32_read_and_add_lt ( atomic32_t *v, int i, int t )
{
    int rtn, sum;
    __asm__ __volatile__
    (
        "mov (%2), %0;"
    "1:"
        "cmp %4, %0;"
        "mov %3, %1;"
        "jge 2f;"
        "add %0, %1;"
    "lock;"
        "cmpxchg %1, (%2);"
        "jne 1b;"
    "2:"
        : "=&a" ( rtn ), "=&r" ( sum )
        : "r" ( & v -> counter ), "r" ( i ), "r" ( t )
    );
    return rtn;
}

#define atomic32_add_if_lt( v, i, t ) \
    ( atomic32_read_and_add_lt ( v, i, t ) < ( t ) )

static __inline__
int atomic32_read_and_add_le ( atomic32_t *v, int i, int t )
{
    int rtn, sum;
    __asm__ __volatile__
    (
        "mov (%2), %0;"
    "1:"
        "cmp %4, %0;"
        "mov %3, %1;"
        "jg 2f;"
        "add %0, %1;"
    "lock;"
        "cmpxchg %1, (%2);"
        "jne 1b;"
    "2:"
        : "=&a" ( rtn ), "=&r" ( sum )
        : "r" ( & v -> counter ), "r" ( i ), "r" ( t )
    );
    return rtn;
}

#define atomic32_add_if_le( v, i, t ) \
    ( atomic32_read_and_add_le ( v, i, t ) <= ( t ) )

static __inline__
int atomic32_read_and_add_eq ( atomic32_t *v, int i, int t )
{
    int rtn, sum;
    __asm__ __volatile__
    (
        "mov (%2), %0;"
    "1:"
        "cmp %4, %0;"
        "mov %3, %1;"
        "jne 2f;"
        "add %0, %1;"
    "lock;"
        "cmpxchg %1, (%2);"
        "jne 1b;"
    "2:"
        : "=&a" ( rtn ), "=&r" ( sum )
        : "r" ( & v -> counter ), "r" ( i ), "r" ( t )
    );
    return rtn;
}

#define atomic32_add_if_eq( v, i, t ) \
    ( atomic32_read_and_add_eq ( v, i, t ) == ( t ) )

static __inline__
int atomic32_read_and_add_ne ( atomic32_t *v, int i, int t )
{
    int rtn, sum;
    __asm__ __volatile__
    (
        "mov (%2), %0;"
    "1:"
        "cmp %4, %0;"
        "mov %3, %1;"
        "je 2f;"
        "add %0, %1;"
    "lock;"
        "cmpxchg %1, (%2);"
        "jne 1b;"
    "2:"
        : "=&a" ( rtn ), "=&r" ( sum )
        : "r" ( & v -> counter ), "r" ( i ), "r" ( t )
    );
    return rtn;
}

#define atomic32_add_if_ne( v, i, t ) \
    ( atomic32_read_and_add_ne ( v, i, t ) != ( t ) )

static __inline__
int atomic32_read_and_add_ge ( atomic32_t *v, int i, int t )
{
    int rtn, sum;
    __asm__ __volatile__
    (
        "mov (%2), %0;"
    "1:"
        "cmp %4, %0;"
        "mov %3, %1;"
        "jl 2f;"
        "add %0, %1;"
    "lock;"
        "cmpxchg %1, (%2);"
        "jne 1b;"
    "2:"
        : "=&a" ( rtn ), "=&r" ( sum )
        : "r" ( & v -> counter ), "r" ( i ), "r" ( t )
    );
    return rtn;
}

#define atomic32_add_if_ge( v, i, t ) \
    ( atomic32_read_and_add_ge ( v, i, t ) >= ( t ) )

static __inline__
int atomic32_read_and_add_gt ( atomic32_t *v, int i, int t )
{
    int rtn, sum;
    __asm__ __volatile__
    (
        "mov (%2), %0;"
    "1:"
        "cmp %4, %0;"
        "mov %3, %1;"
        "jle 2f;"
        "add %0, %1;"
    "lock;"
        "cmpxchg %1, (%2);"
        "jne 1b;"
    "2:"
        : "=&a" ( rtn ), "=&r" ( sum )
        : "r" ( & v -> counter ), "r" ( i ), "r" ( t )
    );
    return rtn;
}

#define atomic32_add_if_gt( v, i, t ) \
    ( atomic32_read_and_add_gt ( v, i, t ) > ( t ) )

static __inline__
int atomic32_read_and_add_odd ( atomic32_t *v, int i )
{
    int rtn, sum;
    __asm__ __volatile__
    (
        "mov (%2), %0;"
    "1:"
        "bt $0, %0;"
        "mov %3, %1;"
        "jnc 2f;"
        "add %0, %1;"
    "lock;"
        "cmpxchg %1, (%2);"
        "jne 1b;"
    "2:"
        : "=&a" ( rtn ), "=&r" ( sum )
        : "r" ( & v -> counter ), "r" ( i )
    );
    return rtn;
}

static __inline__
int atomic32_read_and_add_even ( atomic32_t *v, int i )
{
    int rtn, sum;
    __asm__ __volatile__
    (
        "mov (%2), %0;"
    "1:"
        "bt $0, %0;"
        "mov %3, %1;"
        "jc 2f;"
        "add %0, %1;"
    "lock;"
        "cmpxchg %1, (%2);"
        "jne 1b;"
    "2:"
        : "=&a" ( rtn ), "=&r" ( sum )
        : "r" ( & v -> counter ), "r" ( i )
    );
    return rtn;
}


#ifdef __cplusplus
}
#endif

#endif /* _h_atomic32_ */
