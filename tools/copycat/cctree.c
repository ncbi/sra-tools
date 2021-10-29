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

#include "copycat-priv.h"
#include "cctree-priv.h"
#include <klib/rc.h>
#include <klib/log.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>


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

static
void CCFileNodeInit ( CCFileNode *self, uint64_t expected )
{
#if STORE_ID_IN_NODE
    static uint32_t file_id;
    self -> id = ++ file_id;
#endif
    self -> expected = expected;
    self -> size = 0;   /* we learn this with the counter file */
    self -> lines = 0;   /* we learn this with the counter file */
    self -> crc32 = 0;
    self -> rc = 0;
    self -> err = false;
    memset ( self -> ftype, 0, sizeof self -> ftype );
    memset ( self -> _md5, 0, sizeof self -> _md5 );
    SLListInit ( &self->logs );
}

/* Make
 *  creates an object with provided properties
 *  md5 digest needs to be filled in afterward
 */
rc_t CCFileNodeMake ( CCFileNode **np, uint64_t expected )
{
    CCFileNode *n = malloc ( sizeof * n );
    if ( n == NULL )
        return RC ( rcExe, rcTree, rcInserting, rcMemory, rcExhausted );

    CCFileNodeInit ( n, expected );

    * np = n;
    return 0;
}

/*--------------------------------------------------------------------------
 * CCArcFileNode
 *  a file with an offset into another file
 */

/* Make
 *  creates an object with provided properties
 *  md5 digest needs to be filled in afterward
 */
rc_t CCArcFileNodeMake ( CCArcFileNode **np,
    uint64_t offset, uint64_t expected )
{
    CCArcFileNode *n = malloc ( sizeof * n );
    if ( n == NULL )
        return RC ( rcExe, rcTree, rcInserting, rcMemory, rcExhausted );

    CCFileNodeInit ( & n -> dad, expected );
    n -> offset = offset;

    * np = n;
    return 0;
}


/*--------------------------------------------------------------------------
 * CChunkFileNode
 *  a file with one or more chunks (offset/size) into another file
 */

/* Whack
 */
static
void CChunkWhack ( SLNode *n, void *ignore )
{
    free ( n );
}

void CChunkFileNodeWhack ( CChunkFileNode *self )
{
    if ( self != NULL )
    {
        SLListWhack ( & self -> chunks, CChunkWhack, NULL );
        CCFileNodeWhack ( & self -> dad );
    }
}

/* Make
 *  creates an object with provided properties
 *  md5 digest needs to be filled in afterward
 */
rc_t CChunkFileNodeMake ( CChunkFileNode **np, uint64_t expected )
{
    CChunkFileNode *n = malloc ( sizeof * n );
    if ( n == NULL )
        return RC ( rcExe, rcTree, rcInserting, rcMemory, rcExhausted );

    CCFileNodeInit ( & n -> dad, expected );
    SLListInit ( & n -> chunks );

    * np = n;
    return 0;
}

/* AddChunk
 *  adds a chunk to the chunk file
 */
rc_t CChunkFileNodeAddChunk ( CChunkFileNode *self,
    uint64_t offset, uint64_t size )
{
    CChunk *c = malloc ( sizeof * c );
    if ( c == NULL )
        return RC ( rcExe, rcTree, rcInserting, rcMemory, rcExhausted );

    c -> offset = offset;
    c -> size = size;

    SLListPushTail ( & self -> chunks, & c -> n );
    return 0;
}


/*--------------------------------------------------------------------------
 * CCCachedFileNode
 *  a file wrapper with cached file name
 */

/* Make
 *  creates a cached file wrapper
 */
rc_t CCCachedFileNodeMake ( CCCachedFileNode **np, const char *path,
    enum CCType type, const void *entry )
{
    CCCachedFileNode *n = malloc ( sizeof * n + strlen ( path ) + 1 );
    if ( n == NULL )
        return RC ( rcExe, rcTree, rcInserting, rcMemory, rcExhausted );

    strcpy ( ( char* ) ( n + 1 ), path );
    StringInitCString ( & n -> cached, ( char* ) ( n + 1 ) );
    n -> entry = ( void* ) entry;
    n -> type = type;

    * np = n;
    return 0;
}

/* Whack
 */
void CCCachedFileNodeWhack ( CCCachedFileNode *self )
{
    if ( self != NULL )
    {
        switch ( self -> type )
        {
        case ccFile:
            CCFileNodeWhack ( self -> entry );
            break;
        case ccArcFile:
            CCArcFileNodeWhack ( self -> entry );
            break;
        case ccChunkFile:
            CChunkFileNodeWhack ( self -> entry );
            break;
        }
        free ( self );
    }
}


/*--------------------------------------------------------------------------
 * CCSymlinkNode
 *  a directory entry with a substitution path
 */

/* Make
 *  creates a symlink object
 */
rc_t CCSymlinkNodeMake ( CCSymlinkNode **np, const char *path )
{
    CCSymlinkNode *n = malloc ( sizeof * n + strlen ( path ) + 1 );
    if ( n == NULL )
        return RC ( rcExe, rcTree, rcInserting, rcMemory, rcExhausted );

    strcpy ( ( char* ) ( n + 1 ), path );
    StringInitCString ( & n -> path, ( char* ) ( n + 1 ) );

    * np = n;
    return 0;
}


/*--------------------------------------------------------------------------
 * CCName
 *  an entry name in a CCTree
 */


/* Whack
 */
static
void CCNameWhack ( BSTNode *n, void *ignore )
{
    CCName *self = ( CCName* ) n;
    if ( self -> entry != NULL ) switch ( self -> type )
    {
    case ccFile:
    case ccContFile:
        CCFileNodeWhack ( self -> entry );
        break;
    case ccArcFile:
        CCArcFileNodeWhack ( self -> entry );
        break;
    case ccChunkFile:
        CChunkFileNodeWhack ( self -> entry );
        break;
    case ccContainer:
        CCContainerNodeWhack ( self -> entry );
        break;
    case ccSymlink:
        CCSymlinkNodeWhack ( self -> entry );
        break;
    case ccHardlink:
        break;
    case ccDirectory:
        CCTreeWhack ( self -> entry );
        break;
    }

    free ( self );
}


/* Make
 *  make a node name
 */
static
rc_t CCNameMake ( CCName **np, KTime_t mtime, CCName *dad,
    const String *name, enum CCType type, const void *entry )
{
    CCName *n = malloc ( sizeof * n + name -> size + 1 );
    if ( n == NULL )
        return RC ( rcExe, rcTree, rcInserting, rcMemory, rcExhausted );

    string_copy ( ( char* ) ( n + 1 ), name -> size + 1, name -> addr, name -> size );
    n -> mtime = mtime;
    n -> dad = dad;
    n -> entry = ( void* ) entry;
    StringInit ( & n -> name, ( char* ) ( n + 1 ), name -> size, name -> len );
    n -> type = ( uint32_t ) type;

    * np = n;
    return 0;
}

/* Cmp
 * Sort
 */
static
int64_t CCNameCmp ( const void *item, const BSTNode *n )
{
    const String *a = item;
    const CCName *b = ( const CCName* ) n;
    return StringCompare ( a, & b -> name );
}

static
int64_t CCNameSort ( const BSTNode *item, const BSTNode *n )
{
    const CCName *a = ( const CCName* ) item;
    const CCName *b = ( const CCName* ) n;
    int64_t cmp = StringCompare ( & a -> name, & b -> name );
    if (cmp != 0)
        return cmp;
#if 0
    if (b->type == ccReplaced)
        return 1;
#endif
    return 1; /* make new item always greater than existing n */
}


/*--------------------------------------------------------------------------
 * CCContainerNode
 *  an archive file entry
 *  a file with a sub-directory
 */


/* Whack
 */
void CCContainerNodeWhack ( CCContainerNode *self )
{
    if ( self != NULL )
    {
        BSTreeWhack ( & self -> sub, CCNameWhack, NULL );
        switch ( self -> type )
        {
        case ccFile:
            CCFileNodeWhack ( self -> entry );
            break;
        case ccArcFile:
            CCArcFileNodeWhack ( self -> entry );
            break;
        case ccChunkFile:
            CChunkFileNodeWhack ( self -> entry );
            break;
        }
        free ( self );
    }
}

/* Make
 *  creates an archive object
 */
rc_t CCContainerNodeMake ( CCContainerNode **np,
    enum CCType type, const void *entry )
{
    CCContainerNode *n = malloc ( sizeof * n );
    if ( n == NULL )
        return RC ( rcExe, rcTree, rcInserting, rcMemory, rcExhausted );

    BSTreeInit ( & n -> sub );
    n -> entry = ( void* ) entry;
    n -> type = ( uint32_t ) type;

    * np = n;
    return 0;
}



/*--------------------------------------------------------------------------
 * CCReplacedNode
 *  an archive file entry
 *  a file with a sub-directory
 */


/* Whack
 */
void CCReplacedNodeWhack ( CCReplacedNode *self )
{
    if ( self != NULL )
    {
        switch ( self -> type )
        {
        case ccFile:
            CCFileNodeWhack ( self -> entry );
            break;
        case ccArcFile:
            CCArcFileNodeWhack ( self -> entry );
            break;
        case ccChunkFile:
            CChunkFileNodeWhack ( self -> entry );
            break;
        }
        free ( self );
    }
}

/* Make
 *  creates an archive object
 */
rc_t CCReplacedNodeMake ( CCReplacedNode **np,
    enum CCType type, const void *entry )
{
    CCReplacedNode *n = malloc ( sizeof * n );
    if ( n == NULL )
        return RC ( rcExe, rcTree, rcInserting, rcMemory, rcExhausted );

    n -> entry = ( void* ) entry;
    n -> type = ( uint32_t ) type;

    * np = n;
    return 0;
}



/*--------------------------------------------------------------------------
 * CCTree
 *  a binary search tree with CCNodes
 */


/* Whack
 */
void CCTreeWhack ( CCTree *self )
{
    if ( self != NULL )
    {
        BSTreeWhack ( self, CCNameWhack, NULL );
        free ( self );
    }
}


/* Make
 *  make a root tree or sub-directory
 */
rc_t CCTreeMake ( CCTree **tp )
{
    CCTree *t = malloc ( sizeof * t );
    if ( t == NULL )
        return RC ( rcExe, rcTree, rcInserting, rcMemory, rcExhausted );

    BSTreeInit ( t );

    * tp = t;
    return 0;
}


/* Insert
 *  create an entry into a tree
 *  parses path into required sub-directories
 */
static
void CCTreePatchSubdirPath ( BSTNode *n, void *data )
{
    CCName *sym = ( CCName* ) n;
    sym -> dad = data;
}

rc_t CCTreeInsert ( CCTree *self, KTime_t mtime,
    enum CCType type, const void *entry, const char *path )
{
    rc_t rc;
    size_t sz;
    String name;
    CCName *dad, *sym;

    int i, j, len = strlen(path);

    while ( len > 0 && path [ len - 1 ] == '/' )
        --len;

    /* create/navigate path */
    for ( dad = NULL, i = 0; i < len; i = j + 1 )
    {
        for ( j = i; j < len; ++ j )
        {
            if ( path [ j ] == '/' )
            {
                /* detect non-empty names */
                sz = j - i;
                if ( sz != 0 )
                {
                    CCTree *dir;

                    /* ignore '.' */
                    if ( sz == 1 && path [ i ] == '.' )
                        break;

                    /* '..' is not allowed */
                    if ( sz == 2 && path [ i ] == '.' && path [ i + 1 ] == '.' )
                        return RC ( rcExe, rcTree, rcInserting, rcPath, rcIncorrect );

                    /* get name of directory */
                    StringInit ( & name, & path [ i ], sz, string_len ( & path [ i ], sz ) );

                    /* find existing */
                    sym = ( CCName* ) BSTreeFind ( self, & name, CCNameCmp );

                    /* handle a hard link */
                    while ( sym != NULL && sym -> type == ccHardlink )
                        sym = sym -> entry;

                    /* should be a directory-ish thing */
                    if ( sym != NULL )
                    {
                        switch ( sym -> type )
                        {
                        case ccContainer:
                        case ccArchive:
                            self = & ( ( CCContainerNode* ) sym -> entry ) -> sub;
                            break;
                        case ccDirectory:
                            self = sym -> entry;
                            break;
                        default:
                            return RC ( rcExe, rcTree, rcInserting, rcPath, rcIncorrect );
                        }

                        dad = sym;
                        break;
                    }

                    /* create new sub-directory */
                    rc = CCTreeMake ( & dir );
                    if ( rc != 0 )
                        return rc;

                    /* create directory name */
                    rc = CCNameMake ( & sym, mtime, dad, & name, ccDirectory, dir );
                    if ( rc != 0 )
                    {
                        CCTreeWhack ( dir );
                        return rc;
                    }

                    /* enter it into current directory
                       don't need to validate it's unique */
                    BSTreeInsert ( self, & sym -> n, CCNameSort );
                    dad = sym;
                    self = dir;
                }
                break;
            }
        }

        if ( j == len )
            break;
    }

    /* create entry name */
    if ( i >= len )
        return RC ( rcExe, rcTree, rcInserting, rcPath, rcIncorrect );
    sz = len - i;
    StringInit ( & name, & path [ i ], sz, string_len ( & path [ i ], sz ) );


    /* create named entry */
    rc = CCNameMake ( & sym, mtime, dad, & name, type, entry );
    if ( rc == 0 )
    {
#if 0
        /* enter it into tree */
        rc = BSTreeInsertUnique ( self, & sym -> n, NULL, CCNameSort );
        if ( rc != 0 )
            free ( sym );
#else
        BSTNode * n = BSTreeFind (self, &sym->name, CCNameCmp);
        if (n != NULL)
        {
            CCReplacedNode * rn;
            CCName * nn = (CCName*)n;

            switch (nn->type)
            {
            case ccDirectory:
                if (sym->type == ccDirectory)
                {
                    /* better would be to capture directory traits then goto */
                    nn->mtime = sym->mtime;

                    /* we aren't yet handling a directory duplicate other than tar files */

                    if (((CCContainerNode*)sym->entry)->sub.root != NULL)
                        rc = RC (rcExe, rcTree, rcInserting, rcNode, rcIncorrect);

                    goto skip_insert;
                }
            default:
                rc = CCReplacedNodeMake (&rn, nn->type, nn->entry);
                if (rc == 0)
                {
                    nn->type = ccReplaced;
                    nn->entry = rn;
                }
            }
        }
        if (rc == 0)
            rc = BSTreeInsert (self, &sym->n, CCNameSort);
    skip_insert:
        if (rc)
            free (sym);
#endif
        /* if this guy has children, become dad */
        else if ( entry != NULL ) switch ( type )
        {
        case ccContainer:
        case ccArchive:
            BSTreeForEach ( & ( ( CCContainerNode* ) entry ) -> sub, false, CCTreePatchSubdirPath, sym );
            break;
        case ccDirectory:
            BSTreeForEach ( entry, false, CCTreePatchSubdirPath, sym );
            break;
	default: /* shushing warnings */
	    break;
        }
    }

    return rc;
}

/* Find
 *  find a named node
 *  returns NULL if not found
 */
const CCName *CCTreeFind ( const CCTree *self, const char *path )
{
    size_t sz;
    String name;
    CCName /* *dad, */ *sym;

    int i, j, len = strlen(path);

    while ( len > 0 && path [ len - 1 ] == '/' )
        --len;

    /* create/navigate path */
    for ( /* dad = NULL, */ i = 0; i < len; i = j + 1 )
    {
        for ( j = i; j < len; ++ j )
        {
            if ( path [ j ] == '/' )
            {
                /* detect non-empty names */
                sz = j - i;
                if ( sz != 0 )
                {
                    /* ignore '.' */
                    if ( sz == 1 && path [ i ] == '.' )
                        break;

                    /* '..' is not allowed */
                    if ( sz == 2 && path [ i ] == '.' && path [ i + 1 ] == '.' )
                        return NULL;

                    /* get name of directory */
                    StringInit ( & name, & path [ i ], sz, string_len ( & path [ i ], sz ) );

                    /* find existing */
                    sym = ( CCName* ) BSTreeFind ( self, & name, CCNameCmp );

                    /* handle hard-link */
                    while ( sym != NULL && sym -> type == ccHardlink )
                        sym = sym -> entry;

                    /* handle not found */
                    if ( sym == NULL )
                        return NULL;

                    /* loop or return the found object */
                    switch ( sym -> type )
                    {
                    case ccContainer:
                    case ccArchive:
                        self = & ( ( CCContainerNode* ) sym -> entry ) -> sub;
                        break;
                    case ccDirectory:
                        self = sym -> entry;
                        break;
                    default:
                        return NULL;
                    }
                    
                    /* dad = sym; */
                    break;
                }
            }
        }

        if ( j == len )
            break;
    }

    if ( i >= len )
        return NULL;

    sz = len - i;
    StringInit ( & name, & path [ i ], sz, string_len ( & path [ i ], sz ) );
    return ( const CCName* ) BSTreeFind ( self, & name, CCNameCmp );
}

/* Link
 *  create a symlink to existing node
 */
rc_t CCTreeLink ( CCTree *self, KTime_t mtime,
    const char *targ, const char *alias )
{
    const CCName *orig = CCTreeFind ( self, targ );
    if ( orig == NULL )
        return RC ( rcExe, rcTree, rcInserting, rcPath, rcNotFound );

    return CCTreeInsert ( self, mtime, ccHardlink, orig, alias );
}
