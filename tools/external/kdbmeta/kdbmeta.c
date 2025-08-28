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
/*
 * This program has a command line sytax that isn't suited to the
 * normally expected way of handling command line arguments.
 *
 * We'll use the normal Args processing for the standard options
 * but a simple stepping throught the argv stored arguments
 * for the "big loop"
 */


/* The original intention of this tool was to support updating nodes.
   This was later removed with the thought that this functionality would
   be better implemented by another tool.

   Going to temporarily add it in...
*/
#define ALLOW_UPDATE 1

#include <stdint.h>
#include <sra/srapath.h>
#include <kdb/manager.h>
#include <kdb/database.h>
#include <kdb/table.h>
#include <kdb/column.h>
#include <kdb/meta.h>
#include <kdb/namelist.h>
#include <kdb/kdb-priv.h>

#include <kfg/config.h> /* KConfigSetNgcFile */

#include <vfs/manager.h>
#include <vfs/resolver.h>
#include <vfs/path.h>
#include <kapp/main.h>
#include <kapp/args.h>
#include <klib/text.h>
#include <klib/printf.h>
#include <klib/vector.h>
#include <klib/namelist.h>
#include <klib/log.h>
#include <klib/out.h>
#include <klib/writer.h>
#include <klib/rc.h>
#include <kfs/file.h>
#include <sysalloc.h>
#include <os-native.h>


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

/* use cpp define CONST */
#undef CONST
#if ALLOW_UPDATE
#define CONST
#else
#define CONST const
#endif

typedef struct KDBMetaParms KDBMetaParms;
struct KDBMetaParms
{
    CONST KDBManager *mgr;
    CONST KMetadata *md;
    const VPath *local;
    const VPath *remote;
    const VPath *cache;
    const char *targ;
    const Vector *q;
    rc_t rc;
};

/* select reporting
 */
static bool xml_ish = true;
static int indent_lvl;
static int tabsz = 2;
static const char *spaces = "                                ";
static bool as_unsigned = false;
static bool as_valid_xml = true;
static const char *table_arg = NULL;
static bool read_only_arg = true;

static
void indent ( void )
{
    if ( indent_lvl > 0 )
    {
        int total, num_spaces = indent_lvl * tabsz;
        for ( total = 0; total < num_spaces; total += 32 )
        {
            int to_write = num_spaces - total;
            if ( to_write > 32 )
                to_write = 32;
            OUTMSG (( & spaces [ 32 - to_write ] ));
        }
    }
}

static
void node_open ( const char *path, size_t plen )
{
    if ( plen && path[plen - 1] == '/' ) {
    /* do not use trailing slash */
        --plen;
    }
    if ( xml_ish )
    {
        int size = plen;
        /* find leaf name of node */
        const char *slash = string_rchr ( path, plen, '/' );
        if ( slash ) {
            /* make sure not to print trailing slash */
            size = plen - (slash - path) - 1;
            if ( size < 0 ) {
                size = 0;
            }
        }

        indent ();
        OUTMSG (( "<%.*s", size, ( slash != NULL ) ? slash + 1 : path ));
    }
}

static
void node_close ( const char *path, size_t plen,
    size_t vsize, uint32_t num_children, bool close_indent )
{
    if ( plen && path[plen - 1] == '/' ) {
        --plen;
    }
    if ( xml_ish )
    {
        if ( vsize == 0 && num_children == 0 )
            OUTMSG (( "/>\n" ));
        else
        {
            int size = plen;
            const char *slash = string_rchr ( path, plen, '/' );
            if ( slash ) {
                size = plen - (slash - path) - 1;
                if ( size < 0 ) {
                    size = 0;
                }
            }
            if ( close_indent )
                indent ();
            OUTMSG (( "</%.*s>\n", size, ( slash != NULL ) ? slash + 1 : path ));
        }
    }
}

static
void attr_select ( const char *name, const char *value )
{
    if ( xml_ish )
        OUTMSG (( " %s=\"%s\"", name, value ));
}

static void value_print(char value) {
    const char *replacement = NULL;

    switch (value) {
        case '\"':
            replacement = "&quot;";
            break;
        case '&':
            replacement = "&amp;";
            break;
        case '<':
            replacement = "&lt;";
            break;
        case '>':
            replacement = "&gt;";
            break;
        default:
            break;
    }

    if (replacement != NULL && as_valid_xml) {
        OUTMSG(("%s", replacement));
    }
    else {
        OUTMSG(("%c", value));
    }
}

static
void value_select ( const char *value, size_t vlen, uint32_t num_children, bool *close_indent )
{
    if ( xml_ish )
    {
        size_t i;
        bool binary = false;

        /* discover if text or apparently binary */
        for ( i = 0; i < vlen && ! binary; ++ i )
        {
            if ( ! isprint ( value [ i ] ) )
            {
                switch ( value [ i ] )
                {
                case '\t':
                case '\r':
                case '\n':
                    break;
                default:
                    binary = true;
                }
            }
        }

        /* if there are children, create special tag */
        if ( num_children != 0 || ( binary && vlen > 16 ) || ( vlen > 64 ) )
        {
            OUTMSG (( ">\n" ));
            ++ indent_lvl;
            indent ();
            if ( num_children != 0 )
                OUTMSG (( "<%%>" ));
        }
        else
        {
            OUTMSG (( ">" ));
        }

        /* if binary, print as hex */
        if ( binary )
        {
            bool as_hex = !as_unsigned;
            if( as_unsigned ) {
                switch(vlen) {
                    case 1:
                        OUTMSG (( "%hu", ( ( const uint8_t* ) value ) [ 0 ] ));
                        break;
                    case 2:
                        OUTMSG (( "%u", ( ( const uint16_t* ) value ) [ 0 ] ));
                        break;
                    case 4:
                        OUTMSG (( "%u", ( ( const uint32_t* ) value ) [ 0 ] ));
                        break;
                    case 8:
                        OUTMSG (( "%lu", ( ( const uint64_t* ) value ) [ 0 ] ));
                        break;
                    default:
                        as_hex = true;
                        break;
                }
            }
            if( as_hex ) {
                for ( i = 0; i < vlen; ++ i )
                {
                    if ( i != 0 && ( i & 15 ) == 0 )
                    {
                        OUTMSG (( "\n" ));
                        indent ();
                    }
                    OUTMSG (( "\\x%02X", ( ( const uint8_t* ) value ) [ i ] ));
                }
            }
        }

        /* text */
        else
        {
            /* int tab_stop; */

            OUTMSG (( "'" ));
            for ( /* tab_stop = 0, */ i = 0; i < vlen; ++ i )
            {
                switch ( value [ i ] )
                {
                case '\t': /* OUTMSG (( "%.*s", 4 - ( tab_stop & 3 ), "    " )); */
                    /* tab_stop = ( tab_stop + 4 ) & -4; */
                    OUTMSG (( "\t" ));
                    break;
                case '\r': OUTMSG (( "\\r" ));
                    break;
                case '\n': OUTMSG (( "\n" ));
                    indent ();
                    if ( i + 1 < vlen )
                        OUTMSG (( " " ));
                    /* tab_stop = 0; */
                    break;
                default:
                    value_print(value[i]);
                }
            }
            OUTMSG (( "'" ));
        }

        /* if there are children, close special tag */
        if ( num_children != 0 )
        {
            OUTMSG (( "</%%>\n" ));
            -- indent_lvl;
            * close_indent = true;
        }
        else if ( ( binary && vlen > 16 ) || ( vlen > 64 ) )
        {
            OUTMSG (( "\n" ));
            -- indent_lvl;
            * close_indent = true;
        }
    }
}

static
void children_begin ( size_t vsize, uint32_t num_children )
{
    ++ indent_lvl;

    if ( vsize == 0 && num_children != 0 )
        OUTMSG (( ">\n" ));
}

static
void children_end ( void )
{
    if ( indent_lvl > 0 )
        -- indent_lvl;
}


/* select
 */
static
rc_t md_select_expr ( const KMDataNode *node, char *path, size_t psize, int plen, const char *attr, bool wildcard )
{
    rc_t rc;
    const char *name;
    uint32_t i, num_children;
    KNamelist *children;
    bool close_indent = false;

    size_t vsize = 0;
    char value [ 1024 ];

    /* list node children */
    rc = KMDataNodeListChild ( node, & children );
    if ( rc == 0 )
        rc = KNamelistCount ( children, & num_children );
    if ( rc != 0 )
    {
        PLOGERR ( klogErr,  (klogErr, rc, "failed to list child nodes of '$(path)'", "path=%s", path ));
        return rc;
    }

    /* report on single node */
    if ( ! wildcard )
    {
        if ( attr != NULL )
        {
            /* report only attribute */
            rc = KMDataNodeReadAttr ( node, attr, value, sizeof value, & vsize );
            if ( rc != 0 )
            {
                KNamelistRelease ( children );
                if ( GetRCState ( rc ) == rcNotFound )
                    return 0;

                PLOGERR ( klogErr,  (klogErr, rc, "failed to read attribute '$(path)@$(attr)'", "path=%s,attr=%s", path, attr ));
                return rc;
            }

            /* open node report */
            node_open ( path, plen );

            /* report attribute */
            attr_select ( attr, value );

            /* close it off */
            node_close ( path, plen, 0, 0, false );

            /* Exit here if an attribute was requested in query */
            return 0;
        }
        else
        {
            uint32_t count;
            size_t remaining;
            char *vp = value;
            KNamelist *attrs;

            /* report entire node */
            rc = KMDataNodeListAttr ( node, & attrs );
            if ( rc == 0 )
                rc = KNamelistCount ( attrs, & count );
            if ( rc != 0 )
            {
                KNamelistRelease ( children );
                PLOGERR ( klogErr,  (klogErr, rc, "failed to list attributes of '$(path)'", "path=%s", path ));
                return rc;
            }

            /* open node report */
            node_open ( path, plen );

            /* report each attribute */
            for ( i = 0; i < count; ++ i )
            {
                rc = KNamelistGet ( attrs, i, & name );
                if ( rc != 0 )
                    PLOGERR ( klogWarn,  (klogWarn, rc, "failed to read attribute name $(idx) of '$(path)'", "idx=%u,path=%s", i, path ));
                else
                {
                    rc = KMDataNodeReadAttr ( node, name, value, sizeof value, & vsize );
                    if ( rc != 0 )
                    {
                        KNamelistRelease ( children );
                        KNamelistRelease ( attrs );
                        PLOGERR ( klogErr,  (klogErr, rc, "failed to read attribute '$(path)@$(attr)'", "path=%s,attr=%s", path, name ));
                        return rc;
                    }

                    /* report attribute */
                    attr_select ( name, value );
                }
            }

            /* done with header */
            KNamelistRelease ( attrs );

            /* read node value */
            rc = KMDataNodeRead ( node, 0, value, sizeof value, & vsize, & remaining );
            if ( rc != 0 )
            {
                KNamelistRelease ( children );
                PLOGERR ( klogErr,  (klogErr, rc, "failed to read value of '$(path)'", "path=%s", path ));
                return rc;
            }

            /* allocate a larger buffer if not complete */
            if ( remaining != 0 )
            {
                size_t remaining_vsize;

                vp = malloc ( vsize + remaining );
                if ( vp == NULL )
                {
                    KNamelistRelease ( children );
                    rc = RC ( rcExe, rcMetadata, rcAllocating, rcMemory, rcExhausted );
                    PLOGERR ( klogInt,  (klogInt, rc, "failed to read value of '$(path)'", "path=%s", path ));
                    return rc;
                }
                memmove ( vp, value, vsize );
                rc = KMDataNodeRead ( node, vsize, & vp [ vsize ], remaining, & remaining_vsize, & remaining );
                if ( rc == 0 && remaining != 0 )
                    rc = RC ( rcExe, rcMetadata, rcReading, rcTransfer, rcIncomplete );
                if ( rc != 0 )
                {
                    free ( vp );
                    KNamelistRelease ( children );
                    PLOGERR ( klogErr,  (klogErr, rc, "failed to read value of '$(path)'", "path=%s", path ));
                    return rc;
                }

                vsize += remaining_vsize;
            }

            /* report node value */
            if ( vsize != 0 )
                value_select ( vp, vsize, num_children, & close_indent );

            /* whack allocation */
            if ( vp != value )
                free ( vp );
        }
    }

    if ( ! wildcard )
        children_begin ( vsize, num_children );

    /* if there are children, list them now */
    for ( i = 0; i < num_children; ++ i )
    {
        rc = KNamelistGet ( children, i, & name );
        if ( rc != 0 )
            PLOGERR ( klogWarn,  (klogWarn, rc, "failed to read child name $(idx) of '$(path)'", "idx=%u,path=%.*s", i, plen, path ));
        else
        {
            size_t childlen;
            rc = string_printf ( & path [ plen ], psize - plen, & childlen, "/%s", name );
            if ( rc != 0 )
                PLOGERR ( klogWarn,  (klogWarn, rc, "failed to select child $(name) of '$(path)'", "name=%s,path=%.*s", name, plen, path ));
            else
            {
                const KMDataNode *child;
                rc = KMDataNodeOpenNodeRead ( node, & child, "%s", name );
                if ( rc != 0 )
                {
                    KNamelistRelease ( children );
                    PLOGERR ( klogErr,  (klogErr, rc, "failed to open child '$(path)'", "path=%s", path ));
                    return rc;
                }

                /* recurse on child */
                rc = md_select_expr ( child, path, psize, plen + childlen, attr, false );
                close_indent = true;

                KMDataNodeRelease ( child );

                if ( rc != 0 )
                {
                    KNamelistRelease ( children );
                    return rc;
                }
            }
        }
    }

    if ( ! wildcard )
        children_end ();

    KNamelistRelease ( children );

    path [ plen ] = 0;

    if ( ! wildcard )
        node_close ( path, plen, vsize, num_children, close_indent );

    return 0;
}

#if ALLOW_UPDATE

static
rc_t md_update_expr ( KMDataNode *node, const char *attr, const char *expr )
{
    rc_t rc;

    /* according to documentation, "expr" is allowed to be text
       or text with escaped hex sequences. examine for escaped hex */
    size_t len = string_size ( expr );
    char *buff = malloc ( len + 1 );
    if ( buff == NULL )
        rc = RC ( rcExe, rcMetadata, rcUpdating, rcMemory, rcExhausted );
    else
    {
        /* will get set to true if value contains an embedded '\0' */
        bool z = false;
        size_t i, j;
        for ( i = j = 0; i < len; ++ i, ++ j )
        {
            if ( ( buff [ j ] = expr [ i ] ) == '\\' )
            {
                /* we know "expr" is NUL-terminated, so this is safe */
                if ( tolower ( expr [ i + 1 ] ) == 'x' &&
                     isxdigit ( expr [ i + 2 ] ) &&
                     isxdigit ( expr [ i + 3 ] ) )
                {
                    int msn = toupper ( expr [ i + 2 ] ) - '0';
                    int lsn = toupper ( expr [ i + 3 ] ) - '0';
                    if ( msn >= 10 )
                        msn += '0' - 'A' + 10;
                    if ( lsn >= 10 )
                        lsn += '0' - 'A' + 10;
                    i += 3;
                }
            }
            z = z || (buff[j] == '\0');
            buff [ j + 1 ] = '\0';
        }
        if ( attr != NULL )
        {
            if (z)
                LOGERR ( klogErr, rc = RC(rcExe, rcFile, rcUpdating, rcConstraint, rcViolated), "node attribute values can not contain embedded nulls" );
            else 
                rc = KMDataNodeWriteAttr ( node, attr, buff );
        }
        else
        {
            rc = KMDataNodeWrite ( node, buff, j );
        }
        free ( buff );
    }

    return rc;
}
#endif

struct Data {
    void *buffer;
    size_t size;
};

static struct Data readFromKFile(KFile const *f, char const *path, rc_t *prc)
{
    struct Data result = { NULL, 0 };
    uint64_t size = 0;
    size_t numread = 0;
    rc_t rc = KFileSize(f, &size); assert(rc == 0);
    
    result.size = size;
    *prc = rc; if (rc) return result;
    result.buffer = malloc(result.size);
    if (result.buffer == NULL) {
        PLOGERR (klogErr, ( klogErr, *prc = RC(rcExe, rcFile, rcReading, rcMemory, rcExhausted), "failed to allocate memory to read '$(path)'", "path=%s", path ));
        return result;
    }
    *prc = rc = KFileReadAll(f, 0, result.buffer, result.size, &numread);
    if (rc == 0 && numread != size)
        *prc = rc = RC(rcExe, rcFile, rcReading, rcSize, rcTooShort);
    if (rc) {
        PLOGERR (klogErr, ( klogErr, rc, "failed to read '$(path)'", "path=%s", path ));
        free(result.buffer);
        result.buffer = NULL;
    }
    return result;
}

static struct Data readFromStdIn(rc_t *prc) {
    struct Data result = { NULL, 0 };
    size_t cap = 0;
    int ch = EOF;

    while ((ch = fgetc(stdin)) != EOF) {
        if (result.size == cap) {
            void *tmp = realloc(result.buffer, cap = (cap < 4096 ? 4096 : cap * 2));
            if (tmp == NULL) {
                free(result.buffer);
                result.buffer = NULL;
                LOGERR (klogErr, *prc = RC(rcExe, rcFile, rcReading, rcMemory, rcExhausted), "failed to allocate memory to read stdin");
                return result;
            }
            result.buffer = tmp;
        }
        ((char *)result.buffer)[result.size++] = ch;
    }
    if (result.size == 0)
        result.buffer = malloc(1);
    return result;
}

static struct Data readFromFile(char const *path, rc_t *prc)
{
    struct Data result = {NULL, 0};
    if (strcmp(path, "/dev/stdin") == 0)
        result = readFromStdIn(prc);
    else {
        KDirectory *dir = NULL;
        KFile const *f = NULL;
        
        *prc = KDirectoryNativeDir(&dir); assert(*prc == 0);
        if (*prc == 0) {
            *prc = KDirectoryOpenFileRead(dir, &f, "%s", path);
            KDirectoryRelease(dir);
            if (*prc == 0)
                result = readFromKFile(f, path, prc);
            KFileRelease(f);
        }
    }
    return result;
}

static
bool CC md_select ( void *item, void *data )
{
    bool fail = true;
    KDBMetaParms *pb = data;

    CONST KMDataNode *node;
#if ALLOW_UPDATE
    bool read_only = true;
    if ( ! read_only_arg ) {
        pb -> rc = KMetadataOpenNodeUpdate ( pb -> md, & node, NULL );
        if ( pb -> rc == 0 )
            read_only = false;
    }
#endif
    if ( read_only )
        pb -> rc = KMetadataOpenNodeRead ( pb -> md, ( const KMDataNode** ) & node, NULL );
    if ( pb -> rc != 0 )
        PLOGERR ( klogErr,  (klogErr, pb -> rc, "failed to open root node for '$(path)'", "path=%s", pb -> targ ));
    else
    {
        bool wildcard = false, updating = false;
        char *expr = NULL, *attr = NULL, path [ 4096 ];
        struct Data data = {NULL, 0};
        size_t len = string_copy_measure ( path, sizeof path, item );

        /* detect assignment */
        expr = string_rchr ( path, len, '=' );
        if ( expr != NULL )
        {
            len = expr - path;
            * expr ++ = 0;
            updating = true;
        }
        else {
            /* detect read file */
            char *file = string_rchr(path, len, ':');
            if (file != NULL) {
                len = file - path;
                *file++ = '\0';
                data = readFromFile(file, &pb->rc);
                if (data.buffer != NULL)
                    updating = true;
            }
        }
        attr = string_rchr ( path, len, '@' );
        if ( attr != NULL )
        {
            len = attr - path;
            * attr ++ = 0;
        }

        if ( updating )
        {
#if ALLOW_UPDATE
            if ( read_only )
            {
                PLOGMSG ( klogWarn, ( klogWarn, "node update expressions are not supported - "
                                      "'$(path)' is read-only - "
                                      "'$(expr)' treated as select."
                                      , "path=%s,expr=%s", pb -> targ, item ) );
                expr = NULL;
            }
#else
            PLOGMSG ( klogWarn, ( klogWarn, "node update expressions are not supported - "
                                  "'$(expr)' treated as select.", "expr=%s", item ) );
            expr = NULL;
            free(data.buffer);
            data.buffer = NULL;
#endif
        }

        if ( len >= 1 && path [ len - 1 ] == '*' )
        {
            if ( len == 1 )
            {
                path [ len = 0 ] = 0;
                wildcard = true;
            }
            else if ( len >= 2 && path [ len - 2 ] == '/' )
            {
                path [ len -= 2 ] = 0;
                wildcard = true;
            }
        }

#if ALLOW_UPDATE
        if ( updating )
        {
            KMDataNode *root = node;

            if ( wildcard || len == 0 )
            {
                pb -> rc = RC ( rcExe, rcMetadata, rcUpdating, rcExpression, rcIncorrect );
                PLOGERR ( klogErr, ( klogErr, pb -> rc, "node updates require explicit paths - "
                                     "'$(item)' cannot be evaluated", "item=%s", item ) );
                return true;
            }

            pb -> rc = KMDataNodeOpenNodeUpdate ( root, & node, "%s", path );
            KMDataNodeRelease ( root );

            if ( pb -> rc != 0 )
            {
                PLOGERR ( klogErr,  (klogErr, pb -> rc,
                    "failed to open node '$(node)' for '$(path)'",
                                     "node=%s,path=%s", path, pb -> targ ));
            }
            else if (expr != NULL)
            {
                pb -> rc = md_update_expr ( node, attr, expr );
            }
            else if (attr == NULL) {
                assert(data.buffer != NULL);
                pb -> rc = KMDataNodeWrite ( node, data.buffer, data.size );
            }
        }
        else
#endif

        if ( len == 0 )
            pb -> rc = md_select_expr ( node, path, sizeof path, len, attr, wildcard );
        else
        {
            const KMDataNode *root = node;
            pb -> rc = KMDataNodeOpenNodeRead ( root, ( const KMDataNode** ) & node, "%s", path );
            KMDataNodeRelease ( root );

            if ( pb -> rc != 0 )
            {
                PLOGERR ( klogErr,  (klogErr, pb -> rc,
                    "failed to open node '$(node)' for '$(path)'",
                                     "node=%s,path=%s", path, pb -> targ ));
            }
            else
            {
                pb -> rc = md_select_expr ( node, path, sizeof path, len, attr, wildcard );
            }
        }

        if ( pb -> rc == 0 )
            fail = false;

        KMDataNodeRelease ( node );
        free(data.buffer);
    }

    return fail;
}

static
rc_t col_select ( KDBMetaParms * pb)
{
    CONST KColumn *col;
    rc_t rc;

    bool read_only = true;

#if ALLOW_UPDATE
    if ( ! read_only_arg ) {
        read_only = false;

        rc = KDBManagerOpenColumnUpdate ( pb -> mgr, & col, "%s", pb->targ );
        if ( rc != 0 )
            read_only = true;
    }
#endif
    if ( read_only )
        rc = KDBManagerOpenColumnRead ( pb -> mgr, ( const KColumn** ) & col, "%s", pb->targ );
    if ( rc != 0 )
        PLOGERR ( klogErr,  (klogErr, rc, "failed to open column '$(col)'", "col=%s", pb->targ ));
    else
    {
#if ALLOW_UPDATE
        if ( ! read_only_arg ) {
            read_only = false;
            rc = KColumnOpenMetadataUpdate ( col, & pb -> md );
            if ( rc != 0 )
                read_only = true;
        }
#endif
        if ( read_only )
            rc = KColumnOpenMetadataRead ( col, ( const KMetadata** ) & pb -> md );
        if ( rc != 0 )
            PLOGERR ( klogErr,  (klogErr, rc, "failed to open metadata for column '$(col)'", "col=%s", pb->targ ));
        else
        {
            bool fail;

            fail = VectorDoUntil ( pb -> q, false, md_select, pb );

            if (fail)
                rc = pb->rc;

            KMetadataRelease ( pb -> md ), pb -> md = NULL;
        }

        KColumnRelease ( col );
    }
    return rc;
}

static
rc_t tbl_select ( KDBMetaParms * pb)
{
    CONST KTable *tbl;
    rc_t rc;

    bool read_only = true;

#if ALLOW_UPDATE
    if ( ! read_only_arg ) {
        read_only = false;

        rc = KDBManagerOpenTableUpdate ( pb -> mgr, & tbl, "%s", pb->targ );
        if ( rc != 0 )
            read_only = true;
    }
#endif
    if ( read_only ) {
            rc = KDBManagerOpenTableReadVPath ( pb -> mgr,
                ( const KTable** ) & tbl, pb->local);
    }
    if ( rc != 0 )
        PLOGERR ( klogErr,  (klogErr, rc, "failed to open table '$(tbl)'", "tbl=%s", pb->targ ));
    else
    {
        read_only = true;
#if ALLOW_UPDATE
        if ( ! read_only_arg ) {
            read_only = false;
            rc = KTableOpenMetadataUpdate ( tbl, & pb -> md );
            if ( rc != 0 )
                read_only = true;
        }
#endif
        if ( read_only )
            rc = KTableOpenMetadataRead ( tbl, ( const KMetadata** ) & pb -> md );
        if ( rc != 0 )
            PLOGERR ( klogErr,  (klogErr, rc, "failed to open metadata for table '$(tbl)'", "tbl=%s", pb->targ ));
        else
        {
            bool fail;

            fail = VectorDoUntil ( pb -> q, false, md_select, pb );
            if (fail)
                rc = pb->rc;

            KMetadataRelease ( pb -> md ), pb -> md = NULL;
        }

        KTableRelease ( tbl );
    }

    return rc;
}

static
rc_t db_select (KDBMetaParms * pb)
{
    CONST KDatabase *db;
    rc_t rc;

    bool read_only = true;

#if ALLOW_UPDATE
    if ( ! read_only_arg ) {
        read_only = false;

        rc = KDBManagerOpenDBUpdate ( pb -> mgr, & db, "%s", pb->targ );
        if ( rc != 0 )
            read_only = true;
    }
#endif
    if ( read_only ) {
        if (VPathIsRemote(pb->local))
            rc = KDBManagerVPathOpenRemoteDBRead ( pb -> mgr,
                ( const KDatabase** ) & db, pb->local, pb->cache );
        else
            rc = KDBManagerOpenDBRead ( pb -> mgr,
                ( const KDatabase** ) & db, "%s", pb->targ );
    }
    if ( rc != 0 ) {
        PLOGERR ( klogErr,  (klogErr, rc, "failed to open db '$(db)'",
            "db=%s", pb->targ ));
    }
    else {
        CONST KTable* tbl = NULL;
        if (table_arg) {
            read_only = true;
#if ALLOW_UPDATE
            if ( ! read_only_arg ) {
                read_only = false;
                rc = KDatabaseOpenTableUpdate ( db, &tbl, "%s", table_arg );
                if ( rc != 0 )
                    read_only = true;
            }
#endif
            if ( read_only )
                rc = KDatabaseOpenTableRead ( db, ( const KTable** ) &tbl, "%s", table_arg );
            if ( rc != 0 ) {
                PLOGERR ( klogErr,  (klogErr, rc,
                    "failed to open table '$(table)'", "table=%s", table_arg ));
            }
        }
        if ( rc == 0) {
            if (tbl) {
                read_only = true;
#if ALLOW_UPDATE
                if ( ! read_only_arg ) {
                    read_only = false;
                    rc = KTableOpenMetadataUpdate ( tbl, & pb -> md );
                    if ( rc != 0 )
                        read_only = true;
                }
#endif
                if ( read_only )
                    rc = KTableOpenMetadataRead ( tbl, ( const KMetadata** ) & pb -> md );
                if ( rc != 0 ) {
                    PLOGERR ( klogErr,  (klogErr, rc,
                        "failed to open metadata for table '$(table)'",
                        "table=%s", table_arg ));
                }
            }
            else {
                read_only = true;
#if ALLOW_UPDATE
                if ( ! read_only_arg ) {
                    read_only = false;
                    rc = KDatabaseOpenMetadataUpdate ( db, & pb -> md );
                    if ( rc != 0 )
                        read_only = true;
                }
#endif
                if ( read_only )
                    rc = KDatabaseOpenMetadataRead ( db, ( const KMetadata** ) & pb -> md );
                if ( rc != 0 ) {
                    PLOGERR ( klogErr,  (klogErr, rc,
                        "failed to open metadata for db '$(db)'",
                        "db=%s", pb->targ ));
                }
            }
            if ( rc == 0 ) {
                bool fail;

                fail = VectorDoUntil ( pb -> q, false, md_select, pb );
                if(fail)
                    rc = pb->rc;
                KMetadataRelease ( pb -> md ), pb -> md = NULL;
            }
        }
        KTableRelease ( tbl );
        KDatabaseRelease ( db );
    }

    return rc;
}

static
rc_t tool_select ( CONST KDBManager * mgr, uint32_t type,
    const VPath * local, const VPath * remote, const VPath * cache,
    const char * targ, const Vector * q)
{
    KDBMetaParms pb;
    rc_t rc;

    pb.mgr = mgr;
    pb.md = NULL;
    pb.local = local;
    pb.remote = remote;
    pb.cache = cache;
    pb.targ = targ;
    pb.q = q;
    pb.rc = 0;

    switch (type)
    {
    default:
        rc = RC (rcExe, rcNoTarg, rcAccessing, rcParam, rcInvalid);
        break;

    case kptDatabase:
        rc = db_select (&pb);
        break;

    case kptPrereleaseTbl:
    case kptTable:
        rc = tbl_select (&pb);
        break;

    case kptColumn:
        rc = col_select (&pb);
        break;
    }
    return rc;
}

const char UsageDefaultName[] = "kdbmeta";


rc_t CC UsageSummary (const char * progname)
{
    return KOutMsg ("\n"
                    "Usage:\n"
                    "  %s [Options] <target> [<query> ...]\n"
                    "\n"
                    "Summary:\n"
                    "  Display the contents of one or more metadata stores.\n"
#if ALLOW_UPDATE
                    "  Update metadata.\n"
#endif
                  , progname);
}

static const char *const t1 [] = { "path-to-database", "access database metadata", NULL };
static const char *const t2 [] = { "path-to-table",    "access table metadata", NULL };
static const char *const t3 [] = { "path-to-column",   "access column metadata", NULL };
static const char *const t4 [] = { "accession",        "sra global access id", NULL };

static const char *const q1 [] = { "*","all nodes and attributes", NULL };
static const char *const q2 [] = { "NAME","a named root node and children", NULL };
static const char *const q3 [] = { "PATH/NAME","an internal node and children", NULL };
static const char *const q4 [] = { "<node>@ATTR","a named attribute", NULL };
#if ALLOW_UPDATE
static const char *const q5 [] = { "<obj>=VALUE","a simple value assignment where",
                                   "value string is text, and binary",
                                   "values use hex escape codes", NULL };
#endif

#define ALIAS_READ_ONLY             "r"
#define OPTION_READ_ONLY            "read-only"
static const char* USAGE_READ_ONLY[] = { "operate in read-only mode", NULL };

#define ALIAS_TABLE             "T"
#define OPTION_TABLE            "table"
static const char* USAGE_TABLE[] = { "table-name", NULL };

#define ALIAS_UNSIGNED             "u"
#define OPTION_UNSIGNED            "unsigned"
static const char* USAGE_UNSIGNED[]
                                 = { "print numeric values as unsigned", NULL };

#define OPTION_OUT "output"
#define ALIAS_OUT  "X"
static const char* USAGE_OUT[] = { "Output type: one of (xml text): ",
    "whether to generate well-formed XML. Default: xml (well-formed)", NULL };

#define OPTION_NGC "ngc"
#define ALIAS_NGC  NULL
static const char* USAGE_NGC[] = { "path to ngc file", NULL };


const OptDef opt[] = {
  { OPTION_TABLE    , ALIAS_TABLE    , NULL, USAGE_TABLE    , 1, true , false }
 ,{ OPTION_UNSIGNED , ALIAS_UNSIGNED , NULL, USAGE_UNSIGNED , 1, false, false }
#if ALLOW_UPDATE
 ,{ OPTION_READ_ONLY, ALIAS_READ_ONLY, NULL, USAGE_READ_ONLY, 1, false, false }
#endif
 ,{ OPTION_OUT      , ALIAS_OUT      , NULL, USAGE_OUT      , 1, true , false }
 ,{ OPTION_NGC      , ALIAS_NGC      , NULL, USAGE_NGC      , 1, true , false }
};

static const char * const * target_usage [] = { t1, t2, t3, t4 };
static const char * const * query_usage []  =
{
    q1, q2, q3, q4
#if ALLOW_UPDATE
    , q5
#endif
};


rc_t CC Usage (const Args * args)
{
    const char * progname = UsageDefaultName;
    const char * fullpath = UsageDefaultName;
    unsigned idx;
    rc_t rc;

    if (args == NULL)
        rc = RC (rcApp, rcArgv, rcAccessing, rcSelf, rcNull);
    else
        rc = ArgsProgram (args, &fullpath, &progname);

    UsageSummary (progname);

    KOutMsg ("  The target metadata are described by one or more\n"
             "  target specifications, giving the path to a database, a table\n"
             "  or a column. the command and query are executed on each target.\n"
             "\n"
             "  queries name one or more objects, and '*' acts as a wildcard.\n"
             "  query objects are nodes or attributes. nodes are named with a\n"
             "  hierarchical path, like a file-system path. attributes are given\n"
             "  as a node path followed by a '@' followed by the attribute name.\n"
             "\n"
             "target:\n");

    for (idx = 0; idx < sizeof (target_usage) / sizeof target_usage[0]; ++idx)
        HelpParamLine ( (target_usage[idx])[0], (target_usage[idx])+1 );

    OUTMSG (("\n"
             "query:\n"));

    for (idx = 0; idx < sizeof (query_usage) / sizeof query_usage[0]; ++idx)
        HelpParamLine ( (query_usage[idx])[0], (query_usage[idx])+1 );

    OUTMSG (("\n"
             "Options:\n"));

    for(idx = 0; idx < sizeof(opt) / sizeof(opt[0]); ++idx) {
        const char *param = NULL;
        if (opt[idx].aliases == NULL) {
            if (strcmp(opt[idx].name, OPTION_NGC) == 0)
                param = "path";
        }
        else if (strcmp(opt[idx].aliases, ALIAS_TABLE) == 0) {
            param = "table";
        }
        else if (strcmp(opt[idx].aliases, ALIAS_OUT) == 0) {
            param = "value";
        }
        HelpOptionLine(opt[idx].aliases, opt[idx].name, param, opt[idx].help);
    }

    OUTMSG(("\n"));

    HelpOptionsStandard ();

    HelpVersion (fullpath, KAppVersion());

    return rc;
}

MAIN_DECL( argc, argv )
{
    VDB_INITIALIZE(argc, argv, VDB_INIT_FAILED);

    Args * args = NULL;
    rc_t rc;

    SetUsage( Usage );
    SetUsageSummary( UsageSummary );

    rc = ArgsMakeAndHandle(&args, argc, argv, 1, opt, sizeof(opt) / sizeof(opt[0]));
    if (rc == 0)
    {
        do
        {
            const char * pc;
            KDirectory *curwd;
            uint32_t pcount;
            int ix;

#if ALLOW_UPDATE
            read_only_arg = false;
            rc = ArgsOptionCount (args, OPTION_READ_ONLY, &pcount);
            if (rc) {
                LOGERR(klogErr, rc,
                    "Failure to get '" OPTION_READ_ONLY "' argument");
                break;
            }
            read_only_arg = pcount > 0;
#endif

            rc = ArgsOptionCount (args, OPTION_UNSIGNED, &pcount);
            if (rc) {
                LOGERR(klogErr, rc,
                    "Failure to get '" OPTION_UNSIGNED "' argument");
                break;
            }
            as_unsigned = pcount > 0;

            rc = ArgsOptionCount (args, OPTION_TABLE, &pcount);
            if (rc) {
                LOGERR(klogErr, rc,
                    "Failure to get '" OPTION_TABLE "' argument");
                break;
            }
            if (pcount) {
                rc = ArgsOptionValue (args, OPTION_TABLE, 0, (const void **)&table_arg);
                if (rc) {
                    LOGERR(klogErr, rc,
                        "Failure to get '" OPTION_TABLE "' argument");
                    break;
                }
            }

            rc = ArgsOptionCount (args, OPTION_OUT, &pcount);
            if (rc) {
                LOGERR(klogErr, rc, "Failure to get '" OPTION_OUT "' argument");
                break;
            }
            if (pcount) {
                const char* dummy = NULL;
                rc = ArgsOptionValue (args, OPTION_OUT, 0, (const void **)&dummy);
                if (rc) {
                    LOGERR(klogErr, rc,
                        "Failure to get '" OPTION_OUT "' argument");
                    break;
                }
                else if (strcmp(dummy, "t") == 0) {
                    as_valid_xml = false;
                }
            }

/* OPTION_NGC */
            {
                rc = ArgsOptionCount(args, OPTION_NGC, &pcount);
                if (rc != 0) {
                    LOGERR(klogErr, rc,
                        "Failure to get '" OPTION_NGC "' argument");
                    break;
                }
                if (pcount > 0) {
                    const char* dummy = NULL;
                    rc = ArgsOptionValue(args, OPTION_NGC, 0,
                        (const void **)&dummy);
                    if (rc != 0) {
                        LOGERR(klogErr, rc,
                            "Failure to get '" OPTION_NGC "' argument");
                        break;
                    }

                    KConfigSetNgcFile(dummy);
                }
            }

            rc = ArgsParamCount (args, &pcount);
            if (rc)
                break;

            if (pcount == 0)
            {
                OUTMSG (( "missing database target path and queries\n" ));
                MiniUsage (args);
                ArgsWhack(args);
                exit(EXIT_FAILURE);
            }

            rc = KDirectoryNativeDir (&curwd);
            if (rc)
            {
                LOGERR (klogInt, rc, "Unable to open the file system");
            }
            else
            {
                CONST KDBManager * mgr;
#if ALLOW_UPDATE
                rc = KDBManagerMakeUpdate (&mgr, curwd);
#else
                rc = KDBManagerMakeRead (&mgr, curwd);
#endif
                if (rc)
                    LOGERR (klogInt, rc, "Unable to open the database system");
                else
                {
                    /* get target */
                    rc = ArgsParamValue (args, 0, (const void **)&pc);
                    if (rc)
                        LOGERR (klogInt, rc, "Unable to read target parameter");
                    else
                    {
                        const VFSManager * vfs = NULL;
                        const VPath * cache = NULL;
                        const VPath * resolved = NULL;
                        char objpath [ 4096 ];
                        bool found = false;
                        uint32_t type;

                        rc = KDBManagerGetVFSManager ( mgr,
                            ( VFSManager ** )&vfs );
                        if ( rc == 0 ) {
                            if ( rc == 0 ) {
                                    rc = VFSManagerResolveWithCache(vfs,
                                        pc, &resolved, &cache);
                                    if (rc == 0 && !VPathIsRemote(resolved))
                                    {
                                        rc = VPathReadPath(resolved,
                                            objpath, sizeof objpath, NULL);
                                        if (rc == 0)
                                            found = true;
                                    }
                                    else if ( GetRCState ( rc ) == rcNotFound )
                                        rc = 0;
                            }
                        }

                        if ( ! found)
                        {
                            rc = KDirectoryResolvePath (curwd, true, objpath,
                                                        sizeof objpath, "%s", pc);

                            if (rc)
                                LOGERR (klogFatal, rc,
                                    "Unable to resolved target path");
                        }

                        if (found)
                            /* resolved locally */
                            type = KDBManagerPathTypeVP(mgr, resolved);
                        else
                            /* check as local path */
                            type = KDBManagerPathType (mgr, "%s", objpath);

                        switch (type)
                        {
                        case kptDatabase:
                        case kptPrereleaseTbl:
                        case kptTable:
                        case kptColumn:
                            break;

                        case kptBadPath:
                            rc = RC ( rcDB, rcMgr, rcAccessing, rcPath, rcInvalid );
                            PLOGERR ( klogErr, (klogErr, rc, "'$(path)' -- bad path", "path=%s", pc ));
                            break;
                        default:
                            rc = RC ( rcDB, rcMgr, rcAccessing, rcPath, rcIncorrect );
                            PLOGERR ( klogErr, (klogErr, rc, "'$(path)' -- type unknown", "path=%s", pc ));
                            break;

                        case kptNotFound:
                          {
                            if (resolved != NULL) {
                                if (rc == 0) {
                                    /* resolved remotely */
                                    type = KDBManagerPathTypeVP(mgr, resolved);
                                    rc = VPathReadUri(resolved,
                                        objpath, sizeof objpath, NULL);
                                    if (rc == 0)
                                        found = true;
                                }
                            }
                            if (!found) {
                                rc = RC ( rcDB,
                                    rcMgr, rcAccessing, rcPath, rcNotFound );
                                PLOGERR ( klogErr, ( klogErr, rc,
                                    "'$(path)' not found", "path=%s", pc ));
                            }
                            break;
                          }
                        }
                        if (rc == 0)
                        {
                            Vector q;

                            VectorInit (&q, 0, 8);

                            if (pcount == 1)
                            {
                                const char *default_query = "*";
                                rc = VectorAppend (&q, NULL, default_query);
                            }
                            else for (ix = 1; ix < pcount; ++ix)
                            {
                                rc = ArgsParamValue (args, ix, (const void **)&pc);
                                if (rc)
                                    break;

                                rc = ArgsParamValue (args, ix, (const void **)&pc);
                                if (rc)
                                    break;

                                rc = VectorAppend ( &q, NULL, pc );
                                if (rc)
                                    break;
                            }

                            if (rc)
                                LOGERR (klogErr, rc, "Unable to queue queries");
                            else
                            {
                                rc = tool_select (mgr, type,
                                    resolved, 0, cache, objpath, &q);

                                VectorWhack (&q, NULL, NULL);
                            }
                        }
                        VPathRelease(resolved);
                        VPathRelease(cache);
                        VFSManagerRelease(vfs);
                    }
                    KDBManagerRelease (mgr);
                }
                KDirectoryRelease (curwd);
            }
        } while (0);
    }

    ArgsWhack(args);
    args = NULL;

    return VDB_TERMINATE( rc );
}
