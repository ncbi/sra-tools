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

#ifndef _delite_k_h_
#define _delite_k_h_

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 *  That file is a copy-pasta from sra_tools/tools/kar/kar.c
 *  may be we will make a library later
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 *  KAR file is living here
 *  Basically KAR file is archive with such structure:
 * 
 *     HEADER
 *     TOC      <- BSTree
 *     ENTRY
 *     ENTRY
 *     ...
 *     ENTRY
 *
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/


/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 *  That stuct represents KAR file, and there are functions which opens
 *  KAR file ( or receives opened file )
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/

struct karChive;

LIB_EXPORT rc_t karChiveMake ( 
                            const struct karChive ** Chive,
                            const struct KFile * File
                            );
LIB_EXPORT rc_t karChiveOpen ( 
                            const struct karChive ** Chive,
                            const char * Path
                            );

LIB_EXPORT rc_t karChiveEdit (
                            const struct karChive * self,
                            bool IdleRun
                            );

LIB_EXPORT rc_t karChiveWriteFile ( 
                            const struct karChive * self,
                            struct KFile * File
                            );
LIB_EXPORT rc_t karChiveWrite ( 
                            const struct karChive * self,
                            const char * Path,
                            bool Force,
                            bool IdleRun
                            );

LIB_EXPORT rc_t karChiveAddRef ( const struct karChive * self );
LIB_EXPORT rc_t karChiveRelease ( const struct karChive * self );

LIB_EXPORT rc_t karChiveDump (
                            const struct karChive * self,
                            bool MorDetails
                            );

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif  /* _delite_k_h_ */
