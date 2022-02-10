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

#ifndef _hpp_ngs_itf_vtable_
#define _hpp_ngs_itf_vtable_

#ifndef _hpp_ngs_itf_error_msg_
#include <ngs/itf/ErrorMsg.hpp>
#endif

#ifndef _h_ngs_itf_vtable_
#include <ngs/itf/VTable.h>
#endif

namespace ngs
{
    /*----------------------------------------------------------------------
     * ItfTok
     */
    struct ItfTok
    {
        ItfTok ( const char * name )
            : itf_name ( name )
        {
            assert ( parent == 0 );
            assert ( idx == 0 );
        }

        ItfTok ( const char * name, const ItfTok & dad )
            : itf_name ( name )
            , parent ( & dad )
        {
            assert ( idx == 0 );
        }

        /* name of the interface */
        const char * itf_name;

        // single-inheritance - interface hierarchy
        const ItfTok * parent;

        // index into linear inheritance array
        uint32_t mutable volatile idx;
    };

    /*----------------------------------------------------------------------
     * Resolve
     *  creates a cache of interface hierarchy
     *  resolves array indices of itf tokens
     */
    void Resolve ( const ItfTok & itf )
        NGS_NOTHROW ();
    void Resolve ( const NGS_VTable * vt, const ItfTok & itf )
        NGS_THROWS ( ErrorMsg );

    /*----------------------------------------------------------------------
     * Cast
     *  cast an NGS_VTable to the desired level
     */
    inline
    const void * Cast ( const NGS_VTable * vt, const ItfTok & itf )
        NGS_THROWS ( ErrorMsg )
    {
        if ( vt != 0 )
        {
            if ( itf . idx == 0 )
                Resolve ( itf );
            if ( vt -> cache == 0 )
                Resolve ( vt, itf );

            assert ( itf . idx != 0 );
            assert ( itf . idx <= ( unsigned int ) vt -> cache -> length );
            if ( vt -> cache -> hier [ itf . idx - 1 ] . itf_tok == ( const void* ) & itf )
                return vt -> cache -> hier [ itf . idx - 1 ] . parent;
            if ( vt -> cache -> hier [ itf . idx - 1 ] . itf_tok == 0 )
            {
                Resolve ( vt, itf );
                if ( vt -> cache -> hier [ itf . idx - 1 ] . itf_tok == ( const void* ) & itf )
                    return vt -> cache -> hier [ itf . idx - 1 ] . parent;
            }
        }

        return 0;
    }
}

#endif // _hpp_ngs_itf_vtable_
