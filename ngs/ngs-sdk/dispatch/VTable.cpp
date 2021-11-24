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

#include <ngs/itf/VTable.hpp>

#include <stdlib.h>
#include <string.h>

namespace ngs
{
    /*----------------------------------------------------------------------
     * Depth
     *  recursively discovers depth of runtime VTable
     *  marks depth in ItfTok
     */
    static
    uint32_t ItfTokDepth ( const ItfTok * itf )
    {
        uint32_t depth = 1;

        if ( itf -> parent != 0 )
            depth = ItfTokDepth ( itf -> parent ) + 1;

        if ( itf -> idx == 0 )
            itf -> idx = depth;

        assert ( itf -> itf_name != 0 );
        assert ( itf -> itf_name [ 0 ] != 0 );
        assert ( itf -> idx == depth );

        return depth;
    }

    static
    uint32_t VTableDepth ( const NGS_VTable * vt )
    {
        uint32_t depth = 1;

        if ( vt -> parent != 0 )
            depth = VTableDepth ( vt -> parent ) + 1;

        assert ( vt -> itf_name != 0 );

        return depth;
    }

    /*----------------------------------------------------------------------
     * PopulateCache
     */
    static
    void VTablePopulateCache ( const NGS_VTable * vt, uint32_t depth, const ItfTok * itf, NGS_HierCache * cache )
    {
        do
        {
            assert ( vt != 0 );
            assert ( itf != 0 );
            assert ( depth != 0 );
            assert ( itf -> idx <= depth );

            vt -> cache = cache;

            if ( itf -> idx == depth -- )
            {
                assert ( strcmp ( vt -> itf_name, itf -> itf_name ) == 0 );
                cache -> hier [ depth ] . itf_tok = ( const void* ) itf;
                itf = itf -> parent;
            }

            cache -> hier [ depth ] . parent = vt;
            vt = vt -> parent;
        }
        while ( vt != 0 );
    }


    /*----------------------------------------------------------------------
     * Resolve
     *  creates a cache of interface hierarchy
     *  resolves array indices of itf tokens
     */
    void Resolve ( const ItfTok  & itf )
        NGS_NOTHROW ()
    {
        // interfaces only support single-inheritance
        // perform a one-shot runtime depth assignment
        ItfTokDepth ( & itf );
    }

    void Resolve ( const NGS_VTable * vt, const ItfTok & itf )
        NGS_THROWS ( ErrorMsg )
    {
        if ( vt != 0 )
        {
            // determine depth of hierarchy
            uint32_t depth = VTableDepth ( vt );
            if ( itf . idx > depth )
                throw ErrorMsg ( "interface not supported" );

            // check for existing cache object
            NGS_HierCache * cache = const_cast < NGS_HierCache* > ( vt -> cache );
            if ( cache == 0 )
            {
                // allocate a cache object
                cache = ( NGS_HierCache* ) calloc ( 1, sizeof * cache
                    - sizeof cache -> hier + sizeof cache -> hier [ 0 ] * depth );
                if ( cache == 0 )
                    throw ErrorMsg ( "out of memory allocating NGS_HierCache" );

                cache -> length = depth;
            }
            else if ( cache -> length != depth )
            {
                throw ErrorMsg ( "corrupt vtable cache" );
            }

            // populate
            VTablePopulateCache ( vt, depth, & itf, cache );
        }
    }

}
