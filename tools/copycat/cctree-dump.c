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

#include "cctree-priv.h"
#include "copycat-priv.h"
#include <klib/printf.h>
#include <klib/rc.h>
#include <kapp/main.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <time.h>


/*--------------------------------------------------------------------------
 * fowards
 */
static bool CCNameDump ( BSTNode *n, void *data );


/*--------------------------------------------------------------------------
 * CCDumper
 */
typedef struct CCDumper CCDumper;
struct CCDumper
{
    rc_t ( * flush ) ( void*, const void*, size_t );
    void *out;

    const char *sep;

#if ! STORE_ID_IN_NODE
    uint32_t id;
#endif
    uint32_t indent;
    rc_t rc;

    size_t total;
    char buffer [ 4096 ];
};


/* Init
 *  sets up block
 */
static
void CCDumperInit ( CCDumper *self,
    rc_t ( * flush ) ( void *out, const void *buffer, size_t size ), void *out )
{
    self -> flush = flush;
    self -> out = out;
    self -> sep = "";
#if ! STORE_ID_IN_NODE
    self -> id = 0;
#endif
    self -> indent = 0;
    self -> rc = 0;
    self -> total = 0;
}


/* Flush
 */
static
rc_t CCDumperFlush ( CCDumper *self )
{
    rc_t rc = ( * self -> flush ) ( self -> out, self -> buffer, self -> total );
    if ( rc == 0 )
        self -> total = 0;
    return rc;
}

static
rc_t CCDumperFlushLine ( CCDumper *self )
{
#if ! _DEBUGGING
    if ( self -> total < sizeof self -> buffer / 2 )
        return 0;
#endif
    return CCDumperFlush ( self );
}


/* Whack
 *  flushes buffer if necessary
 */
static
rc_t CCDumperWhack ( CCDumper *self )
{
    if ( self -> rc == 0 && self -> total != 0 )
        return CCDumperFlush ( self );
    return 0;
}


/* Write
 *  writes data to buffer, flushes as necessary
 */
static
rc_t CCDumperWrite ( CCDumper *self, const char *buffer, size_t size )
{
    rc_t rc;
    size_t total, num_writ;

    for ( rc = 0, total = 0; total < size; total += num_writ )
    {
        if ( self -> total == sizeof self -> buffer )
        {
            rc = CCDumperFlush ( self );
            if ( rc != 0 )
                break;
        }

        num_writ = size - total;
        if ( num_writ > sizeof self -> buffer - self -> total )
            num_writ = sizeof self -> buffer - self -> total;

        memcpy ( & self -> buffer [ self -> total ], & buffer [ total ], num_writ );
        self -> total += num_writ;
    }

    return rc;
}

/* IndentLevel
 *  increase or decrease indentation level
 */
static
void CCDumperIncIndentLevel ( CCDumper *self )
{
    ++ self -> indent;
}

static
void CCDumperDecIndentLevel ( CCDumper *self )
{
    if ( self -> indent > 0 )
        -- self -> indent;
}


/* Indent
 *  writes indentation spacing
 */
static
rc_t CCDumperIndent ( CCDumper *self )
{
    rc_t rc;
    uint32_t total, num_writ;

    /* use 2 spaces per tab */
    const char *tabs = "                                ";

    for ( rc = 0, total = 0; total < self -> indent; total += num_writ )
    {
        num_writ = ( ( self -> indent - total - 1 ) & 0xF ) + 1;
        rc = CCDumperWrite ( self, tabs, num_writ + num_writ );
        if ( rc != 0 )
            break;
    }

    return rc;
}

/* Sep
 *  write separator string
 */
static
rc_t CCDumperSep ( CCDumper *self )
{
    if ( self -> sep == NULL )
        return 0;

    return CCDumperWrite ( self, self -> sep, strlen ( self -> sep ) );
}

/* Print
 *  \t - indent
 *  \n - end of line
 *  %d - integer
 *  %u - unsigned
 *  %ld - int64_t
 *  %lu - uint64_t
 *  %f - double
 *  %s - null-terminated C-string
 *  %p - separator
 *  %S - String*
 *  %I - unique id
 *  %T - timestamp
 *  %M - MD5 digest
 *  %C - CRC32
 *  %N - CCName*
 *  %F - full CCName*
 */
static
rc_t StringPrint ( const String *self, CCDumper *d )
{
/* BUFF_WARNING_TRACK has to be larger than all size differences
 * between a special character and its encoding in XML
 * 8 is big enough for all I know about when writing this with some
 * safety margin
 */
#define BUFF_WARNING_TRACK 8
#define BUFF_SIZE (256)
#define REPLACE_COPY(C,S) \
    case C: \
        sp++;\
        count--;\
        memcpy (bp,(S),sizeof(S)-1);\
        bp += sizeof(S)-1;\
        break

    char buff [256 + 5];
    char * bp;
    const char * sp;
    size_t count;
    rc_t rc = 0;

    /* start at the beginnings of the string and buffer */
    sp = self->addr;
    bp = buff;

    count = self->size;

    while (count > 0)
    {
        if ((bp - buff) > (BUFF_SIZE - BUFF_WARNING_TRACK))
        {
            rc = CCDumperWrite (d, buff, bp-buff);
            if (rc)
                return rc;
            bp = buff;
        }
        switch (*sp)
        {
            /* just copy most characters */
        default:
            *bp++ = *sp++;
            count --;
            break;

            REPLACE_COPY('<',"&lt;");
            REPLACE_COPY('>',"&gt;");
            REPLACE_COPY('&',"&amp;");
            REPLACE_COPY('"',"&quot;");
            REPLACE_COPY('\'',"&apos;");

        }
    }
    if (bp > buff)
        rc = CCDumperWrite (d, buff, bp-buff);
    return rc;

}

static
rc_t CCNamePrint ( const CCName *self, CCDumper *d )
{
    return StringPrint ( & self -> name, d );
}

static
rc_t CCNamePrintFull ( const CCName *self, CCDumper *d )
{
    if ( self -> dad != NULL )
    {
        rc_t rc = CCNamePrintFull ( self -> dad, d );
        if ( rc == 0 )
            rc = CCDumperWrite ( d, "/", 1 );
        if ( rc != 0 )
            return rc;
    }
    return CCNamePrint ( self, d );
}

static
rc_t KTimePrint ( KTime_t self, CCDumper *d )
{
    int len;
    char buffer [ 64 ];
    time_t t = ( time_t ) self;

    struct tm gmt;
    gmtime_r ( & t, & gmt );
    len = sprintf ( buffer, "%04d-%02d-%02dT%02d:%02d:%02dZ"
                    , gmt . tm_year + 1900
                    , gmt . tm_mon + 1
                    , gmt . tm_mday
                    , gmt . tm_hour
                    , gmt . tm_min
                    , gmt . tm_sec
        );

    return CCDumperWrite ( d, buffer, len );
}

static
rc_t MD5Print ( const uint8_t *digest, CCDumper *d )
{
    int i, len;
    char buff [ 36 ];

    for ( i = len = 0; i < 16; ++ i )
        len += sprintf ( & buff [ len ], "%02x", digest [ i ] );

    return CCDumperWrite ( d, buff, 32 );
}

static
rc_t CCDumperVPrint ( CCDumper *self, const char *fmt, va_list args )
{
    rc_t rc;
    const char *start, *end;

    for ( rc = 0, start = end = fmt; * end != 0; ++ end )
    {
        int len;
        size_t size;
        char buffer [ 256 ];

        switch ( * end )
        {
        case '\t':
            if ( end > start )
                rc = CCDumperWrite ( self, start, end - start );
            if ( rc == 0 )
                rc = CCDumperIndent ( self );
            start = end + 1;
            break;
        case '\n':
            rc = CCDumperWrite ( self, start, end - start + 1 );
            if ( rc == 0 )
                rc = CCDumperFlushLine ( self );
            start = end + 1;
            break;
        case '%':
            if ( end > start )
            {
                rc = CCDumperWrite ( self, start, end - start );
                if ( rc != 0 )
                    break;
            }
            switch ( * ( ++ end ) )
            {
            case 'd':
                len = sprintf ( buffer, "%d", va_arg ( args, int ) );
                rc = CCDumperWrite ( self, buffer, len );
                break;
            case 'u':
                len = sprintf ( buffer, "%u", va_arg ( args, unsigned int ) );
                rc = CCDumperWrite ( self, buffer, len );
                break;
            case 'x':
                len = sprintf ( buffer, "%x", va_arg ( args, unsigned int ) );
                rc = CCDumperWrite ( self, buffer, len );
                break;
            case 'f':
                len = sprintf ( buffer, "%f", va_arg ( args, double ) );
                rc = CCDumperWrite ( self, buffer, len );
                break;
            case 'l':
                switch ( * ( ++ end ) )
                {
                case 'd':
                    rc = string_printf ( buffer, sizeof buffer, & size, "%ld", va_arg ( args, int64_t ) );
                    if ( rc == 0 )
                        rc = CCDumperWrite ( self, buffer, size );
                    break;
                case 'u':
                    rc = string_printf ( buffer, sizeof buffer, & size, "%lu", va_arg ( args, uint64_t ) );
                    if ( rc == 0 )
                        rc = CCDumperWrite ( self, buffer, size );
                    break;
                }
                break;
            case 's':
                len = sprintf ( buffer, "%s", va_arg ( args, const char* ) );
                rc = CCDumperWrite ( self, buffer, len );
                break;
            case 'p':
                rc = CCDumperSep ( self );
                break;
            case 'S':
                rc = StringPrint ( va_arg ( args, const String* ), self );
                break;
            case 'I':
#if STORE_ID_IN_NODE
                len = sprintf ( buffer, "%u", va_arg ( args, uint32_t ) );
#else
                len = sprintf ( buffer, "%u", ++ self -> id );
#endif
                rc = CCDumperWrite ( self, buffer, len );
                break;
            case 'T':
                rc = KTimePrint ( va_arg ( args, KTime_t ), self );
                break;
            case 'M':
                rc = MD5Print ( va_arg ( args, const uint8_t* ), self );
                break;
            case 'C':
                len = sprintf ( buffer, "%08x", va_arg ( args, unsigned int ) );
                rc = CCDumperWrite ( self, buffer, len );
                break;
            case 'N':
                rc = CCNamePrint ( va_arg ( args, const CCName* ), self );
                break;
            case 'F':
                rc = CCNamePrintFull ( va_arg ( args, const CCName* ), self );
                break;
            case '%':
                rc = CCDumperWrite ( self, "%", 1 );
                break;
            }
            start = end + 1;
            break;
        }

        if ( rc != 0 )
            break;
    }

    if ( rc == 0 && end > start )
    {
        rc = CCDumperWrite ( self, start, end - start );
        if ( rc == 0 )
            rc = CCDumperFlushLine ( self );
    }

    return rc;
}

static
rc_t CCDumperPrint ( CCDumper *self, const char *fmt, ... )
{
    rc_t rc;
    va_list args;

    va_start ( args, fmt );
    rc = CCDumperVPrint ( self, fmt, args );
    va_end ( args );

    return rc;
}


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

/* Dump
 */
static
rc_t CCFileNodeDumpCmn ( const CCFileNode *self, const char *tag,
    const CCName *name, const String *cached, CCDumper *d )
{
    rc_t rc = CCDumperPrint ( d,
                              "\t<%s "             /* node class */
                              "id=\"%I\" "         /* unique id  */
                              "path=\"%F\" "       /* full path  */
                              "name=\"%N\" "       /* node name  */
                              , tag
#if STORE_ID_IN_NODE
                              , self -> id
#endif
                              , name
                              , name );
    if ( rc == 0 && cached != NULL )
        rc = CCDumperPrint ( d,
                             "cached=\"%S\" "     /* cached name */
                             , cached );
    if ( rc == 0 )
        rc = CCDumperPrint ( d, 
                             "size=\"%lu\" "
                             , self->size );
    if (( rc == 0 ) && ( self->size > 0 ) && ( self->lines != 0 ))
        rc = CCDumperPrint ( d, 
                             "lines=\"%lu\" "
                             , self->lines );
    if ( rc == 0 )
        rc = CCDumperPrint ( d,
                             "mtime=\"%T\" "      /* mod time   */
                             , name -> mtime );
    if ( rc == 0 )
    {
        if ( self -> rc )
            rc = CCDumperPrint ( d,
                                 "filetype=\"Errored%s\" "   /* file type  */
                                 , self -> ftype );
        else
            rc = CCDumperPrint ( d,
                                 "filetype=\"%s\" "   /* file type  */
                                 , self -> ftype );
    }
    if ( rc == 0 && ! no_md5 )
        rc = CCDumperPrint ( d,
                             "md5=\"%M\" "         /* md5 digest */
                             , self -> _md5 );

    return rc;
}

typedef struct dump_log_data
{
    rc_t rc;
    CCDumper * d;
} dump_log_data;

static
rc_t CCNodeDumpLog ( void * n, CCDumper * d )
{
    String s;

    StringInitCString (&s, n); /* cast after add gets past node */

    return CCDumperPrint ( d, "\t<CCError>%S</CCError>\n", &s );
}


static
rc_t CCFileNodeDump ( const CCFileNode *cself, const char *tag,
    const CCName *name, const String *cached, CCDumper *d, bool close )
{
    rc_t rc;
    bool trunc;
    CCFileNode * self = (CCFileNode *)cself;

    trunc = ((self->expected != SIZE_UNKNOWN) && 
             (self->expected != self->size));

    rc = CCFileNodeDumpCmn ( self, tag, name, cached, d );
    if ( rc == 0 && self -> crc32 != 0 )
        rc = CCDumperPrint ( d, " crc32=\"%C\"", self -> crc32 );
    if ( rc == 0 )
    {
        if (self->err || trunc || (self->logs.head != NULL))
        {
            do
            {
                rc = CCDumperPrint (d, ">\n");
                if (rc) break;

                if (trunc)
                {
                    rc = CCDumperPrint (d, "\t<CCErrSize expected=\"%lu\">"
                                        "Error in file size expected %lu but got %lu"
                                        "</CCErrSize>\n", self->expected,
                                        self->expected, self->size);
                    if (rc) break;
                }
                
                if (self->err)
                {
                    if (self->logs.head != NULL)
                    {
                        SLNode* log;

                        while ((log = SLListPopHead (&self->logs)) != NULL)
                        {
                            CCNodeDumpLog (log+1, d);
                            free (log);
                        }
                    }
                    else
                        rc = CCDumperPrint (d, "\t<CCErr>Not specified</CCErr>\n");
                }
                if (rc) break;

                if (close)
                    rc = CCDumperPrint (d, "\t</%s>\n", tag);
            } while (0);
        }
        else if (close)
            rc = CCDumperPrint (d, "/>\n");
        else
            rc = CCDumperPrint (d, ">\n");                
    }
    return rc;
}

/*--------------------------------------------------------------------------
 * CCArcFileNode
 *  a file with an offset into another file
 */

/* Dump
 */
static
rc_t CCArcFileNodeDump ( const CCArcFileNode *cself, const char *tag,
    const CCName *name, const String *cached, CCDumper *d, bool close )
{
    rc_t rc;
    bool trunc;
    CCArcFileNode * self = (CCArcFileNode *)cself;

    trunc = ((self->dad.expected != SIZE_UNKNOWN) && 
             (self->dad.expected != self->dad.size));

    rc = CCFileNodeDumpCmn ( & self -> dad, tag, name, cached, d );
    if ( rc == 0 )
    {
        if (!xml_dir)
            rc = CCDumperPrint ( d, " offset=\"%lu\"", self->offset);
        if (rc == 0) do
        {
            if (self->dad.err || trunc || (self->dad.logs.head != NULL))
            {
                rc = CCDumperPrint (d, ">\n");
                if (rc) break;

                if (trunc)
                {
                    rc = CCDumperPrint (d, "\t<CCErrSize expected=\"%lu\">"
                                        "Error in file size expected %lu but got %lu"
                                        "</CCErrSize>\n", self->dad.expected,
                                        self->dad.expected, self->dad.size);
                    if (rc) break;
                }

                if (self->dad.err)
                {
                    if (self->dad.logs.head != NULL)
                    {
                        SLNode* log;

                        while ((log = SLListPopHead (&self->dad.logs)) != NULL)
                        {
                            CCNodeDumpLog (log+1, d);
                            free (log);
                        }
                    }
                    else
                        rc = CCDumperPrint (d, "\t<CCErr>Not specified</CCErr>\n");
                }
                if (rc) break;

                if (close)
                    rc = CCDumperPrint (d, "\t</%s>\n", tag);
            }
            else if (close)
                rc = CCDumperPrint (d, "/>\n");
            else
                rc = CCDumperPrint (d, ">\n");                
        } while (0);
    }
    return rc;
}


/*--------------------------------------------------------------------------
 * CChunkFileNode
 *  a file with one or more chunks (offset/size) into another file
 */

/* Dump
 */
static
bool CChunkDump ( SLNode *n, void *data )
{
    CCDumper *d = data;
    const CChunk *self = ( const CChunk* ) n;

    d -> rc = CCDumperPrint ( d, "\t<chunk offset=\"%lu\" size=\"%lu\"/>\n",
                              self -> offset, self -> size );

    return ( d -> rc != 0 ) ? true : false;
}

static
rc_t CChunkFileNodeDump ( const CChunkFileNode *self, const char *tag,
    const CCName *name, const String *cached, CCDumper *d, bool close )
{
    rc_t rc = CCFileNodeDumpCmn ( & self -> dad, tag, name, cached, d );
    if ( rc == 0 )
        rc = CCDumperPrint ( d, " size=\"%lu\">\n", self -> dad . size );
    CCDumperIncIndentLevel ( d );
    if ( rc == 0 )
    {
        if ( SLListDoUntil ( & self -> chunks, CChunkDump, d ) )
            rc = d -> rc;
    }
    CCDumperDecIndentLevel ( d );
    if ( rc == 0 && close )
        rc = CCDumperPrint ( d, "\t</%s>\n", tag );

    return rc;
}


/*--------------------------------------------------------------------------
 * CCCachedFileNode
 *  a file wrapper with cached file name
 */

/* Dump
 */
#if 0 /* why is this commented out... */
static
rc_t CCCachedFileNodeDump ( const CCCachedFileNode *self,
    const CCName *name, CCDumper *d )
{
    rc_t rc;
    const void *entry = self -> entry;

    switch ( self -> type )
    {
    case ccFile:
    case ccContFile:
        rc = CCFileNodeDump ( entry, "file", name, & self -> cached, d, true );
        break;
    case ccArcFile:
        rc = CCArcFileNodeDump ( entry, "file", name, & self -> cached, d, true );
        break;
    case ccChunkFile:
        rc = CChunkFileNodeDump ( entry, "file", name, & self -> cached, d, true );
        break;
    default:
        rc = RC ( rcExe, rcTree, rcWriting, rcNode, rcUnrecognized );
    }

    return rc;
}
#endif

/*--------------------------------------------------------------------------
 * CCContainerNode
 *  a container/archive file entry
 *  a file with sub-entries
 */

/* Dump
 */
static
rc_t CCContainerNodeDump ( const CCContainerNode *self, const char *node,
    const CCName *name, CCDumper *d )
{
    rc_t rc;
    const void *entry = self -> entry;

    switch ( self -> type )
    {
    case ccFile:
    case ccContFile:
        rc = CCFileNodeDump ( entry, node, name, NULL, d, false );
        break;
    case ccArcFile:
        rc = CCArcFileNodeDump ( entry, node, name, NULL, d, false );
        break;
    case ccChunkFile:
        rc = CChunkFileNodeDump ( entry, node, name, NULL, d, false );
        break;
    default:
        rc = RC ( rcExe, rcTree, rcWriting, rcNode, rcUnrecognized );
    }

    if ( rc != 0 )
        return rc;

    CCDumperIncIndentLevel ( d );

    if ( BSTreeDoUntil ( & self -> sub, false, CCNameDump, d ) )
        rc = d -> rc;

    CCDumperDecIndentLevel ( d );

    if ( rc == 0 )
        rc = CCDumperPrint ( d, "\t</%s>\n", node );

    return rc;
}


/*--------------------------------------------------------------------------
 * CCSymlinkNode
 *  a directory entry with a substitution path
 */

/* Dump
 */
static
rc_t CCSymlinkNodeDump ( const CCSymlinkNode *self, const CCName *name, CCDumper *d, bool replaced )
{
    const char * tag = replaced ? "replaced-symlink" : "symlink";
    return CCDumperPrint ( d, "\t<%s name=\"%N\" mtime=\"%T\">%S</%s>\n",
                           tag, name, name -> mtime, & self -> path, tag );
}


/*--------------------------------------------------------------------------
 * CCTreeNode
 *  doesn't actually exist, but is treated separately from tree
 */

/* Dump
 */
static
rc_t CCTreeNodeDump ( const CCTree *self, const CCName *name, CCDumper *d, bool replaced )
{
    const char * tag = replaced ? "replaced-directory" : "directory";
    rc_t rc = CCDumperPrint ( d, "\t<%s name=\"%N\" mtime=\"%T\">\n",
                              tag, name, name -> mtime );

    CCDumperIncIndentLevel ( d );

    if ( rc == 0 && BSTreeDoUntil ( self, false, CCNameDump, d ) )
        rc = d -> rc;

    CCDumperDecIndentLevel ( d );

    if ( rc == 0 )
        rc = CCDumperPrint ( d, "\t</%s>\n", tag );

    return rc;
}


/*--------------------------------------------------------------------------
 * CCName
 *  the main entrypoint
 */

/* Dump
 */
static
bool CCNameDump ( BSTNode *n, void *data )
{
    CCDumper * d = data;
    const CCName * self = (const CCName*)n;
    void * entry = self->entry;
    uint32_t type = self->type;
    bool replaced = false;

    if (type == ccReplaced)
    {
        const CCReplacedNode * node = entry;
        type = node->type;
        entry = node->entry;
        replaced = true;
    }

    if (type == ccCached)
    {
        const CCCachedFileNode * node = entry;
        type = node->type;
        entry = node->entry;
    }

    if ( type == ccHardlink )
    {
        do
        {
            /* if for some reason the link is broken */
            if ( self -> entry == NULL )
                return false;
            self = self -> entry;
        }
        while ( self -> type == ccHardlink );

        entry = self -> entry;
        type = self -> type;
        self = ( const CCName* ) n;
    }

    switch ( type )
    {
    case ccFile:
    case ccContFile:
        d -> rc = CCFileNodeDump ( entry, replaced ? "replaced-file" : "file", self, NULL, d, true );
        break;
    case ccArcFile:
        d -> rc = CCArcFileNodeDump ( entry, replaced ? "replaced-file" : "file", self, NULL, d, true );
        break;
    case ccChunkFile:
        d -> rc = CChunkFileNodeDump ( entry, replaced ? "replaced-file" : "file", self, NULL, d, true );
        break;
    case ccContainer:
        d -> rc = CCContainerNodeDump ( entry, replaced ? "replaced-container" : "container", self, d );
        break;
    case ccArchive:
        d -> rc = CCContainerNodeDump ( entry, replaced ? "replaced-archive" : "archive", self, d );
        break;
    case ccSymlink:
        d -> rc = CCSymlinkNodeDump ( entry, self, d, replaced );
        break;
    case ccDirectory:
        d -> rc = CCTreeNodeDump ( entry, self, d, replaced );
        break;
    case ccCached:
#if 0
        d -> rc = CCCachedFileNodeDump ( entry, self, d );
#else
        d -> rc = RC ( rcExe, rcTree, rcWriting, rcNode, rcCorrupt );
#endif
        break;
    default:
        d -> rc = RC ( rcExe, rcTree, rcWriting, rcNode, rcUnrecognized );
    }

    return ( d -> rc != 0 ) ? true : false;
}

    

/*--------------------------------------------------------------------------
 * CCTree
 *  a binary search tree with CCNodes
 */

/* Dump
 *  dump tree using provided callback function
 *
 *  "write" [ IN, NULL OKAY ] and "out" [ IN, OPAQUE ] - callback function
 *  for writing. if "write" is NULL, print to stdout.
 */
static
rc_t CCTreeDumpInt2 ( const CCTree *self, CCDumper *d, SLList * logs )
{
    rc_t rc = 0;

    /* print logs attached to this node */
    if (logs->head != NULL)
    {
        SLNode * log;
        while ((log = SLListPopHead (logs)) != NULL)
        {
            CCNodeDumpLog (log+1, d);
            free (log);
        }
    }

    CCDumperIncIndentLevel ( d );

    if ( BSTreeDoUntil ( self, false, CCNameDump, d ) )
        rc = d -> rc;

    CCDumperDecIndentLevel ( d );
    
    return rc;
}

/* print root node and call out to print what that contains */
static
rc_t CCTreeDumpInt ( const CCTree *self, CCDumper *d, SLList * logs )
{
    ver_t v = KAppVersion ();
    rc_t rc = CCDumperPrint ( d, "<ROOT version=\"%u.%u.%u\">\n",
                              VersionGetMajor(v),
                              VersionGetMinor(v),
                              VersionGetRelease(v));
    if ( rc == 0 )
    {
        rc = CCTreeDumpInt2 ( self, d, logs );

        if ( rc == 0 )
            CCDumperPrint ( d, "</ROOT>\n" );
    }
    return rc;
}

static
rc_t write_FILE ( void *out, const void *buffer, size_t bytes )
{
    size_t num_writ;

    if ( bytes == 0 )
        return 0;

    num_writ = fwrite ( buffer, 1, bytes, out );
    if ( num_writ == bytes )
        return 0;
    if ( num_writ != 0 )
        return RC ( rcExe, rcFile, rcWriting, rcTransfer, rcIncomplete );
    if ( buffer == NULL )
        return RC ( rcExe, rcFile, rcWriting, rcParam, rcNull );

    return RC ( rcExe, rcFile, rcWriting, rcNoObj, rcUnknown );
}

rc_t CCTreeDump ( const CCTree *self,
    rc_t ( * write ) ( void *out, const void *buffer, size_t bytes ),
                  void *out, SLList * logs )
{
    rc_t rc, rc2;
    CCDumper d;

    if ( write == NULL )
    {
        write = write_FILE;
        out = stdout;
    }

    CCDumperInit ( & d, write, out );

    rc = CCTreeDumpInt ( self, & d, logs );

    rc2 = CCDumperWhack ( & d );

    return rc ? rc : rc2;
}
