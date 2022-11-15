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

#ifndef _hpp_klib_xml_
#define _hpp_klib_xml_

#ifndef _h_klib_xml_
#include <kxml/xml.h>
#endif

/*--------------------------------------------------------------------------
 * XML node
 */
struct KXMLNode
{
    /* AddRef
     * Release
     */
    inline rc_t AddRef ( void ) const
    { return KXMLNodeAddRef ( this ); }
    inline rc_t Release ( void ) const
    { return KXMLNodeRelease ( this ); }

    /* GetName
     */
    inline rc_t GetName ( const char **name ) const
    { return KXMLNodeGetName ( this, name ); }

    /* OpenNodesetRead
     */
    inline rc_t OpenNodesetRead ( struct KXMLNodeset const **ns,
        const char *path, ... ) const
    {
        va_list args;
        va_start ( args, path );
        rc_t status = KXMLNodeVOpenNodesetRead ( this, ns, path, args );
        va_end ( args );
        return status;
    }

    /* ReadAttr
     */
    inline rc_t ReadAttr ( const char *attr, void *buffer, size_t bsize,
         size_t *num_read, size_t *remaining ) const
    {
        return KXMLNodeReadAttr
            ( this, attr, buffer, bsize, num_read, remaining );
    }

    /* CountNodes
     *  count child nodes
     */
    inline rc_t CountChildNodes ( uint32_t *count ) const
    { return KXMLNodeCountChildNodes ( this, count ); }

    /* GetNodeRead
     *  access indexed node
     *  "idx" [ IN ] - a zero-based index
     */
    inline rc_t GetNodeRead ( const KXMLNode **node,
        uint32_t idx ) const 
    { return KXMLNodeGetNodeRead ( this, node, idx ); }

    /* ListChild
     *  list all named children
     */
    inline rc_t ListChild ( struct KNamelist const **names ) const
    { return KXMLNodeListChild ( this, names ); }

    inline rc_t GetFirstChildNodeRead ( const KXMLNode **node,
        const char *path, ... ) const
    {
        va_list args;
        va_start ( args, path );
        rc_t rc = KXMLNodeVGetFirstChildNodeRead ( this, node, path, args );
        va_end ( args );
        return rc;
    }

    inline rc_t Read ( int32_t *i ) const
    { return KXMLNodeReadAsI32 ( this, i ); }

    inline rc_t Read ( char *buffer, size_t bsize, size_t *size ) const
    { return KXMLNodeReadCString ( this, buffer, bsize, size ); }

    inline rc_t ReadAttr ( const char *attr, int32_t *i ) const 
    { return KXMLNodeReadAttrAsI32 ( this, attr, i ); }

    inline rc_t ReadAttr ( const char *attr,
        char *buffer, size_t bsize, size_t *size ) const
    { return KXMLNodeReadAttrCString ( this, attr, buffer, bsize, size ); }
};

/*--------------------------------------------------------------------------
 * XML node set
 */
struct KXMLNodeset
{
    /* AddRef
     * Release
     */
    inline rc_t AddRef ( void ) const
    { return KXMLNodesetAddRef ( this ); }
    inline rc_t Release ( void ) const
    { return KXMLNodesetRelease ( this ); }

    /* Count
     *  retrieve the number of nodes in set
     */
    inline rc_t Count ( uint32_t *count ) const
    { return KXMLNodesetCount ( this, count ); }

    /* GetNode
     *  access indexed node
     */
    inline rc_t GetNodeRead ( const KXMLNode **node, uint32_t idx ) const
    { return KXMLNodesetGetNodeRead ( this, node, idx ); }
};

/*--------------------------------------------------------------------------
 * XML document
 */
struct KXMLDoc
{
    /* AddRef
     * Release
     *  ignores NULL references
     */
    inline rc_t AddRef ( void ) const
    { return KXMLDocAddRef ( this ); }
    inline rc_t Release ( void ) const
    { return KXMLDocRelease ( this ); }

    /* OpenNodesetRead
     *  opens a node set with given path
     */
    inline rc_t OpenNodesetRead ( const KXMLNodeset **ns, const char *path, ... ) const
    {
        va_list args;
        va_start ( args, path );
        int status = KXMLDocVOpenNodesetRead ( this, ns, path, args );
        va_end ( args );
        return status;
    }
};

/*--------------------------------------------------------------------------
 * XML manager
 */
struct KXMLMgr
{
    /* Make
     *  make an XML manager object
     */
    static inline rc_t MakeRead ( const KXMLMgr **mgr )
    { return KXMLMgrMakeRead ( mgr ); }

    /* AddRef
     * Release
     *  ignores NULL references
     */
    inline rc_t AddRef ( void ) const
    { return KXMLMgrAddRef ( this ); }
    inline rc_t Release ( void ) const
    { return KXMLMgrRelease ( this ); }

    /* MakeDoc
     *  create a document object from source file
     */
    inline rc_t MakeDocRead ( const KXMLDoc **doc, struct KFile const *src ) const
    { return KXMLMgrMakeDocRead ( this, doc, src ); }

};

#endif /* _hpp_klib_xml_ */
