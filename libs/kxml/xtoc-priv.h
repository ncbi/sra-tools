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

#ifndef _h_kfs_xtoc_priv_h_
#define _h_kfs_xtoc_priv_h_

#include <klib/defs.h>
#include <klib/text.h>
#include <klib/container.h>
#include <stdarg.h>

struct KFile;
struct KDirectory;
struct XToc;
struct XTocEntry;
struct XTocEntryDir;
struct XTocEntryFile;
struct XTocListing;

/* ======================================================================
 * shared path routines that really should be their own class
 */

rc_t XTocPathMakePath (char ** ppath, bool canon, const char * path,
                       va_list args);

/*
 * In parsing the copycat XML we add nodes stating at the root XTocEntry
 * of the XToc.
 *
 * For each node in the DOM add one entry in the XTocEntry for the containing
 * container/archive/directory
 *
 * use the id="xxx" attribute to create a symbolic link in the root.
 * we'll come up with a safe version of the name such as "id:1" using a name
 * that won't exist in the regular filename space.
 *
 *Parameters:
 *   self:      Use the inner most archive, directory, or container for self.
 *              Use the root for the entries inside of root
 *
 *   entry:     Use a local XTocEntry pointer for each new node.  Some will 
 *              remembered for a while and some won't.  It depends upon whether
 *              it has subnodes.  Do not free or release these pointers.  Just
 *              stop using them as they are not referenced counted and freed only
 *              by the KToc itself when it is Whacked.
 *
 *   container: Use the most recent containing archive or container XTocEntry for
 *              each new node.  Use root for those outside the outer most
 *              container or archive.  Do not use directory nodes for this.
 *
 *   name:      comes directly form the XML as is
 *
 *   mtime:     comes directly form the XML converted to 64 bit unsigned
 *
 *   filetype:  comes directly form the XML as is
 *
 *   md5:       comes directly form the XML converted from hex string to byte string
 *
 *   size:      comes directly form the XML converty to 64 bit unsigned
 *
 * For each directory node in the XML use XTocTreeAddDir()
 * For each archive node in the XML use XTocTreeAddArchive()
 * For each container node in the XML use XTocTreeAddContainer()
 * For each file node in the XML use XTocTreeAddFile()
 * For each symlink node in the XML use XTocTreeAddSymLink()
 *
 * For each file, container, or archive use XTocTreeAddSymLink()
 *  use the original root as self, a local pointer, name, mtime from the target and
 *  path of the file, archive or container from the XML as target
 *
 * All of these are local and in Windows build they do not get the CC needed for inter-library calls.
 */
rc_t XTocParseXml (struct XTocEntry * root, const struct KFile * xml);

rc_t XTocTreeAddFile      (struct XTocEntry * self, struct XTocEntry ** pentry,
                           struct XTocEntry * container, const char * name,
                           KTime_t mtime, const char * id, const char * filetype, 
                           uint64_t size, uint64_t offset, uint8_t md5 [16]);

rc_t XTocTreeAddContainer (struct XTocEntry * self, struct XTocEntry ** pentry,
                           struct XTocEntry * container, const char * name,
                           KTime_t mtime, const char * id, const char * filetype, 
                           uint64_t size, uint64_t offset, uint8_t md5 [16]);

rc_t XTocTreeAddArchive (struct XTocEntry * self, struct XTocEntry ** pentry,
                         struct XTocEntry * container, const char * name,
                         KTime_t mtime, const char * id, const char * filetype,
                         uint64_t size, uint64_t offset, uint8_t md5 [16]);

rc_t XTocTreeAddDir       (struct XTocEntry * self, struct XTocEntry ** entry,
                           struct XTocEntry * container, const char * name,
                           KTime_t mtime);

rc_t XTocTreeAddSymlink   (struct XTocEntry * self, struct XTocEntry ** entry,
                           struct XTocEntry * container, const char * name,
                           KTime_t mtime, const char * target);


#endif /* #ifndef _h_kfs_xtoc_priv_h_ */
/* end of file */
