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

#include "keyring-database.h"

#include <vdb/database.h>
#include <vdb/table.h>
#include <vdb/manager.h>
#include <vdb/schema.h>
#include <vdb/cursor.h>

#include "keyring-data.h"

static const char schema_text[] =
"version 1; "

" table projects : MyProjects #1 { "
"     extern column U32 id = .id;"
"     physical column U32 .id = id;"

"     extern    column ascii name = .name;"
"     physical  column ascii .name = name;"

"     extern    column ascii download_ticket = .download_ticket;"
"     physical  column ascii .download_ticket = download_ticket;"

"     extern    column ascii encryption_key = .encryption_key;"
"     physical  column ascii .encryption_key = encryption_key;"
" };"

" table objects : MyObjects #1 { "
"     extern    column U32 id = .id;"
"     physical  column U32 .id = id;"

"     extern    column ascii name = .name;"
"     physical  column ascii .name = name;"

"     extern    column ascii project = .project;"
"     physical  column ascii .project = project;"

"     extern    column ascii display_name = .display_name;"
"     physical  column ascii .display_name = display_name;"

"     extern    column U64   size = .size;"
"     physical  column U64   .size = size;"

"     extern    column ascii checksum = .checksum;"
"     physical  column ascii .checksum = checksum;"

"     extern    column ascii encryption_key = .encryption_key;"
"     physical  column ascii .encryption_key = encryption_key;"
" };"

" table keys : MyKeys #1 { "
"     physical column U32   .id;"
"     physical column ascii .value;"
" };"
"database keyring : KEYRING #1 { "
"    table objects: MyObjects   #1 object_inst;"
"    table projects: MyProjects #1 project_inst;"
"    table keys : MyKeys        #1 keys_inst;"
"};";

static rc_t SaveProjects( const ProjectTable* data, VDatabase* db );
static rc_t SaveObjects ( const ObjectTable* data,  VDatabase* db );
static rc_t LoadProjects( ProjectTable* data, const VDatabase* db );
static rc_t LoadObjects ( ObjectTable* data,  const VDatabase* db );

rc_t KeyRingDatabaseSave ( struct KeyRingData* self, struct KDirectory* wd, const char* path )
{
    rc_t rc;
    VDBManager* vdbMgr;
    rc = VDBManagerMakeUpdate( &vdbMgr, wd );
    if (rc == 0)
    {
        VSchema* schema;
        rc = VDBManagerMakeSchema(vdbMgr, &schema);
        if (rc == 0)
        {
            rc = VSchemaParseText ( schema, "keyring_schema", schema_text, string_measure(schema_text, NULL) );
            if (rc == 0)
            {   /* create a database */
                VDatabase* db;
                rc = VDBManagerCreateDB(vdbMgr, & db, schema, "keyring:KEYRING", kcmCreate | kcmMD5, path);
                if (rc == 0)
                {   
                    rc_t rc2;
                    rc = SaveProjects(&self->projects, db);
                    if (rc == 0)
                        rc = SaveObjects(&self->objects, db);
                    /*TODO: SaveKeys */
                    rc2 = VDatabaseRelease(db);
                    if (rc == 0)
                        rc = rc2;
                }

            }
            VSchemaRelease(schema);
        }
        VDBManagerRelease(vdbMgr);
    }

    return rc;
}

rc_t KeyRingDatabaseLoad ( struct KeyRingData* self, const struct KDirectory* dir, const char* path )
{
    VDBManager* innerMgr;
    rc_t rc = VDBManagerMakeUpdate( &innerMgr, (KDirectory*)dir );
    if (rc == 0)
    {
        rc_t rc2;
        const VDatabase* db;
        rc = VDBManagerOpenDBRead(innerMgr, & db, NULL, "%s", path);
        if (rc == 0)
        {
            rc = LoadProjects(&self->projects, db);
            if (rc == 0)
                rc = LoadObjects(&self->objects, db);
            /*TODO: LoadKeys */
            rc2 = VDatabaseRelease(db);
            if (rc == 0)
                rc = rc2;
        }
        rc2 = VDBManagerRelease(innerMgr);
        if (rc == 0)
            rc = rc2;
    }

    return rc;
}

static 
rc_t SaveProjects( const ProjectTable* data, VDatabase* db )
{
    VTable* tbl;
    rc_t rc = VDatabaseCreateTable(db, &tbl, "project_inst", kcmCreate | kcmMD5, "PROJECTS");
    if (rc == 0)
    {
        rc_t rc2;
        VCursor *cur;
        rc = VTableCreateCursorWrite( tbl, &cur, kcmInsert );
        if (rc == 0)
        {
            uint32_t id_idx, name_idx, dl_idx, enc_idx;
            rc = VCursorAddColumn( cur, &id_idx, "id" );
            rc = VCursorAddColumn( cur, &name_idx, "name" );
            rc = VCursorAddColumn( cur, &dl_idx, "download_ticket" );
            rc = VCursorAddColumn( cur, &enc_idx, "encryption_key" );
            if (rc == 0)
            {
                rc = VCursorOpen( cur );
                if (rc == 0)
                {
                    const Project* p = (const Project*)BSTreeFirst(data);
                    while (rc == 0 && p != NULL)
                    {
                        rc = VCursorOpenRow( cur );
                        
                        if (rc == 0) rc = VCursorWrite( cur, id_idx,    sizeof(p->id) * 8,                      &p->id,                     0, 1);
                        if (rc == 0) rc = VCursorWrite( cur, name_idx,  StringLength(p->name) * 8,              p->name->addr,              0, 1);
                        if (rc == 0) rc = VCursorWrite( cur, dl_idx,    StringLength(p->download_ticket) * 8,   p->download_ticket->addr,   0, 1);
                        if (rc == 0) rc = VCursorWrite( cur, enc_idx,   StringLength(p->encryption_key) * 8,    p->encryption_key->addr,    0, 1);
                        
                        if (rc == 0) rc = VCursorCommitRow( cur );
                        if (rc == 0) rc = VCursorCloseRow( cur );
                        
                        p = (const Project*)BSTNodeNext(&p->dad);
                    }
                    if (rc == 0)
                        rc = VCursorCommit( cur );
                }
            }
            rc2 = VCursorRelease(cur);
            if (rc == 0)
                rc = rc2;
        }
        
        rc2 = VTableRelease(tbl);
        if (rc == 0)
            rc = rc2;
    }
    return rc;
}

static 
rc_t SaveObjects ( const ObjectTable* data,  VDatabase* db )
{
    VTable* tbl;
    rc_t rc = VDatabaseCreateTable(db, &tbl, "object_inst", kcmCreate | kcmMD5, "OBJECTS");
    if (rc == 0)
    {
        rc_t rc2;
        VCursor *cur;
        rc = VTableCreateCursorWrite( tbl, &cur, kcmInsert );
        if (rc == 0)
        {
            uint32_t id_idx, name_idx, proj_idx, dname_idx, size_idx, csum_idx, enc_idx;
            if (rc == 0) rc = VCursorAddColumn( cur, &id_idx,    "id" );
            if (rc == 0) rc = VCursorAddColumn( cur, &name_idx,  "name" );
            if (rc == 0) rc = VCursorAddColumn( cur, &proj_idx,    "project" );
            if (rc == 0) rc = VCursorAddColumn( cur, &dname_idx, "display_name" );
            if (rc == 0) rc = VCursorAddColumn( cur, &size_idx,  "size" );
            if (rc == 0) rc = VCursorAddColumn( cur, &csum_idx,  "checksum" );
            if (rc == 0) rc = VCursorAddColumn( cur, &enc_idx,   "encryption_key" );

            if (rc == 0)
            {
                rc = VCursorOpen( cur );
                if (rc == 0)
                {
                    const Object* obj = (const Object*)BSTreeFirst(data);
                    while (rc == 0 && obj != NULL)
                    {
                        rc = VCursorOpenRow( cur );
                        
                        if (rc == 0) rc = VCursorWrite( cur, id_idx,    sizeof(obj->id) * 8,                   &obj->id,                  0, 1);
                        if (rc == 0) rc = VCursorWrite( cur, name_idx,  StringLength(obj->name) * 8,           obj->name->addr,           0, 1);
                        if (rc == 0) rc = VCursorWrite( cur, proj_idx,  StringLength(obj->project) * 8,        obj->project->addr,        0, 1);
                        if (rc == 0) rc = VCursorWrite( cur, dname_idx, StringLength(obj->display_name) * 8,   obj->display_name->addr,   0, 1);
                        if (rc == 0) rc = VCursorWrite( cur, size_idx,  sizeof(obj->size) * 8,                 &obj->size,                0, 1);
                        if (rc == 0) rc = VCursorWrite( cur, csum_idx,  StringLength(obj->encryption_key) * 8, obj->encryption_key->addr, 0, 1);
                        if (rc == 0) rc = VCursorWrite( cur, enc_idx,   StringLength(obj->encryption_key) * 8, obj->encryption_key->addr, 0, 1);
                        
                        if (rc == 0) rc = VCursorCommitRow( cur );
                        if (rc == 0) rc = VCursorCloseRow( cur );
                        
                        obj = (const Object*)BSTNodeNext(&obj->dad);
                    }
                    if (rc == 0) rc = VCursorCommit( cur );
                }
            }
            rc2 = VCursorRelease(cur);
            if (rc == 0)
                rc = rc2;
        }
        
        rc2 = VTableRelease(tbl);
        if (rc == 0)
            rc = rc2;
    }
    return rc;
}

static size_t CursorCacheSize = 32*1024;

static
bool HasData(const VTable* tbl)
{
    bool ret = false;
    KNamelist *names;
    if (VTableListCol(tbl, &names) == 0) 
    {
        uint32_t n;
        ret = KNamelistCount( names, &n ) == 0 && n > 0;
    }
    KNamelistRelease( names );
    return ret;
}

static 
rc_t LoadProjects( ProjectTable* data, const VDatabase* db )
{
    const VTable* tbl;
    rc_t rc = VDatabaseOpenTableRead(db, &tbl, "PROJECTS");
    if (rc == 0)
    {
        rc_t rc2;
        const VCursor *cur;

        rc = VTableCreateCachedCursorRead( tbl, &cur, CursorCacheSize );
        if (rc == 0)
        {
            uint32_t id_idx, name_idx, dl_idx, enc_idx;
            rc = VCursorAddColumn( cur, &id_idx,    "id" );
            if (rc == 0) rc = VCursorAddColumn( cur, &name_idx,  "name" );
            if (rc == 0) rc = VCursorAddColumn( cur, &dl_idx,    "download_ticket" );
            if (rc == 0) rc = VCursorAddColumn( cur, &enc_idx,   "encryption_key" );
            if (rc == 0 && HasData(tbl))
            {
                rc = VCursorOpen( cur );
                if (rc == 0)
                {
                    int64_t  first;
                    uint64_t count;
                    rc = VCursorIdRange( cur, 0, &first, &count );
                    if (rc == 0)
                    {
                        uint64_t i;
                        for (i=0; i < count; ++i)
                        {
                            const void* ptr;
                            uint32_t elem_count;
                            uint32_t id;
                            String name;
                            String download_ticket;
                            String encryption_key;

                            rc = VCursorSetRowId(cur, first + i);
                            if (rc == 0) rc = VCursorOpenRow( cur );
                            
                            if (rc == 0) rc = VCursorCellData( cur, id_idx, NULL, &ptr, NULL, NULL);
                            if (rc == 0) id = *(uint32_t*)ptr;
                            if (rc == 0) rc = VCursorCellData( cur, name_idx, NULL, &ptr, NULL, &elem_count);
                            if (rc == 0) StringInit(&name, (const char*)ptr, elem_count, elem_count);
                            if (rc == 0) rc = VCursorCellData( cur, dl_idx, NULL, &ptr, NULL, &elem_count);
                            if (rc == 0) StringInit(&download_ticket, (const char*)ptr, elem_count, elem_count);
                            if (rc == 0) rc = VCursorCellData( cur, enc_idx, NULL, &ptr, NULL, &elem_count);
                            if (rc == 0) StringInit(&encryption_key, (const char*)ptr, elem_count, elem_count);
                            
                            if (rc == 0) rc = KeyRingDataInsertProject(data, id, &name, &download_ticket, &encryption_key);
                            if (rc == 0) rc = VCursorCloseRow( cur );
                            if (rc != 0) 
                                break;
                        }
                    }
                }
            }
            rc2 = VCursorRelease(cur);
            if (rc == 0)
                rc = rc2;
        }
        
        rc2 = VTableRelease(tbl);
        if (rc == 0)
            rc = rc2;
    }
    return rc;
}

static 
rc_t LoadObjects ( ObjectTable* data,  const VDatabase* db )
{
    const VTable* tbl;
    rc_t rc = VDatabaseOpenTableRead(db, &tbl, "OBJECTS");
    if (rc == 0)
    {
        rc_t rc2;
        const VCursor *cur;

        rc = VTableCreateCachedCursorRead( tbl, &cur, CursorCacheSize );
        if (rc == 0)
        {
            uint32_t id_idx, name_idx, proj_idx, dname_idx, size_idx, csum_idx, enc_idx;
            if (rc == 0) rc = VCursorAddColumn( cur, &id_idx,    "id" );
            if (rc == 0) rc = VCursorAddColumn( cur, &name_idx,  "name" );
            if (rc == 0) rc = VCursorAddColumn( cur, &proj_idx,    "project" );
            if (rc == 0) rc = VCursorAddColumn( cur, &dname_idx, "display_name" );
            if (rc == 0) rc = VCursorAddColumn( cur, &size_idx,  "size" );
            if (rc == 0) rc = VCursorAddColumn( cur, &csum_idx,  "checksum" );
            if (rc == 0) rc = VCursorAddColumn( cur, &enc_idx,   "encryption_key" );
            if (rc == 0 && HasData(tbl))
            {
                rc = VCursorOpen( cur );
                if (rc == 0)
                {
                    int64_t  first;
                    uint64_t count;
                    rc = VCursorIdRange( cur, 0, &first, &count );
                    if (rc == 0)
                    {
                        uint64_t i;
                        for (i=0; i < count; ++i)
                        {
                            const void* ptr;
                            uint32_t elem_count;
                            uint32_t id;
                            String name;
                            String project;
                            String display_name;
                            uint64_t size;
                            String checksum;
                            String encryption_key;

                            rc = VCursorSetRowId(cur, first + i);
                            if (rc == 0) rc = VCursorOpenRow( cur );
                            
                            if (rc == 0) rc = VCursorCellData( cur, id_idx, NULL, &ptr, NULL, NULL);
                            if (rc == 0) id = *(uint32_t*)ptr;
                            
                            if (rc == 0) rc = VCursorCellData( cur, name_idx, NULL, &ptr, NULL, &elem_count);
                            if (rc == 0) StringInit(&name, (const char*)ptr, elem_count, elem_count);
                            
                            if (rc == 0) rc = VCursorCellData( cur, proj_idx, NULL, &ptr, NULL, &elem_count);
                            if (rc == 0) StringInit(&project, (const char*)ptr, elem_count, elem_count);
                            
                            if (rc == 0) rc = VCursorCellData( cur, dname_idx, NULL, &ptr, NULL, &elem_count);
                            if (rc == 0) StringInit(&display_name, (const char*)ptr, elem_count, elem_count);
                            
                            if (rc == 0) rc = VCursorCellData( cur, size_idx, NULL, &ptr, NULL, NULL);
                            if (rc == 0) size = *(uint32_t*)ptr;
                            
                            if (rc == 0) rc = VCursorCellData( cur, enc_idx, NULL, &ptr, NULL, &elem_count);
                            if (rc == 0) StringInit(&encryption_key, (const char*)ptr, elem_count, elem_count);
                            
                            if (rc == 0) rc = VCursorCellData( cur, csum_idx, NULL, &ptr, NULL, &elem_count);
                            if (rc == 0) StringInit(&checksum, (const char*)ptr, elem_count, elem_count);

                            if (rc == 0) rc = KeyRingDataInsertObject(data, id, &name, &project, &display_name, size, &checksum, &encryption_key);
                            if (rc == 0) rc = VCursorCloseRow( cur );
                            if (rc != 0)
                                break;
                        }
                    }
                }
            }
            rc2 = VCursorRelease(cur);
            if (rc == 0)
                rc = rc2;
        }
        
        rc2 = VTableRelease(tbl);
        if (rc == 0)
            rc = rc2;
    }
    return rc;
}

