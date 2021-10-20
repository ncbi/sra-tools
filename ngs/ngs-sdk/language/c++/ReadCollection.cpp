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

#include <ngs/ReadCollection.hpp>

namespace ngs
{

    ReadCollection & ReadCollection :: operator = ( ReadCollectionRef ref )
        NGS_NOTHROW ()
    {
        self -> Release ();
        self = ref;

        return * this;
    }

    ReadCollection :: ReadCollection ( ReadCollectionRef ref )
            NGS_NOTHROW ()
        : self ( ref )
    {
    }

    ReadCollection & ReadCollection :: operator = ( const ReadCollection & obj )
        NGS_NOTHROW ()
    {
        self -> Release ();
        self = obj . self -> Duplicate ();

        return * this;
    }

    ReadCollection :: ReadCollection ( const ReadCollection & obj )
            NGS_NOTHROW ()
        : self ( obj . self -> Duplicate () )
    {
    }

    ReadCollection :: ~ ReadCollection ()
        NGS_NOTHROW ()
    {
        self -> Release ();
        self = 0;
    }

} // namespace ngs