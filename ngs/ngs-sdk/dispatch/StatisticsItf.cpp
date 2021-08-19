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

#include <ngs/itf/StatisticsItf.hpp>
#include <ngs/itf/StringItf.hpp>
#include <ngs/itf/ErrBlock.hpp>
#include <ngs/itf/VTable.hpp>

#include <ngs/itf/StatisticsItf.h>

namespace ngs
{
    /*----------------------------------------------------------------------
     * metadata
     */
    extern ItfTok NGS_Refcount_v1_tok;
    ItfTok NGS_Statistics_v1_tok ( "NGS_Statistics_v1", NGS_Refcount_v1_tok );


    /*----------------------------------------------------------------------
     * access vtable
     */
    static inline
    const NGS_Statistics_v1_vt * Access ( const NGS_VTable * vt )
    {
        const NGS_Statistics_v1_vt * out = static_cast < const NGS_Statistics_v1_vt* >
            ( Cast ( vt, NGS_Statistics_v1_tok ) );
        if ( out == 0 )
            throw ErrorMsg ( "object is not of type NGS_Statistics_v1" );
        return out;
    }

    /*----------------------------------------------------------------------
     * StatisticsItf
     */

    uint32_t StatisticsItf :: getValueType ( const char * path ) const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_Statistics_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Statistics_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_type != 0 );
        uint32_t ret  = ( * vt -> get_type ) ( self, & err, path );

        // check for errors
        err . Check ();

        return ret;
    }

    StringItf * StatisticsItf :: getAsString ( const char * path ) const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_Statistics_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Statistics_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> as_string != 0 );
        NGS_String_v1 * ret  = ( * vt -> as_string ) ( self, & err, path );

        // check for errors
        err . Check ();

        return StringItf :: Cast ( ret );
    }

    int64_t StatisticsItf :: getAsI64 ( const char * path ) const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_Statistics_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Statistics_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> as_I64 != 0 );
        int64_t ret  = ( * vt -> as_I64 ) ( self, & err, path );

        // check for errors
        err . Check ();

        return ret;
    }

    uint64_t StatisticsItf :: getAsU64 ( const char * path ) const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_Statistics_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Statistics_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> as_U64 != 0 );
        uint64_t ret  = ( * vt -> as_U64 ) ( self, & err, path );

        // check for errors
        err . Check ();

        return ret;
    }

    double StatisticsItf :: getAsDouble ( const char * path ) const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_Statistics_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Statistics_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> as_F64 != 0 );
        double ret  = ( * vt -> as_F64 ) ( self, & err, path );

        // check for errors
        err . Check ();

        return ret;
    }

    StringItf * StatisticsItf :: nextPath ( const char * path ) const
        NGS_NOTHROW ()
    {
        try
        {
            // the object is really from C
            const NGS_Statistics_v1 * self = Test ();

            // cast vtable to our level
            const NGS_Statistics_v1_vt * vt = Access ( self -> vt );

            // call through C vtable
            ErrBlock err;
            assert ( vt -> next_path != 0 );
            NGS_String_v1 * ret  = ( * vt -> next_path ) ( self, & err, path );

            // check for errors
            err . Check ();

            return StringItf :: Cast ( ret );
        }
        catch ( ErrorMsg & x )
        {
            // warn
        }
        catch ( ... )
        {
            // warn
        }

        return 0;
    }

} // namespace ngs
