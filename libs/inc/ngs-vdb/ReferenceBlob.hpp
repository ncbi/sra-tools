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

#ifndef _hpp_ReferenceBlob_hpp_
#define _hpp_ReferenceBlob_hpp_

#include <ngs/ErrorMsg.hpp>

typedef struct NGS_ReferenceBlob* ReferenceBlobRef;

namespace ncbi
{
    namespace ngs
    {
        namespace vdb
        {
            class ReferenceBlob
            {
            public:

                const char* Data() const
                    NGS_NOTHROW();

                uint64_t Size() const;

                uint64_t UnpackedSize() const;

                void GetRowRange ( int64_t * first, uint64_t * count ) const;

                void ResolveOffset ( uint64_t inBlob, uint64_t * inReference, uint32_t * repeatCount, uint64_t * increment ) const;

            public:

                // C++ support

                // increases refcount on the base object
                ReferenceBlob ( ReferenceBlobRef ref )
                    NGS_NOTHROW();

                ReferenceBlob & operator = ( const ReferenceBlob & obj );
                ReferenceBlob ( const ReferenceBlob & obj );

                ~ ReferenceBlob ()
                    NGS_NOTHROW();

            private:
                ReferenceBlobRef self;
            };
        };
    }
} // ncbi

#endif // _hpp_VdbReadCollection_hpp_
