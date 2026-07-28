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

#include "sbuffer.h"

#ifndef _h_err_msg_
#include "err_msg.h"
#endif

#ifndef _h_helper_
#include "helper.h" /* split_string_r */
#endif

#ifndef _h_klib_printf_
#include <klib/printf.h>    /* string_vprintf */
#endif

rc_t make_SBuffer( SBuffer_t * buffer, size_t len ) {
    rc_t rc = 0;
    String * S = &buffer -> S;
    S -> size = S -> len = 0;
    S -> addr = malloc( len );
    if ( NULL == S -> addr ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        ErrMsg( "make_SBuffer().malloc( %d ) -> %R", ( len ), rc );
    } else {
        buffer -> buffer_size = len;
    }
    return rc;
}

void release_SBuffer( SBuffer_t * self ) {
    if ( NULL != self ) {
        String * S = &self -> S;
        if ( NULL != S -> addr ) {
            free( ( void * ) S -> addr );
        }
    }
}

rc_t increase_SBuffer_by( SBuffer_t * self, size_t by ) {
    rc_t rc;
    if ( NULL == self ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcSelf, rcNull );
    } else {
        size_t new_size = self -> buffer_size + by;
        release_SBuffer( self );
        rc = make_SBuffer( self, new_size );
    }
    return rc;
}

rc_t increase_SBuffer_to( SBuffer_t * self, size_t new_size ) {
    rc_t rc = 0;
    if ( NULL == self ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcSelf, rcNull );
    } else if ( self -> buffer_size < new_size ) {
        release_SBuffer( self );
        rc = make_SBuffer( self, new_size );
    }
    return rc;
}

rc_t print_to_SBufferV( SBuffer_t * self, const char * fmt, va_list args ) {
    rc_t rc = 0;
    if ( NULL == self ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcSelf, rcNull );
    } else {
        char * dst = ( char * )( self -> S . addr );
        size_t num_writ = 0;

        rc = string_vprintf( dst, self -> buffer_size, &num_writ, fmt, args );
        if ( 0 == rc ) {
            self -> S . size = num_writ;
            self -> S . len = ( uint32_t )self -> S . size;
        }
    }
    return rc;
}

rc_t try_to_enlarge_SBuffer( SBuffer_t * self, rc_t rc_err ) {
    rc_t rc = rc_err;
    if ( ( GetRCObject( rc ) == ( enum RCObject )rcBuffer ) && ( GetRCState( rc ) == rcInsufficient ) )
    {
        rc = increase_SBuffer_by( self, self -> buffer_size ); /* double it's size */
        if ( 0 != rc ) {
            ErrMsg( "try_to_enlarge_SBuffer().increase_SBuffer() -> %R", rc );
        }
    }
    return rc;
}

rc_t print_to_SBuffer( SBuffer_t * self, const char * fmt, ... ) {
    rc_t rc = 0;
    bool done = false;
    while ( 0 == rc && !done ) {
        va_list args;

        va_start( args, fmt );
        rc = print_to_SBufferV( self, fmt, args );
        va_end( args );

        done = ( 0 == rc );
        if ( !done ) {
            rc = try_to_enlarge_SBuffer( self, rc );
        }
    }
    return rc;
}

rc_t make_and_print_to_SBuffer( SBuffer_t * self, size_t len, const char * fmt, ... ) {
    rc_t rc = make_SBuffer( self, len );
    if ( 0 == rc ) {
        va_list args;

        va_start( args, fmt );
        rc = print_to_SBufferV( self, fmt, args );
        va_end( args );
    }
    return rc;
}

rc_t split_filename_insert_idx( SBuffer_t * dst, size_t dst_size,
                                const char * filename, uint32_t idx ) {
    rc_t rc;
    if ( idx > 0 ) {
        /* we have to split md -> cmn -> output_filename into name and extension
           then append '_%u' to the name, then re-append the extension */
        String S_in, S_name, S_ext;
        StringInitCString( &S_in, filename );
        /* rc = hlp_split_string_r( &S_in, &S_name, &S_ext, '.' ); */ /* helper.c */
        rc = hlp_split_path_into_stem_and_extension( &S_in, &S_name, &S_ext );
        if ( 0 == rc ) {
            /* we found a dot to split the filename! */
            if ( S_ext . len > 0 ) {
                rc = make_and_print_to_SBuffer( dst, dst_size, "%S_%u.%S",
                            &S_name, idx, &S_ext ); /* helper.c */
            } else {
                rc = make_and_print_to_SBuffer( dst, dst_size, "%s_%u.fastq",
                            filename, idx ); /* helper.c */
            }
        } else {
            /* we did not find a dot to split the filename! */
            rc = make_and_print_to_SBuffer( dst, dst_size, "%s_%u.fastq",
                        filename, idx ); /* helper.c */
        }
    } else {
        rc = make_and_print_to_SBuffer( dst, dst_size, "%s", filename ); /* helper.c */
    }

    if ( 0 != rc ) {
        release_SBuffer( dst );
    }
    return rc;
}

rc_t copy_SBuffer( SBuffer_t * self, const SBuffer_t * src ) {
    rc_t rc = 0;
    if ( NULL == self || NULL == src ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcSelf, rcNull );
    } else {
        if ( src -> buffer_size > self -> buffer_size ) {
            rc = increase_SBuffer_to( self, src -> buffer_size );
        }
        if ( 0 == rc ) {
            size_t new_size = string_copy( ( char * )self -> S . addr, self -> S . size,
                                            src -> S . addr, src -> S . size );
            self -> S . size = new_size;
            self -> S . len = ( uint32_t )self -> S . size;
        }
    }
    return rc;
}

rc_t append_SBuffer( SBuffer_t * self, const SBuffer_t * src ) {
    rc_t rc = 0;
    if ( NULL == self || NULL == src ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcSelf, rcNull );
    } else {
        size_t needed = self -> S . size + src -> S . size + 1;
        if ( needed > self -> buffer_size ) {
            /* we have to store the content of self, because increase_SBuffer_to() will destroy it! */
            SBuffer_t tmp;
            rc = make_SBuffer( &tmp, self -> buffer_size );
            if ( 0 == rc ) {
                rc = copy_SBuffer( &tmp, self );
                if ( 0 == rc ) {
                    rc = increase_SBuffer_to( self, needed );
                    if ( 0 == rc ) {
                        rc = copy_SBuffer( self, &tmp );
                    }
                }
                release_SBuffer( &tmp );
            }
        }
        if ( 0 == rc ) {
            char * dst = ( char * )&( self -> S . addr[ self -> S . size ] );
            size_t dst_size = self -> buffer_size - self -> S . size;
            size_t new_size = string_copy( dst, dst_size,
                                           src -> S . addr, src -> S . size );
            self -> S . size = new_size;
            self -> S . len = ( uint32_t )self -> S . size;
        }
    }
    return rc;
}

rc_t clear_SBuffer( SBuffer_t * self ) {
    rc_t rc = 0;
    if ( NULL == self ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcSelf, rcNull );
    } else {
        self -> S . len = 0;
        self -> S . size = 0;
    }
    return rc;
}
