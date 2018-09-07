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

#ifndef _delite_d_h_
#define _delite_d_h_

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 *  That file represents different Data Sources for karChive files
 *  and opertations with it
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/

struct karChiveDS;
struct KMMap;
struct KMFile;
struct karCBuf;

rc_t CC karChiveDSAddRef ( const struct karChiveDS * self );
rc_t CC karChiveDSRelease ( const struct karChiveDS * self );

rc_t CC karChiveDSRead (
                        const struct karChiveDS * self,
                        uint64_t Offset,
                        void * Buffer,
                        size_t BufferSize,
                        size_t * NumReaded
                        );
rc_t CC karChiveDSSize (
                        const struct karChiveDS * self,
                        size_t * Size
                        );

    /*  Do not use that method too often, it is too greedy, LOOL
     */
rc_t CC karChiveDSReadAll (
                        const struct karChiveDS * self,
                        void ** DataRead,
                        size_t * ReadSize
                        );

    /*  That one will create data source from MemoryMap
     */
rc_t CC karChiveMMapDSMake (
                        const struct karChiveDS ** DS,
                        const struct KMMap * Map,
                        uint64_t Offset,
                        uint64_t Size
                        );

    /*  That one will create data source from opened for read KFile
     */
rc_t CC karChiveFileDSMake (
                        const struct karChiveDS ** DS,
                        const struct KFile * File,
                        uint64_t Offset,
                        uint64_t Size
                        );

    /*  That one will create data source from memory buffer
     *  NOTE, that one creates copy of memory buffer, and keeps it
     */
rc_t CC karChiveMemDSMake (
                        const struct karChiveDS ** DS,
                        const void * Buffer,
                        uint64_t Size
                        );

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif  /* _delite_d_h_ */
