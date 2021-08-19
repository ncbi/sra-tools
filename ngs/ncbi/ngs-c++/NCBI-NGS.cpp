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

#include <ngs/ncbi/NGS.hpp>

#include <kns/manager.h>

#include <ngs/itf/ErrBlock.hpp>

#include <ngs/itf/ReadCollectionItf.h>
#include <ngs/itf/ReferenceSequenceItf.h>
#include "NCBI-NGS.h"
#include "../klib/release-vers.h"

namespace ncbi
{

    static bool have_user_version_string;

    /* setAppVersionString
     *  updates User-Agent header in HTTP communications
     *
     *  example usage:
     *    ncbi::NGS::setAppVersionString ( "pileup-stats.1.0.0" );
     */
    void NGS :: setAppVersionString ( const String & app_version )
        /* nothrow */
    {
        // get a KNSManager
        KNSManager * kns;
        rc_t rc = KNSManagerMake ( & kns );
        if ( rc == 0 )
        {
            have_user_version_string = true;
            KNSManagerSetUserAgent ( kns, "ncbi-ngs.%V %.*s", RELEASE_VERS, ( uint32_t ) app_version . size (), app_version . data () );
            KNSManagerRelease ( kns );
        }
    }

    /* open
     *  create an object representing a named collection of reads
     *  "spec" may be a path to an object
     *  or may be an id, accession, or URL
     */
    ReadCollection NGS :: openReadCollection ( const String & spec )
        /*NGS_THROWS ( ErrorMsg )*/
    {
        if ( ! have_user_version_string )
            setAppVersionString ( "ncbi-ngs: unknown-application" );

        // call directly into ncbi-vdb library
        ngs :: ErrBlock err;
        NGS_ReadCollection_v1 * ret = NCBI_NGS_OpenReadCollection ( spec . c_str (), & err );

        // check for errors
        err . Check ();

        // create ReadCollection object
        return ReadCollection ( ( ngs :: ReadCollectionRef ) ret );
    }

    /* open
     *  create an object representing a named reference
     *  "spec" may be a path to an object
     *  or may be an id, accession, or URL
     */
    ReferenceSequence NGS :: openReferenceSequence ( const String & spec )
        /*NGS_THROWS ( ErrorMsg )*/
    {
        if ( ! have_user_version_string )
            setAppVersionString ( "ncbi-ngs: unknown-application" );

        // call directly into ncbi-vdb library
        ngs :: ErrBlock err;
        NGS_ReferenceSequence_v1 * ret = NCBI_NGS_OpenReferenceSequence ( spec . c_str (), & err );

        // check for errors
        err . Check ();

        // create ReferenceSequence object
        return ReferenceSequence ( ( ngs :: ReferenceSequenceRef ) ret );
    }
}
