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

#include <kfs/extern.h>

#include <kxml/xml.h>

#include <klib/defs.h>
#include <klib/container.h>
#include <klib/text.h>
#include <klib/rc.h>
#include <klib/refcount.h>
#include <klib/vector.h>
#include <klib/log.h>
#include <klib/out.h>
#include <klib/namelist.h>

#include <kfs/file.h>
#include <kfs/bzip.h>
#include <kfs/gzip.h>
#include <kfs/directory.h>

#include <vfs/manager.h>
#include <vfs/path.h>

#include <sysalloc.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <assert.h>

#include <klib/debug.h>
#define DBGTOC(m)   DBGMSG (DBG_KFS, DBG_FLAG(DBG_KFS_TOC), m)
#define DBGENTRY(m) DBGMSG (DBG_KFS, DBG_FLAG(DBG_KFS_TOC), m)
#define DBGCACHE(m) DBGMSG (DBG_KFS, DBG_FLAG(DBG_KFS_TOC), m)

#include "xtoc-priv.h"

struct KSysFile;

typedef struct XToc XToc;
static rc_t XTocRelease (const XToc * self);
static rc_t XTocAddRef (const XToc * self);

typedef struct XTocListing XTocListing;
typedef struct XTocEntry XTocEntry;
typedef struct KXTocDir KXTocDir;
typedef struct KXTocFile KXTocFile;
typedef struct XTocCache XTocCache;


/* ======================================================================
 * XTocEntry_t
 *   Each node in the XToc tree needs to be one of a limited number of types
 *   these relate to the KPathType for KDirectory but are more limited in
 *   number.  However the file can explicity be treated as a directory if
 *   flagged as such.
 */

typedef uint32_t XTocEntry_t;
typedef enum XTocEntry_e
{
    xtoce_undefined,
    xtoce_root,
    xtoce_id,
    xtoce_file,
    xtoce_dir,
    xtoce_link
} XTocEntry_e;

#define KDIR_IMPL struct KXTocDir
#define KFILE_IMPL struct KXTocFile
#include <kfs/impl.h>


#define KNAMELIST_IMPL struct XTocListing
#include <klib/impl.h>


/* ======================================================================
 * XTocEntry
 *
 * TBD:  More efficient use of memory might be obtained by converting
 *       the union into space allocated after the main entry
 */
struct XTocEntry
{
    BSTNode     node;           /* book keeping for the tree structure */
    String	name;           /* leaf name */
    XTocEntry * parent;         /* directory containing this entry */
    XTocEntry * container;      /* compressed file or archive holding this */
    XTocEntry * root;           /* base entry of the TOC that is '/' */
    XTocCache * cache;          /* if opened this will be non-NULL */
    KTime_t     mtime;          /* object modified time/date from the XML */
    BSTree      tree;           /* objects below this (inverse of parent */
    XTocEntry_t type;           /* XTocEntry_e */
    union utype
    {
        struct /* idtype - name conflict on Solaris */
        {
            XTocEntry * target;
        } id;

        struct dirtype
        {
            int ignored;        /* currently nothing is here */
        } dir;

        struct filetype
        {
            String   filetype;  /* string from copycat XML describing file */
            uint64_t size;      /* size of file within archive or decompressed */
            uint64_t offset;    /* offset with in containing file */
            uint8_t  md5 [16];  /* md5 sum digest */
            bool has_tree;      /* tree is meaningfull if not false: see next two */
            bool archive;       /* set if this is an archive, has_tree also set */
            bool container;     /* set if this is acompressed file, has_tree also set */
        } file;

        struct linktype
        {
            String target;      /* path of the symbolic link */
        } link;
    } u;
};

#if _DEBUGGING
#define DBG_XTocEntry(p)                                                \
    {                                                                   \
        DBGENTRY(("XTocEntry: %p\n", p));                               \
        if (p)                                                          \
        {                                                               \
            DBGENTRY(("    name:      %S\n", &(p)->name));              \
            DBGENTRY(("    parent:    %p\n", (p)->parent));             \
            DBGENTRY(("    container: %p\n", (p)->container));          \
            DBGENTRY(("    root:      %p\n", (p)->root));               \
            DBGENTRY(("    cache:     %p\n", (p)->cache));              \
            DBGENTRY(("    tree:      %p\n", (p)->tree));               \
            DBGENTRY(("    type:      %u\n", (p)->type));               \
            if ((p)->type == xtoce_id)                                  \
                DBGENTRY(("    target:      %p\n", &(p)->u.id.target)); \
            else if ((p)->type == xtoce_link)                          \
                DBGENTRY(("    target:      %S\n", &(p)->u.link.target)); \
            else if((p)->type == xtoce_file)                            \
            {                                                           \
                DBGENTRY(("    filetype:     %S\n", &(p)->u.file.filetype)); \
                DBGENTRY(("    size:         %lu\n", (p)->u.file.size)); \
                DBGENTRY(("    offset:       %lu\n", (p)->u.file.offset)); \
                DBGENTRY(("    base_tree:    %u\n", (p)->u.file.has_tree)); \
                DBGENTRY(("    archive:      %u\n", (p)->u.file.archive)); \
                DBGENTRY(("    container:    %u\n", (p)->u.file.container));  }}}
#else
#define DBG_XTocEntry(p)
#endif

/* function for BSTreeInsert or BSTreeInsertUnique */
static
int64_t CC XTocEntrySort (const BSTNode * item, const BSTNode * n)
{
    return StringCompare (&((const XTocEntry*)item)->name,
                          &((const XTocEntry*)n)->name);
}

/* function for BSTreeFind */
static
int64_t CC XTocEntryCmpString (const void * item, const BSTNode * n)
{
    return (int)StringCompare ((const String *)item, 
                               &((const XTocEntry*)n)->name);
}

/* forward as we have a loop in calls */
static
void XTocTreeWhack (BSTNode * item, void * data);

/*
 * XTocEntryDestroy
 *  object destructor
 *  It recursively destroys entries in its tree
 */
static
void XTocEntryDestroy (XTocEntry * self)
{
    if (self)
    {
        switch (self->type)
        {
        case xtoce_root:
        case xtoce_dir:
        case xtoce_file:
            /* delete an entries below this one */
            BSTreeWhack (&self->tree, XTocTreeWhack, NULL);
            break;
        case xtoce_link:
        case xtoce_id:
        default:
            break;
        }
        free (self);
    }
}

/* recursive destructor call for BSTreeWhack */
static
void XTocTreeWhack (BSTNode * item, void * data)
{
    XTocEntryDestroy ((XTocEntry*)item);
}


rc_t XTocEntryResolvePath (const XTocEntry * self, const char * path_, bool follow_link,
                           XTocEntry ** pnode)
{
    rc_t rc = 0;
    const char * path;
    const char * limit;
    XTocEntry * node;
    size_t size;

    assert (self);
    assert (path_);
    assert (pnode);
#if 0
    DBGENTRY (("%s: resolve %s from %p\n", __func__, path_, self));
#endif
    node = (XTocEntry *)self;
#if 0
    DBGENTRY(("%s: node starts at %s\n", __func__, node ? node->name.addr : "NULL"));
#endif
    path = path_;
#if 0
    /* this was causing a seg fault for some reason */
    limit = path + string_size (path);
#else
    limit = path + string_measure(path, &size);
#endif

    do
    {
        String s;
        char * slash;
        size_t z;
        BSTree * t;
#if 0
        DBGENTRY (("%s: path %s size %lu %p - %p = %lu\n", __func__, path, string_size(path), limit, path, limit-path));
#endif
        while (*path == '/')
                ++path;

        z = limit - path;
        if (z == 0)
            break;
#if 0
        DBGENTRY (("%s: path %s z %lu %p - %p = %lu\n", __func__, path, z, limit, path, limit-path));
        DBG_XTocEntry(node);
#endif
        switch (node->type)
        {
        case xtoce_link:
        {
            XTocEntry * nnode;
            rc = XTocEntryResolvePath (node, node->u.link.target.addr,
                                       follow_link, &nnode);
#if 0
            DBGENTRY(("%s: node becomes %s\n", __func__, node ? node->name.addr : "NULL"));
#endif
            if (rc)
                return rc;

            node = nnode;
            continue;
        }
        case xtoce_id:
            /* this needed to be a leaf */
            rc = RC (rcFS, rcDirectory, rcResolving, rcPath, rcInvalid);
            LOGERR (klogErr, rc, "bad path resolving path by id in XML FS");
            return rc;
        default:
            return RC (rcFS, rcDirectory, rcResolving, rcPath, rcIncorrect);

        case xtoce_file:
            if ( ! node->u.file.has_tree)
            {
                /* this needed to be a leaf */
                rc = RC (rcFS, rcDirectory, rcResolving, rcPath, rcInvalid);
                LOGERR (klogErr, rc, "bad path resolving path by name in XML FS");
                return rc;
            }
            /* fall through */
        case xtoce_root:
        case xtoce_dir:
            t = &node->tree;
            break;
        }
        slash = string_chr (path, z, '/');
        if (slash)
            z = slash - path;

        StringInit (&s, path, z, string_len (path, z));
#if 0
        DBGENTRY (("%s: as String %S\n", __func__, &s));
#endif
        node = (XTocEntry*)BSTreeFind (t, &s, XTocEntryCmpString);
#if 0
        DBGENTRY (("%s: node is now %p\n", __func__, node));
#endif
        if (node == NULL)
            return RC (rcFS, rcDirectory, rcResolving, rcPath, rcNotFound);

        path += StringSize (&s);
#if 0
        DBGENTRY (("%s: looping at %s %p %p\n", __func__, path, path, limit-1));
#endif
    } while (path < limit-1);

    if (rc == 0)
    {
        if(path[0] == '\0')
        {
            switch (node->type)
            {
            case xtoce_id:
                node = node->u.id.target;
                if (node == NULL)
                    rc = RC (rcFS, rcDirectory, rcResolving, rcLink, rcCorrupt);
                break;
            case xtoce_link:
                if (follow_link)
                    return XTocEntryResolvePath (node, path, follow_link, pnode);
            default:
                break;
            }
        }
        *pnode = node;
    }
#if 1
    DBGENTRY(("%s: %R\n",__func__, rc));
#endif
    return rc;
}


KPathType XTocEntryPathType (const XTocEntry * self, const char * path)
{
    rc_t rc;
    KPathType type = 0;
    XTocEntry * node;

    rc = XTocEntryResolvePath (self, path, false, &node);
    if ((rc == 0) && (node->type == xtoce_link))
        type = kptAlias;

    rc = XTocEntryResolvePath (self, path, true, &node);
    if (rc)
    {
        if (GetRCState (rc) == rcNotFound)
            return type | kptNotFound;
        return type | kptBadPath;
    }

    switch (node->type)
    {
    default:
    case xtoce_undefined:
        return type | kptBadPath;

    case xtoce_root:
    case xtoce_dir:
        return type | kptDir;

    case xtoce_file:
#if 0
    /* archives and containers are files */
        return type | kptFile;
#else
    /* archives and containers are directories */
        return type | (node->u.file.has_tree) ? kptDir : kptFile;
#endif
        /* not sure how we'd get here */
    case xtoce_link:
        return type | kptBadPath;
    }
}


/* ======================================================================
 * rc_t XTocTree
 *   This set of functions is the API for building up the entries within an 
 *   XToc.
 */

/*
 * XTocEntryMakeInt
 *
 *  This function creates and initializes the common part of the
 *  XTocEntry
 *
 *  extra_alloc is space set aside for string space not initialized
 *              here.  target path in links for example
 */
static
rc_t XTocEntryMakeInt (XTocEntry ** pself, const char * name,
                       XTocEntry * parent, XTocEntry * container,
                       KTime_t mtime, size_t extra_alloc)
{
    XTocEntry * self;
    char * pc;
    size_t  z;
    rc_t rc = 0;

    assert ((container != NULL) || (strcmp(name,"/") == 0));
    assert (name);
    assert (pself);

    z = string_size (name) + 1;

    if (z <= 1)
    {
        rc = RC (rcFS, rcTocEntry, rcConstructing, rcName, rcTooShort);
        LOGERR (klogErr, rc, "No name for directory entry in XML TOC");
        return rc;
    }

    /* allocate the memory for the entry node.  
     * size of the node, space for the name and extra space requested
     * by the caller of this intermediate function
     */
    self = malloc (sizeof (*self) + z + extra_alloc);
    DBGENTRY (("%s: entry %p name %s\n",__func__,self,name));
    if (self == NULL)
        rc = RC (rcFS, rcToc, rcConstructing, rcMemory, rcExhausted);

    else
    {
        /* copy the name immediately after the common section */
        pc = (char*)(self+1);
        string_copy (pc, z, name, z);
        z --;
        StringInit (&self->name, pc, z, string_len (pc, z));

        self->container = container;
        self->parent = parent;
        self->root = container == NULL ? container : container->root;
        self->cache = NULL;
        self->mtime = mtime;
        BSTreeInit (&self->tree);
        if (parent)
            rc = BSTreeInsert (&parent->tree, &self->node, XTocEntrySort);
        if (rc == 0)
        {
            *pself = self;
            return 0;
        }
        XTocEntryDestroy (self);
    }
    return rc;
}

static
rc_t XTocTreeAddID (XTocEntry * self, const char * name, KTime_t mtime, 
                    XTocEntry * target)
{
    XTocEntry * entry;
    rc_t rc;

    DBGENTRY(("%s: self %p name %s target %p\n", __func__, self, name, target));
    rc = XTocEntryMakeInt (&entry, name, self, self, mtime, 0);
    if (rc == 0)
    {
        entry->u.id.target = target;
        entry->type = xtoce_id;
        DBG_XTocEntry (entry);
    }
    return rc;
}


rc_t XTocTreeAddDir (XTocEntry * self, XTocEntry ** pentry, XTocEntry * container,
                           const char * name, KTime_t mtime)
{
    rc_t rc = XTocEntryMakeInt (pentry, name, self, container, mtime, 0);
    if (rc == 0)
    {
        (*pentry)->type = xtoce_dir;
        DBG_XTocEntry (*pentry);
    }
    return rc;
}


rc_t XTocTreeAddSymlink   (XTocEntry * self, XTocEntry ** pentry, XTocEntry * container,
                           const char * name, KTime_t mtime, const char * target)
{
    XTocEntry * entry;
    size_t z = string_size (target) + 1;
    rc_t rc = XTocEntryMakeInt (&entry, name, self, container, mtime, z);

    if (rc == 0)
    {
        char * pc = (char *)entry->name.addr + entry->name.size + 1;
        string_copy (pc, z, target, z);
        z--;
        StringInit (&entry->u.link.target, pc, z, string_len (pc, z));
        entry->type = xtoce_link;
        DBG_XTocEntry (entry);
    }
    *pentry = entry;
    return rc;
}


rc_t XTocTreeAddFile      (XTocEntry * self, XTocEntry ** pentry,
                           XTocEntry * container, const char * name,
                           KTime_t mtime, const char * id,
                           const char * filetype, uint64_t size,
                           uint64_t offset, uint8_t md5 [16])
{
    XTocEntry * entry;
    size_t z;
    rc_t rc;

    DBGENTRY (("%s: name %s id %s filetype %s\n", __func__, name, id, filetype));

    z = string_size (filetype) + 1;
    rc = XTocEntryMakeInt (&entry, name, self, container, mtime, z);
    if (rc == 0)
    {
        char * pc;

        pc = (char*)entry->name.addr + entry->name.size + 1;
        string_copy (pc, z, filetype, z);
        z--;

        StringInit (&entry->u.file.filetype, pc, z, string_len (pc, z));
        entry->u.file.size = size;
        entry->u.file.offset = offset;
        memmove (entry->u.file.md5, md5, sizeof entry->u.file.md5);
        entry->u.file.has_tree = false;
        entry->u.file.archive = false;
        entry->u.file.container = false;
        entry->type = xtoce_file;

        DBGENTRY (("%s: entry %p entry->root %p id %s\n",
                   __func__, entry, id, entry->root));
        rc = XTocTreeAddID (entry->root, id, mtime, entry);
        if (rc)
        {
            LOGERR (klogErr, rc, "failed to create alias id - continuing");
            rc = 0;
        }
        DBG_XTocEntry (entry);
    }
    *pentry = entry;
    return rc;
}


rc_t XTocTreeAddContainer (XTocEntry * self, XTocEntry ** pentry,
                           XTocEntry * container, const char * name,
                           KTime_t mtime, const char * id,
                           const char * filetype, uint64_t size,
                           uint64_t offset, uint8_t md5 [16])
{
    rc_t rc = XTocTreeAddFile (self, pentry, container, name, mtime, id, 
                               filetype, size, offset, md5);
    if (rc == 0)
    {
        (*pentry)->u.file.has_tree = true;
        (*pentry)->u.file.container = true;
        DBG_XTocEntry ((*pentry));
    }
    return rc;
}


rc_t XTocTreeAddArchive (XTocEntry * self, XTocEntry ** pentry,
                         XTocEntry * container, const char * name,
                         KTime_t mtime, const char * id,
                         const char * filetype, uint64_t size,
                         uint64_t offset, uint8_t md5 [16])
{
    rc_t rc = XTocTreeAddFile (self, pentry, container, name, mtime, id, 
                               filetype, size, offset, md5);
    if (rc == 0)
    {
        (*pentry)->u.file.has_tree = true;
        (*pentry)->u.file.archive = true;
        DBG_XTocEntry ((*pentry));
    }
    return rc;
}


/* ======================================================================
 * XTocCache
 *
 * When an entry in the TOC is opened as either a file or a directory
 * it is recorded in the tree indexed by path.  The objects in that
 * tree are of tupe XTocCache
 */
struct XTocCache
{
    BSTNode node;
    KRefcount refcount;
    String path;
    XToc * toc;                 /* ref counted keep daddy alive */

    /* these three references are not reference counted */
    XTocEntry * entry;

    /* these references are back pointers for "re-opens" */
    const KXTocFile * file;
    const KXTocDir * dir;
};
static const char XTocCacheClassname[] = "XTocCache";

#if _DEBUGGING
#define DBG_XTocCache(p)                                                \
    {   DBGCACHE(("XTocCache: %p\n", (p)));                             \
        if (p) {                                                        \
            DBGCACHE(("    refcount: %u\n",*(unsigned*)&(p)->refcount)); \
            DBGCACHE(("    path:     %S\n",&(p)->path));                \
            DBGCACHE(("    toc:      %p\n",(p)->toc));                  \
            DBGCACHE(("    entry:    %p\n",(p)->entry));                \
            DBGCACHE(("    file:     %p\n",(p)->file));                 \
            DBGCACHE(("    dir:      %p\n",(p)->dir));                  \
            if ((p)->file) DBG_KXTocDir((p)->file);                     \
            if ((p)->dir) DBG_KXTocDir((p)->dir); }}
#else
#define DBG_XTocCache(p)
#endif

/* Sort
 *   A sort comparitor suitable for BSTreeInsert/NSTreInsertUnique
 */
static
int64_t CC XTocCacheSort (const BSTNode * item, const BSTNode * n)
{
    return StringCompare (&((const XTocCache*)item)->path,
                          &((const XTocCache*)n)->path);
}


/* 
 * Cmp
 *   A sort comparitor suitable for BSTreeFind
 */
static
int64_t CC XTocCacheCmp (const void * item, const BSTNode * n)
{
    const String * s = item;
    const XTocCache * node = (const XTocCache *)n;
    return StringCompare (s, &node->path);
}


/* 
 * Destroy
 */
static void XTocCacheDestroy (XTocCache * self);


/*
 * AddRef
 */
static
rc_t XTocCacheAddRef (const XTocCache * self)
{
    assert (self);
    switch (KRefcountAdd (&self->refcount, XTocCacheClassname))
    {
    case krefLimit:
        return RC (rcFS, rcToc, rcAttaching, rcRange, rcExcessive);
    }
    return 0;
}


/*
 * Release
 */
static
rc_t XTocCacheRelease (const XTocCache * cself)
{
    if (cself)
    {
        XTocCache * self = (XTocCache*)cself;
        switch (KRefcountDrop (&self->refcount, XTocCacheClassname))
        {
        case krefWhack:
            XTocCacheDestroy (self);
        }
    }
    return 0;    
}


/*
 * Make
 *
 * This creates the node with one reference.  The file or directory
 * are not opened by this, nor are they initialized to non-NULL.
 */
static
rc_t XTocCacheMake (XTocCache ** pself, String * path,
                         XToc * toc, XTocEntry * entry)
{
    XTocCache * self;
    char * pc;
    rc_t rc;

    assert (pself);
    assert (path);
    assert (entry);

    *pself = NULL;

    rc = XTocAddRef (toc);
    if (rc)
        return rc;

    self = calloc (sizeof (*self) + 1 + StringSize (path), 1);
    if (self == NULL)
        return RC (rcFS, rcTocEntry, rcConstructing, rcMemory, rcExhausted);


    KRefcountInit (&self->refcount, 1,  XTocCacheClassname, "Init",
                   path->addr);

    self->entry = entry;
    self->toc = toc;

    pc = (char *)(self + 1);
    string_copy (pc, StringSize (path), path->addr, StringSize (path));
    StringInit (&self->path, pc, StringSize(path), StringLength(path));
    *pself = self;
    return 0;
}


/* ======================================================================
 * XToc
 */

/* ----------------------------------------------------------------------
 * XToc
 *   The flagshop class for this is a very simple class.  It has no use 
 *   outside of NCBI at this point.
 *
 *   It maintains in debugging situations the path for the object that
 *   was opened upon creation.
 *
 *   It maintains the directory like entries created by copycat when it
 *    cataloged a submission.
 *
 *   It maintains a list of what fiels and directories within that catalog
 *   are currently open.  A request to open it again will end up just reusing
 *   the same KFile/KDirectory
 */

struct XToc
{
    String base_path;
    KRefcount refcount;
    XTocEntry * root;   /* binary search tree full objects of type XTocEntry */
    BSTree open;        /* opened entries stored in an XTocCache */
    XTocCache * cache;  /* shortcut to "/" cache entry - not ref counted */
};
static char XTocClassname[] = "XToc";

#if _DEBUGGING
#define DBG_XToc(p) \
    {   DBGTOC(("XToc: %p\n", (p))); \
        if (p) {                                                        \
            DBGTOC(("    base_path: %S\n", &(p)->base_path));           \
            DBGTOC(("    refcount:  %u\n", *(unsigned*)&(p)->refcount)); \
            DBGTOC(("    root       %p\n", (p)->root));                 \
            DBGTOC(("    open       %p\n", *(void**)&(p)->open)); \
            DBGTOC(("    cache      %p\n", (p)->cache)); \
            if((p)->root) DBG_XTocEntry((p)->root); \
            if((p)->cache) DBGXTocCache((p)->cache); }}
#else
#define DBG_XToc(p)
#endif

static
rc_t XTocDestroy (XToc * self)
{
    XTocEntryDestroy (self->root);
    /* the open tree will be empty for us to get here */
    free (self);
    return 0;
}


static
rc_t XTocAddRef (const XToc * self)
{
    if (self)
        switch (KRefcountAdd (&self->refcount, XTocClassname))
        {
        case krefLimit:
            return RC (rcFS, rcToc, rcAttaching, rcRange, rcExcessive);
        }
    return 0;
}


static
rc_t XTocRelease (const XToc * cself)
{
    if (cself)
    {
        XToc * self = (XToc*)cself;
        switch (KRefcountDrop (&self->refcount, XTocClassname))
        {
        case krefWhack:
            XTocDestroy (self);
        }
    }
    return 0;    
}


static
void XTocCacheDestroy (XTocCache * self)
{
    assert (self);
    BSTreeUnlink (&self->toc->open, &self->node);
    self->entry->cache = NULL;
    XTocRelease (self->toc);
    free (self);
}


static
rc_t XTocMake (XToc ** pself, const String * base_path)
{
    XToc * self;
    char * pc;
    rc_t rc;

    assert (pself);
    assert (base_path);

    self = malloc (sizeof (*self) + StringSize(base_path) + 1);
    if (self == NULL)
        return RC (rcFS, rcToc, rcConstructing, rcMemory, rcExhausted);

    pc = (char*)(self+1);
    memmove (pc, base_path->addr, StringSize(base_path));
    pc[StringSize(base_path)] = '\0';
    StringInit (&self->base_path, pc, StringSize(base_path),
                StringLength(base_path));

    KRefcountInit (&self->refcount, 1, XTocClassname, "Init", base_path->addr);

    rc = XTocEntryMakeInt (&self->root, "/", NULL, NULL,
                           (KTime_t)time(NULL), 0);
    if (rc)
    {
        free (self);
        return rc;
    }
    self->root->type = xtoce_root;
    self->root->container = self->root->root = self->root;
    
    BSTreeInit (&self->open);

    *pself = self;
    return 0;
}


/*--------------------------------------------------------------------------
 * KXTocFile
 *  
 *
 * This type exists only to keep the XToc Open
 */
struct KXTocFile
{
    KFile dad;
    const KFile * base;
    XTocCache * cache;
    XTocEntry * entry;
    String base_path;
};
#if _DEBUGGING
#define DBG_KXTocFile(p)                             \
    {   DBGTOC(("  KXTocFile: %p\n", (p)));                               \
        if (p) {                                                        \
            DBGTOC(("    dad.vt:            %p\n", (p)->dad.vt));           \
            DBGTOC(("    dad.file:          %p\n",(p)->dad.dir));           \
            DBGTOC(("    dad.refcount:      %u\n",*(unsigned*)&(p)->dad.refcount)); \
            DBGTOC(("    dad.read_enabled:  %u\n",*(unsigned*)&(p)->dad.read_enabled)); \
            DBGTOC(("    dad.write_enabled: %u\n",*(unsigned*)&(p)->dad.write_enabled)); \
            DBGTOC(("    base:              %p\n",(p)->base));              \
            DBGTOC(("    cache:             %p\n",(p)->cache));             \
            DBGTOC(("    entry:             %p\n",(p)->entry));             \
            DBGTOC(("    base_path:         %S\n",&(p)->base_path));        \
            if ((p)->base){                                             \
                DBGTOC(("      base->vt:            %p\n", (p)->base->vt));       \
                DBGTOC(("      base->file:          %p\n",(p)->base->dir));       \
                DBGTOC(("      base->refcount:      %u\n",*(unsigned*)&(p)->base->refcount)); \
                DBGTOC(("      base->read_enabled:  %u\n",*(unsigned*)&(p)->base->read_enabled)); \
                DBGTOC(("      base->write_enabled: %u\n",*(unsigned*)&(p)->base->write_enabled)); \
            }}}
#else
#define DBG_KXTocFile(p)
#endif

/* Destroy
 */
static
rc_t KXTocFileDestroy ( KXTocFile *self )
{
    assert (self);
    KFileRelease (self->base);
    XTocCacheRelease (self->cache);
    free (self);
    return 0;
}

/* GetSysFile
 *  returns an underlying system file object
 *  and starting offset to contiguous region
 *  suitable for memory mapping, or NULL if
 *  no such file is available.
 */
static
struct KSysFile *KXTocFileGetSysFile ( const KXTocFile *self, uint64_t *offset )
{
#if 1
    return NULL;
#else
    struct KSysfile * sf;
    uint64_t loffset;

    assert (self);
    assert (offset);

    sf = KFileGetSysFile (self->base, &loffset);
    if (sf)
        *offset = 0;
    else
        *offset = loffset + self->entry->u.file.offset;
    return sf;
#endif
}

/* RandomAccess
 *  ALMOST by definition, the file is random access
 *  certain file types ( notably compressors ) will refuse random access
 *
 *  returns 0 if random access, error code otherwise
 */
static
rc_t KXTocFileRandomAccess ( const KXTocFile *self )
{
    assert (self);
    return KFileRandomAccess (self->base);
}


/* Type
 *  returns a KFileDesc
 *  not intended to be a content type,
 *  but rather an implementation class
 */
static
uint32_t KXTocFileType ( const KXTocFile *self )
{
    assert (self);
    return KFileType (self->base);
}


/* Size
 *  returns size in bytes of file
 *
 *  "size" [ OUT ] - return parameter for file size
 */
static
rc_t KXTocFileSize ( const KXTocFile *self, uint64_t *size )
{
    assert (self);
    assert (size);
    *size = self->entry->u.file.size;
    return 0;
}


/* SetSize
 *  sets size in bytes of file
 *
 *  "size" [ IN ] - new file size
 */
static
rc_t KXTocFileSetSize ( KXTocFile *self, uint64_t size )
{
    return RC (rcFS, rcFile, rcUpdating, rcSelf, rcUnsupported);
}


/* Read
 *  read file from known position
 *
 *  "pos" [ IN ] - starting position within file
 *
 *  "buffer" [ OUT ] and "bsize" [ IN ] - return buffer for read
 *
 *  "num_read" [ OUT, NULL OKAY ] - optional return parameter
 *  giving number of bytes actually read
 */
static
rc_t CC KXTocFileRead ( const KXTocFile *self, uint64_t pos,
    void *buffer, size_t bsize, size_t *num_read )
{
    uint64_t size;
    uint64_t limit;

    assert (self);
    assert (buffer);
    assert (num_read);

    DBG_XTocEntry(self->entry);

    size = self->entry->u.file.size;    /* how big is our portion of the file */
    if(pos >= size)  {                  /* if past end of file we're done */
        *num_read = 0;
        return 0;
    }
    limit = pos + bsize;                /* requested limit */
    if(limit > size) {                  /* if beyond end of file trim it down */
        limit = size;
    }
    return KFileRead (self->base, pos + self->entry->u.file.offset, buffer,
                      limit - pos, num_read);
}


/* Write
 *  write file at known position
 *
 *  "pos" [ IN ] - starting position within file
 *
 *  "buffer" [ IN ] and "size" [ IN ] - data to be written
 *
 *  "num_writ" [ OUT, NULL OKAY ] - optional return parameter
 *  giving number of bytes actually written
 */
static
rc_t KXTocFileWrite ( KXTocFile *self, uint64_t pos,
    const void *buffer, size_t size, size_t *num_writ)
{
    return RC (rcFS, rcFile, rcUpdating, rcSelf, rcUnsupported);
}


/* Make
 *  create a new file object
 *  from file descriptor
 */
static KFile_vt_v1 vtKXTocFile =
{
    /* version 1.1 */
    1, 1,

    /* start minor version 0 methods */
    KXTocFileDestroy,
    KXTocFileGetSysFile,
    KXTocFileRandomAccess,
    KXTocFileSize,
    KXTocFileSetSize,
    KXTocFileRead,
    KXTocFileWrite,
    /* end minor version 0 methods */

    /* start minor version == 1 */
    KXTocFileType
    /* end minor version == 1 */
};


static
 rc_t KXTocFileMake (KXTocFile **pself, const KFile * base,
                     XTocCache * cache, XTocEntry * entry,
                     const String *path)
{
    KXTocFile * self;
    rc_t rc;

    assert (pself);
    assert (base);
    assert (cache);
    assert (path);

    self = malloc (sizeof (*self) + 1 + StringSize (path));
    if (self == NULL)
        rc = RC (rcFS, rcFile, rcConstructing, rcMemory, rcExhausted);
    else
    {
        rc = KFileInit (&self->dad, (const KFile_vt*)&vtKXTocFile, "KXTocFile", "no-name", true, false);
        if (rc == 0)
        {
            rc = KFileAddRef (base);
            if (rc == 0)
            {
                rc = XTocCacheAddRef (cache);
                if (rc == 0)
                {
                    char * pc;

                    pc = (char*)(self+1);
                    string_copy (pc, StringSize(path), path->addr, StringSize(path));
                    pc[StringSize(path)] = '\0';
                    StringInit (&self->base_path, pc, StringSize (path),
                                StringLength (path));
                    self->base = base;
                    self->entry = entry;
                    self->cache = cache;

                    cache -> file = self;

                    * pself = self;
                    return 0;
                }

                KFileRelease ( base );
            }
        }

        free ( self );
    }

    PLOGERR (klogErr, 
             (klogErr, rc,
              "Unable to make/open $(F)",
              "F=%S", path));

    *pself = NULL;
    return rc;
}


struct KXTocDir
{
    KDirectory dad;
    const KDirectory * base;
    XToc * toc;
    XTocCache * cache;    /* owns a reference */
    XTocEntry * entry;
    XTocEntry * container;
    XTocEntry * root;
    String      base_path;
};

#if _DEBUGGING
#define DBG_KXTocDir(p)                                                 \
    {   if (p)                                                          \
            DBGTOC(("KXTocDir -   %p\n"                                 \
                    "   base_path %S\n"                                 \
                    "   base      %p\n"                                 \
                    "   cache     %p\n"                                 \
                    "   entry     %p\n"                                 \
                    "   container %p\n"                                 \
                    "   root      %p\n",                                \
                    p, &p->base_path, p->base, p->cache,                \
                    p->entry, p->container, p->root));                  \
        else DBGTOC(("KXTocDir @ NULL\n")); }
#else
#define DBG_KXTocDir(p)
#endif
/*--------------------------------------------------------------------------
 * XTocListing
 *  a directory listing
 *
 * NOTE:
 * This is nearly identical to KSysDirListing but both are currently private to
 * their compilation units.
 */
struct XTocListing
{
    KNamelist dad;      /* base class */
    Vector    list;
    XTocEntry * entry;
};



/* ======================================================================
 * shared path routines that really should be their own class
 */
/* ----------------------------------------------------------------------
 * KXTocDirCanonPath
 *
 * In this context CanonPath means to make the path a pure /x/y/z with no back tracking 
 * by using ~/../~ or redundant ~/./~ or ~//~ here notations.  Not exactly the usage of 
 * canonical in describing a path in other places but consistent within KFS.  It matches
 * the common meaning of canonical path as the one true path except that processing out
 * of sym links isn't done here but would normally have been.  Not processing the 
 * links means potentially more than one canonical path can reach the same target 
 * violating the usual meaning of canonical path as the one true shortest path to any
 * element.
 *
 * const KXTocDir *		self		Object oriented C; KXTocDir object for this method
 * enum RCContext 		ctx
 * char * 			path
 * size_t			psize
 */
static
rc_t XTocPathCanonize (char * path, size_t psize)
{
    char *	low;	/* a pointer to the root location in path; not changed after initialization */
    char *	dst;	/* a target reference for compressing a path to remove . and .. references */
    char *	last;	/* the end of the last processed facet of the path */
    char *	end;	/* absolute end of the incoming path */
    char * 	src;	/* the start of the current facet to be processed */

    /* end is the character after the end of the incoming path */
    end = path + psize;

    /* point all other temp pointers at the root point in the incoming path */
    last = path;

    /* handle windows / / network path starter */
    if ((last == path) && (last[0] == '/') && (last[1] == '/'))
      last ++;

    low = dst = last;

    for (;;)
    {

	/* point at the first / after the most recent processed facet */
	src = string_chr (last + 1, end-last-1, '/');
	if (src == NULL)	/* if no '/' point to the end */
	    src = end;

	/* detect special sequences */
	switch (src - last)
	{
	case 1: /* / / (with nothing between) is a superflouous / hurting us to parse later;
		 * /. is a here reference to the same directory as the previous */
	    if ((last[1] == '/')||(last[1] == '.'))
	    {
		/* skip over */
		last = src;
		if (src != end)
		    continue;
	    }
	    break;
	case 2: /* /./ is a "here" reference point and is omitted by not copying it over */
	    if (last[1] == '.')
	    {
		/* skip over */
		last = src;
		if (src != end)
		    continue;
	    }
	    break;

	case 3: /* /../ is a up one directory and is processed by deleting the last facet copied */
	    if (last [1] == '.' && last [2] == '.')
	    {
		/* remove previous leaf in path */
		dst [ 0 ] = 0;
		dst = string_rchr (path, end-path, '/');
		/* can't up a directory past the root */
		if (dst == NULL || dst < low)
		{
		    return RC (rcFS, rcDirectory, rcResolving, rcPath, rcInvalid);
		}

		last = src;
		if (src != end)
		    continue;
	    }
	    break;
	}

	/* if rewriting, copy leaf */
	if (dst != last)
	{
	    memmove (dst, last, src - last);
	}

	/* move destination ahead */
	dst += src - last;
        
	/* if we're done, go */
	if (src == end)
	    break;

	/* find next separator */
	last = src;
    }

    /* NUL terminate if modified */
    if (dst != end)
	* dst = 0;

    /* say we did did it with no problems */
    return 0;
}


rc_t XTocMakePath (const char ** ppath, bool canon, const char * path, va_list args)
{
    char * b;
    size_t bz;  /* size to allocate */
    rc_t rc = 0;
    bool   use_printf;

    assert (ppath);
    assert (path);
#if 0
    DBGTOC(("%s: make path from %s\n", __func__, path));
#endif
    b = NULL;
    bz = string_size (path) + 1;  /* start assuming out is same size as in */

    use_printf = (string_chr (path, bz, '%') != NULL);
    if (use_printf)
    {
        char shortbuff [32];
        int  pz = vsnprintf (shortbuff, sizeof shortbuff, path, args);
        if (pz < 0)
        {
            rc = RC (rcFS, rcDirectory, rcFormatting, rcPath, rcInvalid);
            DBGTOC(("%s: invalid path %s\n", __func__, path));
            LOGERR(klogErr, rc, "Error building XTOC path");
            return rc;
        }
        bz = pz + 1;
    }

    b = malloc (bz);
    if (b == NULL)
        return RC (rcFS, rcDirectory, rcFormatting, rcMemory, rcExhausted);

    if (use_printf)
    {
        int pz = vsnprintf (b, bz, path, args);
        if (pz < 0)
            rc = RC (rcFS, rcDirectory, rcFormatting, rcPath, rcInvalid);
        else
            b[pz] = '\0';
    }
    else
        string_copy (b, bz, path, bz);

    if ((rc == 0) && canon)
        rc = XTocPathCanonize (b, bz);

    if (rc == 0)
        *ppath = b;
    else
        free (b);
    return 0;
}

static char * XTocEntryMakeFullPathRecur (XTocEntry * self, size_t also, 
                                          char ** phere)
{
    char * path;
    char * here;

    assert (self);

#if 0
    DBGENTRY(("%s: self %p also %lu  %p\n",__func__,self,also,phere));
    DBG_XTocEntry(self);
#endif
    if (self->type == xtoce_root)
    {
        *phere = NULL;
        path = malloc (2 + also);
        if (path != NULL)
        {
            path[0] = '/';
            path[1] = '\0';
            *phere = path;
        }
#if 0
        DBGENTRY (("%s: path %s %p %p\n", __func__, path, path, *phere));
#endif
    }
    else
    {
        path = XTocEntryMakeFullPathRecur (self->parent,
                                           also + 1 + StringSize (&self->name),
                                           &here);
        if (path == NULL)
        {
            *phere = NULL;
            return NULL;
        }
#if 0
        DBGENTRY (("%s: path '%s' %p '%s' %p\n",__func__, path, path, here, here));
#endif
        here[0] = '/';
        string_copy (here+1, StringSize (&self->name) + 1,
                     self->name.addr, StringSize (&self->name));
        *phere = here + 1 + StringSize (&self->name);
    }
    return path;
}

static
rc_t XTocEntryMakeFullPath (XTocEntry * self, const char ** pfull)
{
    char * ignored;
    char * full_path;
#if 0
    DBGENTRY(("%s: self %p pfull %p\n",__func__,self,pfull));
#endif
    assert (pfull);
    *pfull = NULL;
    full_path = XTocEntryMakeFullPathRecur (self, 0, &ignored);
    if (full_path == NULL)
        return RC (rcFS, rcPath, rcCreating, rcMemory, rcExhausted);
    *pfull = full_path;
    return 0;
}



/* ======================================================================
 */
/* ----------------------------------------------------------------------
 * XTocListingDestroy
 * Class destructor
 *
 *
 * [RET] rc_t					0 for success; anything else for a failure
 *						see itf/klib/rc.h for general details
 * [INOUT] XTocListing *	self		Listing self reference: object oriented in C
 */
static
rc_t CC XTocListingDestroy (XTocListing *self)
{
    VectorWhack (&self->list, NULL, NULL);
    free (self);
    return 0;
}


/* ----------------------------------------------------------------------
 * XTocListingCount
 *
 *
 * [RET] rc_t					0 for success; anything else for a failure
 *						see itf/klib/rc.h for general details
 * [IN]  const XTocListing *	self		Listing self reference: object oriented in C
 * [OUT] uint32_t *		count		Where to put the count of names
 */
static rc_t CC XTocListingCount (const XTocListing *self, uint32_t *count)
{
    *count = VectorLength (&self->list);
    return 0;
}


/* ----------------------------------------------------------------------
 * XTocListingGet
 *
 *
 * [RET] rc_t					0 for success; anything else for a failure
 *						see itf/klib/rc.h for general details
 * [IN]  const XTocListing *	self		Listing self reference: object oriented in C
 * [IN]  uint32_t		idx		?
 * [OUT] const char **		name		Where to put the name
 */
static rc_t CC XTocListingGet (const XTocListing *self, uint32_t idx, const char **name)
{
    uint32_t count;
    rc_t rc;

    rc = XTocListingCount(self, &count);
    if (rc == 0)
    {
        if (idx >= count)
            return RC (rcFS, rcNamelist, rcAccessing, rcParam, rcExcessive);
        *name = VectorGet (&self->list, idx);
    }
    return 0;
}


/* ----------------------------------------------------------------------
 * XTocListingSort
 *
 * This function has the signature needed to use with the NameList base class for
 * XTocListings to determine the order of two names.  Matches the signature of
 * strcmp() and other functions suitable for use by qsort() and others
 *
 * [RET] int					0:  if a == b 
 *						<0: if a < b
 *						>0: if a > b
 * [IN] const void *		a
 * [IN] const void *		b
 *
 * Elements are typed as const void * to match the signature needed for 
 * a call to qsort() but they should be pointers to namelist elements
 */
#if NOT_YET
static int CC XTocListingSort (const void *a, const void *b, void * ignored)
{
    return strcmp (*(const char**)a, *(const char**)b);
}
#endif
static KNamelist_vt_v1 vtXTocListing =
{
    /* version 1.0 */
    1, 0,

    /* start minor version 0 methods */
    XTocListingDestroy,
    XTocListingCount,
    XTocListingGet
    /* end minor version 0 methods */
};


/* ----------------------------------------------------------------------
 * XTocListingInit
 *
 * [RET] rc_t					0 for success; anything else for a failure
 *						see itf/klib/rc.h for general details
 * [INOUT] XTocListing *	self		Listing self reference: object oriented in C
 * [IN]    const char *		path		?
 * [IN]    const KDirectory *	dir		?
 * [IN]    bool (* 		f	)(const KDirectory*, const char*, void*),
 *						This is a filter function - any listing element
 *						passed to this function will generate a true ot
 *						a false.  If false that listing element is dropped.
 *						If this parameter is NULL all elements are kept.
 * [IN]	   void *		data		Ignored.  May use NULL if permitted
 *						by 'f'.
 */
struct XTocListingInitData
{
    Vector * v;
    rc_t rc;
};
static
void CC XTocListingAdd (BSTNode * n_, void * data_)
{
    struct XTocListingInitData * data = data_;
    const XTocEntry * n = (const XTocEntry *)n_;

    if (data->rc == 0)
    {
        data->rc = VectorAppend (data->v, NULL, n->name.addr);
    }
}


rc_t XTocListingMake (XTocListing ** pself, XTocEntry * entry,
                      bool (CC* f) (const KDirectory*, const char*, void*),
                      void *data)
{
    XTocListing * self;
    BSTree * tree;
    rc_t rc;

    /* is self parameter possibly NULL? */
    if (pself == NULL)
    {
	return  RC (rcFS, rcDirectory, rcConstructing, rcSelf, rcNull);
    }
    *pself = NULL;

    self = malloc (sizeof (*self));
    if (self == NULL)
        return RC (rcFS, rcListing, rcConstructing, rcMemory, rcExhausted);

    switch (entry->type)
    {
    default:
        return RC (rcFS, rcDirectory, rcConstructing, rcListing, rcIncorrect);

    case xtoce_file:
    case xtoce_dir:
        tree = &entry->tree;
        break;
    }

    /* start with an empty name list */
    VectorInit (&self->list, 0, 16);

    /* initialize the Namelist base class */
    if ((rc = KNamelistInit (& self -> dad,
			     (const KNamelist_vt*)&vtXTocListing)) == 0)
    {
        struct XTocListingInitData data;

        data.v = &self->list;
        data.rc = 0;

        BSTreeForEach (tree, false, XTocListingAdd, &data);

        rc = data.rc;
    }
    if (rc == 0)
        *pself = self;
    return rc;
}


static
rc_t XTocFindXTocCache (XToc * self, XTocCache ** pcache,
                        const String * spath)
{
    XTocCache * cache;
    assert (self);
    assert (pcache);
    assert (spath);

    DBGTOC(("%s: self %p pcache %p spath %S\n",__func__,self,pcache,spath));
    cache = (XTocCache*)BSTreeFind (&self->open, spath, XTocCacheCmp);
    DBGTOC(("%s: found cache %p\n",__func__,cache));
    *pcache = cache;
    return cache ? 0 : RC (rcFS, rcToc, rcSearching, rcNode, rcNotFound);
}


static
rc_t XTocMakeXTocCache (XToc * self, XTocCache ** pcache,
                        String * path, XTocEntry * entry)
{
    XTocCache * cache;
    rc_t rc;

    assert (self);
    assert (pcache);
    assert (path);

/*     DBGTOC(("%s: %S\n",path)); */
    *pcache = NULL;
    rc = XTocCacheMake (&cache, path, self, entry);
    if (rc)
        PLOGERR (klogErr,
                 (klogErr, rc, "error creating cache entry for $(P)",
                  "P=%S", path));
    else
    {
        union
        {
            BSTNode * b;
            XTocCache * x;
        } exist;
        DBGTOC(("%s: insert cache\n",__func__));
        rc = BSTreeInsertUnique (&self->open, &cache->node, &exist.b, XTocCacheSort);
        if (rc)
            PLOGERR (klogErr,
                     (klogErr, rc, "error inserting cache entry for $(P)",
                      "P=%S", path));
        else
        {
            *pcache = entry->cache = cache;
            return 0;
        }
        XTocCacheDestroy (cache);
    }
    return rc;
}


static
rc_t XTocDirGetCache (const KXTocDir * self, const char * path_, XTocCache ** pcache)
{
    const char * path;  /* absolute canonical path in ASCIZ */
    String spath;       /* absolute canonical path as a String */
    XTocEntry * entry;  /* toc entry for the path */
    XTocCache * cache;  /* cache entry for the path */
    rc_t rc;

    assert (self);
    assert (path_);
    assert (pcache);

    DBGCACHE(("%s: Get cache node for %s\n", __func__, path_));

    /* assume not found or errors */
    *pcache = NULL;

    /* first resolve the path to an entry node */
    rc = XTocEntryResolvePath (self->entry, path_, true, &entry);
    DBGCACHE (("%s: path resolved\n",__func__));
    if (rc)
    {
        PLOGERR (klogErr, 
                 (klogErr, rc, "Unable to resolve path $(P)",
                  "P=%s", path_));
        return rc;
    }

    DBGCACHE(("%s: entry  %p\n",__func__,entry));

    /* now create a actual canonical path from the root to the located entry
     * the incoming path could have had symbolic links or the like so we're
     * making it a single non-ambiguous version
     */
    rc = XTocEntryMakeFullPath (entry, &path);
    if (rc)
    {
        PLOGERR (klogErr, 
                 (klogErr, rc, "Unable to build full path for $(P)",
                  "P=%s", path_));
        return rc;
    }

    /* make a String version as that is what the cache holds */
    StringInitCString (&spath, path);
    DBGCACHE(("%s: made full path %S %s\n",__func__,&spath,path));

    /* see if it's already open */

    rc = XTocFindXTocCache (self->toc, &cache, &spath);
    if (rc == 0)
    {
        DBGCACHE(("%s: found an existing cache node for %S\n",__func__,&spath));
        XTocCacheAddRef (cache);
    }
    else
    {
        DBGCACHE(("%s: did not find an existing cache node for %S\n",__func__,&spath));
        /* not found is not an error condition */
        if (GetRCState(rc) == rcNotFound)
        {
            /* not found so make one */
            rc = XTocMakeXTocCache (self->toc, &cache, &spath, entry);
            DBGCACHE(("%s: made a cache node %p\n",__func__,cache));
        }
    }
    /* for rc to be 0 here we either found or created a cache node
     * we will have an ownership apassed to the caller
     */
    if (rc == 0)
        *pcache = cache;
    /* free allocated path in a buffer */
    free ((void*)path);
    return rc;
}


/* ----------------------------------------------------------------------
 * KXTocDirDestroy
 */
static
rc_t CC KXTocDirDestroy (KXTocDir *self)
{
    assert (self);

    XTocCacheRelease (self->cache);
    KDirectoryRelease (self->base);
    free (self);
    return 0;
}




/* ----------------------------------------------------------------------
 * KXTocDirList
 *  create a directory listing
 *
 *  "list" [ OUT ] - return parameter for list object
 *
 *  "path" [ IN, NULL OKAY ] - optional parameter for target
 *  directory. if NULL, interpreted to mean "."
 *
 * [RET] rc_t					0 for success; anything else for a failure
 *						see itf/klib/rc.h for general details
 * [IN]	 const KXTocDir *	self		Object oriented C; KXTocDir object for this method
 * [OUT] KNamelist **		listp,
 * [IN]  bool (* 		f	)(const KDirectory*,const char *, void *)
 * [IN]  void *			data
 * [IN]  const char *		path
 * [IN]  va_list		args
 */
static
rc_t CC KXTocDirList (const KXTocDir *self,
                     KNamelist **listp,
                     bool (CC* f) (const KDirectory *dir, const char *name, void *data),
                     void *data,
                     const char *path_,
                     va_list args)
{
    const char * path;
    rc_t rc;

    if (path_ == NULL)
        path_ = "/";

    rc = XTocMakePath (&path, true, path_, args);
    if (rc == 0)
    {
        struct XTocEntry * node;

        rc = XTocEntryResolvePath (self->entry->root, path, true, &node);
        if (rc == 0)
        {
            rc = XTocListingMake ((struct XTocListing **)listp, node, f, data);
        }
        free ((void*)path);
    }
    return rc;
}


/* ----------------------------------------------------------------------
 * KXTocDirPathType
 *  returns a KPathType
 *
 *  "path" [ IN ] - NUL terminated string in directory-native character set
 *
 * [RET] uint32_t
 * [IN]  const KXTocDir *	self		Object oriented C; KXTocDir object for this method
 * [IN]  const char *		path
 * [IN]  va_list		args
 */
static KPathType CC KXTocDirPathType (const KXTocDir *self, const char *path_, va_list args)
{
    const char * path;
    rc_t rc;
    KPathType type = kptBadPath;

    rc = XTocMakePath (&path, true, path_, args);
    if (rc == 0)
    {
        type = XTocEntryPathType (self->cache->entry->root, path);
        free ((void*)path);
    }
    return type;
}


/* ----------------------------------------------------------------------
 * KXTocDirRelativePath
 *
 * KXTocDirRelativePath
 *  makes "path" relative to "root"
 *  both "root" and "path" MUST be absolute
 *  both "root" and "path" MUST be canonical, i.e. have no "//", "/./" or "/../" sequences
 *
 * [RET] rc_t					0 for success; anything else for a failure
 *						see itf/klib/rc.h for general details
 * [IN] const KXTocDir *		self		Object oriented C; KXTocDir object for this method
 * [IN] enum RCContext 		ctx
 * [IN] const char *		root
 * [IN] char *			path
 * [IN] size_t			path_max
 */
#if not_USED
static
rc_t KXTocDirRelativePath (const KXTocDir *self, enum RCContext ctx,
                           const char *root, char *path, size_t path_max)
{
#if 1
    return -1;
#else
    int backup;
    size_t bsize, psize;
    size_t size;

    const char *r = root + self -> root;
    const char *p = path + self -> root;

    assert (r != NULL && r [ 0 ] == '/');
    assert (p != NULL && p [ 0 ] == '/');

    for (; * r == * p; ++ r, ++ p)
    {
	/* disallow identical paths */
	if (* r == 0)
	    return RC (rcFS, rcDirectory, ctx, rcPath, rcInvalid);
    }

    /* paths are identical up to "r","p"
       if "r" is within a leaf name, then no backup is needed
       by counting every '/' from "r" to end, obtain backup count */
    for (backup = 0; * r != 0; ++ r)
    {
	if (* r == '/')
	    ++ backup;
    }

    /* the number of bytes to be inserted */
    bsize = backup * 3;

    /* align "p" to last directory separator */
    while (p [ -1 ] != '/') -- p;

    /* the size of the remaining relative path */
    psize = string_measure(p, &size);

    /* open up space if needed */
    if ( (size_t)(p - path) < bsize )
    {
	/* prevent overflow */
	if (bsize + psize >= path_max)
	    return RC (rcFS, rcDirectory, ctx, rcPath, rcExcessive);
	memmove (path + bsize, p, psize);
    }

    /* insert backup sequences */
    for (bsize = 0; backup > 0; bsize += 3, -- backup)
	memmove (& path [ bsize ], "../", 3);

    /* close gap */
    if ( (size_t)( p - path ) > bsize )
	{
        size_t size;
		string_copy (& path [ bsize ], path_max - bsize, p, measure_string(p, &size));
	}

	return 0;
#endif
}
#endif

/* ----------------------------------------------------------------------
 * KXTocDirVisit
 *  visit each path under designated directory,
 *  recursively if so indicated
 *
 *  "recurse" [ IN ] - if non-zero, recursively visit sub-directories
 *
 *  "f" [ IN ] and "data" [ IN, OPAQUE ] - function to execute
 *  on each path. receives a base directory and relative path
 *  for each entry, where each path is also given the leaf name
 *  for convenience. if "f" returns non-zero, the iteration will
 *  terminate and that value will be returned. NB - "dir" will not
 *  be the same as "self".
 *
 *  "path" [ IN ] - NUL terminated string in directory-native character set
 */
/* ----------------------------------------------------------------------
 * KXTocDirVisitDir
 *
 * [IN] KXTocDirVisitData *	pb
 */
#if NOT_YET
typedef struct KXTocDirVisitData
{
    int nada;
} KXTocDirVisitData;
static
rc_t KXTocDirVisitDir(KXTocDirVisitData *pb)
{
    return -1;
}
#endif
static 
rc_t CC KXTocDirVisit (const KXTocDir *self, 
                      bool recurse,
                      rc_t (CC* f) (const KDirectory *, uint32_t, const char *, void *), 
                      void *data,
                      const char *path,
                      va_list args)
{
#if 1
    return RC (rcFS, rcDirectory, rcVisiting, rcSelf, rcUnsupported);
#else
    char * full_path;
    rc_t   rc;

    if (path == NULL)
        path = "/";

    /* -----
     * First fix the path to make it useable
     */
    rc = KXTocDirMakePath (self, rcVisiting, true, &full_path, path, args);
    if (rc != 0)
    {
	LOGERR (klogInt, rc, "failed to make path in Visit");
    }
    else
    {
	const XTocEntry * pnode;

	/* -----
	 * Now find that path as a node and validate it is a directory
	 */
	rc = KXTocEntryResolvePath (&self->toc->root, full_path, true, &pnode);
	if (rc != 0)
	{
	    PLOGERR (klogInt, (klogInt, rc, "failed to resolve path $(P) in Visit", "P=%s", full_path));
	}
	else
	{
            switch (pnode->type)
            {
            case xtoce_file:
                if (pnode->u.file.has_tree)
                    break;
            default:
                rc = RC (rcFS, rcDirectory, rcVisiting, rcPath, rcInvalid);
                break;
            case xtoce_dir:
                break;
            }

            if (rc == 0)
            {
                size_t size;
		KXTocDir * full_dir;
		uint32_t path_size;

		/* -----
		 * make a locally accessible private KDirectory/KXTocDir
		 */
		for ( path_size = (uint32_t)string_measure( full_path, &size );
		      ( path_size > self->root ) && ( full_path[ path_size - 1 ] == '/' );
		      -- path_size )
		{}
		rc = KXTocDirMake (&full_dir, 
				  rcVisiting,
				  self->parent,
				  self->toc,
				  pnode,
				  self->archive.v,
				  self->arctype,
				  self->root,
				  full_path,
				  path_size, 
				  true,
				  false);
		if (rc == 0)
		{
		    KXTocDirVisitData pb;

		    pb.f = f;
		    pb.data = data;
		    pb.dir = full_dir;
		    pb.recurse = recurse;
/*		    pb.dir.path[--pb.dir.size] = 0; */

		    rc = KXTocDirVisitDir (&pb);

		    KXTocDirDestroy (full_dir);
		}
	    }
	    else
	    {
		rc = RC (rcFS, rcDirectory, rcVisiting, rcPath, rcIncorrect);
		LOGERR (klogInt, rc, "Type is not a directory");
	    }
	}
	free (full_path);
    }
    return rc;
#endif
}

/* ----------------------------------------------------------------------
 * KXTocDirVisitUpdate
 */
static rc_t CC KXTocDirVisitUpdate (KXTocDir *self,
                                   bool recurse,
                                   rc_t (CC*f) (KDirectory *,uint32_t,const char *,void *),
                                   void *data,
                                   const char *path,
                                   va_list args)
{
    return RC (rcFS, rcDirectory, rcUpdating, rcSelf, rcUnsupported);
}

/* ----------------------------------------------------------------------
 * KXTocDirResolvePath
 *
 *  resolves path to an absolute or directory-relative path
 *
 * [IN]  const KXTocDir *self		Objected oriented self
 * [IN]	 bool 		absolute	if non-zero, always give a path starting
 *  					with '/'. NB - if the directory is 
 *					chroot'd, the absolute path
 *					will still be relative to directory root.
 * [OUT] char *		resolved	buffer for NUL terminated result path in 
 *					directory-native character set
 * [IN]	 size_t		rsize		limiting size of resolved buffer
 * [IN]  const char *	path		NUL terminated string in directory-native
 *					character set denoting target path. 
 *					NB - need not exist.
 */
static rc_t CC KXTocDirResolvePath (const KXTocDir *self,
                                   bool absolute,
                                   char *resolved,
                                   size_t rsize,
                                   const char *path_,
                                   va_list args)
{
    rc_t rc;
    const char * path;

    KOutMsg ("+++++\n%s: absolute %d\n", __func__, absolute);

    assert (self);
    assert (resolved);
    assert (path_);

    *resolved = '\0';
    rc = XTocMakePath (&path, false, path_, args);
    KOutMsg ("%s: rc %R new '%s' old '%s'\n", __func__, rc, path, path_);
    if (rc != 0)
    {
        PLOGERR (klogErr,
                 (klogErr, rc, "Error building path based on $(P)", "P=%s", path_));
    }
    else
    {
        size_t path_sz = string_size(path);

        if (absolute)
        {
            XTocEntry * entry;

            rc = XTocEntryResolvePath (self->entry, path, false, &entry);

            if (rc)
                PLOGERR (klogErr,
                         (klogErr, rc, "Error resolving path based on $(P)", "P=%s", path_));
            else
            {
                free ((void*)path);
                path = NULL;
                rc = XTocEntryMakeFullPath (entry, &path);
                if (rc == 0)
                {
                    size_t k = self->toc->base_path.size;
                    if (k > 1)
                    {
                        memmove (resolved, self->toc->base_path.addr, k);
                        resolved += k;
                        rsize -= k;
                    }
                }
            }
        }
        if (rc == 0)
        {
            if (rsize < (path_sz - 1))
                rc = RC(rcFS, rcDirectory, rcResolving, rcBuffer, rcInsufficient);
            else
                string_copy(resolved, rsize, path, path_sz);
        }
        if (path)
            free ((void*)path);
    }
    return rc;
}

/* ----------------------------------------------------------------------
 * KXTocDirResolveAlias
 *  resolves an alias path to its immediate target
 *  NB - the resolved path may be yet another alias
 *
 *  "alias" [ IN ] - NUL terminated string in directory-native
 *  character set denoting an object presumed to be an alias.
 *
 *  "resolved" [ OUT ] and "rsize" [ IN ] - buffer for
 *  NUL terminated result path in directory-native character set
 *
 * NOTE: Does not meet a design target of on stack (localized variable) allocation of single 4kb path
 */
static rc_t CC KXTocDirResolveAlias (const KXTocDir * self, 
				 bool absolute,
				 char * resolved,
				 size_t rsize,
				 const char *path_,
				 va_list args)
{
#if 0
    return -1;
#else
    rc_t rc;
    const char * path;

    assert (self);
    assert (resolved);
    assert (path_);

    *resolved = '\0';
    rc = XTocMakePath (&path, false, path_, args);
    if (rc != 0)
    {
        PLOGERR (klogErr,
                 (klogErr, rc, "Error building path based on $(P)", "P=%s", path_));
    }
    else
    {
        XTocEntry * entry;

        rc = XTocEntryResolvePath (self->entry, path, true , &entry);
        if (rc)
            PLOGERR (klogErr,
                     (klogErr, rc, "Error resolving path based on $(P)", "P=%s", path_));
        else
        {
            free ((void*)path);
            path = NULL;
            rc = XTocEntryMakeFullPath (entry, &path);
        }
    }

/* THIS IS WRONG!
 * this is the absolute path not the relative one
 */
    if (rc == 0)
        string_copy (resolved, rsize, path, string_size (path));
    
    if (path)
        free ((void*)path);

    return rc;
#endif
}

/* ----------------------------------------------------------------------
 * KXTocDirRename
 *  rename an object accessible from directory, replacing
 *  any existing target object of the same type
 *
 *  "from" [ IN ] - NUL terminated string in directory-native
 *  character set denoting existing object
 *
 *  "to" [ IN ] - NUL terminated string in directory-native
 *  character set denoting existing object
 */
static
rc_t CC KXTocDirRename (KXTocDir *self, bool force, const char *from, const char *to)
{
    assert (self != NULL);
    assert (from != NULL);
    assert (to != NULL);

    return RC (rcFS, rcArc, rcUpdating, rcSelf, rcUnsupported);
}

/* ----------------------------------------------------------------------
 * KXTocDirRemove
 *  remove an accessible object from its directory
 *
 *  "path" [ IN ] - NUL terminated string in directory-native
 *  character set denoting target object
 *
 *  "force" [ IN ] - if non-zero and target is a directory,
 *  remove recursively
 */
static
rc_t CC KXTocDirRemove (KXTocDir *self, bool force, const char *path, va_list args)
{
    assert (self != NULL);
    assert (path != NULL);

    return RC (rcFS, rcArc, rcUpdating, rcSelf, rcUnsupported);
}

/* ----------------------------------------------------------------------
 * KXTocDirClearDir
 *  remove all directory contents
 *
 *  "path" [ IN ] - NUL terminated string in directory-native
 *  character set denoting target directory
 *
 *  "force" [ IN ] - if non-zero and directory entry is a
 *  sub-directory, remove recursively
 */
static
rc_t CC KXTocDirClearDir (KXTocDir *self, bool force, const char *path, va_list args)
{
    assert (self != NULL);
    assert (path != NULL);

    return RC (rcFS, rcArc, rcUpdating, rcSelf, rcUnsupported);
}

/* ----------------------------------------------------------------------
 * KXTocDirAccess
 *  get access to object
 *
 *  "access" [ OUT ] - return parameter for Unix access mode
 *
 *  "path" [ IN ] - NUL terminated string in directory-native
 *  character set denoting target object
 */
static rc_t CC KXTocDirVAccess (const KXTocDir *self,
			    uint32_t *access,
			    const char *path_,
			    va_list args)
{
#if 1
    return -1;
#else
    char * path;
    rc_t rc;

    if (path_ == NULL)
        path_ = "/";

    rc = KXTocDirMakePath (self, rcAccessing, true, &path, path_, args);
    if (rc == 0)
    {
        XTocEntry * node;

        rc = XTocEntryResolvePath (&self->toc->root, path, true, &node);


    const KTocEntry *	entry;
    rc_t 		rc;
    uint32_t		acc;
    KTocEntryType	type;
    char * 		full;

    assert (self != NULL);
    assert (access != NULL);
    assert (path != NULL);

    /* -----
     * by C standard the nested ifs (if A { if B { if C ... could have been if A && B && C
     */
    if ((rc = KXTocDirMakePath (self, rcAccessing, false, &full, path, args)) == 0)
    {
	if ((rc = KXTocDirResolvePathNode (self, rcAccessing, path, true, &entry, &type)) == 0)
	{
	    if ((rc = KTocEntryGetAccess (entry, &acc)) == 0)
	    {
                /*
                 * We want to filter the access because within an Archive
                 * a file is unwritable
                 */
		*access = acc & ~(S_IWRITE|S_IWGRP|S_IWOTH);
		rc = 0;
	    }
	}
    }
    if (full != NULL)
	free (full);
    return rc;
#endif
}

/* ----------------------------------------------------------------------
 * KXTocDirSetAccess
 *  set access to object a la Unix "chmod"
 *
 *  "path" [ IN ] - NUL terminated string in directory-native
 *  character set denoting target object
 *
 *  "access" [ IN ] and "mask" [ IN ] - definition of change
 *  where "access" contains new bit values and "mask defines
 *  which bits should be changed.
 *
 *  "recurse" [ IN ] - if non zero and "path" is a directory,
 *  apply changes recursively.
 */
static rc_t CC KXTocDirSetAccess (KXTocDir *self,
			      bool recurse,
			      uint32_t access,
			      uint32_t mask,
			      const char *path,
			      va_list args)
{
    assert (self != NULL);
    assert (path != NULL);

    return RC (rcFS, rcArc, rcUpdating, rcSelf, rcUnsupported);
}


static	rc_t CC KXTocDirVDate		(const KXTocDir *self,
					 KTime_t *date,
					 const char *path_,
					 va_list args)
{
    const char * path;
    rc_t rc;

    assert (self);
    assert (date);
    assert (path_);

    *date = 0;
    path = NULL;
    rc = XTocMakePath (&path, false, path_, args);
    if (rc)
        PLOGERR (klogErr,
                 (klogErr, rc, "Error resolving path from $(P)", "P=%s", path_));
    else
    {
        XTocEntry * entry;
        rc = XTocEntryResolvePath (self->entry, path, false, &entry);
        if (rc)
            PLOGERR (klogErr,
                     (klogErr, rc, "Error resolving path from $(P)", "P=%s", path_));
        else
            *date = entry->mtime;
        free ((void*)path);
    }
    return rc;
}

static	rc_t CC KXTocDirSetDate		(KXTocDir *self,
					 bool recurse,
					 KTime_t date,
					 const char *path,
					 va_list args)
{
    assert (self != NULL);
    assert (path != NULL);

    return RC (rcFS, rcArc, rcUpdating, rcSelf, rcUnsupported);
}

static
struct KSysDir *CC KXTocDirGetSysDir ( const KXTocDir *self )
{
    return NULL;
}

/* ----------------------------------------------------------------------
 * KXTocDirCreateAlias
 *  creates a path alias according to create mode
 *
 *  "targ" [ IN ] - NUL terminated string in directory-native
 *  character set denoting target object
 *
 *  "alias" [ IN ] - NUL terminated string in directory-native
 *  character set denoting target alias
 *
 *  "access" [ IN ] - standard Unix directory access mode
 *  used when "mode" has kcmParents set and alias path does
 *  not exist.
 *
 *  "mode" [ IN ] - a creation mode (see explanation above).
 */
static
rc_t CC KXTocDirCreateAlias (KXTocDir *self,
			 uint32_t access,
			 KCreateMode mode,
			 const char *targ,
			 const char *alias)
{
    assert (self != NULL);
    assert (targ != NULL);
    assert (alias != NULL);

    return RC (rcFS, rcArc, rcCreating, rcSelf, rcUnsupported);
}

/* ----------------------------------------------------------------------
 * KXTocDirOpenFileRead
 *  opens an existing file with read-only access
 *
 *  "f" [ OUT ] - return parameter for newly opened file
 *
 *  "path" [ IN ] - NUL terminated string in directory-native
 *  character set denoting target file
 * NOTE: Does not meet a design target of on stack (localized variable) allocation of single 4kb path
 */
static
rc_t  KXTocDirOpenFileReadInt	(const KXTocDir *self,
                                 const KXTocFile **f,
                                 const char *path)
{
    XTocCache * cache;
    rc_t rc;

    DBGTOC (("%s: Opening %s\n", __func__, path));
    /* resolve path to a cache entry - one will be created if it did not yet exist */
    rc = XTocDirGetCache (self, path, &cache);
    DBGTOC(("%s: got cache %p\n",__func__,cache));
    if (rc == 0)
    {
        /* if the file is already open this is easiest */
        if (cache->file)
        {
            DBGTOC(("%s: found open\n",__func__));
            rc = KFileAddRef (&cache->file->dad);
            if (rc)
                *f = cache->file;
        }
        else
        {
            do
            {
                const KFile * bfile = NULL;     /* base file for this new KXTocFile */
                XTocEntry * entry;

                entry = cache->entry;

                /* next easiest is files not in a container or archive
                 * the base file will be a KSysFile (most likely) */
                if (entry->container == entry->root)
                {
                    DBGTOC(("%s: NOT in a container or archive\n",__func__));
                    rc = KDirectoryOpenFileRead (((const KXTocDir*)self->toc->cache->dir)->base,
                                                 &bfile, "%s", path);
                }
                else do
                {
                    /* more difficult is a file inside a compressed container
                     * or an archive container */
                    const KXTocFile * cfile;    /* container's KXTocFile */
                    XTocCache * ccache;

                    DBGTOC(("%s: in a container or archive\n",__func__));
                    ccache = entry->container->cache;

                    /* is the container open? */
                    if (ccache)
                    {
                        /* was the container open but not the file?
                         * not sure if this is possible */
                        if (ccache->file == NULL)
                        {
                            rc = KXTocDirOpenFileReadInt (self->toc->cache->dir, 
                                                          &cfile, 
                                                          ccache->path.addr);
                            if (rc)
                                break;
                        }
                        else
                            cfile = ccache->file;
                    }
                    /* else the container was not yet open */
                    else
                    {
                        /* build the conatiner's path */
                        const char * cpath;
                        rc = XTocEntryMakeFullPath (entry->container, &cpath);
                        if (rc)
                            break;
                        else
                        {
                            /* now open the container's KXTocFile */
                            rc = KXTocDirOpenFileReadInt (self->toc->cache->dir, &cfile, cpath);
                            free ((void*)cpath);
                            if (rc)
                                break;
                        }                        
                    }

                    /* we should now have an opened container file
                     * it might be our base file or the source for our base */
                    DBGTOC(("%s: we have a container file %p\n", __func__, cfile));
                    DBG_KXTocFile (cfile);
                    
                    if (entry->container->u.file.archive) /* extracting from an archive file */
                    {
                        /* base file is the container file for an archive */
                        bfile = &cfile->dad;
                            DBGTOC(("%s: archive file set cfile %p bfile %p\n",
                                    __func__, cfile, bfile));
                    }
                    else if (entry->container->u.file.container) /* extracting from a compressed file */
                    {
                        /* the base file is built from the container's file via decompression */
                        if (strcmp (entry->container->u.file.filetype.addr, "Compressed/GnuZip") == 0)
                        {
                            rc = KFileMakeGzipForRead (&bfile, &cfile->dad);
                        }
                        else if (strcmp (entry->container->u.file.filetype.addr, "Compressed/Bzip") == 0)
                        {
                            rc = KFileMakeBzip2ForRead (&bfile, &cfile->dad);
                        }
                        else
                        {
                            DBGTOC(("%s: unsupported %s %s\n",__func__, cfile, 
                                    entry->container->u.file.filetype.addr));
                            LOGERR (klogErr, rc, "Unsupported compression type");
                            rc = RC (rcFS, rcToc, rcOpening, rcToc, rcUnsupported);
                        }
                        if (rc == 0)
                        {
                            /* release the container file leaving only the
                             * decompressed file owning it */
                            KFileRelease (&cfile->dad);
                            DBGTOC(("%s: container file created cfile %p bfile %p\n",
                                    __func__, cfile, bfile));
                        }
                    }
                    else
                        rc = RC (rcFS, rcToc, rcOpening, rcToc, rcCorrupt);
                }  while (0);
                if (rc == 0)
                {
                    /* now we have a base file for our XTocFile */
                    String spath;
                    KXTocFile * xfile;
                    StringInitCString (&spath, path);
                    rc = KXTocFileMake (&xfile, bfile, cache, entry, &spath);
                    DBGTOC(("%s: we have a new KXTocFile file %p bfile %p)\n", __func__, xfile, bfile));
                    DBG_KXTocFile(xfile);
                    if (rc == 0)
                        *f = xfile;
                }
                KFileRelease (bfile);
            } while (0);
        }
        XTocCacheRelease (cache);
    }
    return rc;
}


static
rc_t CC KXTocDirOpenFileRead	(const KXTocDir *self,
                                 const KFile **f,
                                 const char *path_,
                                 va_list args)
{
    const char * path;
    rc_t rc;

    assert (self);
    assert (f);
    assert (path_);

    DBGTOC (("%s: open %s\n", __func__, path_));

    /* make sure we'll write something to the output pointer */
    *f = NULL;

    /* build path from format and arguments or just the format that is a path in and of itself */
    rc = XTocMakePath (&path, true, path_, args);
    if (rc)
    {
        PLOGERR (klogErr,
                 (klogErr, rc, "Error opening file $(P)", "P=%s", path_));
    }
    else
        rc = KXTocDirOpenFileReadInt (self, (const KXTocFile**)f, path);
    free ((void*)path);
    return rc;
}


/* ----------------------------------------------------------------------
 * KXTocDirOpenFileWrite
 *  opens an existing file with write access
 *
 *  "f" [ OUT ] - return parameter for newly opened file
 *
 *  "path" [ IN ] - NUL terminated string in directory-native
 *  character set denoting target file
 *
 *  "update" [ IN ] - if non-zero, open in read/write mode
 *  otherwise, open in write-only mode
 */
static
rc_t CC KXTocDirOpenFileWrite	(KXTocDir *self,
					 KFile **f,
					 bool update,
					 const char *path,
					 va_list args)
{
    assert (self != NULL);
    assert (f != NULL);
    assert (path != NULL);

    return RC (rcFS, rcArc, rcCreating, rcSelf, rcUnsupported);
}

/* ----------------------------------------------------------------------
 * KXTocDirCreateFile
 *  opens a file with write access
 *
 *  "f" [ OUT ] - return parameter for newly opened file
 *
 *  "path" [ IN ] - NUL terminated string in directory-native
 *  character set denoting target file
 *
 *  "access" [ IN ] - standard Unix access mode, e.g. 0664
 *
 *  "update" [ IN ] - if non-zero, open in read/write mode
 *  otherwise, open in write-only mode
 *
 *  "mode" [ IN ] - a creation mode (see explanation above).
 */
static
rc_t CC KXTocDirCreateFile	(KXTocDir *self,
					 KFile **f,
					 bool update,
					 uint32_t access,
					 KCreateMode cmode,
					 const char *path,
					 va_list args)
{
    assert (self != NULL);
    assert (f != NULL);
    assert (path != NULL);

    return RC (rcFS, rcArc, rcCreating, rcSelf, rcUnsupported);
}

/* ----------------------------------------------------------------------
 * KXTocDirFileLocator
 *  returns locator in bytes of target file
 *
 *  "path" [ IN ] - NUL terminated string in directory-native
 *  character set denoting target file
 *
 *  "locator" [ OUT ] - return parameter for file locator
 * NOTE: Does not meet a design target of on stack (localized variable) allocation of single 4kb path
 */
static
rc_t CC KXTocDirFileLocator		(const KXTocDir *self,
					 uint64_t *locator,
					 const char *path_,
					 va_list args)
{
    const char * path;
    rc_t rc;

    assert (self);
    assert (locator);
    assert (path_);

    *locator = 0;
    path = NULL;
    rc = XTocMakePath (&path, false, path_, args);
    if (rc)
        PLOGERR (klogErr,
                 (klogErr, rc, "Error resolving path from $(P)", "P=%s", path_));
    else
    {
        XTocEntry * entry;
        rc = XTocEntryResolvePath (self->entry, path, false, &entry);
        if (rc)
            PLOGERR (klogErr,
                     (klogErr, rc, "Error resolving path from $(P)", "P=%s", path_));
        else if (entry->type != xtoce_file)
            rc = RC (rcFS, rcDirectory, rcAccessing, rcPath, rcIncorrect);
        else
            *locator = entry->u.file.offset;
        free ((void*)path);
    }
    return rc;
}


/* ----------------------------------------------------------------------
 * KXTocDirFileSize
 *  returns size in bytes of target file
 *
 *  "path" [ IN ] - NUL terminated string in directory-native
 *  character set denoting target file
 *
 *  "size" [ OUT ] - return parameter for file size
 * NOTE: Does not meet a design target of on stack (localized variable) allocation of single 4kb path
 */
static
rc_t CC KXTocDirFileSize		(const KXTocDir *self,
					 uint64_t *size,
					 const char *path_,
					 va_list args)
{
    const char * path;
    rc_t rc;

    assert (self);
    assert (size);
    assert (path_);

    *size = 0;
    path = NULL;
    rc = XTocMakePath (&path, false, path_, args);
    if (rc)
        PLOGERR (klogErr,
                 (klogErr, rc, "Error resolving path from $(P)", "P=%s", path_));
    else
    {
        XTocEntry * entry;
        rc = XTocEntryResolvePath (self->entry, path, false, &entry);
        if (rc)
            PLOGERR (klogErr,
                     (klogErr, rc, "Error resolving path from $(P)", "P=%s", path_));
        else if (entry->type != xtoce_file)
            rc = RC (rcFS, rcDirectory, rcAccessing, rcPath, rcIncorrect);
        else
            *size = entry->u.file.size;
        free ((void*)path);
    }
    return rc;
}


/* ----------------------------------------------------------------------
 * KXTocDirFileSize
 *  returns size in bytes of target file
 *
 *  "path" [ IN ] - NUL terminated string in directory-native
 *  character set denoting target file
 *
 *  "size" [ OUT ] - return parameter for file size
 * NOTE: Does not meet a design target of on stack (localized variable) allocation of single 4kb path
 */
static
rc_t CC KXTocDirFilePhysicalSize        (const KXTocDir *self,
					 uint64_t *size,
					 const char *path_,
					 va_list args)
{
    const char * path;
    rc_t rc;

    assert (self);
    assert (size);
    assert (path_);

    *size = 0;
    path = NULL;
    rc = XTocMakePath (&path, false, path_, args);
    if (rc)
        PLOGERR (klogErr,
                 (klogErr, rc, "Error resolving path from $(P)", "P=%s", path_));
    else
    {
        XTocEntry * entry;
        rc = XTocEntryResolvePath (self->entry, path, false, &entry);
        if (rc)
            PLOGERR (klogErr,
                     (klogErr, rc, "Error resolving path from $(P)", "P=%s", path_));
        else if (entry->type != xtoce_file)
            rc = RC (rcFS, rcDirectory, rcAccessing, rcPath, rcIncorrect);
        else
            *size = entry->u.file.size;
        free ((void*)path);
    }
    return rc;
}


/* ----------------------------------------------------------------------
 * KXTocDirSetFileSize
 *  sets size in bytes of target file
 *
 *  "path" [ IN ] - NUL terminated string in directory-native
 *  character set denoting target file
 *
 *  "size" [ IN ] - new file size
 */
static
rc_t CC KXTocDirSetFileSize	(KXTocDir *self,
					 uint64_t size,
					 const char *path,
					 va_list args)
{
    assert (self != NULL);
    assert (path != NULL);

    return RC (rcFS, rcArc, rcWriting, rcSelf, rcUnsupported);
}

/* ----------------------------------------------------------------------
 * KXTocDirOpenDirRead
 *
 *  opens a sub-directory
 *
 * [IN]  const KXTocDir *	self	Object Oriented C KXTocDir self
 * [OUT] const KDirectory **	subp	Where to put the new KDirectory/KXTocDir
 * [IN]  bool			chroot	Create a chroot cage for this new subdirectory
 * [IN]  const char *		path	Path to the directory to open
 * [IN]  va_list		args	So far the only use of args is possible additions to path
 */
static 
rc_t CC KXTocDirOpenDirRead	(const KXTocDir *self,
					 const KDirectory **subp,
					 bool chroot,
					 const char *path,
					 va_list args)
{
#if 1
    return -1;
#else
    char * full;
    rc_t rc;
    size_t size;

    assert (self != NULL);
    assert (subp != NULL);
    assert (path != NULL);

    rc = KXTocDirMakePath (self, rcOpening, true, &full, path, args);
    if (rc == 0)
    {
	const KTocEntry *	pnode;
	KTocEntryType		type;
	size_t path_size = string_measure (full, &size);

	/* -----
	 * get rid of any extra '/' characters at the end of path
	 */
	while (path_size > 0 && full [ path_size - 1 ] == '/')
	    full [ -- path_size ] = 0;

	/* -----
	 * get the node for this path 
	 */
	rc = KXTocDirResolvePathNode (self, rcOpening, full, true, &pnode, &type);
	if (rc == 0)
	{
            switch (type)
            {
            default:
		/* fail */
		rc = RC (rcFS, rcDirectory, rcOpening, rcPath, rcIncorrect);
                break;
            case ktocentrytype_dir:
            case ktocentrytype_hardlink:
	    {
		KXTocDir *	sub;

		rc = KXTocDirMake (&sub,
				  rcOpening,
				  self->parent,
				  self->toc,
				  pnode,
				  self->archive.v,
				  self->arctype,
				  self->root,
				  full,
				  (uint32_t)path_size,
				  false,
				  chroot);
		if (rc == 0)
		{
		    /* succeed */
		    *subp = &sub->dad;
		}
	    }
            }
	}
	free (full);
    }
    return rc;
#endif
}

/* ----------------------------------------------------------------------
 * KXTocDirOpenDirUpdate
 *  opens a sub-directory
 *
 *  "path" [ IN ] - NUL terminated string in directory-native
 *  character set denoting target directory
 *
 *  "chroot" [ IN ] - if non-zero, the new directory becomes
 *  chroot'd and will interpret paths beginning with '/'
 *  relative to itself.
 */
static
rc_t CC KXTocDirOpenDirUpdate	(KXTocDir *self,
					 KDirectory ** subp, 
					 bool chroot, 
					 const char *path, 
					 va_list args)
{
    assert (self != NULL);
    assert (subp != NULL);
    assert (path != NULL);

    return RC (rcFS, rcArc, rcUpdating, rcSelf, rcUnsupported);
}

/* ----------------------------------------------------------------------
 * KXTocDirCreateDir
 *  create a sub-directory
 *
 *  "path" [ IN ] - NUL terminated string in directory-native
 *  character set denoting target directory
 *
 *  "access" [ IN ] - standard Unix directory permissions
 *
 *  "mode" [ IN ] - a creation mode (see explanation above).
 */
static
rc_t CC KXTocDirCreateDir	(KXTocDir *self,
					 uint32_t access,
					 KCreateMode mode,
					 const char *path,
					 va_list args)
{
    assert (self != NULL);
    assert (path != NULL);

    return RC (rcFS, rcArc, rcCreating, rcSelf, rcUnsupported);
}

/* ----------------------------------------------------------------------
 * KXTocDirDestroyFile
 */
static
rc_t CC KXTocDirDestroyFile	(KXTocDir *self,
					 KFile * f)
{
    assert (self != NULL);
    assert (f != NULL);

    return RC (rcFS, rcArc, rcDestroying, rcSelf, rcUnsupported);
}

/* ----------------------------------------------------------------------
 * KXTocDirFileContiguous
 *  
 *
 *  "path" [ IN ] - NUL terminated string in directory-native
 *  character set denoting target file
 *
 *  "contiguous" [ OUT ] - return parameter for file status
 */
static
rc_t CC KXTocDirFileContiguous		(const KXTocDir *self,
                                         bool * contiguous,
					 const char *path,
                                         va_list args)
{
#if 1
    return -1;
#else
    char *		full_path;
    rc_t		rc;

    assert (self != NULL);
    assert (contiguous != NULL);
    assert (path != NULL);

    rc = KXTocDirMakePath (self, rcResolving, true,
			  &full_path, path, args);

    if (rc != 0)
    {
	/* can't "fix" path */
	/*rc = RC (rcFS, rcDirectory, rcResolving, rcPath, rcInvalid); ? or tweak it? */
    }
    else
    {
	const KTocEntry * pnode;
	KTocEntryType     type;

	rc = KXTocDirResolvePathNode (self, rcResolving, full_path, /*follow links*/true, &pnode, &type);

	if (rc != 0)
	{
	    /* can't resolve path */
	    /*rc = RC (rcFS, rcDirectory, rcResolving, rcPath, rcInvalid); ? or tweak it? */
	}
	else
	{
	    switch (type)
	    {
	    default:
                *contiguous = false;
		break;
	    case ktocentrytype_emptyfile:
	    case ktocentrytype_file:
                *contiguous = true;
		break;
	    }
	}
	free (full_path);
    }
    return rc;
#endif
}
/* ----------------------------------------------------------------------
 *
 */
static KDirectory_vt_v1 vtKXTocDir =
{
    /* version 1.0 */
    1, 3,

    /* start minor version 0 methods*/
    KXTocDirDestroy,
    KXTocDirList,
    KXTocDirVisit,
    KXTocDirVisitUpdate,
    KXTocDirPathType,
    KXTocDirResolvePath,
    KXTocDirResolveAlias,
    KXTocDirRename,
    KXTocDirRemove,
    KXTocDirClearDir,
    KXTocDirVAccess,
    KXTocDirSetAccess,
    KXTocDirCreateAlias,
    KXTocDirOpenFileRead,
    KXTocDirOpenFileWrite,
    KXTocDirCreateFile,
    KXTocDirFileSize,
    KXTocDirSetFileSize,
    KXTocDirOpenDirRead,
    KXTocDirOpenDirUpdate,
    KXTocDirCreateDir,
    KXTocDirDestroyFile,
    /* end minor version 0 methods*/
    /* start minor version 1 methods*/
    KXTocDirVDate,
    KXTocDirSetDate,
    KXTocDirGetSysDir,
    /* end minor version 2 methods*/
    KXTocDirFileLocator,
    /* end minor version 2 methods*/
    /* end minor version 3 methods*/
    KXTocDirFilePhysicalSize,
    KXTocDirFileContiguous
    /* end minor version 3 methods*/
};

static
rc_t KXTocDirMake (const KXTocDir ** pself, const KDirectory * base,
                   XToc * toc, XTocCache * cache, XTocEntry * entry,
                   const String * path)
{
    rc_t rc;
    KXTocDir * self;

    assert (pself);
    assert (base);
    assert (toc);
    assert (cache);
    assert (entry);
    assert (path);

    self = malloc (sizeof (*self) + 1 + StringSize (path));
    if (self == NULL)
        rc = RC (rcFS, rcDirectory, rcConstructing, rcMemory, rcExhausted);
    else
    {
        char * pc;

        self->base = base;        /* we take ownership of base */
        self->toc = toc;
        self->cache = cache;
        self->entry = entry;
        self->container = entry->container;
        self->root = toc->root;

        pc = (char *)(self + 1);
        string_copy (pc, StringSize (path), path->addr, StringSize (path));
        StringInit (&self->base_path, pc, StringSize (path), StringLength (path));

        rc = KDirectoryInit (&self->dad, (const KDirectory_vt*)&vtKXTocDir,
                             "KXTocDir", pc, false);
        if (rc == 0)
        {
            *pself = self;
            return 0;
        }
        free (self);
    }
    return rc;
}


static 
rc_t KDirectoryOpenXTocDirReadInt (const KDirectory * dir,
                                   const KDirectory ** pnew_dir,
                                   const KFile * xml,
                                   const String * spath)
{
    XToc * xtoc;
    rc_t rc;

    rc = XTocMake (&xtoc, spath);
    if (rc)
        LOGERR (klogErr, rc, "Error creating toc for xtc directory");
    else
    {
        String sroot;
        XTocCache * cache;

        StringInitCString (&sroot, ".");
        rc = XTocMakeXTocCache (xtoc, &cache, &sroot, xtoc->root);
        if (rc)
            LOGERR (klogErr, rc, 
                    "Failed to make cache for root for "
                    "xtoc directory");
        else
        {
            const KXTocDir * xdir;

            /* now that we have a cache node we should release
             * the original reference to the xtoc */
/* ??? */
            XTocRelease (xtoc);

            xtoc->cache = xtoc->root->cache = cache;

            rc = KXTocDirMake (&xdir, dir, xtoc, cache, xtoc->root,
                               &sroot);
            if (rc)
                LOGERR (klogErr, rc, "Error making xtoc directory");
            else
            {
                DBG_KXTocDir((xdir));
                cache->dir = xdir;

                rc = XTocParseXml (xtoc->root, xml);
                if (rc)
                    LOGERR (klogErr, rc, "Error parsing copycat xml file");
                else
                {
                    *pnew_dir = &xdir->dad;
                    return 0;
                }
                KDirectoryRelease (&xdir->dad);
            }                
            XTocCacheRelease (cache);
        }
        XTocRelease (xtoc);
    }
    return rc;
}

rc_t CC KDirectoryOpenXTocDirReadDir (const KDirectory * self,
                                                 const KDirectory ** pnew_dir,
                                                 const KFile * xml,
                                                 const String * spath)
{
    rc_t rc;
    
    /* loosely validate parameters */
    if (pnew_dir == NULL)
    {
        rc = RC (rcFS, rcDirectory, rcOpening, rcParam, rcNull);
        LOGERR (klogErr, rc,
                "new directory parameter is NULL for opening XToc Directory");
        return rc;
    }
    *pnew_dir = NULL;

    if (self == NULL)
    {
        rc = RC (rcFS, rcDirectory, rcOpening, rcSelf, rcNull);
        LOGERR (klogErr, rc, "self is NULL for opening XToc Directory");
        return rc;
    }
    else if (xml == NULL)
    {
        rc = RC (rcFS, rcDirectory, rcOpening, rcParam, rcNull);
        LOGERR (klogErr, rc, "xml parameter is NULL for opening XToc Directory");
        return rc;
    }
    else if (spath == NULL)
    {
        rc = RC (rcFS, rcDirectory, rcOpening, rcParam, rcNull);
        LOGERR (klogErr, rc, "base path parameter is NULL for opening XToc Directory");
        return rc;
    }
    else
    {
        rc = KDirectoryOpenXTocDirReadInt (self, pnew_dir, xml, spath);
    }
    return rc;
}


rc_t CC KDirectoryVOpenXTocDirRead (const KDirectory * self,
                                               const KDirectory ** pnew_dir,
                                               bool chroot,
                                               const KFile * xml,
                                               const char * _path,
                                               va_list args )
{
    rc_t rc;
    
    /* loosely validate parameters */
    if (pnew_dir == NULL)
    {
        rc = RC (rcFS, rcDirectory, rcOpening, rcParam, rcNull);
        LOGERR (klogErr, rc,
                "new directory parameter is NULL for opening XToc Directory");
        return rc;
    }
    *pnew_dir = NULL;

    if (self == NULL)
    {
        rc = RC (rcFS, rcDirectory, rcOpening, rcSelf, rcNull);
        LOGERR (klogErr, rc, "self is NULL for opening XToc Directory");
        return rc;
    }
    else if (xml == NULL)
    {
        rc = RC (rcFS, rcDirectory, rcOpening, rcParam, rcNull);
        LOGERR (klogErr, rc, "xml parameter is NULL for opening XToc Directory");
        return rc;
    }
    else if (_path == NULL)
    {
        rc = RC (rcFS, rcDirectory, rcOpening, rcParam, rcNull);
        LOGERR (klogErr, rc, "base path parameter is NULL for opening XToc Directory");
        return rc;
    }
    else
    /* okay none of our parameters are NULL; they could still be bad but let's start */
    {
        String spath;
        const KDirectory * bdir;
        KPathType type;
        char path [8192];

        rc = KDirectoryVResolvePath (self, true, path, sizeof path, _path, args);
        if (rc)
            return rc;

        type = KDirectoryPathType (self, "%s", path);
        switch (type & ~kptAlias)
        {
        case kptNotFound:
            rc = RC (rcFS, rcDirectory, rcOpening, rcParam, rcNotFound);
            LOGERR (klogErr, rc, "base path parameter is not found when opening XToc Directory");
            return rc;

        case kptBadPath:
            rc = RC (rcFS, rcDirectory, rcOpening, rcParam, rcInvalid);
            LOGERR (klogErr, rc, "base path parameter is a bad path when opening XToc Directory");
            return rc;

        case kptFile:
        case kptCharDev:
        case kptBlockDev:
        case kptFIFO:
        case kptZombieFile:
        case kptDataset:
        case kptDatatype:
            KOutMsg ("%s: type '%u' path '%s'\n",type,path);
            rc = RC (rcFS, rcDirectory, rcOpening, rcParam, rcWrongType);
            LOGERR (klogErr, rc, "base path parameter is an unusable type when opening XToc Directory");
            return rc;

        default:
            rc = RC (rcFS, rcDirectory, rcOpening, rcParam, rcCorrupt);
            LOGERR (klogErr, rc, "base path parameter is not a known type when opening XToc Directory");
            return rc;

        case kptDir:
            break;
        }

        rc = KDirectoryVOpenDirRead (self, &bdir, true, path, args);
        if (rc)
        {
            LOGERR (klogErr, rc, "failed to open base directory for XToc directory");
            return rc;
        }
        else
        {
            static const char absroot[] = "/";

            if (chroot)
                StringInitCString (&spath, absroot);
            else
                StringInitCString (&spath, path);

            rc = KDirectoryOpenXTocDirReadInt (bdir, pnew_dir, xml, &spath);
            if (rc == 0)
                return 0;

            KDirectoryRelease (bdir);
        }
    }
    return rc;
}


rc_t CC KDirectoryOpenXTocDirRead (const KDirectory * self,
                                              const KDirectory ** pnew_dir,
                                              bool chroot,
                                              const KFile * xml,
                                              const char * path, ... )
{
    rc_t rc;
    va_list args;

    va_start (args, path);
    rc = KDirectoryVOpenXTocDirRead (self, pnew_dir, chroot, xml, path, args);
    va_end (args);

    return rc;
}


struct VFSManager;

rc_t CC VFSManagerOpenXTocDirRead (const struct VFSManager * self,
                                              const struct KDirectory ** pnew_dir,
                                              const struct KFile * xml,
                                              const struct VPath * path)
{
    rc_t rc = 0;

    /* needs to be replaced with proper checking and rc returns */
    assert (pnew_dir);

    *pnew_dir = NULL;

    assert (self);
    assert (xml);
    assert (path);

    if (rc == 0)
    {
        const KDirectory * dir;

        rc = VFSManagerOpenDirectoryRead (self, &dir, path);
        if (rc == 0)
        {
            char pbuff [8192];
            size_t z;

            rc = VPathReadPath (path, pbuff, sizeof pbuff, &z);
            if (rc == 0)
            {
                String string;

                StringInit (&string, pbuff, z, string_len (pbuff, z));

                rc = KDirectoryOpenXTocDirReadDir (dir, pnew_dir, xml, &string);
                if (rc == 0)
                    return 0;
            }

            KDirectoryRelease (dir);
        }
    }
    return rc;
}



/* end of file */
