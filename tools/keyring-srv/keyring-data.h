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

#ifndef _h_keyring_data_
#define _h_keyring_data_

#include <klib/defs.h>
#include <klib/container.h>
#include <klib/text.h>

struct VDatabase;

typedef struct Project Project;
struct Project
{
    BSTNode dad;

    uint32_t id;
    String* name;
    /*TODO: replace Strings with key-ids */
    String* download_ticket;
    String* encryption_key;
};
extern rc_t ProjectInit ( Project* self, 
                          uint32_t p_id, 
                          const String* p_name, 
                          const String* p_download_ticket, 
                          const String* p_encyption_key );
extern void ProjectWhack ( BSTNode *n, void *data );

typedef struct Object Object;
struct Object
{
    BSTNode dad;

    uint32_t id;
    String* name;
    String* project;
    String* display_name; 
    uint64_t size; 
    String* checksum;
    /*TODO: replace String with key-id */
    String* encryption_key;
};
extern rc_t ObjectInit ( Object*        self, 
                         uint32_t       id, 
                         const String*  name, 
                         const String*  project, 
                         const String*  display_name,
                         uint64_t       size,
                         const String*  checksum,
                         const String*  encryption_key);
extern void ObjectWhack ( BSTNode *n, void *data );

typedef BSTree ProjectTable;
typedef BSTree ObjectTable;

typedef struct KeyRingData KeyRingData;
struct KeyRingData
{
    ProjectTable projects;
    ObjectTable objects;
    
    uint32_t next_projectId;
    uint32_t next_objectId;
};

extern rc_t KeyRingDataInit ( KeyRingData* self );
extern void KeyRingDataWhack ( KeyRingData* self ); /* does not call free(self) */

/* make sure does not exist, assign an id*/
extern rc_t KeyRingDataAddProject (KeyRingData*  data, 
                                   const String* name, 
                                   const String* download_ticket, 
                                   const String* encryption_key);

/* known to be new, id assigned*/
extern rc_t KeyRingDataInsertProject (ProjectTable*  data, 
                                      uint32_t      p_id, 
                                      const String* name, 
                                      const String* download_ticket, 
                                      const String* encryption_key);

extern const Project* KeyRingDataGetProject (const KeyRingData* data, const String* name);

/* make sure does not exist, assign an id*/
extern rc_t KeyRingDataAddObject (KeyRingData*  data, 
                                  const String* name, 
                                  const String* project, 
                                  const String* display_name,
                                  uint64_t      size,
                                  const String* checksum,
                                  const String* encryption_key);
/* known to be new, id assigned*/
extern rc_t KeyRingDataInsertObject(ObjectTable*  data, 
                                    uint32_t      p_id, 
                                    const String* name, 
                                    const String* project, 
                                    const String* display_name,
                                    uint64_t      size,
                                    const String* checksum,
                                    const String* encryption_key);
                                  
extern const Object* KeyRingDataGetObject (const KeyRingData*  data, const String* name);


#endif /* _h_keyring_data_ */
