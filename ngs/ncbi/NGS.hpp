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

#ifndef _hpp_ngs_ncbi_NGS_
#define _hpp_ngs_ncbi_NGS_

#ifndef _hpp_ngs_read_collection_
#include <ngs/ReadCollection.hpp>
#endif

#ifndef _hpp_ngs_reference_sequence_
#include <ngs/ReferenceSequence.hpp>
#endif


/*==========================================================================
 * NCBI NGS Engine
 *  this class binds the NGS interface to NCBI's NGS implementation
 *  all of the code operates natively on SRA files
 */
namespace ncbi
{

    /*----------------------------------------------------------------------
     * typedefs used to import names from ngs namespace
     */
    typedef :: ngs :: String String;
    typedef :: ngs :: ErrorMsg ErrorMsg;
    typedef :: ngs :: ReadCollection ReadCollection;
    typedef :: ngs :: ReferenceSequence ReferenceSequence;


    /*======================================================================
     * NGS
     *  static implementation root
     */
    class NGS
    {
    public:

        /* setAppVersionString
         *  updates User-Agent header in HTTP communications
         *
         *  example usage:
         *    ncbi::NGS::setAppVersionString ( "pileup-stats.1.0.0" );
         */
        static
        void setAppVersionString ( const String & app_version );
            /*nothrow*/

        /* openReadCollection
         *  create an object representing a named collection of reads
         *  "spec" may be a path to an object
         *  or may be an id, accession, or URL
         */
        static 
        ReadCollection openReadCollection ( const String & spec );
            /*NGS_THROWS ( ErrorMsg );*/

        /* openReferenceSequence
         *  create an object representing a named reference
         *  "spec" may be a path to an object
         *  or may be an id, accession, or URL
         */
        static 
        ReferenceSequence openReferenceSequence ( const String & spec );
            /*NGS_THROWS ( ErrorMsg );*/
    };

} // ncbi

#endif // _hpp_ngs_ncbi_NGS_
