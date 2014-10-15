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
#ifndef _h_remote_cache_fuser_
#define _h_remote_cache_fuser_

/*
 * Lyrics ... because quite exotic requirements of remote file access
 * which means that we are accessing only files which will be described
 * in XML file, there are several propositions which were made:
 *   1) if there -x parameter to fuser contains URL path, we suppose
 *      that fuser is working in REMOTE MODE
 *   2) REMOTE MODE required parameter cache directory, and all files
 *      will be accessed as CACHEDTEE files.
 *   3) the XML document, which describes filesystem will contain only
 *      these entries: Directory, File and another XML document.
 *   4) cached files are valid only on session time, or at the time
 *      while fuser is working. That means that each time when fuser
 *      started, it removes all cached files, if those left from
 *      previous session.
 *   5) There could be two types of files: plain files and XML
 *      documents, which represents filesystem node. Files are stored
 *      in cache directory, and XML documents are loaded and interpreted
 *      imediately
 *   6) IMPORTANT: we are not going to initialize and start thread for
 *      SRA_List, as for checking and updating XML_root document thread
 *   8) I beleive that it could me much easiest way to write designated
 *      application for accessing remote files, however, client wants
 *      it as fuser extention
 */

/*)))
 ///   MALICIOUS CODE ON
(((*/

/*))
 //  That method will check if path is remote ... starts from "http"
((*/
bool CC IsRemotePath ( const char * Path );

/*))
 //  That method will check if path is local ... exists
((*/
bool CC IsLocalPath ( const char * Path );

    /*)))
     ///  Remote cache initialisation and so on
    (((*/
/* Lyrics:
 * We consider that cache is a directory in local filesystem, which
 * fully defined by it's path. The content of directory is valid only
 * for session period, from the time of Cache Initialize to Finalize
 * Once new session is started, the content of cache left from previous
 * session is dropped. 
 * For a moment we do beleive that we do have only one cache directory
 * per session, which could be initialized only once
 * UPDATE: from now we allow non-cacheing or diskless mode. In that case
 *         fuzer will not create any additional files or directories and
 *         will not use cachetee file, but direct HTTP connection 
 */

/* That structure will represent CacheFile
 */
struct RCacheEntry;

    /*))
     //  Three methods to work with CacheEntry
    ((*/
rc_t CC RCacheEntryAddRef ( struct RCacheEntry * self );
rc_t CC RCacheEntryRelease ( struct RCacheEntry * self );
rc_t CC RCacheEntryRead (
                    struct RCacheEntry * self,
                    char * Buffer,
                    size_t BufferSize,
                    uint64_t Offset,
                    size_t * NumRead
                    );
    /*))
     //  This method will set block size for HTTP transport
     \\  If user want to use default block size value, 0 should
     //  be used. Will return previous value for block size
    ((*/
uint32_t CC RemoteCacheSetHttpBlockSize ( uint32_t HttpBlockSize );

    /*))
     //  This method will set path for local cache dir
     \\
     //  NOTE: CachePath could be NULL, then it is diskless mode
    ((*/
rc_t CC RemoteCacheInitialize ( const char * CachePath );
    /*))
     //  This method will initialize local cache dir:
     \\    It will create cache directory if it does not exist
     //    It will remove all leftovers from previous session
    ((*/
rc_t CC RemoteCacheCreate ();
    /*))
     //  This method will finalise cache and destroy it's content
    ((*/
rc_t CC RemoteCacheDispose ();

    /*))
     //  Misc methods
    ((*/
const char * RemoteCachePath ();

bool RemoteCacheIsDisklessMode ();

rc_t CC RemoteCacheFindOrCreateEntry (
                        const char * Url,
                        struct RCacheEntry ** Entry
                    );


    /*))
     //  Found that interesting
    ((*/
rc_t CC ReadHttpFileToMemory (
                            const char * Url,
                            char ** RetBuffer,
                            uint64_t * RetSize
                            );

rc_t CC ReadLocalFileToMemory (
                            const char * FileName,
                            char ** RetBuffer,
                            uint64_t * RetSize
                            );

rc_t CC ExecuteCGI ( const char * CGICommand );

/*)))
 ///  Shamefull macro
(((*/
 #define RM_DEAF_LGO

 #ifdef RM_DEAF_LGO
 #define RmOutMsg  KOutMsg
 #else /* RM_DEAF_LGO */
 #define RmOutMsg(...)
 #endif /* RM_DEAF_LGO */

/*)))
 ///   MALICIOUS CODE OFF
(((*/

#endif /* _h_remote_cache_fuser_ */
