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
#ifndef _h_sra_fuse_xml_
#define _h_sra_fuse_xml_

#include <kxml/xml.h>
#include <kfs/directory.h>

#include "node.h"

typedef uint32_t EXMLValidate;
enum {
    eXML_NoCheck = 0,
    eXML_NoFail,
    eXML_Full
};

rc_t XML_Make(KDirectory* dir, const char* const work_dir, const char* xml_path,
              unsigned int sync, EXMLValidate xml_validate);

void XML_Init(void);

rc_t XML_FindLock(const char* path, bool recur, const FSNode** node, const char** subpath);

void XML_FindRelease(void);

rc_t XML_MgrGet(const KXMLMgr** xmlmgr);

rc_t XML_ParseTimestamp(const KXMLNode* xml_node, const char* attr, KTime_t* tm, bool optional);

rc_t XML_WriteTimestamp(char* dst, size_t bsize, size_t *num_writ, KTime_t ts);

rc_t XML_ParseBool(const KXMLNode* xml_node, const char* attr, bool* val, bool optional);

void XML_Fini(void);

#endif /* _h_sra_fuse_xml_ */
