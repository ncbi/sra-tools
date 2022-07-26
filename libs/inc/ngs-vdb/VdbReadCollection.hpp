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

#ifndef _hpp_VdbReadCollection_hpp_
#define _hpp_VdbReadCollection_hpp_


#ifndef _hpp_ngs_read_collection_
#include <ngs/ReadCollection.hpp>
#endif

#include <ngs-vdb/FragmentBlobIterator.hpp>
#include <ngs-vdb/VdbReferenceIterator.hpp>

namespace ncbi
{
    namespace ngs
    {
        namespace vdb
        {
            class VdbReadCollection : protected :: ngs :: ReadCollection
            {
            public:

                :: ngs :: ReadCollection toReadCollection () const { return *this; }

                FragmentBlobIterator getFragmentBlobs() const;

                /* getReferences
                *  returns an iterator of all References used
                *  iterator will be empty if no Reads are aligned
                */
                VdbReferenceIterator getReferences () const;

                /* getReference
                */
                VdbReference getReference ( const :: ngs :: String & spec ) const;

            public:

                // C++ support

                VdbReadCollection ( const :: ngs :: ReadCollection & dad )
                    NGS_NOTHROW();

                VdbReadCollection & operator = ( const VdbReadCollection & obj )
                    NGS_NOTHROW();
                VdbReadCollection ( const VdbReadCollection & obj )
                    NGS_NOTHROW();

                ~ VdbReadCollection ()
                    NGS_NOTHROW();

            };
        };
    }
} // ncbi

#endif
