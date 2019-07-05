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

#ifndef _delite_s_h_
#define _delite_s_h_

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 *  That file represents SCHEMA parsing and editing
 *
 *  Das JOJOBA!
 *
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
struct karChiveScm;
struct KMetadata;

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 * Because Ken produces new schemas, and they should work.
 * 
 * This interface will contain all schemas, which user can-could
 * transform. There are three types of schema tranformation :
 *
 *   1) transform every meta node
 *   2) transform only quality meta nodes
 *   2) transform #1 or #2 with dictionary and without
 *
 * Dictionary is a file in a format :
 *
 *   ...
 *   entry_name<tab>old_version<tab>new_version
 *   entry_name<tab>old_version<tab>new_version
 *   ...
 *
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
struct scmDepot;

    /*  Note, both SchemaPath and TransformFile could be NULL
     *  SchemaPath - directory, which will be scaned for "*.vschema"
     *  TransformFile - file in format "entry<tab>old_v<tab>new_v"
     */
rc_t CC scmDepotMake (
                    struct scmDepot ** Dpt,
                    const char * SchemaPath,
                    const char * TransformFile
                    );
rc_t CC scmDepotDispose ( struct scmDepot * self );

rc_t CC scmDepotTransform (
                            struct scmDepot * self,
                            struct KMetadata * Meta,
                            bool ForDelite
                            );

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif  /* _delite_s_h_ */
