/*===========================================================================
 *
 *                            Public DOMAIN NOTICE
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

#include "copycat-priv.h"
#include "cctree-priv.h"

#include <vfs/path.h>
#include <vfs/path-priv.h>
#include <krypto/key.h>
#include <krypto/encfile.h>
#include <krypto/encfile-priv.h>
#include <krypto/wgaencrypt.h>
#include <kfs/kfs-priv.h>
#include <kfs/file.h>
#include "../shared/teefile.h"
#include <kfs/gzip.h>
#include <kfs/bzip.h>
#include <kfs/md5.h>
#include <kfs/countfile.h>
#include <kfs/readheadfile.h>
#include <kfs/buffile.h>
#include <kfs/crc.h>
#include <klib/checksum.h>
#include <klib/log.h>
#include <klib/rc.h>
#include <klib/text.h>

#include <os-native.h>

#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <assert.h>

/* make it last include */
#include "debug.h"

#define EXAMINE_KAR_FILES 0
#define DECRYPT_FAIL_AS_PLAIN_FILE 0
/* the readhead file isn't working yet */
#define USE_KBUFFILE 1

static
const VPath * src_path = NULL;

static
const VPath * dst_path = NULL;

static
bool do_encrypt = false;
static
bool do_decrypt = false;
static
bool wga_pw_read = false;
static
bool src_pw_read = false;
static
bool dst_pw_read = false;

static
char wga_pwd [256];
static
char src_pwd [256];
static
char dst_pwd [256];

static
size_t wga_pwd_sz;
static
size_t src_pwd_sz;
static
size_t dst_pwd_sz;

static
KKey src_key;

static
KKey dst_key;

static
KDirectory * cwd = NULL;


static
rc_t get_password (const VPath * path, char * pw, size_t pwz, size_t * num_read, KKey * key, bool * read)
{
    const KFile * pwfile;
    size_t z;
    rc_t rc;
    char obuff [8096];

    if (VPathOption (path, vpopt_encrypted, obuff, sizeof obuff, &z) == 0)
    {
        if (VPathOption (path, vpopt_pwpath, obuff, sizeof obuff, &z) == 0)
            rc = KDirectoryOpenFileRead (cwd, &pwfile, "%s", obuff);

        else if (VPathOption (path, vpopt_pwfd, obuff, sizeof obuff, &z) == 0)
            rc = KFileMakeFDFileRead (&pwfile, atoi (obuff));

        else
            rc = RC (rcExe, rcPath, rcAccessing, rcParam, rcUnsupported);
        if (rc == 0)
        {
            rc = KFileRead (pwfile, 0, pw, pwz, num_read);

            if (rc == 0)
            {
                char * pc;

                if (*num_read < pwz)
                    pw[*num_read] = '\0';

                pc = string_chr (pw, *num_read, '\r');
                if (pc)
                {
                    *pc = '\0';
                    *num_read = pc - pw;
                }

                pc = string_chr (pw, *num_read, '\n');
                if (pc)
                {
                    *pc = '\0';
                    *num_read = pc - pw;
                }

                *read = true;
                rc = KKeyInitRead (key, kkeyAES128, pw, *num_read);
            }

            KFileRelease (pwfile);
        }
    }
    else
        rc = RC (rcExe, rcPath, rcAccessing, rcFunction, rcNotFound);
    return rc;
}


static
rc_t wga_password (const VPath * path, char * pw, size_t pwz, size_t * num_read, bool * read)
{
    const KFile * pwfile;
    size_t z;
    rc_t rc;
    char obuff [8096];

    if (VPathOption (path, vpopt_encrypted, obuff, sizeof obuff, &z) == 0)
    {
        if (VPathOption (path, vpopt_pwpath, obuff, sizeof obuff, &z) == 0)
            rc = KDirectoryOpenFileRead (cwd, &pwfile, "%s", obuff);

        else if (VPathOption (path, vpopt_pwfd, obuff, sizeof obuff, &z) == 0)
            rc = KFileMakeFDFileRead (&pwfile, atoi (obuff));

        else
            rc = RC (rcExe, rcPath, rcAccessing, rcParam, rcUnsupported);
        if (rc == 0)
        {
            rc = KFileRead (pwfile, 0, pw, pwz, num_read);

            if (rc == 0)
            {
                char * pc;

                if (*num_read < pwz)
                    pw[*num_read] = '\0';

                pc = string_chr (pw, *num_read, '\r');
                if (pc)
                {
                    *pc = '\0';
                    *num_read = pc - pw;
                }

                pc = string_chr (pw, *num_read, '\n');
                if (pc)
                {
                    *pc = '\0';
                    *num_read = pc - pw;
                }

                *read = true;
            }

            KFileRelease (pwfile);
        }
    }
    else
        rc = RC (rcExe, rcPath, rcAccessing, rcFunction, rcNotFound);
    return rc;
}


/*--------------------------------------------------------------------------
 * copycat
 */
static
rc_t ccat_cache ( CCCachedFileNode **np, const KFile *sf,
                  enum CCType ntype, CCFileNode *node, const char *name )
{
    rc_t rc;
    KFile *out;

    /* create path */
    char path [ 256 ];
    int len = snprintf ( path, sizeof path, "%s", name );

    DEBUG_STATUS (("%s: name '%s'\n", __func__, name));

    if ( len < 0 || len >= sizeof path )
        return RC ( rcExe, rcFile, rcWriting, rcPath, rcExcessive );

    /* look for a name that has not yet been written */
    if ( CCTreeFind ( ctree, path ) != NULL )
    {
        uint32_t i;
        const char *ext = strrchr ( name, '.' );
        if ( ext != NULL )
        {
            for ( i = 2; ; ++ i )
            {
                len = snprintf ( path, sizeof path, "%.*s-%u%s", ( int ) ( ext - name ), name, i, ext );
                if ( len < 0 || len >= sizeof path )
                    return RC ( rcExe, rcFile, rcWriting, rcPath, rcExcessive );
                if ( CCTreeFind ( ctree, path ) == NULL )
                    break;
            }
        }
        else
        {
            for ( i = 2; ; ++ i )
            {
                len = snprintf ( path, sizeof path, "%s-%u", name, i );
                if ( len < 0 || len >= sizeof path )
                    return RC ( rcExe, rcFile, rcWriting, rcPath, rcExcessive );
                if ( CCTreeFind ( ctree, path ) == NULL )
                    break;
            }
        }
    }

    /* create an output file */
    rc = KDirectoryCreateFile ( cdir, & out, false, 0640, cm, "%s", path );
    if ( rc != 0 && GetRCState ( rc ) == rcUnauthorized )
    {
        /* respond to a file that has no write privs */
        uint32_t access;
        rc_t rc2 = KDirectoryAccess ( cdir, & access, "%s", path );
        if ( rc2 == 0 )
        {
            rc2 = KDirectorySetAccess ( cdir, false, 0640, 0777, "%s", path );
            if ( rc2 == 0 )
            {
                rc = KDirectoryCreateFile ( cdir, & out, false, 0640, cm, "%s", path );
                if ( rc != 0 )
                    KDirectorySetAccess ( cdir, false, access, 0777, "%s", path );
            }
        }
    }

    if ( rc != 0 )
        PLOGERR ( klogErr,  (klogErr, rc, "failed to create cached file '$(path)'", "path=%s", path ));
    else
    {
        const KFile *tee;
        rc = KFileMakeTeeRead ( & tee, sf, out );
        if ( rc != 0 )
            PLOGERR ( klogInt,  (klogInt, rc, "failed to create cache tee file on '$(path)'", "path=%s", path ));
        else
        {
            KFileAddRef ( sf );
            KFileAddRef ( out );
            rc = KFileRelease ( tee );
            if ( rc != 0 )
                PLOGERR ( klogInt,  (klogInt, rc, "failed to close cache tee file on '$(path)'", "path=%s", path ));
        }

        KFileRelease ( out );

        if ( rc == 0 )
        {
            rc = CCCachedFileNodeMake ( np, path, ntype, node );
            if ( rc != 0 )
                LOGERR ( klogInt, rc, "failed to create cached file node" );
            else
            {
                rc = KDirectorySetAccess ( cdir, false, 0440, 0777, "%s", path );

                /* create named entry in ctree */
                rc = CCTreeInsert ( ctree, 0, ccFile, NULL, path );
                if ( rc != 0 )
                    LOGERR ( klogInt, rc, "failed to record cached file" );
            }
        }
    }

    return rc;
}


static
rc_t ccat_extract_path (char * path, size_t pathz, const char * name)
{
    rc_t rc = 0;
    int len;

    DEBUG_STATUS (("%s: name '%s'\n", __func__, name));

    if (extract_dir)
        len = snprintf ( path, pathz, "%s%s", epath, name );
    else
        len = snprintf ( path, pathz, "%s", name );
    DEBUG_STATUS (("%s: path '%s'\n",__func__, path));

    if ( len < 0 || len >= pathz )
        return RC ( rcExe, rcFile, rcWriting, rcPath, rcExcessive );

    /* look for a name that has not yet been written */
    if ( CCTreeFind ( etree, path ) != NULL )
    {
        uint32_t i;
        const char *ext = strrchr ( name, '.' );
        if ( ext != NULL )
        {
            for ( i = 2; ; ++ i )
            {
                len = snprintf ( path, pathz, "%.*s-%u%s", ( int ) ( ext - name ), name, i, ext );
                if ( len < 0 || len >= pathz )
                    return RC ( rcExe, rcFile, rcWriting, rcPath, rcExcessive );
                if ( CCTreeFind ( etree, path ) == NULL )
                    break;
            }
        }
        else
        {
            for ( i = 2; ; ++ i )
            {
                len = snprintf ( path, pathz, "%s-%u", name, i );
                if ( len < 0 || len >= pathz )
                    return RC ( rcExe, rcFile, rcWriting, rcPath, rcExcessive );
                if ( CCTreeFind ( etree, path ) == NULL )
                    break;
            }
        }
    }
    DEBUG_STATUS (("%s: rc '%u(%R)' path '%s'\n",__func__, rc, rc, path));
    return rc;
}


static
rc_t ccat_extract (const KFile *sf, const char * path)
{
    rc_t rc;
    KFile *out;

    rc = KDirectoryCreateFile ( edir, & out, false, 0640, cm, "%s", path );
    if ( rc != 0 && GetRCState ( rc ) == rcUnauthorized )
    {
        /* respond to a file that has no write privs */
        uint32_t access;
        rc_t rc2 = KDirectoryAccess ( edir, & access, "%s", path );
        if ( rc2 == 0 )
        {
            rc2 = KDirectorySetAccess ( edir, false, 0640, 0777, "%s", path );
            if ( rc2 == 0 )
            {
                rc = KDirectoryCreateFile ( edir, & out, false, 0640, cm, "%s", path );
                DBG_KFILE(("%s: called KDirectoryCreateFile rc %R path %s\n",__func__,rc,path));
                DBG_KFile(out);
                if ( rc != 0 )
                    KDirectorySetAccess ( edir, false, access, 0777, "%s", path );
            }
        }
    }
    if ( rc != 0 )
        PLOGERR ( klogErr,
                  ( klogErr, rc,
                    "failed to create extracted file '$(path)'",
                    "path=%s", path ));
    else
    {
        const KFile *tee;
        rc = KFileMakeTeeRead ( & tee, sf, out );
        DBG_KFILE(("%s: called KFileMakeTeeRead rc %R \n",__func__,rc));
        DBG_KFile(tee);
        if ( rc != 0 )
            PLOGERR ( klogInt,  ( klogInt, rc, "failed to create extract tee file on '$(path)'", "path=%s", path ));
        else
        {
            KFileAddRef ( sf );
            KFileAddRef ( out );
            rc = KFileRelease ( tee );
            if ( rc != 0 )
                PLOGERR (klogInt,
                         (klogInt, rc,
                          "failed to close extract tee file on '$(path)'",
                          "path=%s", path));
            else if (!xml_dir)
            {
                CCCachedFileNode * np;

                rc = CCCachedFileNodeMake (&np, path, ccCached, NULL );
                if ( rc != 0 )
                    LOGERR ( klogInt, rc, "failed to create cached extract file node" );
                else
                {
                    rc = CCTreeInsert (etree, 0, ccFile, NULL, path);
                    if ( rc != 0 )
                        LOGERR ( klogInt, rc, "failed to record cached extract file" );
                }
            }
        }
        KFileRelease ( out );
    }

    return rc;
}

static
rc_t ccat_arc ( CCContainerNode **np, const KFile *sf, KTime_t mtime,
                enum CCType ntype, CCFileNode *node, const char *name, uint32_t type_id )
{
    rc_t rc /*, orc */;

    /* ensure we handle this type of archive */
    switch ( type_id )
    {
    case ccfftaHD5:
        * np = NULL;
        return 0;
    case ccfftaSra:
#if ! EXAMINE_KAR_FILES
        * np = NULL;
        return 0;
#endif
    case ccfftaTar:
        break;
    default:
        /* don't recognize archive format - treat as a normal file */
        PLOGMSG ( klogWarn, ( klogWarn, "archive '$(name)' type '$(ftype)' will not be analyzed: "
                              "unknown format", "name=%s,ftype=%s", name, node -> ftype ));
        * np = NULL;
        return 0;
    }

    /* create container node */
    rc = CCContainerNodeMake ( np, ntype, node );
    if ( rc != 0 )
        LOGERR ( klogInt, rc, "failed to create container node" );
    else
    {
        CCContainerNode *cont = * np;

        /* orc = 0; */
        switch ( type_id )
        {
        case ccfftaTar:
            /* orc = */ ccat_tar ( cont, sf, name );
            break;
        case ccfftaSra:
            /* orc = */ ccat_sra (cont, sf, name);
            break;
        }
    }

    return rc;
}

static
rc_t ccat_enc ( CCContainerNode **np, const KFile *sf, KTime_t mtime,
                enum CCType ntype, CCFileNode *node, const char *name,
                uint32_t type_id )
{
    rc_t rc = 0;
    const KFile *df;
    uint64_t expected = SIZE_UNKNOWN;      /* assume we won't know */

    switch ( type_id )
    {
    case ccffteNCBI:
        if (!src_pw_read)
        {
            rc = get_password (src_path, src_pwd, sizeof src_pwd, &src_pwd_sz, &src_key, &src_pw_read);
            if (rc)
            {
            validate_instead:
                rc = KFileAddRef (sf);
                if (rc == 0)
                {
                    PLOGMSG ( klogWarn, ( klogWarn, "file '$(name)' type '$(ftype)' will be validated but not be decoded: "
                                          "no password given", "name=%s,ftype=%s", name, node -> ftype ));
                    /* can't decompress it - treat it as a normal file */
                    rc = KEncFileValidate (sf);
                    if (rc)
                    {
                        memmove (node->ftype + sizeof "Errored" - 1, node->ftype, strlen (node->ftype));
                        memmove (node->ftype, "Errored", sizeof "Errored" - 1);
                    }
                    * np = NULL;
                    return 0;
                }
                return rc;
            }
        }
        rc = KEncFileMakeRead (&df, sf, &src_key);
        if (rc)
            goto validate_instead;
        break;

    case ccffteWGA:
        if (!wga_pw_read)
            rc = wga_password (src_path, wga_pwd, sizeof wga_pwd, &wga_pwd_sz, &wga_pw_read);
        if (rc == 0)
        {
            rc = KFileMakeWGAEncRead (&df, sf, wga_pwd, wga_pwd_sz);
            break;
        }
        /* can't decompress it - treat it as a normal file */
        PLOGMSG ( klogWarn, ( klogWarn, "file '$(name)' type '$(ftype)' will not be decoded: "
                              "no password given", "name=%s,ftype=%s", name, node -> ftype ));
        * np = NULL;
        return 0;
        
    default:
        /* can't decrypt it - treat it as a normal file */
        PLOGMSG ( klogWarn, ( klogWarn, "file '$(name)' type '$(ftype)' will not be decoded: "
                              "unknown encoding format", "name=%s,ftype=%s", name, node -> ftype ));
        * np = NULL;
        return 0;
    }

    if ( rc != 0 )
        PLOGERR ( klogInt,  (klogInt, rc, "failed to decode file '$(path)'", "path=%s", name ));
    else
    {
        rc = CCContainerNodeMake ( np, ntype, node );
        if ( rc != 0 )
            LOGERR ( klogInt, rc, "failed to create container node" );
        else
        {
            CCContainerNode *cont = * np;
            CCFileNode *nnode;

            /* now create a new contained file node */
            rc = CCFileNodeMake ( & nnode, expected );
            if ( rc != 0 )
                LOGERR ( klogInt, rc, "failed to create contained file node" );
            else
            {
                int len;
                char newname [ 256 ];

                /* invent a new name for file */
                const char *ext = strrchr ( name, '.' );
                if ( ext == NULL )
                    len = snprintf ( newname, sizeof newname, "%s", name );
                else
                    len = snprintf ( newname, sizeof newname, "%.*s", ( int ) ( ext - name ), name );

                if ( len < 0 || len >= sizeof newname )
                {
                    rc = RC ( rcExe, rcNode, rcConstructing, rcName, rcExcessive );
                    LOGERR ( klogErr, rc, "failed to create contained file node" );
                }
                else
                {
                    void * save;
                    const KFile * cf;
                    /* rc_t krc; */

                    copycat_log_set (&node->logs, &save);

                    rc = CCFileMakeRead (&cf, df, &node->rc);
                    if (rc == 0)
                    {
                        /* recurse with buffer on decoding */
                        /* krc = */ ccat_buf ( & cont -> sub, cf, mtime,
                                         ccContFile, nnode, newname);

                        KFileRelease (cf);
                        /* if successful, "node" ( allocated locally above )
                           will have been entered into "cont->sub" */
                    }

                    copycat_log_set (&save, NULL);
                }
            }
        }

        KFileRelease ( df );
    }

    return rc;
}

static
rc_t ccat_cmp ( CCContainerNode **np, const KFile *sf, KTime_t mtime,
                enum CCType ntype, CCFileNode *node, const char *name, uint32_t type_id )
{
    const KFile *zf;    
    /* use a variable incase we ever get a compression that can be queried 
     * about file size */
    uint64_t expected = SIZE_UNKNOWN;
    rc_t rc;

    switch ( type_id )
    {

    case ccfftcGzip:
        /* this code attaches a new reference to "sf" */
        rc = KFileMakeGzipForRead ( & zf, sf );
        break;
    case ccfftcBzip2:
        /* this code attaches a new reference to "sf" */
        rc = KFileMakeBzip2ForRead ( & zf, sf );
        break;
    default:
        /* can't decompress it - treat it as a normal file */
        PLOGMSG ( klogWarn, ( klogWarn, "file '$(name)' type '$(ftype)' will not be decompressed: "
                              "unknown compression format", "name=%s,ftype=%s", name, node -> ftype ));
        * np = NULL;
        return 0;
    }

    if ( rc != 0 )
        PLOGERR ( klogInt,  (klogInt, rc, "failed to decompress file '$(path)'", "path=%s", name ));
    else
    {
        rc = CCContainerNodeMake ( np, ntype, node );
        if ( rc != 0 )
            LOGERR ( klogInt, rc, "failed to create container node" );
        else
        {
            CCContainerNode *cont = * np;
            CCFileNode *nnode;

            /* now create a new contained file node */
            rc = CCFileNodeMake ( & nnode, expected );
            if ( rc != 0 )
                LOGERR ( klogInt, rc, "failed to create contained file node" );
            else
            {
                int len;
                char newname [ 256 ];

                /* invent a new name for file */
                const char *ext = strrchr ( name, '.' );
                if ( ext == NULL )
                    len = snprintf ( newname, sizeof newname, "%s", name );
                else if ( strcasecmp ( ext, ".tgz" ) == 0 )
                    len = snprintf ( newname, sizeof newname, "%.*s.tar", ( int ) ( ext - name ), name );
                else
                    len = snprintf ( newname, sizeof newname, "%.*s", ( int ) ( ext - name ), name );

                if ( len < 0 || len >= sizeof newname )
                {
                    rc = RC ( rcExe, rcNode, rcConstructing, rcName, rcExcessive );
                    LOGERR ( klogErr, rc, "failed to create contained file node" );
                }
                else
                {
                    void * save;
                    const KFile * cf;
                    /* rc_t krc; */

                    copycat_log_set (&nnode->logs, &save);

                    rc = CCFileMakeRead (&cf, zf, &node->rc);
                    if (rc == 0)
                    {
                        /* recurse with buffer on decompression */
                        /* krc = */ ccat_buf (& cont -> sub, cf, mtime,
                                        ccContFile, nnode, newname);

                        KFileRelease (cf);
                        /* if successful, "node" ( allocated locally above )
                           will have been entered into "cont->sub" */
                    }

                    copycat_log_set (save, NULL);
                }
            }
        }

        KFileRelease ( zf );
    }

    return rc;
}

static
rc_t ccat_path_append (const char * name)
{
    size_t z;
    z = string_size (name);
    DEBUG_STATUS (("%s: in epath %s name %s z %zu \n",__func__, epath, name, z));
    if (ehere + z + 1 >= epath + sizeof (epath))
        return RC (rcExe, rcNoTarg, rcConcatenating, rcBuffer, rcTooShort);
    memmove(ehere, name, z);
    ehere += z;
    *ehere++ = '/';
    *ehere = '\0';
    DEBUG_STATUS (("%s: out name %s epath %s\n",__func__,name, epath));
    return 0;
}

static
rc_t ccat_main ( CCTree *tree, const KFile *sf, KTime_t mtime,
                 enum CCType ntype, CCFileNode *node, const char *name )
{
    /* the pointer e_go_back allows us to remove additions to the stored
     * path built up as we descend into deeper container/archive/directories */
    char * e_go_back = ehere;

    /* determine file type based upon contents and name */
    uint32_t type_id, class_id;
/*     rc_t orc; */
    rc_t rc = CCFileFormatGetType ( filefmt, sf, name, node, & type_id, & class_id );

    DEBUG_STATUS (("%s: name '%s' type '%s'\n",__func__,name,node->ftype));

    if ( rc != 0 )
        PLOGERR ( klogErr,  (klogErr, rc, "failed to determine type of file '$(path)'", "path=%s", name ));
    else
    {
        /* file could be a container */
        CCContainerNode *cont = NULL;
        CCCachedFileNode *cfile = NULL;

        /* assume this node will get name */
        void *entry = node;

        bool xml_insert = false;
        bool tee_done = false;
        /* create path */
        const char * basename;
        char path [ 8192 + 256 ];

        if ((basename = strrchr (name, '/')) != NULL)
            ++basename;
        else
            basename = name;


        /* look for special files */
        switch ( class_id )
        {
        case ccffcEncoded:
            rc = ccat_path_append (name);
            if (rc == 0)
            {
                rc = ccat_enc ( & cont, sf, mtime, ntype, node, basename, type_id );
                if ( rc == 0 && cont != NULL )
                {
                    ntype = ccContainer;
                    entry = cont;
                }
            }
            ehere = e_go_back;
            *ehere = '\0';
            break;
        case ccffcCompressed:
            rc = ccat_path_append (name);
            if (rc == 0)
            {
                rc = ccat_cmp ( & cont, sf, mtime, ntype, node, basename, type_id );
                if ( rc == 0 && cont != NULL )
                {
                    ntype = ccContainer;
                    entry = cont;
                }
            }
            ehere = e_go_back;
            *ehere = '\0';
            break;
        case ccffcArchive:
            rc = ccat_path_append (name);
            if (rc == 0)
            {
                rc = ccat_arc ( & cont, sf, mtime, ntype, node, basename, type_id );
                if ( rc == 0 && cont != NULL )
                {
                    ntype = ccArchive;
                    entry = cont;
                }
            }
            ehere = e_go_back;
            *ehere = '\0';
            break;
        case ccffcCached:
            if ( cdir != NULL )
            {
                rc = ccat_cache ( & cfile, sf, ntype, node, basename );
                if ( rc == 0 && cont != NULL )
                {
                    tee_done = true;
                    ntype = ccCached;
                    entry = cfile;
                }
            }
            /* fall through */
        default:
            rc = ccat_extract_path (path, sizeof path, name);
            DEBUG_STATUS (("%s: extract_path '%s'\n",__func__,path));
            if ((edir != NULL) && (! tee_done))
            {
                rc = ccat_extract (sf, path);
                if (rc == 0)
                    tee_done = true;
            }
            if (rc == 0)
            {
                xml_insert = true;
            }
            if (!tee_done)
            {
                const KFile * cf;

                rc = CCFileMakeRead (&cf, sf, &node->rc);
                if (rc == 0)
                {
                    const KFile * tee;

                    rc = KFileMakeTeeRead (&tee, cf, fnull);
                    if (rc == 0)
                    {
                        rc = KFileAddRef (fnull);

                        KFileRelease (tee);
                    }
                }
            }
            break;
        }

        /* create entry into tree */
        if ( rc == 0 )
        {
            if (! xml_dir)
            {
                DEBUG_STATUS (("%s: ready to insert '%s'\n",__func__,name));
                rc = CCTreeInsert ( tree, mtime, ntype, entry, name );
                /* if we are extracting create a name node with other information being incorrect */
                if (edir != NULL)
                {
                }
            }
            else if (xml_insert)
            {
                DEBUG_STATUS (("%s: ready to insert '%s' for '%s'\n",__func__,path, name));
                rc = CCTreeInsert (etree, mtime, ntype, entry, path);
            }

            if ( rc != 0 )
                PLOGERR (klogInt,  (klogInt, rc,
                                    "failed to enter node '$(name)'",
                                    "name=%s", name ));
        }
    }

    return rc;
}

static
rc_t ccat_sz ( CCTree *tree, const KFile *sf, KTime_t mtime,
               enum CCType ntype, CCFileNode *node, const char *name )
{
    /* create a counting file to fill out its size */
    const KFile *sz;
    rc_t rc, orc;

    rc = KFileMakeCounterRead ( & sz, sf, & node -> size, & node -> lines, true );


    if ( rc != 0 )
        PLOGERR ( klogInt,  (klogInt, rc, "failed to create counting wrapper for '$(path)'", "path=%s", name ));
    else
    {
        /* give the wrapper its own reference
           rather than taking the one we gave it */
        rc = KFileAddRef ( sf );
        if (rc)
            PLOGERR (klogInt, 
                     (klogInt, rc, "Error in reference counting for '$(path)'", "path=%s", name ));
        else
            /* the main catalog function */
            rc = ccat_main ( tree, sz, mtime, ntype, node, name );

        /* release the sizer */
        orc = KFileRelease ( sz );
        if (orc)
        {
            PLOGERR (klogInt, 
                     (klogInt, orc, "Error in closing byte counting for '$(path)'", "path=%s", name ));
            if (rc == 0)
                rc = orc;
        }
    }

    return rc;
}

rc_t ccat_md5 ( CCTree *tree, const KFile *sf, KTime_t mtime,
                enum CCType ntype, CCFileNode *node, const char *name )
{
    /* all files have an MD5 hash for identification.
       the following code is for expediency.
       we already have a formatter for output,
       and to reuse it we simply pipe its output
       to /dev/null, so to speak */
    KMD5SumFmt *fmt;
    rc_t rc, orc;

    /* NEW - there are some cases where md5sums would not be useful
       and only take up CPU power. */
    if ( no_md5 )
        return ccat_sz ( tree, sf, mtime, ntype, node, name );

    /* normal md5 path */
    rc = KMD5SumFmtMakeUpdate ( & fmt, fnull );
    if ( rc != 0 )
        PLOGERR ( klogInt,  (klogInt, rc, "failed to create md5sum formatter for '$(path)'", "path=%s", name ));
    else
    {
        const KFile *md5;

        /* give another fnull reference to formatter */
        rc = KFileAddRef ( fnull );
        if (rc)
            LOGERR (klogInt, rc, "Error referencing MD5 format file");
        else
        {

            /* this is the wrapper that calculates MD5 */
            rc = KFileMakeNewMD5Read ( & md5, sf, fmt, name );
            if ( rc != 0 )
                PLOGERR ( klogInt,  (klogInt, rc, "failed to create md5 wrapper for '$(path)'", "path=%s", name ));
            else
            {
                /* give the wrapper its own reference
                   rather than taking the one we gave it */
                rc = KFileAddRef ( sf );
                if (rc)
                    PLOGERR (klogInt,
                             (klogInt, rc,
                              "failure in reference counting file for '$(path)'",
                              "path=%s", name ));
                else
                {
                    /* continue on to obtaining file size */
                    rc = ccat_sz ( tree, md5, mtime, ntype, node, name );

                    /* this will drop the MD5 calculator, but not
                       its source file, and cause the digest to be
                       written to its formatter */
                    orc = KFileRelease ( md5 );
                    if (orc)
                    {
                        PLOGERR (klogInt,
                                 (klogInt, rc,
                                  "failure in release reference counting file for '$(path)'",
                                  "path=%s", name ));
                        if (rc == 0)
                            rc = orc;
                    }

                    /* if there were no errors, read the MD5 from formatter.
                       this must be done AFTER releasing the MD5 file,
                       or nothing will ever get written */
                    if ( rc == 0 )
                    {
                        bool bin;
                        rc = KMD5SumFmtFind ( fmt, name, node -> _md5, & bin );
                    }
                }
            }
        }

        /* dump the formatter, but not fnull */
        orc = KMD5SumFmtRelease ( fmt );
        if (orc)
        {
            PLOGERR (klogInt,
                     (klogInt, rc,
                      "failure in releasing  MD5 format file for '$(path)'",
                      "path=%s", name ));
            if (rc == 0)
                rc = orc;
        }
    }

    return rc;
}

/* buffered recursion entrypoint */
rc_t ccat_buf ( CCTree *tree, const KFile *sf, KTime_t mtime,
                enum CCType ntype, CCFileNode *node, const char *name )
{
    /* create a buffered file to counter random access */
    const KFile *buf;
    rc_t rc, orc;

#if USE_KBUFFILE
    rc = KBufFileMakeRead (&buf, sf, 2 * 32 * 1024);
#else
    rc = KFileMakeReadHead (&buf, sf, 4096);
#endif
    if ( rc != 0 )
        PLOGERR ( klogInt,  (klogInt, rc,
                             "failed to create buffer for '$(path)'",
                             "path=%s", name ));
    else
    {
        /* skip ccat */
        rc = ccat_md5 ( tree, buf, mtime, ntype, node, name );

        /* release the buffer */
        orc = KFileRelease ( buf );
        if (orc)
        {
            PLOGERR (klogInt, (klogInt, orc, "Error closing buffer for '$(path)'", "path=%s", name ));
            if (rc == 0)
                rc = orc;
        }
    }
    return rc;
}


typedef struct copycat_pb
{
    CCTree * tree;
    const KFile * sf;
    KFile * df;
    KTime_t mtime;
    enum CCType ntype;
    CCFileNode * node;
    const char * name;
} copycat_pb;


rc_t copycat_add_tee (const copycat_pb * ppb)
{
    const KFile * tee;
    rc_t rc, orc;

    rc = KFileMakeTeeRead (&tee, ppb->sf, ppb->df);
    if (rc)
        PLOGERR (klogInt,  
                 (klogInt, rc, "failed to create encrypter for '$(path)'",
                  "path=%s", ppb->name ));
    else
    {
        rc = KFileAddRef (ppb->df);
        if (rc)
            LOGERR (klogInt, rc, "Reference counting error");
        else
        {
            rc = KFileAddRef (ppb->sf);
            if (rc)
                LOGERR (klogInt, rc, "Reference counting error");
            else
            {
                orc = ccat_md5 (ppb->tree, tee, ppb->mtime, ppb->ntype, ppb->node, ppb->name);

                /* report? */
                orc = KFileRelease (tee);
                if (orc)
                    PLOGERR (klogInt, 
                             (klogInt, orc,
                              "Error in closing byte counting for '$(path)'",
                              "path=%s", ppb->name ));

/*                 orc = KFileRelease (ppb->sf); */
/*                 if (orc) */
/*                     PLOGERR (klogInt,  */
/*                              (klogInt, orc, */
/*                               "Error in closing byte counting of tee source for '$(path)'", */
/*                               "path=%s", ppb->name )); */
            }
/*             orc = KFileRelease (ppb->df); */
/*             if (orc) */
/*                 PLOGERR (klogInt,  */
/*                          (klogInt, orc, */
/*                           "Error in closing byte counting of tee destination for '$(path)'", */
/*                           "path=%s", ppb->name )); */
        }       
    }
    return rc;
}


static
rc_t copycat_add_node (const copycat_pb * ppb)
{
    rc_t rc;

    rc = ccat_path_append (ppb->name);
    if (rc == 0)
    {
        CCContainerNode *cont;

        /* make a container node for the encrypted output file */
        rc = CCContainerNodeMake (&cont, ccFile, ppb->node);
        if (rc)
        {
            PLOGERR (klogInt,
                     (klogInt, rc, "error creating container node for '$(path)'",
                      "path=%s", ppb->name));
        }
        else
        {
            copycat_pb pb = *ppb;
            uint64_t expected = /*(ppb->node->expected != SIZE_UNKNOWN)
                                  ? ppb->node->expected :*/ SIZE_UNKNOWN;
            pb.tree = &cont->sub;

            /* this will be the node for the unencrypted version of the output file */
            rc = CCFileNodeMake (&pb.node, expected );
            if (rc)
                LOGERR (klogInt, rc, "failed to create contained node");
            else
            {
                void * save;
                size_t len;
                size_t elen;
                size_t clen;
                rc_t orc;
                char name [ 1024 ];

                len = strlen (pb.name);
                elen = strlen (ncbi_encryption_extension);
                clen = len - elen;

                /* 
                 *if the name had .ncbi_enc at the end remove it
                 * for the inner node.
                 */
                if (strcmp (ncbi_encryption_extension, pb.name + clen) == 0)
                {
                    memmove (name, pb.name, clen);
                    name[clen] = '\0';
                }
                /*
                 * if it did not, prepend '.' to the name
                 */
                else
                {
                    name[0] = '.';
                    if (len > sizeof name - 2)
                        len = sizeof name - 2;
                    strncpy (name+1, pb.name, len);
                    name[len+1] = '\0';
                }
                pb.name = name;

                copycat_log_set (&pb.node->logs, &save);

                rc = copycat_add_tee (&pb);

                copycat_log_set (save, NULL);

                orc = CCTreeInsert (ppb->tree, ppb->mtime, ccContainer, cont, ppb->name);

                if (rc == 0)
                    rc = orc;
            }
        }
    }
    return rc;
}

/* -----
 * copycat_add_enc
 *
 * add an encryptor to the copy chain
 */
rc_t copycat_add_enc (const copycat_pb * ppb)
{
    copycat_pb pb = *ppb;
    rc_t rc = 0;

    if (!dst_pw_read)
        rc = get_password (dst_path, dst_pwd, sizeof dst_pwd, &dst_pwd_sz, &dst_key, &dst_pw_read);
    if (rc)
        return rc;

    rc = KEncFileMakeWrite (&pb.df, ppb->df, &dst_key);
    if (rc)
        PLOGERR (klogInt,
                 (klogInt, rc, "failed to create encrypter for '$(path)'",
                  "path=%s", pb.name ));
    else
    {
        strncpy (pb.node->ftype, "Encoded/NCBI", sizeof pb.node->ftype);


        /* add the container to the cataloging tree */
        rc = copycat_add_node (&pb);
        if (rc == 0)
        {
            rc = KFileRelease (pb.df);
            if (rc)
            {
                PLOGERR (klogInt, 
                         (klogInt, rc,
                          "Error in closing byte counting for '$(path)'",
                          "path=%s", pb.name ));
            }
        }
    }
    return rc;
}


/* -----
 * copycat_add_sz
 *
 * if we are encrypting the output we need to do an actual byte count across the
 * write side of the encryption so we add a filter into the copy chain.  If we
 * are not encrypting we do this calculation in the ccat chain for the outside file
 * and never reach this function.
 */
rc_t copycat_add_sz (const copycat_pb * ppb)
{
    /* create a counting file to fill out its size */
    copycat_pb pb = *ppb;
    rc_t rc, orc;

    rc = KFileMakeCounterWrite (&pb.df, ppb->df, &pb.node->size, &pb.node->lines, true);
    if (rc)
        PLOGERR (klogInt,
                 (klogInt, rc,
                  "failed to create counting wrapper for '$(path)'",
                  "path=%s", pb.name ));
    else
    {
        /* give the wrapper its own reference
           rather than taking the one we gave it */
        rc = KFileAddRef ( ppb->df );
        if (rc)
            PLOGERR (klogInt, 
                     (klogInt, rc, "Error in reference counting for '$(path)'",
                      "path=%s", pb.name));
        else
            rc = copycat_add_enc (&pb);

        /* release the sizer */
        orc = KFileRelease (pb.df);
        if (orc)
        {
            PLOGERR (klogInt, 
                     (klogInt, orc, "Error in closing byte counting "
                      "(probable read error) for '$(path)'", "path=%s",
                      pb.name ));
            /* a failure in the counter destructor might mean a failure
             * in the copy so return the error */
            if (rc == 0)
                rc = orc;
        }
    }

    return rc;
}


/* -----
 * copycat_add_md5
 *
 * if we are encrypting the output we need to do an md5 calculation across the
 * write side of the encryption so we add a filter into the copy chain.  If we
 * are not encrypting we do the md5 calculation in the ccat chain for the outside file
 * and never reach this function.
 */
rc_t copycat_add_md5 (const copycat_pb * ppb)
{
    /* all files have an MD5 hash for identification.
       the following code is for expediency.
       we already have a formatter for output,
       and to reuse it we simply pipe its output
       to /dev/null, so to speak */
    KMD5SumFmt *fmt;
    rc_t rc;

    if ( no_md5 )
        return copycat_add_sz (ppb);

    rc = KMD5SumFmtMakeUpdate (&fmt, fnull);
    if ( rc != 0 )
        PLOGERR (klogInt,
                 (klogInt, rc,
                  "failed to create md5sum formatter for '$(path)'",
                  "path=%s", ppb->name ));
    else
    {
        rc_t orc;
        /* give another fnull reference to formatter */
        rc = KFileAddRef (fnull);
        if (rc)
            LOGERR (klogInt, rc, "Error referencing MD5 format file");
        else
        {
            copycat_pb pb = *ppb;

            /* this is the wrapper that calculates MD5 */
            rc = KMD5FileMakeWrite ((struct KMD5File**)&pb.df, ppb->df, fmt, ppb->name );
            if ( rc != 0 )
                PLOGERR (klogInt,
                         (klogInt, rc,
                          "failed to create md5 wrapper for '$(path)'",
                          "path=%s", ppb->name ));
            else
            {
                /* give the wrapper its own reference
                   rather than taking the one we gave it */
                rc = KFileAddRef (ppb->df);
                if (rc)
                    PLOGERR (klogInt,
                             (klogInt, rc,
                              "failure in reference counting file for '$(path)'",
                              "path=%s", ppb->name ));
                else
                    /* continue on to obtaining file size */
                    rc = copycat_add_sz (&pb);

                /* this will drop the MD5 calculator, but not
                   its source file, and cause the digest to be
                   written to its formatter */
                orc = KFileRelease (pb.df);
                if (orc)
                {
                    PLOGERR (klogInt,
                             (klogInt, orc,
                              "failure in releasing md5 calculation file for '$(path)'",
                              "path=%s", ppb->name ));
                }

                /* if there were no errors, read the MD5 from formatter.
                   this must be done AFTER releasing the MD5 file,
                   or nothing will ever get written */
                if (orc == 0)
                {
                    bool bin;
                    orc = KMD5SumFmtFind (fmt, ppb->name, ppb->node->_md5, &bin);
                    if (orc)
                        PLOGERR (klogWarn,
                                 (klogWarn, orc,
                                  "Error in obtaining the MD5 for '$(path)'",
                                  "path=%s", pb.name));
                }
            }
        }

        /* dump the formatter, but not fnull */
        orc = KMD5SumFmtRelease (fmt);
        if (orc)
        {
            PLOGERR (klogInt,
                     (klogInt, rc,
                      "failure in releasing  MD5 format file for '$(path)'",
                      "path=%s", ppb->name ));
            /* we can 'forget' this error after logging it */
            if (rc == 0)
                rc = orc;
        }
    }

    return rc;
}

/* -----
 * copycat_add_crc
 *
 * this is the first function in building the write side of the copy chain
 *
 * We calculate a crc on the outgoing file and none of the interior files
 * and we do that at this point in the chain regardless of whether we will
 * encrypt the outgoing file.
 *
 * We'll add the crc calculator to the write side stream and then decide
 * whether to add an encryptor into the chain.  We add one to the write side
 * of the chain if we have an encrypting password or jump to finishing the
 * copy chain if we do not
 */
rc_t copycat_add_crc (const  copycat_pb * ppb)
{
    /* external files have a CRC32 checksum
       the following code is for expediency.
       we already have a formatter for output,
       and to reuse it we simply pipe its output
       to /dev/null, so to speak */
    rc_t rc;
    KCRC32SumFmt * fmt; 

    rc = KCRC32SumFmtMakeUpdate ( &fmt, fnull );
    if ( rc != 0 )
        PLOGERR (klogInt,
                 (klogInt, rc,
                  "failed to create crc32sum formatter for '$(path)'",
                  "path=%s", ppb->name ));
    else
    {
        rc_t orc = 0;

        /* give another fnull reference to formatter */
        rc = KFileAddRef ( fnull );
        if (rc)
            PLOGERR (klogInt,
                     (klogInt, rc, 
                      "error in reference counting crc formatter fnull for '$(path)'",
                      "path=%s", ppb->name ));
        else
        {
            copycat_pb pb = *ppb;

            /* this is the wrapper that calculates CRC32 */
            rc = KCRC32FileMakeWrite ( (KCRC32File**)&pb.df, ppb->df, fmt, ppb->name );
            if ( rc != 0 )
                PLOGERR (klogInt,
                         (klogInt, rc,
                          "failed to create crc32 wrapper for '$(path)'",
                          "path=%s", ppb->name ));
            else
            {
                /* give the wrapper its own reference
                   rather than taking the one we gave it */
                rc = KFileAddRef (ppb->df);
                if (rc)
                    PLOGERR (klogInt,
                             (klogInt, rc, 
                              "error in reference counting file for '$(path)'",
                              "path=%s", ppb->name ));
                else if (do_encrypt)
                {
                    /* add in the size of the enc header */
                    if (pb.node->expected != SIZE_UNKNOWN)
                    {
                        uint64_t temp;

                        temp = pb.node->expected; /* current expected count */

                        temp += (ENC_DATA_BLOCK_SIZE - 1); /* add enough to fill last block */
                        temp /= ENC_DATA_BLOCK_SIZE; /* how many blocks */
                        temp *= sizeof (KEncFileBlock); /* size of encrypted blocks */
                        temp += sizeof (KEncFileHeader) + sizeof (KEncFileFooter);
                        pb.node->expected = temp;
                    }
                    rc = copycat_add_md5 (&pb);
                }
                else
                    rc = copycat_add_tee (&pb);

                /* this will drop the CRC calculator, but not
                   its source file, and cause the CRC to be
                   written to its formatter */
                orc = KFileRelease (pb.df);
                if (orc)
                {
                    LOGERR (klogErr, orc, "Failed to close out crc calculator");
                    /* an error here implies an error in the copy so report it */
                    rc = orc;
                }
            }
            /* if there were no errors, read the CRC from formatter.
               this must be done AFTER releasing the CRC file,
               or nothing will ever get written */
            if ( rc == 0 )
            {
                bool bin;
                orc = KCRC32SumFmtFind ( fmt, ppb->name, &pb.node->crc32, &bin );
                if (orc)
                    PLOGERR (klogWarn,
                             (klogWarn, orc,
                              "Failed to obtain the CRC for '$(path)'",
                              "path=%s", pb.name));
                /* an error here isn't an error in copy */
            }
        }
        /* dump the formatter, but not fnull */
        orc = KCRC32SumFmtRelease ( fmt );
        if (orc)
            LOGERR (klogWarn, orc, "Failed to close off CRC storage");
        /* this error we do not need to track */
        if (rc == 0)
            rc = orc;
    }
    return rc;
}


/* -----
 * copycat_add_buf2
 *
 * This function adds buffering to the outermostDecrypted file read so that we
 * can read the first portion of the file for file type analysis and then
 * re-read it for cataloging.
 *
 * At this point the only file type analysis needed is to determine
 * whether we have in incoming encrypted file We've already checked for
 * the existence of the key or we wouldn't get here.
 */
rc_t copycat_add_buf2 (const copycat_pb * ppb)
{
    copycat_pb pb = *ppb;
    rc_t rc = 0;

    /* create a buffered file to allow re-reading of the first 128 bytes
     * a tad overkill */
#if USE_KBUFFILE
    rc = KBufFileMakeRead (&pb.sf, ppb->sf, 2*32*1024);
#else
    rc = KFileMakeReadHead (&pb.sf, ppb->sf, 4*1024);
#endif
    if (rc)
    {
        PLOGERR (klogInt,  
                 (klogInt, rc,
                  "failed to create buffer for '$(path)'",
                  "path=%s", pb.name ));
    }
    else
    {
        rc_t orc;

        rc = copycat_add_crc (&pb);

        orc = KFileRelease (pb.sf);
        if (orc)
            PLOGERR (klogInt,  
                     (klogInt, orc,
                      "failed to read buffer for '$(path)'",
                      "path=%s", pb.name ));
    }
    return rc;
}


/* -----
 * copycat_add_dec
 *
 * add a decryption to the read side of the copy chain.
 * no other decisions made here
 */
rc_t copycat_add_dec_ncbi (const copycat_pb * ppb)
{
    copycat_pb pb = *ppb;
    rc_t rc = 0;

    if (!src_pw_read)
        rc = get_password (src_path, src_pwd, sizeof src_pwd, &src_pwd_sz, &src_key, &src_pw_read);

    if (rc == 0)
    {
        rc = KEncFileMakeRead (&pb.sf, ppb->sf, &src_key);
        if (rc)
            PLOGERR (klogInt,  
                     (klogInt, rc,
                      "failed to create decrypter for '$(path)'",
                      "path=%s", pb.name ));
    }
    if (rc == 0)
    {
        rc_t orc;

        /* this decryption won't know its output size until it gets there */
        pb.node->expected = SIZE_UNKNOWN;
        rc = copycat_add_buf2 (&pb);
        orc = KFileRelease (pb.sf);
        if (orc)
            PLOGERR (klogInt,  
                     (klogInt, orc,
                      "failed to close decrypter for '$(path)'",
                      "path=%s", pb.name ));
    }
    return rc;
}

rc_t copycat_add_dec_wga (const copycat_pb * ppb)
{
    copycat_pb pb = *ppb;
    rc_t rc = 0;

    if (!src_pw_read)
        rc = wga_password (src_path, src_pwd, sizeof src_pwd, &src_pwd_sz, &src_pw_read);

    if (rc == 0)
    {
        rc = KFileMakeWGAEncRead (&pb.sf, ppb->sf, src_pwd, src_pwd_sz);
        if (rc)
            PLOGERR (klogInt,  
                     (klogInt, rc,
                      "failed to create decrypter for '$(path)'",
                      "path=%s", pb.name ));
    }
    if (rc == 0)
    {
        rc_t orc;

        if (pb.node->expected != SIZE_UNKNOWN)
            pb.node->expected -= 128; /* subtract off the size of the WGA header */
        rc = copycat_add_buf2 (&pb);
        orc = KFileRelease (pb.sf);
        if (orc)
            PLOGERR (klogInt,  
                     (klogInt, orc,
                      "failed to close decrypter for '$(path)'",
                      "path=%s", pb.name ));
    }
    return rc;
}

/* -----
 * copycat_add_dec
 *
 * At this point the only file type analysis needed is to determine
 * whether we have in incoming encrypted file We've already checked for
 * the existence of the key or we wouldn't get here.
 */
rc_t copycat_add_dec (const copycat_pb * ppb)
{
    copycat_pb pb = *ppb;
    rc_t rc = 0;
    size_t num_read;
    uint8_t buff [128];

    rc = KFileReadAll (pb.sf, 0, buff, sizeof buff, &num_read);
    if (rc)
        PLOGERR (klogInt,  
                 (klogInt, rc,
                  "failed to read buffer for '$(path)'",
                  "path=%s", pb.name ));
    else
    {
        rc_t orc;

        /* 
         * if we have an encrypted file add decryption to the chain
         * if not jump sraight to the write side of the chain
         */
        if (CCFileFormatIsNCBIEncrypted (buff))
            rc = copycat_add_dec_ncbi (&pb);
        else if (KFileIsWGAEnc (buff, num_read) == 0)
            rc = copycat_add_dec_wga (&pb);
        else
            rc = copycat_add_crc (&pb);

        orc = KFileRelease (pb.sf);
        if (orc)
            PLOGERR (klogInt,  
                     (klogInt, orc,
                      "failed to read buffer for '$(path)'",
                      "path=%s", pb.name ));
    }
    return rc;
}


/* -----
 * copycat
 *
 * The copycat function is the actual copy and catalog function.
 * All before this function is called is building toward this.
 *
 * The functions prefixed copycat_add_ are functions used in building the 
 * chain of KFS and other filters for doing the copy function.  Some 
 * cataloging is included where the output file is encoded and to
 * generate the CRC unique to the outer file in the catalog.
 * The ccat_add_ functions are filters toward the leafs in the catlog 
 * portion of the program.
 *
 * Much of this could be inlined but the indention creep across the screen
 * and the handling of levels of indention would get intense.  Also some
 * filters are skipped which also would have made it more unweildy to 
 * write and maintain.
 *
 * In this function we build a node for cataloging the outermost layer of
 * the output file which may differ from the file as read here if a
 * decryption and/or encryption is added.
 *
 * We then
 */
rc_t copycat (CCTree *tree, KTime_t mtime, KDirectory * _cwd,
              const VPath * src, const KFile *sf,
              const VPath * dst,  KFile *df,
              const char *spath, const char *name,
              uint64_t expected, bool _do_decrypt, bool _do_encrypt)
{
    void * save;
    copycat_pb pb;
    rc_t rc;

    memset(&pb, 0, sizeof(pb));
    DEBUG_STATUS (("%s: copy file %s\n",__func__, spath));

    cwd = _cwd;
    src_path = src;
    dst_path = dst;

    src_pw_read = false;

    do_decrypt = _do_decrypt;
    do_encrypt = _do_encrypt;

    /* -----
     * Create a cataloging node for the outer most file as written
     */
    rc = CCFileNodeMake ( &pb.node, expected );
    if (rc)
    {
        LOGERR ( klogInt, rc, "failed to allocate file node" );
        return rc;
    }

    pb.tree = tree;
    pb.mtime = mtime;
    pb.ntype = ccFile;
    pb.name = name;


    copycat_log_set (&pb.node->logs, &save);

    if (out_block)
    {
        rc = KBufWriteFileMakeWrite (&pb.df, df, out_block);
        if (rc)
        {
            PLOGERR (klogInt,  
                     (klogInt, rc,
                      "failed to create buffer for '$(path)'",
                      "path=%s", pb.name ));
            return rc;
        }
    }
    else
    {
        pb.df = df;
        KFileAddRef(pb.df);
    }

    if (in_block)
    {
        rc = KBufReadFileMakeRead (&pb.sf, sf, in_block);
    }
    else
    {
#if USE_KBUFFILE
        rc = KBufFileMakeRead (&pb.sf, sf, 2*32*1024);
#else
        rc = KFileMakeReadHead (&pb.sf, sf, 4*1024);
#endif
    }
    if (rc)
    {
        PLOGERR (klogInt,  
                 (klogInt, rc,
                  "failed to create buffer for '$(path)'",
                  "path=%s", spath ));
    }
    else
    {
        /* 
         * if we have a decryption password prepare a decryption read path
         * if not jump to preparing the write path
         */
        rc = do_decrypt
            ? copycat_add_dec (&pb)
            : copycat_add_crc (&pb);
    }
    copycat_log_set (save, NULL);
    
    KFileRelease(pb.df);
    KFileRelease(pb.sf);

    return rc;
}
