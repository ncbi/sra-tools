/*==============================================================================
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

struct s_KNodeNamelist;
#define KNAMELIST_IMPL struct s_KNodeNamelist

#include <sysalloc.h>
#include <kxml/xml.h>

#include <klib/impl.h>
#include <kfs/file.h>
#include <klib/log.h>
#include <klib/rc.h>
#include <klib/debug.h>
#include <klib/text.h>

#include <os-native.h> /* for strndup on mac */
#include <strtol.h>

#include <libxml/xmlreader.h>
#include <libxml/xpath.h>

#include <assert.h>
#include <string.h>


#define XML_DEBUG(msg) DBGMSG (DBG_XML, DBG_FLAG(DBG_XML_XML), msg)

/********* XML manager ********/

struct KXMLMgr {
    atomic32_t refcount;
};

static KXMLMgr s_KXMLMgr;
/** calls vsnprintf and converts errors into KXMLErr */
static rc_t s_KXML_vsnprintf(char *str,
    size_t size,
    const char *format,
    va_list ap)
{
    int printed = vsnprintf(str, size, format, ap);
    if (printed < 0) {
        return RC ( rcXML, rcDoc, rcConstructing, rcData, rcCorrupt );
    }
    if ((size_t) printed >= size) { /* buffer is too small */
        return RC ( rcXML, rcDoc, rcConstructing, rcBuffer, rcExhausted );
    }
    return 0;
}

rc_t KXMLMgrMakeRead(const KXMLMgr **result)
{
    if (!result) {
        return RC ( rcXML, rcMgr, rcConstructing, rcParam, rcNull );
    }
    if (atomic32_read_and_add(&s_KXMLMgr.refcount, 1) == 0) {
        xmlInitParser();
    }
    *result = &s_KXMLMgr;
    return 0;
}

rc_t KXMLMgrAddRef(const KXMLMgr *cself)
{
    if (cself) {
        KXMLMgr *self = (KXMLMgr*) cself;
        atomic32_inc(&self->refcount);
    }
    return 0;
}

rc_t KXMLMgrRelease(const KXMLMgr *cself)
{
    if (cself) {
        KXMLMgr *self = (KXMLMgr*) cself;
        if (atomic32_dec_and_test(&self->refcount)) {
            xmlCleanupParser();
        }
    }
    return 0;
}

/********* XML doucument ********/
struct KXMLDoc {
    const KXMLMgr* parent;
    xmlDocPtr doc;
    xmlXPathContextPtr xpathCtx;
    atomic32_t refcount;
};

/*static int s_XmlInputReadCallback(void * context, 
					 char * buffer, 
					 int len)
{
    static size_t offset = 0;
    KFile *src = (KFile*) context;
    assert(src);
    {
        size_t num_read = 0;
        rc_t rc = KFileRead(src, offset, buffer, len, &num_read);
        if (rc)
        {   return -1; }
        offset += num_read;
        return num_read;
    }
}*/

/* SchemaValidate
 *  validates an XML source file against an XSD schema.
 * schema parameter is the path to the schema
 */
#if 0
rc_t KXMLMgrSchemaValidate ( const KXMLMgr *self,
    struct KFile const *src,
    const char *schema )
{
    if (!src || !schema)
    {   return RC(rcXML, rcDoc, rcValidating, rcParam, rcNull); }

    {
        rc_t rc = 0;
        xmlTextReaderPtr reader
            = xmlReaderForIO(s_XmlInputReadCallback, 0, (void*)src, "/", 0, 0);
        if (!reader)
        {   return RC(rcXML, rcDoc, rcValidating, rcSchema, rcUnknown); }
        {
            int r = xmlTextReaderSchemaValidate(reader, schema);
            if (r != 0) {
                rc = RC(rcXML, rcDoc, rcValidating, rcSchema, rcUnknown);
            } else {
                do {
                    r = xmlTextReaderRead(reader);
                } while (r == 1);
                if (r == -1) {
                    rc = RC(rcXML, rcDoc, rcValidating, rcDoc, rcInvalid);
                } else {
                    assert(r == 0);
                    r = xmlTextReaderIsValid(reader);
                    if (r != 1)
                    {   rc = RC(rcXML, rcDoc, rcValidating, rcDoc, rcInvalid); }
                }
            }
        }
        xmlTextReaderClose(reader);
        return rc;
    }
}
#endif

static void s_xmlGenericErrorDefaultFunc(void *ctx ATTRIBUTE_UNUSED,
    const char *msg,
    ...)
{
    va_list args;
    va_start(args, msg);
/*  vfprintf(stderr, msg, args); */
    va_end(args);
}

static rc_t s_XmlReadFile(const KFile *src, char** aBuffer, uint64_t* aSize)
{
    rc_t rc = 0;
    bool unknownFileSize = false;
    uint64_t size = 0;
    uint32_t t = KFileType(src);
    assert(aBuffer && aSize);
    *aBuffer = NULL;
    *aSize = 0;

    /* Read KFile by KFileRead: can use mmap instead */
    if (t == kfdFIFO) {
        XML_DEBUG (("%s: reading stdin ?", __func__));
        unknownFileSize = true;
        size = 0x10000;
    }
    else {
        rc = KFileSize(src, &size);
        if (rc)
        {   return rc; }
        if (!size) {
            rc = RC(rcXML, rcDoc, rcConstructing, rcFile, rcEmpty);
            return rc;
        }
    }
    {
        size_t num_read = 0;
        char* buffer = (char*) malloc(size);
        if (!buffer)
        {   return RC(rcXML, rcDoc, rcConstructing, rcMemory, rcExhausted); }
        rc = KFileRead(src, 0, buffer, size, &num_read);
        if (rc == 0) {
            if (!unknownFileSize) {
                if (num_read != size) {
                    rc = RC(rcXML, rcDoc, rcConstructing, rcFile, rcIncomplete);
                }
            }
            else {
                if (num_read == size) {
                    rc = RC(rcXML, rcDoc, rcConstructing, rcFile, rcTooBig);
                }
            }
        }
        if (rc) {
            free(buffer);
            return rc;
        }
        *aBuffer = buffer;
        *aSize = size;
    }

    return rc;
}

static int s_UseDefaultErrorHandler = 0;

rc_t KXMLMgrMakeDocRead(const KXMLMgr *self,
    const KXMLDoc **result,
    const KFile *src)
{
    rc_t rc = 0;
    if (self && src) {
        char* buffer = NULL;
        uint64_t size = 0;
        rc = s_XmlReadFile(src, &buffer, &size);
        if (rc)
        {    return rc; }
        rc = KXMLMgrMakeDocReadFromMemory(self, result, buffer, size);
        free(buffer);
    }
    else
    {    rc = RC ( rcXML, rcDoc, rcConstructing, rcParam, rcNull ); }
    return rc;
}

rc_t KXMLMgrMakeDocReadFromMemory(const KXMLMgr *self,
    const KXMLDoc **result,
    const char* buffer,
    uint64_t size)
{
    rc_t rc = 0;
    if (!s_UseDefaultErrorHandler)
    {   xmlSetGenericErrorFunc(0, s_xmlGenericErrorDefaultFunc); }
    if (!result)
    {   return RC ( rcXML, rcDoc, rcConstructing, rcParam, rcNull ); }
    *result = 0;
    if (self && buffer && size) {
        KXMLDoc *obj = (KXMLDoc*) malloc(sizeof(KXMLDoc));
        if (!obj)
        {   return RC ( rcXML, rcDoc, rcConstructing, rcMemory, rcExhausted ); }
        atomic32_set(&obj->refcount, 1);
        obj->parent = self;
        KXMLMgrAddRef(obj->parent);
        obj->doc = 0;
        obj->xpathCtx = 0;

        /* Load XML document */
        obj->doc = xmlParseMemory(buffer, size);

        if (!obj->doc) {
            KXMLDocRelease(obj);
            return RC ( rcXML, rcDoc, rcConstructing, rcXmlDoc, rcInvalid );
        }

        /* Create xpath evaluation context */
        obj->xpathCtx = xmlXPathNewContext(obj->doc);
        if (!obj->xpathCtx) {
            KXMLDocRelease(obj);
            return RC ( rcXML, rcDoc, rcConstructing, rcMemory, rcCorrupt );
        }
        *result = obj;
        rc = 0;
    }
    else {
        rc = RC ( rcXML, rcDoc, rcConstructing, rcParam, rcNull );
    }
    return rc;
}

rc_t KXMLDocAddRef(const KXMLDoc *cself)
{
    if (cself) {
        KXMLDoc *self = (KXMLDoc*) cself;
        atomic32_inc(&self->refcount);
    }
    return 0;
}

rc_t KXMLDocRelease(const KXMLDoc *cself)
{
    if (cself) {
        KXMLDoc *self = (KXMLDoc*) cself;
        if (atomic32_dec_and_test(&self->refcount)) {
            if (self->xpathCtx) {
                xmlXPathFreeContext(self->xpathCtx);
                self->xpathCtx = 0;
            }
            if (self->doc) {
                xmlFreeDoc(self->doc);
                self->doc = 0;
            }
            KXMLMgrRelease(self->parent);
            self->parent = 0;
            free(self);
        }
    }
    return 0;
}

/********* XML node set ********/

struct KXMLNodeset {
    atomic32_t refcount;
    const KXMLDoc *parent;
    char *path;
    xmlXPathObjectPtr xpathObj;
};

/** KXMLNodeset constructor */
static rc_t KXMLNodeset_cTor(const KXMLDoc *parent,
    const KXMLNodeset **result,
    const char *path)
{
    assert(parent && result && path && path[0]);
    if (!parent->xpathCtx) {
        return RC ( rcXML, rcDoc, rcConstructing, rcData, rcIncorrect );
    }
    {
        size_t size;
        KXMLNodeset *obj = (KXMLNodeset*) malloc(sizeof(KXMLNodeset));
        if (!obj) {
            return RC ( rcXML, rcDoc, rcConstructing, rcMemory, rcExhausted );
        }
        atomic32_set(&obj->refcount, 1);
        obj->parent = parent;
        obj->path = 0;
        KXMLDocAddRef(obj->parent);
        obj->xpathObj = xmlXPathEvalExpression(BAD_CAST path, parent->xpathCtx);
        if (!obj->xpathObj) {
            KXMLNodesetRelease(obj);
            return RC ( rcXML, rcDoc, rcConstructing, rcData, rcCorrupt );
        }
        if( obj->xpathObj->type != XPATH_NODESET ) {
            KXMLNodesetRelease(obj);
            return RC(rcXML, rcDoc, rcConstructing, rcData, rcUnsupported );
        }
        obj->path = string_dup(path, string_measure(path, &size));
        if (!obj->path) {
            KXMLNodesetRelease(obj);
            return RC ( rcXML, rcDoc, rcConstructing, rcMemory, rcExhausted );
        }
        *result = obj;
    }
    return 0;
}

static rc_t s_KXML_snprintf(char *str,
    size_t size,
    const char *format,
    ...)
{
    va_list args;
    va_start(args, format);
    {
        rc_t rc = s_KXML_vsnprintf(str, size, format, args);
        va_end(args);
        return rc;
    }
}

#define XPATH_MAX_LEN 1001
/** Opens a node set relative to current node */
rc_t KXMLDocOpenNodesetRead(const KXMLDoc *self,
    const KXMLNodeset **result,
    const char *path,
    ...)
{
    rc_t rc = 0;
    va_list args;
    va_start ( args, path );
    rc = KXMLDocVOpenNodesetRead ( self, result, path, args );
    va_end ( args );
    return rc;
}

rc_t KXMLDocVOpenNodesetRead(const KXMLDoc *self,
    const KXMLNodeset **result,
    const char *path,
    va_list args)
{
    rc_t rc = 0;

    if (!result || !path || !path[0]) {
        return RC ( rcXML, rcDoc, rcConstructing, rcParam, rcNull );
    }

    *result = 0;

    if (self) {
        char buffer[XPATH_MAX_LEN];
        rc = s_KXML_vsnprintf(buffer, sizeof(buffer), path, args);
        if (rc != 0) {
            return rc;
        }
        rc = KXMLNodeset_cTor(self, result, buffer);
    }
    else { rc = RC ( rcXML, rcDoc, rcConstructing, rcSelf, rcNull ); }

    return rc;
}

rc_t KXMLNodesetAddRef(const KXMLNodeset *cself)
{
    if (cself) {
        KXMLNodeset *self = (KXMLNodeset*) cself;
        atomic32_inc(&self->refcount);
    }
    return 0;
}

rc_t KXMLNodesetRelease(const KXMLNodeset *cself)
{
    if (cself) {
        KXMLNodeset *self = (KXMLNodeset*) cself;
        if (atomic32_dec_and_test(&self->refcount)) {
            if (self->xpathObj) {
                xmlXPathFreeObject(self->xpathObj);
                self->xpathObj = 0;
            }
            KXMLDocRelease(self->parent);
            self->parent = 0;
            if (self->path) {
                free(self->path);
                self->path = 0;
            }
            free(self);
        }
    }
    return 0;
}

rc_t KXMLNodesetCount(const KXMLNodeset *self,
    uint32_t *result)
{
    if (!result)
    { return RC ( rcXML, rcDoc, rcListing, rcParam, rcNull ); }

    *result = 0;

    if (self && self->xpathObj) {
        const xmlNodeSetPtr nodes = self->xpathObj->nodesetval;
        if (nodes)
        { *result = nodes->nodeNr; }
        return 0;
    }
    else {
        return RC ( rcXML, rcDoc, rcAccessing, rcSelf, rcNull );
    }
}

/********* XML node ********/

struct KXMLNode {
    const KXMLNodeset *parent;
    /* use parent's refcount; */

    /* parent and index are used to find a node within a nodeset
    (when children == 0) */
    int32_t idx;

    /* when children != 0 then children is an XML node,
    path is node's path relative to parent's path */
    const struct _xmlNode *children;
    char *path;
};

static rc_t s_KXMLNode_cTor(const KXMLNodeset *self,
    const KXMLNode **result,
    uint32_t idx,
    const struct _xmlNode *children,
    char *path)
{
    KXMLNode *obj = (KXMLNode*) malloc(sizeof(KXMLNode));
    if (!obj) {
        return RC ( rcXML, rcDoc, rcAccessing, rcMemory, rcExhausted );
    }
    obj->parent = self;
    obj->path = 0;
    obj->children = children;
    if (children) {
        size_t size = 0;
        size_t path_size;
        if (path)
        {   size += string_measure(path, &path_size) + 1; }
        if (children->name)
        {   
            size_t sz;
            size += string_measure((char*)children->name, &sz); 
        }
        obj->path = (char*)malloc(size + 1);
        if (!obj->path) {
            free(obj);
            return RC ( rcXML, rcDoc, rcAccessing, rcMemory, rcExhausted );
        }
        *obj->path = 0;
        if (path && path[0]) {
            string_copy(obj->path, size + 1, path, path_size);
            strcat(obj->path, "/");
        }
        strcat(obj->path, (char*)children->name);
    }
    KXMLNodesetAddRef(obj->parent);
    obj->idx = idx;
    *result = obj;
    return 0;
}

/* GetName
 */
rc_t KXMLNodeGetName ( const KXMLNode *self, const char **name ) {
    if (!self)
    {   return RC ( rcXML, rcNode, rcAccessing, rcSelf, rcNull ); }
    if (!name)
    {   return RC ( rcXML, rcNode, rcAccessing, rcParam, rcNull ); }
    *name = 0;
    if (!self->children) {
        assert(self->parent);
        *name = self->parent->path;
        /*TODO
        here name if not the last node name but absolute name from xml root*/
    } else {
        *name = (const char*)self->children->name;
    }
    return 0;
}

rc_t KXMLNodeElementName ( const KXMLNode *self, const char **name )
{
    if( self == NULL ) {
        return RC ( rcXML, rcNode, rcAccessing, rcSelf, rcNull );
    }
    if( name == NULL ) {
        return RC ( rcXML, rcNode, rcAccessing, rcParam, rcNull );
    }
    *name = NULL;
    if( self->children == NULL ) {
        *name = (const char*)self->parent->xpathObj->nodesetval->nodeTab[self->idx]->name;
    } else {
        *name = (const char*)self->children->name;
    }
    return 0;
}

rc_t KXMLNodesetGetNodeRead(const KXMLNodeset *self,
    const KXMLNode **result,
    uint32_t idx)
{
    rc_t rc = 0;
    uint32_t count = 0;

    if (!result)
    { return RC ( rcXML, rcNode, rcAccessing, rcParam, rcNull ); }

    *result = 0;

    if (self) {
        rc = KXMLNodesetCount(self, &count);
        if (rc == 0) {
            assert(count >= 0);
            if (count == 0) {
            /* TODO: this test should be made when KXMLNodeset is created */
                rc = RC(rcXML, rcNode, rcAccessing, rcNode, rcNotFound);
            }
            else if (idx < count) {
                rc = s_KXMLNode_cTor(self, result, idx, NULL, NULL);
            }
            else {
                rc = RC (rcXML, rcNode, rcAccessing, rcParam, rcIncorrect);
            }
        }
    }
    else {
        rc = RC ( rcXML, rcDoc, rcAccessing, rcSelf, rcCorrupt );
    }
    return rc;
}

/** Opens a node set relative to current node */
rc_t KXMLNodeVOpenNodesetRead ( const KXMLNode *self,
    struct KXMLNodeset const **result, const char *path, va_list args )
{
    rc_t rc = 0;

    if (!result || !path || !path[0])
    { return RC ( rcXML, rcNode, rcAccessing, rcParam, rcNull ); }

    *result = 0;

    if (self == NULL)
    {  return RC ( rcXML, rcNode, rcAccessing, rcSelf, rcNull );}

    if (self && self->parent && self->parent->path) {
        char *newPath = 0;
        char buffer[XPATH_MAX_LEN];
        char outpath[XPATH_MAX_LEN];
        rc = s_KXML_vsnprintf(outpath, sizeof(outpath), path, args);
        if (rc != 0)
        { return rc; }

        if (path[0] == '/')
        { newPath = outpath; }
        else {
            size_t size;
            if ((string_measure(self->parent->path, &size) + 1 + string_measure(outpath, &size) + 1 + 3)
                > XPATH_MAX_LEN)
            {
                /* buffer is too small */
                return RC
                    ( rcXML, rcNode, rcAccessing, rcBuffer, rcExhausted );
            }
            if( self->parent->path[0] == '/' && self->parent->path[1] == '\0' ) {
                /* correctly construct child path if parent path is "/" */
                rc = s_KXML_snprintf(buffer, sizeof(buffer), "/%s", outpath);
            } else {
                rc = s_KXML_snprintf(buffer, sizeof(buffer), "(%s)[%d]/%s", self->parent->path, self->idx + 1, outpath);
            }
            newPath = buffer;
        }
        assert(self->parent && self->parent->parent);
        rc = KXMLNodeset_cTor(self->parent->parent, result, newPath);
    }
    else { rc = RC ( rcXML, rcNode, rcAccessing, rcSelf, rcCorrupt ); }

    return rc;
}

/** Opens a node set relative to current node */
rc_t KXMLNodeOpenNodesetRead(const KXMLNode *self,
    const KXMLNodeset **result,
    const char *path,
    ...)
{
    rc_t rc = 0;
    va_list args;
    va_start ( args, path );
    rc = KXMLNodeVOpenNodesetRead ( self, result, path, args );
    va_end ( args );
    return rc;
}

rc_t KXMLNodeRelease(const KXMLNode *cself) {
    if (cself) {
        KXMLNode *self = (KXMLNode*) cself;
        KXMLNodesetRelease(self->parent);
        free(self->path);
        free(self);
    }
    return 0;
}

/** Reads node's value.
remaining can be NULL */
/* Implementation:
Iterates over all XML_TEXT_NODE children starting from firstChild.
Uses state machine.

Initial state is eNotFound.
First it skips all nodes until their cumulative size is < offset;
Then it copies the [part of] the first node considering offset:
if the buffer is filled then state changes to eFilled,
otherwise to eStarted.

If the state is eStarted: node is added to buffer.
State remains eStarted or changes to eFilled.

If the state is eFilled then remaining value is added up. */
int s_KXMLNode_readTextNode(const xmlNodePtr firstChild,
    void *buffer,
    size_t bsize,
    size_t *num_read,
    size_t *remaining,
    size_t offset)
{
    enum EState {
        eNotFound,
        eStarted,
        eFilled
    } state = eNotFound;

    size_t head = 0;
    size_t copied = 0;
    size_t remains = 0;
    xmlNodePtr node = firstChild;

    if (!firstChild)
    {   return RC ( rcXML, rcNode, rcReading, rcParam, rcNull ); }

    assert(num_read);

    while (node) {
        if (node->type == XML_TEXT_NODE) {
            char* content = (char*) node->content;
            size_t size;
            size_t chunkSz = string_measure(content, &size);
            switch (state) {
                case eNotFound: {
                    if (offset < head + chunkSz) {
                        size_t chunkOffset = offset - head;
                        size_t readySz = chunkOffset + chunkSz;
                        size_t size = readySz;
                        if (readySz >= (bsize - copied)) {
                            size = bsize - copied;
                            state = eFilled;
                        }
                        else {
                            state = eStarted;
                        }
                        if (size) {
                            assert(buffer);
                            string_copy((char*) buffer + copied, bsize - copied, 
                                content + chunkOffset, size);
                        }
                        copied += size;
                        if (state == eFilled) {
                            remains = readySz - size;
                        }
                    }
                    break;
                }
                case eStarted: {
                    size_t chunkOffset = 0;
                    size_t readySz = chunkSz;
                    size_t size = readySz;
                    if (readySz >= (bsize - copied)) {
                        size = bsize - copied;
                        state = eFilled;
                    }
                    if (size) {
                        assert(buffer);
                        string_copy((char*) buffer + copied, bsize - copied,
                            content + chunkOffset, size);
                    }
                    copied += size;
                    if (state == eFilled) {
                        remains = readySz - size;
                    }
                    break;
                }
                case eFilled:
                    remains += chunkSz;
                    break;
                default:
                    assert(0);
                    break;
            }
        }
        node = node->next;
    }

    *num_read = copied;

    if (remaining)
    { *remaining = remains; }

    return 0;
}

/** Reads XML Node value
    remaining can be NULL.
Can have bsize parameter equal to 0:
can be used to find out the node value length.
*/
rc_t KXMLNodeRead(const KXMLNode *self,
    size_t offset,
    void *buffer,
    size_t bsize,
    size_t *num_read,
    size_t *remaining)
{
    rc_t rc = 0;

    if (!num_read)
    { return RC ( rcXML, rcNode, rcReading, rcParam, rcNull ); }
    if (bsize && !buffer)
    { return RC ( rcXML, rcNode, rcReading, rcParam, rcNull ); }

    *num_read = 0;

    if (remaining)
    { *remaining = 0; }

    if (self) {
        const struct _xmlNode *children = NULL;
        if (self->children) {
            /*the case when (KXMLNode::children != NULL) is implemented here*/
            if (self->children->children)
            { children = self->children->children; }
        }
        else if (self->parent && self->parent->xpathObj) {
            xmlNodePtr node = NULL;
            int32_t idx = self->idx;
            const xmlXPathObjectPtr xpathObj = self->parent->xpathObj;
            assert(xpathObj->nodesetval && idx < xpathObj->nodesetval->nodeNr);
            node = xpathObj->nodesetval->nodeTab[idx];
            if (node && node->type == XML_ELEMENT_NODE) {
                children = node->children;
            }
            rc = 0;
        }
        if (children != NULL) {
            return s_KXMLNode_readTextNode( ( xmlNodePtr )
                children, buffer, bsize, num_read, remaining, offset);
        }
    }
    else { rc = RC ( rcXML, rcNode, rcReading, rcNode, rcCorrupt ); }

    return rc;
}


#define MAX_I16      0x8000 /* 32768 */
#define MAX_U16     0x10000
#define MAX_I32  0x80000000
#define MAX_U32 0x100000000

static
rc_t s_KXMLNodeReadNodeOrAttrCString(const KXMLNode *self,
    char *buffer, size_t bsize, size_t *size, const char *attr)
{
    if (attr)
    { return KXMLNodeReadAttrCString(self, attr, buffer, bsize, size); }
    else { return KXMLNodeReadCString(self, buffer, bsize, size); }
}

static
rc_t s_KXMLNodeReadNodeOrAttrAs_long(const KXMLNode *self, long *l,
    const char *attr)
{
    rc_t rc;
    size_t num_read;
    char buffer [ 256 ];

    assert ( l );

    rc = s_KXMLNodeReadNodeOrAttrCString
        ( self, buffer, sizeof buffer, & num_read, attr );

    if ( rc == 0 )
    {
        char *end;
        *l = strtol ( buffer, & end, 0 );
        if ( end [ 0 ] != 0 )
            rc = RC ( rcXML, rcNode, rcReading, rcType, rcIncorrect );
    }

    return rc;
}

static
rc_t s_KXMLNodeReadNodeOrAttrAs_ulong(const KXMLNode *self, unsigned long *l,
    const char *attr)
{
    rc_t rc;
    size_t num_read;
    char buffer [ 256 ];

    assert ( l );

    rc = s_KXMLNodeReadNodeOrAttrCString
        ( self, buffer, sizeof buffer, & num_read, attr );

    if ( rc == 0 )
    {
        char *end;
        *l = strtoul ( buffer, & end, 0 );

        if ( end [ 0 ] != 0 )
        {
    #if 1
            rc = RC ( rcXML, rcNode, rcReading, rcType, rcIncorrect );
    #else
            if ( end [ 0 ] != '-' && end [ 0 ] != '+' )
                rc = RC ( rcXML, rcNode, rcReading, rcType, rcIncorrect );
            else
            {
                /* read signed, cast to unsigned... ? */
            }
    #endif
        }
    }

    return rc;
}

/* ReadAs ( formatted )
 *  reads as integer or float value in native byte order
 *  casts smaller-sized values to desired size, e.g.
 *    uint32_t to uint64_t
 *
 *  "i" [ OUT ] - return parameter for signed integer
 *  "u" [ OUT ] - return parameter for unsigned integer
 *  "f" [ OUT ] - return parameter for double float
 */
static
rc_t s_KXMLNodeReadNodeOrAttrAsI16 ( const KXMLNode *self, int16_t *i,
    const char *attr )
{
    rc_t rc = 0;
    long val = 0;

    if ( i == NULL )
        return RC ( rcXML, rcNode, rcReading, rcParam, rcNull );

    rc = s_KXMLNodeReadNodeOrAttrAs_long ( self, &val, attr );

    if ( rc == 0 )
    {
        if ( val < -32768  || val >= 32768 )
            rc = RC ( rcXML, rcNode, rcReading, rcRange, rcExcessive );
        else
            * i = ( int16_t ) val;
    }

    return rc;
}

rc_t KXMLNodeReadAsI16 ( const KXMLNode *self, int16_t *i )
{ return s_KXMLNodeReadNodeOrAttrAsI16(self, i, NULL); }
static 
rc_t s_KXMLNodeReadNodeOrAttrAsU16 ( const KXMLNode *self, uint16_t *u,
    const char *attr )
{
    rc_t rc = 0;
    unsigned long val = 0;

    if ( u == NULL )
        return RC ( rcXML, rcNode, rcReading, rcParam, rcNull );

    rc = s_KXMLNodeReadNodeOrAttrAs_ulong(self, &val, attr);

    if ( rc == 0 )
    {
        if ( val >= 0x10000 )
            rc = RC ( rcXML, rcNode, rcReading, rcRange, rcExcessive );
        else
            * u = ( uint16_t ) val;
    }

    return rc;
}

rc_t KXMLNodeReadAsU16 ( const KXMLNode *self, uint16_t *u )
{ return s_KXMLNodeReadNodeOrAttrAsU16(self, u, NULL); }
static
rc_t s_KXMLNodeReadNodeOrAttrAsI32 ( const KXMLNode *self, int32_t *i,
    const char *attr )
{
    rc_t rc = 0;
    long val = 0;

    if ( i == NULL )
        return RC ( rcXML, rcNode, rcReading, rcParam, rcNull );

    rc = s_KXMLNodeReadNodeOrAttrAs_long ( self, &val, attr );

    if ( rc == 0 )
    {
        * i = ( int32_t ) val;
        if ( ( long ) ( * i ) != val )
            rc = RC ( rcXML, rcNode, rcReading, rcRange, rcExcessive );
    }

    return rc;
}

rc_t KXMLNodeReadAsI32 ( const KXMLNode *self, int32_t *i )
{ return s_KXMLNodeReadNodeOrAttrAsI32(self, i, NULL); }
rc_t s_KXMLNodeReadNodeOrAttrAsU32 ( const KXMLNode *self, uint32_t *u,
    const char *attr )
{
    rc_t rc = 0;
    unsigned long val = 0;

    if ( u == NULL )
        return RC ( rcXML, rcNode, rcReading, rcParam, rcNull );

    rc = s_KXMLNodeReadNodeOrAttrAs_ulong(self, &val, attr);

    if ( rc == 0 )
    {
        * u = ( uint32_t ) val;
        if ( ( unsigned long ) ( * u ) != val )
            rc = RC ( rcXML, rcNode, rcReading, rcRange, rcExcessive );
    }

    return rc;
}
rc_t KXMLNodeReadAsU32 ( const KXMLNode *self, uint32_t *u )
{ return s_KXMLNodeReadNodeOrAttrAsU32(self, u, NULL); }
static
rc_t s_KXMLNodeReadNodeOrAttrAsI64 ( const KXMLNode *self, int64_t *i,
    const char *attr )
{
    rc_t rc;

    if ( i == NULL )
        rc = RC ( rcXML, rcNode, rcReading, rcParam, rcNull );
    else
    {
        size_t num_read;
        char buffer [ 256 ];

        rc = s_KXMLNodeReadNodeOrAttrCString
            ( self, buffer, sizeof buffer, & num_read, attr );

        if ( rc == 0 )
        {
            char *end;
            int64_t val = strtoi64 ( buffer, & end, 0 );

            if ( end [ 0 ] != 0 )
                rc = RC ( rcXML, rcNode, rcReading, rcType, rcIncorrect );
            else
                * i = val;
        }
    }

    return rc;
}
rc_t KXMLNodeReadAsI64 ( const KXMLNode *self, int64_t *i )
{ return s_KXMLNodeReadNodeOrAttrAsI64(self, i, NULL); }

rc_t s_KXMLNodeReadNodeOrAttrAsU64 ( const KXMLNode *self, uint64_t *u,
    const char *attr )
{
    rc_t rc;

    if ( u == NULL )
        rc = RC ( rcXML, rcNode, rcReading, rcParam, rcNull );
    else
    {
        size_t num_read;
        char buffer [ 256 ];

        rc = s_KXMLNodeReadNodeOrAttrCString
            ( self, buffer, sizeof buffer, & num_read, attr );

        if ( rc == 0 )
        {
            char *end;
            uint64_t val = strtou64 ( buffer, & end, 0 );

            if ( end [ 0 ] != 0 )
            {
        #if 1
                rc = RC ( rcXML, rcNode, rcReading, rcType, rcIncorrect );
        #else
                if ( end [ 0 ] != '-' && end [ 0 ] != '+' )
                    rc = RC ( rcXML, rcNode, rcReading, rcType, rcIncorrect );
                else
                {
                    /* read signed, cast to unsigned... ? */
                    /* FYI
                       centos64: strtou64("-1") = 0xffff...
                       errno = 1; end[0] = '\0' */
                }
        #endif
            }
            else
                * u = val;
        }
    }

    return rc;
}

rc_t KXMLNodeReadAsU64 ( const KXMLNode *self, uint64_t *u )
{ return s_KXMLNodeReadNodeOrAttrAsU64(self, u, NULL); }
rc_t s_KXMLNodeReadNodeOrAttrAsF64 ( const KXMLNode *self, double *f,
    const char *attr )
{
    rc_t rc;

    if ( f == NULL )
        rc = RC ( rcXML, rcNode, rcReading, rcParam, rcNull );
    else
    {
        size_t num_read;
        char buffer [ 256 ];

        rc = s_KXMLNodeReadNodeOrAttrCString
            ( self, buffer, sizeof buffer, & num_read, attr );

        if ( rc == 0 )
        {
            char *end;
            * f = strtod ( buffer, & end );
            if ( end [ 0 ] != 0 )
                rc = RC ( rcXML, rcNode, rcReading, rcType, rcIncorrect );
        }
    }

    return rc;
}
rc_t KXMLNodeReadAsF64 ( const KXMLNode *self, double *f )
{ return s_KXMLNodeReadNodeOrAttrAsF64(self, f, NULL); }

/* Read ( formatted )
 *  reads as C-string
 *
 *  "buffer" [ OUT ] and "bsize" [ IN ] - output buffer for
 *  NUL terminated string.
 *
 *  "size" [ OUT ] - return parameter giving size of string
 *  not including NUL byte. the size is set both upon success
 *  and insufficient buffer space error.
 */
rc_t KXMLNodeReadCString ( const KXMLNode *self,
    char *buffer, size_t bsize, size_t *size )
{
    rc_t rc;

    if ( size == NULL )
        rc = RC ( rcXML, rcNode, rcReading, rcParam, rcNull );
    else
    {
        size_t remaining;
        rc = KXMLNodeRead ( self, 0, buffer, bsize, size, & remaining );
        if ( rc == 0 )
        {
            if ( * size == bsize )
            {
                rc = RC ( rcXML, rcNode, rcReading, rcBuffer, rcInsufficient );
                * size += remaining;
            }
            else
            {
                buffer [ * size ] = 0;
            }
        }
    }

    return rc;
}

rc_t KXMLNodeReadCStr( const KXMLNode *self, char** str, const char* default_value)
{
    rc_t rc = 0;

    if( self == NULL || str == NULL ) {
        rc = RC( rcXML, rcNode, rcReading, rcParam, rcNull);
    } else { 
        char b[10240];
        size_t to_read = sizeof(b) - 1, nread = 0;
        
        *str = NULL;
        if( (rc = KXMLNodeReadCString(self, b, to_read, &nread)) == 0 ) {
            if( nread == 0 && default_value != NULL ) {
                size_t size;
                *str = string_dup(default_value, string_measure(default_value, &size));
            } else {
                *str = string_dup(b, nread);
            }
            if( *str == NULL ) {
                rc = RC(rcXML, rcNode, rcReading, rcMemory, rcInsufficient);
            }
        }
    }
    return rc;
}


/** Reads XML Node attribute value
attr: attribute name
    remaining can be NULL.
Can have bsize parameter equal to 0:
can be used to find out the attribute value length.
*/
rc_t KXMLNodeReadAttr(const KXMLNode *self,
     const char *attr,
     void *buffer,
     size_t bsize,
     size_t *num_read,
     size_t *remaining)
{
    rc_t status = 0;
    struct _xmlAttr* properti = NULL;
    if (!attr || !num_read) {
        return RC ( rcXML, rcAttr, rcReading, rcParam, rcNull );
    }
    if (bsize && !buffer)
    {   return RC ( rcXML, rcNode, rcReading, rcParam, rcNull ); }
    *num_read = 0;
    if (remaining) {
        *remaining = 0;
    }
    if (!self)
    {   return RC ( rcXML, rcAttr, rcReading, rcSelf, rcCorrupt ); }
    if (self->children) {
        properti = self->children->properties;
    }
    else if (self->parent && self->parent->xpathObj) {
        int32_t idx = self->idx;
        const xmlXPathObjectPtr xpathObj = self->parent->xpathObj;
        assert(xpathObj->nodesetval && idx < xpathObj->nodesetval->nodeNr);
        {
            xmlNodePtr node = xpathObj->nodesetval->nodeTab[idx];
            if (node && node->type == XML_ELEMENT_NODE)
            {   properti = node->properties; }
        }
    }
    else {
        status = RC ( rcXML, rcAttr, rcReading, rcSelf, rcCorrupt );
    }
    if (properti) {
        while (properti) {
            if (!xmlStrcmp(BAD_CAST attr, properti->name)) {
                return s_KXMLNode_readTextNode(properti->children,
                    buffer, bsize, num_read, remaining, 0);
            }
            properti = properti->next;
        }
    }
    status = RC ( rcXML, rcAttr, rcReading, rcAttr, rcNotFound );
    return status;
}


/* ReadAttrAs ( formatted )
 *  reads as integer or float value in native byte order
 *  casts smaller-sized values to desired size, e.g.
 *    uint32_t to uint64_t
 *
 *  "i" [ OUT ] - return parameter for signed integer
 *  "u" [ OUT ] - return parameter for unsigned integer
 *  "f" [ OUT ] - return parameter for double float
 */
rc_t KXMLNodeReadAttrAsI16
    ( const KXMLNode *self, const char *attr, int16_t *i )
{ return s_KXMLNodeReadNodeOrAttrAsI16(self, i, attr); }

rc_t KXMLNodeReadAttrAsU16
    ( const KXMLNode *self, const char *attr, uint16_t *u )
{ return s_KXMLNodeReadNodeOrAttrAsU16(self, u, attr); }

rc_t KXMLNodeReadAttrAsI32 ( const KXMLNode *self,
    const char *attr, int32_t *i )
{ return s_KXMLNodeReadNodeOrAttrAsI32(self, i, attr); }

rc_t KXMLNodeReadAttrAsU32
    ( const KXMLNode *self, const char *attr, uint32_t *u )
{ return s_KXMLNodeReadNodeOrAttrAsU32(self, u, attr); }

rc_t KXMLNodeReadAttrAsI64
    ( const KXMLNode *self, const char *attr, int64_t *i )
{ return s_KXMLNodeReadNodeOrAttrAsI64(self, i, attr); }

rc_t KXMLNodeReadAttrAsU64
    ( const KXMLNode *self, const char *attr, uint64_t *u )
{ return s_KXMLNodeReadNodeOrAttrAsU64(self, u, attr); }

rc_t KXMLNodeReadAttrAsF64 ( const KXMLNode *self, const char *attr, double *f )
{ return s_KXMLNodeReadNodeOrAttrAsF64(self, f, attr); }


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
rc_t KXMLNodeReadAttrCString ( const KXMLNode *self, const char *attr,
    char *buffer, size_t bsize, size_t *size )
{
    rc_t rc;

    if ( size == NULL )
        rc = RC ( rcXML, rcAttr, rcReading, rcParam, rcNull );
    else
    {
        size_t remaining;
        rc = KXMLNodeReadAttr ( self, attr, buffer, bsize, size, & remaining );
        if ( rc == 0 )
        {
            if ( * size == bsize )
            {
                rc = RC ( rcXML, rcAttr, rcReading, rcBuffer, rcInsufficient );
                * size += remaining;
            }
            else
            {
                buffer [ * size ] = 0;
            }
        }
    }

    return rc;
}

rc_t KXMLNodeReadAttrCStr( const KXMLNode *self, const char *attr, char** str, const char* default_value)
{
    rc_t rc = 0;

    if( self == NULL || attr == NULL || str == NULL ) {
        rc = RC( rcXML, rcNode, rcReading, rcParam, rcNull);
    } else { 
        char b[10240];
        size_t to_read = sizeof(b) - 1, nread = 0;
        
        *str = NULL;
        if( (rc = KXMLNodeReadAttrCString(self, attr, b, to_read, &nread)) == 0 ) {
            if( nread == 0 && default_value != NULL ) {
                size_t size;
                *str = string_dup(default_value, string_measure(default_value, &size));
            } else {
                *str = string_dup(b, nread);
            }
        } else if( GetRCState(rc) == rcNotFound && default_value != NULL ) {
            size_t size;
            *str = string_dup(default_value, string_measure(default_value, &size));
            rc = 0;
        }
        if( rc == 0 && *str == NULL ) {
            rc = RC(rcXML, rcNode, rcReading, rcMemory, rcInsufficient);
        }
    }
    return rc;
}


struct s_KNodeNamelist {
    KNamelist dad;
    struct _xmlAttr* properties;
    struct _xmlNode* children;
};
typedef struct s_KNodeNamelist s_KNodeNamelist;
static rc_t KNodeNamelistDestroy (s_KNodeNamelist *self)
{
    free(self);
    return 0;
}

static rc_t s_LibXmlCount(uint32_t *count,
    const struct _xmlNode* children,
    const struct _xmlAttr* properties)
{
    assert(count);

    *count = 0;

    /* It neither of properties nor children exists
    then this node does not have children */

    if (properties) {
        while (properties) {
            ++(*count);
            properties = properties->next;
        }
    }
    else if (children) {
        while (children) {
            if (children->type == XML_ELEMENT_NODE) {
                ++(*count);
            }
            children = children->next;
        }
    }

    return 0;
}

static rc_t KNodeNamelistCount(const s_KNodeNamelist *self,
    uint32_t *count)
{
    if (!count) {
        return RC ( rcXML, rcNamelist, rcListing, rcParam, rcNull );
    }

    return s_LibXmlCount(count, self->children, self->properties);
}

static rc_t s_LibXmlGetNode(const struct _xmlNode** result,
    uint32_t idx,
    const struct _xmlNode* children)
{
    uint32_t count = 0;
    while (children) {
        if (children->type == XML_ELEMENT_NODE) {
            if (count == idx) {
                *result = children;
                return 0;
            }
            ++count;
        }
        children = children->next;
    }
    return RC ( rcXML, rcNode, rcReading, rcIndex, rcNotFound );
}

/* TODO:
quick and dirty version: bad style and code copying: has to be refactored */
rc_t KXMLNodeGetNodeRead ( const KXMLNode *self,
    const KXMLNode **result,
    uint32_t idx )
{
    if (!self)
    {   return RC ( rcXML, rcNode, rcReading, rcSelf, rcNull ); }
    if (!result)
    {   return RC ( rcXML, rcNode, rcReading, rcParam, rcNull ); }

    *result = 0;

    {
        uint32_t count = 0;
        rc_t rc = KXMLNodeCountChildNodes(self, &count);
        if (rc)
        {   return rc; }
        if (idx >= count)
        {   return RC ( rcXML, rcNode, rcCreating, rcIndex, rcInvalid ); }

        {
            const struct _xmlNode *resultChildren = 0;
            const struct _xmlNode *children = self->children;
            if (children) {
                children = children->children;
                assert(children);
            }
            else {
                if (self && self->parent && self->parent->xpathObj) {
                    const xmlNodeSetPtr nodesetval
                        = self->parent->xpathObj->nodesetval;
                    if (nodesetval) {
                        if ((nodesetval->nodeNr > 0)
                            && nodesetval->nodeTab)
                        {
                            if (nodesetval->nodeNr <= self->idx) {
                                return RC ( rcXML, rcNode, rcCreating,
                                    rcSelf, rcCorrupt );
                            }
                            if (nodesetval->nodeTab[self->idx]) {
                                children
                                    = nodesetval->nodeTab[self->idx]->children;
                            /*parent node for our requested parent->child[idx]*/
                                assert(children);
                            }
                            else {
                                return RC ( rcXML, rcNode, rcCreating,
                                    rcSelf, rcCorrupt );
                            }
                        }
                        else {
                            return RC ( rcXML, rcNode, rcCreating,
                                rcSelf, rcCorrupt );
                        }
                    } else {
                        return RC(rcXML, rcNode, rcCreating, rcSelf, rcCorrupt);
                    }
                } else {
                    return RC ( rcXML, rcNode, rcCreating, rcSelf, rcCorrupt );
                }
            }
            rc = s_LibXmlGetNode(&resultChildren, idx, children);
            if (rc)
            {   return rc; }
            rc = s_KXMLNode_cTor
                (self->parent, result, 0, resultChildren, self->path);
            return rc;
        }
    }
}

static rc_t KNodeNamelistGet(const s_KNodeNamelist *self,
    uint32_t idx,
    const char **name)
{
    uint32_t count = 0;
    if (self->properties) {
        struct _xmlAttr* properties = self->properties;
        while (properties) {
            if (count == idx) {
                *name = (const char*)properties->name;
                return 0;
            }
            ++count;
            properties = properties->next;
        }
    }
    else if (self->children) {
        const struct _xmlNode *result = 0;
        rc_t rc = s_LibXmlGetNode(&result, idx, self->children);
        if (rc)
        { return rc; }
        *name = (const char*)result->name;
        return 0;
    }
    return RC ( rcXML, rcNode, rcReading, rcIndex, rcNotFound );
}

static KNamelist_vt_v1 vtKNodeNamelist = {
    /* version 1.0 */
    1, 0,

    /* start minor version 0 methods */
    KNodeNamelistDestroy,
    KNodeNamelistCount,
    KNodeNamelistGet
    /* end minor version 0 methods */
};

enum EKNodeNamelistType {
    eAttr,
    eNode
};

/** KAttrNamelist factory method */
static int s_KXMLNode_createKNodeNamelist(const KXMLNode *self,
    const KNamelist **list,
    uint32_t type)
{
    /*TODO it seems
    that the case when (KXMLNode::children != NULL) is not implemented*/
    rc_t status = 0;
    *list = 0;
    if (self && self->parent && self->parent->xpathObj) {
        s_KNodeNamelist* obj
            = (s_KNodeNamelist*) malloc(sizeof(s_KNodeNamelist));
        if (!obj) {
            return RC ( rcXML, rcNamelist, rcCreating, rcMemory, rcExhausted );
        }
        status
            = KNamelistInit(&obj->dad, (const KNamelist_vt*) &vtKNodeNamelist);
        if (status != 0) {
            free(obj);
            return status;
        }
        obj->properties = 0;
        obj->children = 0;
        *list = &obj->dad;
        if( self->children ) {
            switch (type) {
                case eAttr:
                    obj->properties = self->children->properties;
                    break;
                case eNode:
                    obj->children = self->children->children;
                    break;
                default:
                    assert(0);
                    break;
            }
        } else {
            const xmlNodeSetPtr nodesetval = self->parent->xpathObj->nodesetval;
            if (nodesetval) {
                if ((nodesetval->nodeNr > 0) && nodesetval->nodeTab) {
                    if (nodesetval->nodeNr <= self->idx) {
                        free(obj);
                        *list = 0;
                        return RC
                            (rcXML, rcNamelist, rcCreating, rcSelf, rcCorrupt);
                    }
                    if (nodesetval->nodeTab[self->idx]) {
                        switch (type) {
                            case eAttr:
                                obj->properties =
                                    nodesetval->nodeTab[self->idx]->properties;
                                break;
                            case eNode:
                                obj->children =
                                    nodesetval->nodeTab[self->idx]->children;
                                break;
                            default:
                                assert(0);
                                break;
                        }
                    }
                }
            }
        }
        status = 0;
    }
    else {
        status = RC ( rcXML, rcNamelist, rcCreating, rcSelf, rcCorrupt );
    }
    return status;
}
rc_t KXMLNodeListAttr(const KXMLNode *self,
    const KNamelist **names)
{
    return s_KXMLNode_createKNodeNamelist(self, names, eAttr);
}

rc_t KXMLNodeListChild(const KXMLNode *self,
    const KNamelist **names)
{
    return s_KXMLNode_createKNodeNamelist(self, names, eNode);
}

rc_t KXMLNodeCountChildNodes(const KXMLNode *self,
    uint32_t *count)
{
    if (!self)
    {   return RC ( rcXML, rcNode, rcListing, rcSelf, rcNull ); }
    if (!count)
    {   return RC ( rcXML, rcNode, rcListing, rcParam, rcNull ); }
    *count = 0;
    if (self && self->parent && self->parent->xpathObj) {
        const xmlNodeSetPtr nodesetval = self->parent->xpathObj->nodesetval;
        if (nodesetval) {
            if ((nodesetval->nodeNr > 0) && nodesetval->nodeTab) {
                if (nodesetval->nodeNr <= self->idx) {
                    return RC(rcXML, rcNamelist, rcCreating, rcSelf, rcCorrupt);
                }
                if (nodesetval->nodeTab[self->idx]) {
                    if (!self->children) {
                        return s_LibXmlCount(
                            count, nodesetval->nodeTab[self->idx]->children, 0);
                    } else { 
                        /*assert(0); // not implemented*/
                        if (!self->children->children)
        /* 'self->children'
         is a node that should contain its children in self->children->children.
         If (self->children->children == NULL) then the node has no child. */
                        {   return 0; }
                        else {
                            return s_LibXmlCount
                                (count, self->children->children, 0);
                        }
                    }
                }
            }
        }
    }
    return RC ( rcXML, rcNamelist, rcCreating, rcSelf, rcCorrupt );
}

/* GetFirstChildNodeRead
 *  Returns the first(with index 0) sub-node of self using path.
 *  Is equivalent to:
 *          KXMLNodeOpenNodesetRead(self, &nc, path, ...);
 *          KXMLNodesetGetNodeRead(ns, node, 0);
 */
rc_t KXMLNodeVGetFirstChildNodeRead ( const KXMLNode *self,
    const KXMLNode **node, const char *path, va_list args )
{
    rc_t rc = 0;

    struct KXMLNodeset const *ns = NULL;

    if (node == NULL) {
        return RC(rcXML, rcNode, rcReading, rcParam, rcNull);
    }

    *node = NULL;

    rc = KXMLNodeVOpenNodesetRead(self, &ns, path, args);

    if (rc == 0) {
        rc = KXMLNodesetGetNodeRead(ns, node, 0);
        KXMLNodesetRelease(ns);
    }

    return rc;
}

rc_t KXMLNodeGetFirstChildNodeRead ( const KXMLNode *self,
    const KXMLNode **node, const char *path, ... )
{
    rc_t rc;

    va_list args;
    va_start(args, path);
    rc = KXMLNodeVGetFirstChildNodeRead(self, node, path, args);
    va_end(args);

    return rc;
}

/* EOF */
