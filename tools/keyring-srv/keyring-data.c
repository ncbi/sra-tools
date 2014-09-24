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

#include "keyring-data.h"

#include <sysalloc.h>
#include <stdlib.h>
#include <string.h>

#include <klib/rc.h>

rc_t KeyRingDataInit ( KeyRingData* self )
{
    BSTreeInit( & self->projects );
    BSTreeInit( & self->objects );
    self->next_projectId = 0;
    self->next_objectId = 0;
    return 0;
}

void KeyRingDataWhack ( KeyRingData* self )
{
    BSTreeWhack ( & self->projects, ProjectWhack, NULL );
    BSTreeWhack ( & self->objects, ObjectWhack, NULL );
}

int CC FindProject ( const void *item, const BSTNode *n )
{
    return StringCompare((const String*)item, ((const Project*)n)->name);
}
int CC SortProjects ( const BSTNode *item, const BSTNode *n )
{
    return StringCompare(((const Project*)item)->name, ((const Project*)n)->name);
}

const Project* KeyRingDataGetProject (const KeyRingData* data, const String* name)
{
    return (const Project*)BSTreeFind(&data->projects, name, FindProject);
}

rc_t KeyRingDataInsertProject (ProjectTable*  data, 
                               uint32_t      p_id, 
                               const String* name, 
                               const String* download_ticket, 
                               const String* encryption_key)
{
    rc_t rc = 0;
    Project* p = (Project*) malloc(sizeof(Project));
    if (p != NULL)
    {
        rc = ProjectInit(p, p_id, name, download_ticket, encryption_key);
        if (rc == 0)
        {
            rc = BSTreeInsert(data, &p->dad, SortProjects);
            if (rc != 0)
                ProjectWhack(&p->dad, NULL);
        }
        if (rc != 0)
            free(p);
    }
    else
        rc = RC ( rcApp, rcDatabase, rcUpdating, rcMemory, rcExhausted );
    return rc;
}                               

rc_t KeyRingDataAddProject(KeyRingData* data, const String* name, const String* download_ticket, const String* encryption_key)
{
    rc_t rc = 0;
    Project* p;

    p = (Project*)BSTreeFind(&data->projects, name, FindProject);
    if (p != NULL)
    {
        bool rewrite = false;
        String* dl = NULL;
        String* enc = NULL;
        if (StringCompare(p->download_ticket, download_ticket) != 0)
        {
            dl = p->download_ticket;
            rc = StringCopy((const String**)&p->download_ticket, download_ticket);
            if (rc == 0)
                rewrite = true;
            else
                dl = NULL;
        }
        if (rc == 0 && StringCompare(p->encryption_key, encryption_key) != 0)
        {
            enc = p->encryption_key;
            rc = StringCopy((const String**)&p->encryption_key, encryption_key);
            if (rc == 0)
                rewrite = true;
            else
                enc = NULL;
        }
        if (rc == 0 && rewrite)
        {
            if (dl)
                StringWhack(dl);
            if (enc)
                StringWhack(enc);
        }
    }
    else /* insert new */
    {
        rc = KeyRingDataInsertProject (&data->projects, data->next_projectId, name, download_ticket, encryption_key);
        if (rc == 0)
            ++data->next_projectId;
    }
    return rc;
}

rc_t ProjectInit ( Project* self, uint32_t p_id, const String* name, const String* download_ticket, const String* encryption_key )
{
    rc_t rc = 0;
    memset(self, 0, sizeof(Project)); 
    self->id = p_id;
    rc = StringCopy((const String**)&self->name, name);
    if (rc == 0)
    {
        rc = StringCopy((const String**)&self->download_ticket, download_ticket);
        if (rc == 0)
        {
            rc = StringCopy((const String**)&self->encryption_key, encryption_key);
            if (rc != 0)
                StringWhack(self->download_ticket);
        }
        else
            StringWhack(self->name);
    }
    return rc;
}

void ProjectWhack ( BSTNode *n, void *data )
{
    Project* self = (Project*)n;
    StringWhack(self->name);
    StringWhack(self->download_ticket);
    StringWhack(self->encryption_key);
    free(self);
}

rc_t ObjectInit ( Object* self, 
                  uint32_t p_id, 
                  const String* p_name, 
                  const String* p_project, 
                  const String* p_display_name,
                  uint64_t p_size,
                  const String* p_checksum,
                  const String* p_encryption_key)
{
    rc_t rc = 0;
    memset(self, 0, sizeof(Project)); 
    self->id = p_id;
    rc = StringCopy((const String**)&self->name, p_name);
    if (rc == 0)
    {
        rc = StringCopy((const String**)&self->project, p_project);
        if (rc == 0)
        {
            rc = StringCopy((const String**)&self->display_name, p_display_name);
            if (rc == 0)
            {
                self->size = p_size;
                rc = StringCopy((const String**)&self->checksum, p_checksum);
                if (rc == 0)
                {
                    rc = StringCopy((const String**)&self->encryption_key, p_encryption_key);
                    if (rc != 0)
                        StringWhack(self->checksum);
                }
                if (rc != 0)
                    StringWhack(self->display_name);
            }
            if (rc != 0)
                StringWhack(self->project);
        }
        if (rc != 0)
            StringWhack(self->name);
    }
    return rc;
}                         
                         
void ObjectWhack ( BSTNode *n, void *data )
{
    Object* self = (Object*)n;
    StringWhack(self->name);
    StringWhack(self->project);
    StringWhack(self->display_name);
    StringWhack(self->checksum);
    StringWhack(self->encryption_key);
    free(self);
}

int CC FindObject( const void *item, const BSTNode *n )
{
    return StringCompare((const String*)item, ((const Object*)n)->name);
}
int CC SortObjects( const BSTNode *item, const BSTNode *n )
{
    return StringCompare(((const Project*)item)->name, ((const Object*)n)->name);
}

rc_t KeyRingDataInsertObject(ObjectTable*  data, 
                             uint32_t      p_id, 
                             const String* name, 
                             const String* project, 
                             const String* display_name,
                             uint64_t      size,
                             const String* checksum,
                             const String* encryption_key)
{
    rc_t rc = 0;
    Object* obj = (Object*) malloc(sizeof(Object));
    if (obj != NULL)
    {
        rc = ObjectInit(obj, p_id, name, project, display_name, size, checksum, encryption_key);
        if (rc == 0)
        {
            rc = BSTreeInsert(data, &obj->dad, SortObjects);
            if (rc != 0)
                ProjectWhack(&obj->dad, NULL);
        }
        else
            free(obj);
    }
    else
        rc = RC ( rcApp, rcDatabase, rcUpdating, rcMemory, rcExhausted );
    return rc;
}                             


rc_t KeyRingDataAddObject (KeyRingData*  data, 
                           const String* name, 
                           const String* project, 
                           const String* display_name,
                           uint64_t      size,
                           const String* checksum,
                           const String* encryption_key)
{
    rc_t rc = 0;
    Object* obj;

    obj = (Object*)BSTreeFind(&data->objects, name, FindObject);
    if (obj != NULL)
    {
        bool rewrite = false;
        String* proj = NULL;
        String* disp = NULL;
        String* csum = NULL;
        String* encr = NULL;
        if (StringCompare(obj->project, project) != 0)
        {
            proj = obj->project;
            rc = StringCopy((const String**)&obj->project, project);
            if (rc == 0)
                rewrite = true;
            else
                proj = NULL;
        }
        if (StringCompare(obj->display_name, display_name) != 0)
        {
            disp = obj->display_name;
            rc = StringCopy((const String**)&obj->display_name, display_name);
            if (rc == 0)
                rewrite = true;
            else
                disp = NULL;
        }
        if (size != obj->size)
        {
            obj->size = size;
            rewrite = true;
        }
        if (StringCompare(obj->checksum, checksum) != 0)
        {
            csum = obj->checksum;
            rc = StringCopy((const String**)&obj->checksum, checksum);
            if (rc == 0)
                rewrite = true;
            else
                csum = NULL;
        }
        if (StringCompare(obj->encryption_key, encryption_key) != 0)
        {
            encr = obj->encryption_key;
            rc = StringCopy((const String**)&obj->encryption_key, encryption_key);
            if (rc == 0)
                rewrite = true;
            else
                encr = NULL;
        }
        if (rc == 0 && rewrite)
        {
            if (proj)
                StringWhack(proj);
            if (disp)
                StringWhack(disp);
            if (csum)
                StringWhack(csum);
            if (encr)
                StringWhack(encr);
        }
    }
    else /* insert new */
    {
        rc = KeyRingDataInsertObject(&data->objects, 
                                     data->next_objectId, 
                                     name, 
                                     project, 
                                     display_name,
                                     size,
                                     checksum,
                                     encryption_key);
        if (rc == 0)
            ++data->next_objectId;
    }
    return rc;
}                           
                                  
const Object* KeyRingDataGetObject (const KeyRingData*  data, const String* name)
{
    return (const Object*)BSTreeFind(&data->objects, name, FindObject);
}
