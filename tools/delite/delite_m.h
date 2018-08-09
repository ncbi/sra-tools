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

#ifndef _delite_m_h_
#define _delite_m_h_

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 *  That file represents EDIBLE META DATA for karChive
 *
 *  Lyrics. That file contains interface to KDB meta data.
 *  The original interface requires some KDB object to read meta data,
 *  during parcing KARchive we can not do that, so ... there is hack.
 *  Also, we do need only 'schema' value from META data, which we are
 *  only going to retrieve and edit.
 *  
 *  Kinda not more.
 *
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/

    /*  KAR archive META DATA and meta data NODE
     */
struct karChiveMD;
struct karChiveMDN;

rc_t CC karChiveMDUnpack (
                        struct karChiveMD ** MD,
                        const void * Data,
                        size_t DataSize
                        );
rc_t CC karChiveMDPack (
                        const struct karChiveMD * MD,
                        const void ** Data,
                        size_t * DataSize
                        );

rc_t CC karChiveMDAddRef ( struct karChiveMD * self );
rc_t CC karChiveMDRelease ( struct karChiveMD * self );

    /*  That one will return root MetaData node, editable and 
     *  searchable
     */
rc_t CC karChiveMDRoot (
                        const struct karChiveMD * self,
                        const struct karChiveMDN ** Node
                        );


    /*  This thing will find and return node by name. You may edit
     *  node content then. Also, the search will be done related
     *  to root node, so it is like sequence:
     *      karChiveMDRoot ()
     *      karChiveMDNFind ()
     */
rc_t CC karChiveMDFind (
                        const struct karChiveMD * self,
                        const char * NodeName,
                        const struct karChiveMDN ** Node
                        );

    /*  meta data NODE starts here
     */
rc_t CC karChiveMDNAddRef ( struct karChiveMDN * self );
rc_t CC karChiveMDNRelease ( struct karChiveMDN * self );

    /*  This thing will find and return node by name. You may edit
     *  node content then.
     */
rc_t CC karChiveMDNFind (
                        const struct karChiveMDN * self,
                        const char * NodeName,
                        const struct karChiveMDN ** Node
                        );

    /*  That will alloc null terminated string and will return it :LOL:
     *  Don't forget to free it
     */
rc_t CC karChiveMDNAsSting (
                        const struct karChiveMDN * self,
                        const char ** String
                        );

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif  /* _delite_m_h_ */
