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

#ifndef _delite_h_
#define _delite_h_

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

    /*  length limit for strings I am working with
     */
#define KAR_MAX_ACCEPTED_STRING 16384

LIB_EXPORT rc_t _copyStringSayNothingHopeKurtWillNeverSeeThatCode (
                                            const char ** Dst,
                                            const char * Src
                                            );

LIB_EXPORT rc_t _copySStringSayNothing (
                                            const char ** Dst,
                                            const char * Begin,
                                            const char * End
                                            );

LIB_EXPORT rc_t _copyLStringSayNothing (
                                            const char ** Dst,
                                            const char * Str,
                                            size_t Len
                                            );

LIB_EXPORT rc_t DeLiteResolvePath (
                                            char ** ResolvedLocalPath,
                                            const char * PathOrAccession
                                            );


#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif  /* _delite_h_ */
