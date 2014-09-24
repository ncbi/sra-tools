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

#ifndef _h_cctree_priv_
#define _h_cctree_priv_

#ifndef _h_klib_container_
#include <klib/container.h>
#endif

#ifndef _h_klib_text_
#include <klib/text.h>
#endif

#ifndef _h_kfs_directory_
#include <kfs/directory.h>
#endif

#define STORE_ID_IN_NODE 1

#ifdef __cplusplus
extern "C" {
#endif


/*--------------------------------------------------------------------------
 * forwards
 */
typedef struct BSTree CCTree;
typedef struct CCName CCName;


/*--------------------------------------------------------------------------
 * CCType
 *  enum describing entry type
 */
enum CCType
{
    ccFile,
    ccContFile,
    ccArcFile,
    ccChunkFile,
    ccContainer,
    ccArchive,
    ccSymlink,
    ccHardlink,
    ccDirectory,
    ccCached,
    ccReplaced  /* a name attached to a replaced file (name twice in tar for example) */
};


/*--------------------------------------------------------------------------
 * CCFileNode
 *  a node with a size and modification timestamp
 *  has a file type determined by magic/etc.
 *  has an md5 checksum
 *
 *  how would an access mode be used? access mode of a file is
 *  whatever the filesystem says it is, and within an archive,
 *  it's read-only based upon access mode of outer file...
 */
typedef struct CCFileNode CCFileNode;
struct CCFileNode
{
    uint64_t expected;  /* size expected (0 if not known) */
/* #define SIZE_UNKNOWN    (UINT64_MAX) */
#define SIZE_UNKNOWN    ((uint64_t)(int64_t)-1)
    uint64_t size;      /* actual size */
    uint64_t lines;     /* linecount if ASCII */
    uint32_t crc32;
#if STORE_ID_IN_NODE
    uint32_t id;
#endif
    rc_t rc;
    bool err;          /* errors found while reading/parsing */
    char ftype [ 252 ];
    uint8_t _md5 [ 32 ];
    SLList logs;
};


/* Make
 *  creates an object with provided properties
 *  md5 digest needs to be filled in afterward
 */
rc_t CCFileNodeMake ( CCFileNode **n, uint64_t expected );

/* Whack
 */
#define CCFileNodeWhack( self ) \
    free ( self )


/*--------------------------------------------------------------------------
 * CCArcFileNode
 *  a file with an offset into another file
 */
typedef struct CCArcFileNode CCArcFileNode;
struct CCArcFileNode
{
    CCFileNode dad;
    uint64_t offset;
};

/* Make
 *  creates an object with provided properties
 *  md5 digest needs to be filled in afterward
 */
rc_t CCArcFileNodeMake ( CCArcFileNode **n,
    uint64_t offset, uint64_t size );

/* Whack
 */
#define CCArcFileNodeWhack( self ) \
    free ( self )


/*--------------------------------------------------------------------------
 * CChunkFileNode
 *  a file with one or more chunks (offset/size) into another file
 */
typedef struct CChunk CChunk;
struct CChunk
{
    SLNode n;
    uint64_t offset;
    uint64_t size;
};

typedef struct CChunkFileNode CChunkFileNode;
struct CChunkFileNode
{
    CCFileNode dad;
    SLList chunks;
};

/* Make
 *  creates an object with provided properties
 *  md5 digest needs to be filled in afterward
 */
rc_t CChunkFileNodeMake ( CChunkFileNode **n, uint64_t size );

/* AddChunk
 *  adds a chunk to the chunk file
 */
rc_t CChunkFileNodeAddChunk ( CChunkFileNode *self,
    uint64_t offset, uint64_t size );

/* Whack
 */
void CChunkFileNodeWhack ( CChunkFileNode *self );


/*--------------------------------------------------------------------------
 * CCCachedFileNode
 *  a file wrapper with cached file name
 */
typedef struct CCCachedFileNode CCCachedFileNode;
struct CCCachedFileNode
{
    /* cached name */
    String cached;

    /* container CCFileNode */
    void *entry;
    uint32_t type;
};

/* Make
 *  creates a cached file wrapper
 */
rc_t CCCachedFileNodeMake ( CCCachedFileNode **n,
    const char *path, enum CCType type, const void *entry );

/* Whack
 */
void CCCachedFileNodeWhack ( CCCachedFileNode *self );


/*--------------------------------------------------------------------------
 * CCSymlinkNode
 *  a directory entry with a substitution path
 */
typedef struct CCSymlinkNode CCSymlinkNode;
struct CCSymlinkNode
{
    String path;
};

/* Make
 *  creates a symlink object
 */
rc_t CCSymlinkNodeMake ( CCSymlinkNode **n, const char *path );

/* Whack
 */
#define CCSymlinkNodeWhack( self ) \
    free ( self )


/*--------------------------------------------------------------------------
 * CCTree
 *  a binary search tree with CCNodes
 */

/* Make
 *  make a root tree or sub-directory
 */
rc_t CCTreeMake ( CCTree **t );

/* Insert
 *  create an entry into a tree
 *  parses path into required sub-directories
 *
 *  "mtime" [ IN ] - modification timestamp
 *
 *  "type" [ IN ] and "entry" [ IN ] - typed entry
 *
 *  "path" [ IN ] - vararg-style path of the entry, relative
 *  to "self".
 *
 * NB - '..' is not allowed in this implementation.
 */
rc_t CCTreeInsert ( CCTree *self, KTime_t mtime,
    enum CCType type, const void *entry, const char *path, ... );

/* Find
 *  find a named node
 *  returns NULL if not found
 */
const CCName *CCTreeFind ( const CCTree *self, const char *path, ... );


/* Link
 *  create a symlink to existing node
 */
rc_t CCTreeLink ( CCTree *self, KTime_t mtime,
    const char *targ, const char *alias );


/* Whack
 */
void CCTreeWhack ( CCTree *self );

/* Dump
 *  dump tree using provided callback function
 *
 *  "write" [ IN, NULL OKAY ] and "out" [ IN, OPAQUE ] - callback function
 *  for writing. if "write" is NULL, print to stdout.
 */
rc_t CCTreeDump ( const CCTree *self,
                  rc_t ( * write ) ( void *out, const void *buffer, size_t bytes ),
                  void *out, SLList * logs );


/*--------------------------------------------------------------------------
 * CCContainerNode
 *  its entry is a container file, i.e. an archive or else processed
 *  with some sort of envelope such as compression. its sub nodes
 *  are the contents and have their own names.
 */
typedef struct CCContainerNode CCContainerNode;
struct CCContainerNode
{
    /* contents */
    CCTree sub;

    /* container CCFileNode */
    void *entry;
    uint32_t type;
};

/* Make
 *  creates an archive object
 */
rc_t CCContainerNodeMake ( CCContainerNode **n,
    enum CCType type, const void *entry );

/* Whack
 */
void CCContainerNodeWhack ( CCContainerNode *self );


/*--------------------------------------------------------------------------
 * CCReplacedNode
 * its entry is any other type.  when a name shows up twice in a tar file
 * the first version is replaced.
 */
typedef struct CCReplacedNode CCReplacedNode;
struct CCReplacedNode
{
    /* container CCFileNode */
    void *entry;
    uint32_t type;
};

/* Make
 */
rc_t CCReplacedNodeMake ( CCReplacedNode **n,
    enum CCType type, const void *entry );

/* Whack
 */
void CCReplacedNodeWhack ( CCReplacedNode *self );



/*--------------------------------------------------------------------------
 * CCName
 *  an entry name in a CCTree
 */
struct CCName
{
    BSTNode n;
    CCName *dad;
    KTime_t mtime;
    void *entry;
    String name;
    uint32_t type;
};


#ifdef __cplusplus
}
#endif

#endif /* _h_cctree_priv_ */
