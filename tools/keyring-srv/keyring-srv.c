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

#include "keyring-srv.h"

#include <sysalloc.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <klib/text.h>
#include <klib/log.h>

#include <kfs/file.h>
#include <kfs/toc.h>
#include <kfs/arc.h>
#include <kfs/tar.h>
#include <kfs/impl.h>

#include <krypto/key.h>
#include <krypto/encfile.h>

#include "keyring-data.h"
#include "keyring-database.h"

static rc_t KeyRingInit     ( KKeyRing* self, const char* path );
static rc_t KeyRingWhack    ( KKeyRing* self );
static rc_t CreateDatabase  ( KKeyRing* self );
static rc_t OpenDatabase    ( KKeyRing* self );

#define MaxPwdSize 512

struct KKeyRing {
    KRefcount refcount;
    
    KDirectory* wd; 
    char* path;
    char passwd[MaxPwdSize];
    
    KeyRingData* data;
};

/* location of the temporary database (pre archiving+encryption */
const char tmp_path[] = "keyring.temp";

/* returns a NUL-terminated password fitting in the buffer */
static
rc_t
ReadPassword(const struct KFile* pwd_in, size_t* last_pos, char* buf, size_t buf_size)
{
string_copy(buf, buf_size, "screwuahole", string_size("screwuahole")); return 0;

    rc_t rc = 0;
    size_t i =0;
    do
    {
        size_t num_read;
        if (i == buf_size)
            return RC(rcApp, rcEncryptionKey, rcReading, rcParam, rcTooLong);
            
        rc = KFileRead( pwd_in, *last_pos, & buf[i], 1, &num_read );
        if (rc == 0)
        {
            if (num_read != 1)
                return RC(rcApp, rcEncryptionKey, rcReading, rcSize, rcInvalid);

            if (buf[i] == '\n')
            {
                buf[i] = 0;
                ++ *last_pos;
                break;
            }
        }
        ++ *last_pos;
        ++ i;
    }
    while(rc == 0);
    return rc;
}

static
rc_t GetNewPassword(const struct KFile* pwd_in, struct KFile* pwd_out, char* buf)
{
    rc_t rc = KFileWrite ( pwd_out, 0, KR_PWD_PROMPT_1, string_measure(KR_PWD_PROMPT_1, NULL), NULL);
    if (rc == 0)
    {
        char buf1[MaxPwdSize];
        size_t last_pos = 0;
        rc = ReadPassword(pwd_in, & last_pos, buf1, MaxPwdSize);
        if (rc == 0)
        {
            rc = KFileWrite ( pwd_out, 
                              string_measure(KR_PWD_PROMPT_1, NULL), 
                              KR_PWD_PROMPT_2, string_measure(KR_PWD_PROMPT_2, NULL), NULL );
            if (rc == 0)
            {
                char buf2[MaxPwdSize];
                rc = ReadPassword(pwd_in, & last_pos, buf2, sizeof(buf2));
                if (rc == 0)
                {
                    size_t pwd_size = string_measure(buf1, NULL);
                    if (string_cmp(buf1, pwd_size, buf2, string_measure(buf2, NULL), MaxPwdSize) != 0)
                        rc = RC(rcApp, rcEncryptionKey, rcCreating, rcParam, rcInconsistent);
                    else
                        string_copy(buf, MaxPwdSize, buf1, pwd_size + 1);
                }
            }
        }
    }
    return rc;
}

static
rc_t GetPassword(const struct KFile* pwd_in, struct KFile* pwd_out, char* buf)
{
    rc_t rc = KFileWrite ( pwd_out, 0, KR_PWD_PROMPT_1, string_measure(KR_PWD_PROMPT_1, NULL), NULL);
    if (rc == 0)
    {
        char buf1[MaxPwdSize];
        size_t last_pos = 0;
        rc = ReadPassword(pwd_in, & last_pos, buf1, MaxPwdSize);
        if (rc == 0)
            string_copy(buf, MaxPwdSize, buf1, string_measure(buf1, NULL) + 1);
    }
    return rc;
}

rc_t KeyRingOpen(KKeyRing** self, const char* path, const struct KFile* pwd_in, struct KFile* pwd_out)
{
    rc_t rc; 
    assert(self && path && pwd_in && pwd_out);

    *self = (KKeyRing*) malloc(sizeof(**self));
    if (*self)
    { 
        rc = KeyRingInit(*self, path);
        if (rc == 0)
        {
            rc = KeyRingAddRef(*self);
            if (rc == 0)
            {
                KDirectory* wd; 
                rc = KDirectoryNativeDir(&wd);
                if (rc == 0)
                {   /* open the database */
                    if (KDirectoryPathType(wd, "%s", (*self)->path) == kptFile)
                        rc = GetPassword(pwd_in, pwd_out, (*self)->passwd);
                    else /* does not exist; create first */
                    {   
                        rc = GetNewPassword(pwd_in, pwd_out, (*self)->passwd);
                        if (rc == 0)
                            rc = CreateDatabase(*self);
                    }
                    if (rc == 0)
                        rc = OpenDatabase(*self);
                        
                    {
                        rc_t rc2;
                        rc2 = KDirectoryRelease(wd);
                        if (rc == 0)
                            rc = rc2;
                    }
                }
            }
            
            if (rc != 0)
            {
                KeyRingWhack(*self);
                *self = NULL;
            }
        }
        else
        {
            free(*self);
            *self = NULL;
        }
    }
    else
        rc = RC ( rcApp, rcDatabase, rcOpening, rcMemory, rcExhausted );

    return rc;
}

rc_t KeyRingInit ( KKeyRing* self, const char* path )
{
    rc_t rc;
    
    memset ( self, 0, sizeof * self );
    KRefcountInit ( & self -> refcount, 0, "KKeyRing", "init", "" );
    
    rc = KDirectoryNativeDir(&self->wd);
    if (rc == 0)
    {
        self->path = string_dup(path, string_size(path));
        if (self->path)
        {
            self->data = (KeyRingData*) malloc(sizeof(*self->data));
            if (self->data)
            {
                rc = KeyRingDataInit ( self->data );
                if (rc != 0)
                    free(self->data);
            }
            else
                rc = RC ( rcApp, rcDatabase, rcOpening, rcMemory, rcExhausted );
                
            if (rc != 0)
                free(self->path);
        }
        else
            rc = RC ( rcApp, rcDatabase, rcOpening, rcMemory, rcExhausted );
            
        if (rc != 0)
            KDirectoryRelease(self->wd);
    }
        
    return rc;
}

rc_t KeyRingWhack(KKeyRing* self)
{
    KeyRingDataWhack( self->data );
    free(self->data);
    free(self->path);
    KDirectoryRelease(self->wd);
    free(self);
    return 0;
}

rc_t KeyRingAddRef(KKeyRing* self)
{
    if ( self != NULL )
    {
        switch ( KRefcountAdd ( & self -> refcount, "KKeyRing" ) )
        {
        case krefLimit:
            return RC ( rcApp, rcEncryptionKey, rcAttaching, rcRange, rcExcessive );
        }
    }
    return 0;
}

rc_t KeyRingRelease(KKeyRing* self)
{
    if ( self != NULL )
    {
        switch ( KRefcountDrop ( & self -> refcount, "KKeyRing" ) )
        {
        case krefWhack:
            KeyRingWhack ( self );
        break;
        case krefNegative:
            return RC ( rcApp, rcEncryptionKey, rcReleasing, rcRange, rcExcessive );
        }
    }
    return 0;
}

static
rc_t copy_file (const KFile * fin, KFile *fout)
{
    rc_t rc;
    uint8_t	buff	[64 * 1024];
    size_t	num_read;
    uint64_t	inpos;
    uint64_t	outpos;

    assert (fin != NULL);
    assert (fout != NULL);

    inpos = 0;
    outpos = 0;

    do
    {
        rc = KFileRead (fin, inpos, buff, sizeof (buff), &num_read);
        if (rc != 0)
        {
            PLOGERR (klogErr, (klogErr, rc,
                     "Failed to read from directory structure in creating archive at $(P)",
                               PLOG_U64(P), inpos));
            break;
        }
        else if (num_read > 0)
        {
            size_t to_write;

            inpos += (uint64_t)num_read;

            to_write = num_read;
            while (to_write > 0)
            {
                size_t num_writ;
                rc = KFileWrite (fout, outpos, buff, num_read, &num_writ);
                if (rc != 0)
                {
                    PLOGERR (klogErr, (klogErr, rc,
                             "Failed to write to archive in creating archive at $(P)",
                                       PLOG_U64(P), outpos));
                    break;
                }
                outpos += num_writ;
                to_write -= num_writ;
            }
        }
        if (rc != 0)
            break;
    } while (num_read != 0);
    return rc;
}

rc_t ArchiveAndEncrypt(KDirectory* wd, const char* inpath, const char* outpath, const char* passwd)
{
    const KDirectory* d;
    rc_t rc = KDirectoryOpenDirRead (wd, &d, false, "%s", inpath);
    if (rc == 0)
    {
        const KFile* infile;
        rc_t rc2;
    
        rc = KDirectoryOpenTocFileRead (d, &infile, 4, NULL, NULL, NULL);
        if (rc == 0)
        {
            KFile* outfile;
                
            /* if the file exists, add write access */
            KDirectorySetAccess( wd, false, 0600, 0777, "%s", outpath );
            rc = KDirectoryCreateFile(wd, &outfile, false, 0600, kcmCreate|kcmInit, "%s", outpath);
            if ( rc == 0 )
            {
                KFile* enc_outfile;
                KKey key;
                rc = KKeyInitRead(&key, kkeyAES256, passwd, string_measure(passwd, NULL));
                if ( rc == 0 )
                    rc = KEncFileMakeWrite(&enc_outfile, outfile, &key);
                    
                if (rc == 0)
                    rc = copy_file(infile, enc_outfile);
        
                rc2 = KFileRelease(outfile);
                if (rc == 0)
                    rc = rc2;
                /* remove write access */
                rc2 = KDirectorySetAccess( wd, false, 0400, 0777, "%s", outpath );
                if (rc == 0)
                    rc = rc2;
                rc2 = KFileRelease(enc_outfile);
                if (rc == 0)
                    rc = rc2;
            }
            rc2 = KFileRelease(infile);
            if (rc == 0)
                rc = rc2;
        }
        rc2 = KDirectoryRelease(d);
        if (rc == 0)
            rc = rc2;
    }
    return rc;
}

rc_t CreateDatabase(KKeyRing* self)
{   /* database is presumed write-locked */
    rc_t rc;
    
    assert(self);
    
    KDirectoryRemove(self->wd, true, "%s", tmp_path); /* in case exists */
    rc = KeyRingDatabaseSave(self->data, self->wd, tmp_path);
    if (rc == 0)
    {
        rc_t rc2;
        rc = ArchiveAndEncrypt(self->wd, tmp_path, self->path, self->passwd);
        
        rc2 = KDirectoryRemove(self->wd, true, "%s", tmp_path);
        if (rc == 0)
            rc = rc2;
    }
    return rc;
}

rc_t OpenDatabase(KKeyRing* self)
{
    rc_t rc;
    const KFile* enc_infile;
    
    assert(self);
    rc = KDirectoryOpenFileRead(self->wd, &enc_infile, "%s", self->path);
    if ( rc == 0)
    {
        rc_t rc2;
        const KFile* infile;
        KKey key;
        rc = KKeyInitRead(&key, kkeyAES256, self->passwd, string_measure(self->passwd, NULL));
        if ( rc == 0 )
        {
            rc = KEncFileMakeRead (&infile, enc_infile, &key);
            if (rc == 0)
            {   
                const KDirectory* arc;
                rc = KDirectoryOpenArcDirRead_silent_preopened(self->wd, &arc, true, "/keyring", tocKFile, (void*)infile, KArcParseSRA, NULL, NULL);
                if (rc == 0)
                {
                    /*  Hack: we violate the KDirectory object interface in order for VDBManagerMakeUpdate to succeed,
                        since it would refuse to open a read-only dir (i.e. archive);
                        We will only read from the object, though.
                    */
                    ((KDirectory*)arc)->read_only = false;
                    rc = KeyRingDatabaseLoad(self->data, arc, "/keyring");
                    rc2 = KDirectoryRelease(arc);
                    if (rc == 0)
                        rc = rc2;
                }
                rc2 = KFileRelease(infile);
                if (rc == 0)
                    rc = rc2;
            }
        }
            
        rc2 = KFileRelease(enc_infile);
        if (rc == 0)
            rc = rc2;
    }
    return rc;
}

rc_t KeyRingAddProject(KKeyRing* self, const String* name, const String* download_ticket, const String* encryption_key)
{   
    rc_t rc = 0;
    
    assert(self && name && download_ticket && encryption_key);
    
    /*TODO: write-lock database */
    rc = KeyRingDataAddProject(self->data, name, download_ticket, encryption_key);
    if (rc == 0)
        rc = CreateDatabase(self); 
    /*TODO: unlock database */
    return rc;
}

rc_t KeyRingGetProject(KKeyRing* self, const String* name, String* download_ticket, String* encryption_key)
{   
    rc_t rc = 0;
    const Project* p;
    
    assert(self && name && download_ticket && encryption_key);
    
    p = KeyRingDataGetProject(self->data, name);
    if (p != NULL)
    {
        StringInit(download_ticket, p->download_ticket->addr, p->download_ticket->len, p->download_ticket->size);
        StringInit(encryption_key, p->encryption_key->addr, p->encryption_key->len, p->encryption_key->size);
    }
    else
        rc = RC(rcApp, rcDatabase, rcSearching, rcName, rcNotFound);
    return rc;
}

rc_t KeyRingAddObject(KKeyRing* self, 
                      const struct String* object_name, 
                      const struct String* project_name, 
                      const struct String* display_name, 
                      uint64_t size, 
                      const struct String* checksum)
{
    rc_t rc = 0;
    const Project* p;
    
    assert(self && object_name && project_name && display_name && checksum);
    
    /*TODO: write-lock database */
    p = KeyRingDataGetProject(self->data, project_name);
    if (p != NULL)
    {
        rc = KeyRingDataAddObject(self->data, object_name, project_name, display_name, size, checksum, p->encryption_key);
        if (rc == 0)
            rc = CreateDatabase(self); 
    }
    else
        rc = RC(rcApp, rcDatabase, rcSearching, rcName, rcNotFound);
    /*TODO: unlock database */
    
    return rc;
}                      
                     
rc_t KeyRingGetKey(KKeyRing* self, const struct String* object_name, struct String* encryption_key)
{
    rc_t rc = 0;
    const Object* obj;

    assert(self && object_name && encryption_key);
    
    obj = KeyRingDataGetObject(self->data, object_name);
    if (obj != NULL)
        StringInit(encryption_key, obj->encryption_key->addr, obj->encryption_key->len, obj->encryption_key->size);
    else
        rc = RC(rcApp, rcDatabase, rcSearching, rcName, rcNotFound);
    return rc;
}
