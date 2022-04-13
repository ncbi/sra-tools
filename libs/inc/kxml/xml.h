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

#ifndef _h_kxml_xml_
#define _h_kxml_xml_

#ifndef _h_klib_extern_
#include <klib/extern.h>
#endif

#include <klib/defs.h>

#include <stdarg.h>


#ifdef __cplusplus
extern "C" {
#endif


/*--------------------------------------------------------------------------
 * forwards
 */
struct KFile;
struct KNamelist;
struct KXMLNodeset;


/*--------------------------------------------------------------------------
 * XML node
 */
typedef struct KXMLNode KXMLNode;

/* AddRef
 * Release
 */
KLIB_EXTERN rc_t CC KXMLNodeAddRef ( const KXMLNode *self );
KLIB_EXTERN rc_t CC KXMLNodeRelease ( const KXMLNode *self );

/* GetName
 * Get node path from parent nodeset
 */
KLIB_EXTERN rc_t CC KXMLNodeGetName ( const KXMLNode *self, const char **name );

/* Get element name (tag)
 */
KLIB_EXTERN rc_t CC KXMLNodeElementName ( const KXMLNode *self, const char **name );

/* Read
 */
KLIB_EXTERN rc_t CC KXMLNodeRead ( const KXMLNode *self,
    size_t offset, void *buffer, size_t size,
    size_t *num_read, size_t *remaining );

/* Write
 */
KLIB_EXTERN rc_t CC KXMLNodeWrite ( KXMLNode *self,
    size_t offset, const void *buffer, size_t size );

/* Append
 */
KLIB_EXTERN rc_t CC KXMLNodeAppend ( KXMLNode *self,
    const void *buffer, size_t bsize );


/* ReadAs ( formatted )
 *  reads as integer or float value in native byte order
 *  casts smaller-sized values to desired size, e.g.
 *    uint32_t to uint64_t
 *
 *  "i" [ OUT ] - return parameter for signed integer
 *  "u" [ OUT ] - return parameter for unsigned integer
 *  "f" [ OUT ] - return parameter for double float
 */
KLIB_EXTERN rc_t CC KXMLNodeReadAsI16 ( const KXMLNode *self, int16_t *i );
KLIB_EXTERN rc_t CC KXMLNodeReadAsU16 ( const KXMLNode *self, uint16_t *u );
KLIB_EXTERN rc_t CC KXMLNodeReadAsI32 ( const KXMLNode *self, int32_t *i );
KLIB_EXTERN rc_t CC KXMLNodeReadAsU32 ( const KXMLNode *self, uint32_t *u );
KLIB_EXTERN rc_t CC KXMLNodeReadAsI64 ( const KXMLNode *self, int64_t *i );
KLIB_EXTERN rc_t CC KXMLNodeReadAsU64 ( const KXMLNode *self, uint64_t *u );
KLIB_EXTERN rc_t CC KXMLNodeReadAsF64 ( const KXMLNode *self, double *f );


/* ReadCString ( formatted )
 *  reads as C-string
 *
 *  "buffer" [ OUT ] and "bsize" [ IN ] - output buffer for
 *  NUL terminated string.
 *
 *  "size" [ OUT ] - return parameter giving size of string
 *  not including NUL byte. the size is set both upon success
 *  and insufficient buffer space error.
 */
KLIB_EXTERN rc_t CC KXMLNodeReadCString ( const KXMLNode *self,
    char *buffer, size_t bsize, size_t *size );

/* ReadCStr
 *  reads node value as C-string
 *
 *  "str" [ IN ] - returned pointer to a NULL terminated string.
 *                 Caller responsible to dealloc (free)!
 *  "default_value" [IN] - default value used in case node value is empty, NULL - none
 */
KLIB_EXTERN rc_t CC KXMLNodeReadCStr( const KXMLNode *self, char** str, const char* default_value );

/* Write ( formatted )
 *  writes string
 *
 *  "str" [ IN ] - NULL terminated string.
 */
KLIB_EXTERN rc_t CC KXMLNodeWriteCString ( KXMLNode *self, const char *str );


/* OpenNodesetRead
 *
 * NB. OpenNodesetRead could return Nodeset with 0 elements.
 *  To make sure the node exists you should verify that (KXMLNodesetCount > 0)
 */
KLIB_EXTERN rc_t CC KXMLNodeOpenNodesetRead ( const KXMLNode *self,
    struct KXMLNodeset const **ns, const char *path, ... );
KLIB_EXTERN rc_t CC KXMLNodeVOpenNodesetRead ( const KXMLNode *self,
    struct KXMLNodeset const **ns, const char *path, va_list args );

/* OpenNodesetUpdate
 */
KLIB_EXTERN rc_t CC KXMLNodeOpenNodesetUpdate ( KXMLNode *self,
    struct KXMLNodeset **ns, const char *path, ... );
KLIB_EXTERN rc_t CC KXMLNodeVOpenNodesetUpdate ( KXMLNode *self,
    struct KXMLNodeset **ns, const char *path, va_list args );

/* ReadAttr
 */
KLIB_EXTERN rc_t CC KXMLNodeReadAttr ( const KXMLNode *self,
     const char *attr, void *buffer, size_t bsize,
     size_t *num_read, size_t *remaining );

/* WriteAttr
 */
KLIB_EXTERN rc_t CC KXMLNodeWriteAttr ( KXMLNode *self,
     const char *attr, const void *buffer, size_t size );


/* ReadAttrAs ( formatted )
 *  reads as integer or float value in native byte order
 *  casts smaller-sized values to desired size, e.g.
 *    uint32_t to uint64_t
 *
 *  "i" [ OUT ] - return parameter for signed integer
 *  "u" [ OUT ] - return parameter for unsigned integer
 *  "f" [ OUT ] - return parameter for double float
 */
KLIB_EXTERN rc_t CC KXMLNodeReadAttrAsI16 ( const KXMLNode *self, const char *attr, int16_t *i );
KLIB_EXTERN rc_t CC KXMLNodeReadAttrAsU16 ( const KXMLNode *self, const char *attr, uint16_t *u );
KLIB_EXTERN rc_t CC KXMLNodeReadAttrAsI32 ( const KXMLNode *self, const char *attr, int32_t *i );
KLIB_EXTERN rc_t CC KXMLNodeReadAttrAsU32 ( const KXMLNode *self, const char *attr, uint32_t *u );
KLIB_EXTERN rc_t CC KXMLNodeReadAttrAsI64 ( const KXMLNode *self, const char *attr, int64_t *i );
KLIB_EXTERN rc_t CC KXMLNodeReadAttrAsU64 ( const KXMLNode *self, const char *attr, uint64_t *u );
KLIB_EXTERN rc_t CC KXMLNodeReadAttrAsF64 ( const KXMLNode *self, const char *attr, double *f );


/* ReadAttrCString ( formatted )
 *  reads as C-string
 *
 *  "buffer" [ OUT ] and "bsize" [ IN ] - output buffer for
 *  NUL terminated string.
 *
 *  "size" [ OUT ] - return parameter giving size of string
 *  not including NUL byte. the size is set both upon success
 *  and insufficient buffer space error.
 */
KLIB_EXTERN rc_t CC KXMLNodeReadAttrCString ( const KXMLNode *self, const char *attr,
    char *buffer, size_t bsize, size_t *size );

/* ReadAttrCStr
 *  reads attribute as C-string
 *
 *  "str" [ IN ] - returned pointer to a NULL terminated string.
 *                 Caller responsible to dealloc (free)!
 *  "default_value" [IN] - default value used in case node value is empty, NULL - none
 */
KLIB_EXTERN rc_t CC KXMLNodeReadAttrCStr( const KXMLNode *self, const char *attr, char** str, const char* default_value);

/* CountChildNodes
 *  count child nodes
 */
KLIB_EXTERN rc_t CC KXMLNodeCountChildNodes(const KXMLNode *self,
    uint32_t *count);

/* GetNodeRead
 *  access indexed node
 *  "idx" [ IN ] - a zero-based index
 */
KLIB_EXTERN rc_t CC KXMLNodeGetNodeRead ( const KXMLNode *self,
    const KXMLNode **node, uint32_t idx );

/* ListAttr
 *  list all named attributes
 */
KLIB_EXTERN rc_t CC KXMLNodeListAttr ( const KXMLNode *self,
    struct KNamelist const **names );

/* ListChild
 *  list all named children
 */
KLIB_EXTERN rc_t CC KXMLNodeListChild ( const KXMLNode *self,
    struct KNamelist const **names );


/* GetFirstChildNodeRead
 *  Returns the first(with index 0) sub-node of self using path.
 *  Is equivalent to:
 *          KXMLNodeOpenNodesetRead(self, &nc, path, ...);
 *          KXMLNodesetGetNodeRead(ns, node, 0);
 *  It will return NotFound error if any node exists
 */
KLIB_EXTERN rc_t CC KXMLNodeGetFirstChildNodeRead ( const KXMLNode *self,
    const KXMLNode **node, const char *path, ... );
KLIB_EXTERN rc_t CC KXMLNodeVGetFirstChildNodeRead ( const KXMLNode *self,
    const KXMLNode **node, const char *path, va_list args );


/*--------------------------------------------------------------------------
 * XML node set
 */
typedef struct KXMLNodeset KXMLNodeset;

/* AddRef
 * Release
 */
KLIB_EXTERN rc_t CC KXMLNodesetAddRef ( const KXMLNodeset *self );
KLIB_EXTERN rc_t CC KXMLNodesetRelease ( const KXMLNodeset *self );

/* Count
 *  retrieve the number of nodes in set
 */
KLIB_EXTERN rc_t CC KXMLNodesetCount ( const KXMLNodeset *self, uint32_t *count );

/* GetNode
 *  access indexed node
 *  "idx" [ IN ] - a zero-based index
 */
KLIB_EXTERN rc_t CC KXMLNodesetGetNodeRead ( const KXMLNodeset *self,
    const KXMLNode **node, uint32_t idx );


/*--------------------------------------------------------------------------
 * XML document
 */
typedef struct KXMLDoc KXMLDoc;

/* AddRef
 * Release
 *  ignores NULL references
 */
KLIB_EXTERN rc_t CC KXMLDocAddRef ( const KXMLDoc *self );
KLIB_EXTERN rc_t CC KXMLDocRelease ( const KXMLDoc *self );

/* OpenNodesetRead
 *  opens a node set with given path
 */
KLIB_EXTERN rc_t CC KXMLDocOpenNodesetRead ( const KXMLDoc *self,
    const KXMLNodeset **ns, const char *path, ... );
KLIB_EXTERN rc_t CC KXMLDocVOpenNodesetRead ( const KXMLDoc *self,
    const KXMLNodeset **ns, const char *path, va_list args );

/* OpenNodesetUpdate
 *  opens a node set with given path
 */
KLIB_EXTERN rc_t CC KXMLDocOpenNodesetUpdate ( KXMLDoc *self,
    KXMLNodeset **ns, const char *path, ... );
KLIB_EXTERN rc_t CC KXMLDocVOpenNodesetUpdate ( KXMLDoc *self,
    KXMLNodeset **ns, const char *path, va_list args );


/*--------------------------------------------------------------------------
 * XML manager
 */
typedef struct KXMLMgr KXMLMgr;

/* Make
 *  make an XML manager object
 */
KLIB_EXTERN rc_t CC KXMLMgrMakeRead ( const KXMLMgr **mgr );

/* AddRef
 * Release
 *  ignores NULL references
 */
KLIB_EXTERN rc_t CC KXMLMgrAddRef ( const KXMLMgr *self );
KLIB_EXTERN rc_t CC KXMLMgrRelease ( const KXMLMgr *self );

/* MakeDoc
 *  create a document object from source file
 */
KLIB_EXTERN rc_t CC KXMLMgrMakeDocRead ( const KXMLMgr *self,
    const KXMLDoc **doc, struct KFile const *src );

/* MakeDoc
 *  create a document object from memory
 */
KLIB_EXTERN rc_t KXMLMgrMakeDocReadFromMemory ( const KXMLMgr *self,
    const KXMLDoc **result, const char* buffer, uint64_t size );

/*--------------------------------------------------------------------------
 * KDirectory
 *  this will probably be relocated later on
 */
struct KDirectory;
struct KFile;
struct String;
struct VFSManager;
struct VPath;

/* OpenXTocDirRead
 *  open copycat XML as a chroot'd directory
 *  XML data comes from a KFile in this case
 *
 *  "dir" [ OUT ] - return parameter for directory object
 *
 *  "base_path" [ IN ] - NUL terminated string giving the path
 *   of the new directory relative to "self". NB - can be absolute
 *   or relative, but is always interpreted by "self".
 *
 *  "xml" [ IN ] - file containing XML data produced by copycat tool
 */
rc_t CC KDirectoryOpenXTocDirRead (const struct KDirectory * self,
                                   const struct KDirectory ** pnew_dir,
                                   bool chroot,
                                   const struct KFile * xml,
                                   const char * path, ... );
rc_t CC KDirectoryVOpenXTocDirRead (const struct KDirectory * self,
                                    const struct KDirectory ** pnew_dir,
                                    bool chroot,
                                    const struct KFile * xml,
                                    const char * _path,
                                    va_list args );
rc_t CC KDirectoryOpenXTocDirRead (const struct KDirectory * self,
                                   const struct KDirectory ** pnew_dir,
                                   bool chroot,
                                   const struct KFile * xml,
                                   const char * path, ... );
rc_t CC KDirectoryOpenXTocDirReadDir (const struct KDirectory * self,
                                      const struct KDirectory ** pnew_dir,
                                      const struct KFile * xml,
                                      const struct String * spath);

rc_t CC VFSManagerOpenXTocDirRead (const struct VFSManager * self,
                                   const struct KDirectory ** pnew_dir,
                                   const struct KFile * xml,
                                   const struct VPath * path);




#ifdef __cplusplus
}
#endif

#endif /* _h_kxml_xml_ */
