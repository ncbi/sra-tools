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
#include <kapp/main.h>
#include <klib/log.h>
#include <klib/rc.h>
#include <klib/namelist.h>
#include <klib/container.h>
#include <klib/debug.h>
#include <kdb/manager.h>
#include <kdb/database.h>
#include <kdb/table.h>
#include <kdb/column.h>
#include <kdb/namelist.h>
#include <kdb/meta.h>
#include <kfs/directory.h>
#include <kfs/file.h>
#include <kfs/md5.h>
#include <kfs/arc.h>
#include <kfs/sra.h>
#include <sysalloc.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

rc_t CC Usage ( struct Args const * args ) { return 0; }

static rc_t ValidateColumn(const KColumn *col,
                           const char *name,
                           int64_t start, int64_t stop,
                           int64_t *min, int64_t *max,
                           bool *gapped)
{
    rc_t rc;
    int64_t row;
    const KColumnBlob *blob;
    
    for (row = start; (rc = Quitting()) == 0 && row < stop; ) {
        int64_t first;
        uint32_t count;
        
        rc = KColumnOpenBlobRead(col, &blob, row);
        if (rc == 0) {
            rc = KColumnBlobIdRange(blob, &first, &count);
            if (rc == 0)
                rc = KColumnBlobValidate(blob);
            rc = KColumnBlobRelease(blob);
        }
        if (rc) {
            PLOGERR(klogErr, (klogErr, rc, "$(column) rows $(start) to $(stop)", PLOG_3(PLOG_S(column), PLOG_I64(start), PLOG_I64(stop)), name, first, first + count));
            return rc;
        }
        if (row != first)
            *gapped |= true;
        if (row == start)
            *min = first;
        row = first + count;
        *max = row;
    }
    if (row != stop)
        *gapped |= true;
    return rc;
}

static
rc_t CheckTable(const char path[])
{
    rc_t rc;
    const KDBManager *mgr;
    bool gapped = false;
    bool allsame = true;
    int64_t first_row;
    int64_t last_row;
    
    PLOGMSG(klogInfo, (klogInfo, "verifying column data for '$(path)'", PLOG_S(path), path));
    rc = KDBManagerMakeRead(&mgr, NULL);
    if (rc == 0) {
        const KTable* tbl;
        
        rc = KDBManagerOpenTableRead(mgr, &tbl, "%s", path);
        if (rc == 0) {
            KNamelist *names;
            
            rc = KTableListCol(tbl, &names);
            if (rc == 0) {
                uint32_t count;
                rc = KNamelistCount(names, &count);
                if (rc == 0) {
                    unsigned i;
                    const char *colname;
                    
                    for (i = 0; i != count && rc == 0; ++i) {
                        rc = KNamelistGet(names, i, &colname);
                        if (rc == 0) {
                            const KColumn *kcol;
                            
                            rc = KTableOpenColumnRead(tbl, &kcol, "%s", colname);
                            if (rc == 0) {
                                int64_t start;
                                uint64_t rcount;
                                int64_t min;
                                int64_t max;
                                
                                rc = KColumnIdRange(kcol, &start, &rcount);
                                if (rc == 0) {
                                    PLOGMSG(klogInfo, (klogInfo, "validating data for column '$(name)'...", PLOG_S(name), colname));
                                    rc = ValidateColumn(kcol, colname, start, start + rcount, &min, &max, &gapped);
                                    if (rc == 0) {
                                        PLOGMSG(klogInfo, (klogInfo, "data for column '$(name)' ok", PLOG_S(name), colname));
                                        if (i == 0) {
                                            first_row = min;
                                            last_row = max;
                                        }
                                        else if (first_row != min || last_row != max)
                                            allsame = false;
                                    }
                                    else {
                                        PLOGERR(klogErr, (klogErr, rc, "data for column '$(name)'", PLOG_S(name), colname));
                                    }

                                    rc = 0;
                                }
                                else {
                                    PLOGERR(klogErr, (klogErr, rc, "KColumn '$(name)'", PLOG_S(name), colname));
                                    rc = 0;
                                }
                                KColumnRelease(kcol);
                            }
                            else {
                                PLOGERR(klogErr, (klogErr, rc, "KColumn '$(name)'", PLOG_S(name), colname));
                                rc = 0;
                            }
                        }
                        else {
                            PLOGERR(klogErr, (klogErr, rc, "KColumn #$(number)", PLOG_U32(number), (uint32_t)i));
                            rc = 0;
                        }
                    }
                }
                else {
                    PLOGERR(klogErr, (klogErr, rc, "KNamelistCount", ""));
                }
            }
            KTableRelease(tbl);
        }
        else {
            PLOGERR(klogErr, (klogErr, rc, "KTable '$(name)'", PLOG_S(name), path));
        }
        KDBManagerRelease(mgr);
    }
    else {
        PLOGERR(klogErr, (klogErr, rc, "KDBManagerMakeRead", ""));
    }
    return rc;
}

typedef struct CheckMD5Node_struct {
    BSTNode dad;
    char file[4096];
    bool md5;
    uint8_t digest[16];
} CheckMD5Node;

typedef struct CheckMD5_Data_struct {
    BSTree files;
    bool md5;
    uint8_t digest[16];
} CheckMD5_Data;

static
int CC CheckMD5Node_Cmp( const BSTNode *item, const BSTNode *n )
{
    return strcmp(((CheckMD5Node*)item)->file, ((CheckMD5Node*)n)->file);
}

static
rc_t CC CheckMD5Visitor(const KDirectory *dir, uint32_t type, const char *name, void *data)
{
    rc_t rc = 0;
    CheckMD5_Data* d = (CheckMD5_Data*)data;
    
    if(type == kptFile ) {
        if( strcmp(name, "md5") == 0 || strcmp(&name[strlen(name) - 4], ".md5") == 0 ) {
            const KFile* kfmd5;
            if( (rc = KDirectoryOpenFileRead(dir, &kfmd5, "%s", name)) == 0 ) {
                const KMD5SumFmt *md5;
                if( (rc = KMD5SumFmtMakeRead(&md5, kfmd5)) == 0 ) {
                    uint32_t count = 0;
                    if( (rc = KMD5SumFmtCount(md5, &count)) == 0 ) {
                        char buf[4096];
                        bool bin;
                        do {
                            if( (rc = KMD5SumFmtGet(md5, --count, buf, sizeof(buf) - 1, d->digest, &bin)) == 0 ) {
                                DBGMSG(DBG_APP, 0, ("md5 for $(nm) ", PLOG_S(nm), buf));
                                d->md5 = true;
                                rc = CheckMD5Visitor(dir, kptFile, buf, data);
                                d->md5 = false;
                            }
                        } while( rc == 0 && count > 0 );
                    }
                    rc = KMD5SumFmtRelease(md5);
                } else {
                    KFileRelease(kfmd5);
                }
            }
        } else {
            CheckMD5Node* node = calloc(1, sizeof(*node));
            if( (rc = KDirectoryResolvePath(dir, true, node->file, sizeof(node->file) - 1, "%s", name)) == 0 ) {
                BSTNode* existing = NULL;
                if( (rc = BSTreeInsertUnique(&d->files, &node->dad, &existing, CheckMD5Node_Cmp)) == 0 ) {
                    DBGMSG(DBG_APP, 0, ("adding $(nm) ", PLOG_S(nm), node->file));
                    if( d->md5 ) {
                        node->md5 = true;
                        memcpy(node->digest, d->digest, sizeof(d->digest));
                    }
                } else if( GetRCState(rc) == rcExists ) {
                    DBGMSG(DBG_APP, 0, ("updating $(nm) ", PLOG_S(nm), node->file));
                    if( d->md5 ) {
                        ((CheckMD5Node*)existing)->md5 = true;
                        memcpy(((CheckMD5Node*)existing)->digest, d->digest, sizeof(d->digest));
                    }
                    free(node);
                    rc = 0;
                }
                if( rc != 0 ) {
                    free(node);
                }
            }
        }
    } else if( (type != kptDir) && !(type & kptAlias) ) {
        rc = RC(rcExe, rcTable, rcVisiting, rcDirEntry, rcInvalid);
        PLOGERR(klogInfo, (klogInfo, rc, "$(nm)", PLOG_S(nm), name));
    }
    return rc;
}

typedef struct CheckMD5_WalkData_struct {
    const KDirectory* dir;
    rc_t rc;
} CheckMD5_WalkData;

static
bool CC CheckMD5_Walk( BSTNode *n, void *data )
{
    rc_t rc = 0;
    CheckMD5Node* m = (CheckMD5Node*)n;
    CheckMD5_WalkData* d = (CheckMD5_WalkData*)data;
    
    if( m->md5 ) {
        const KFile* kf;
        DBGMSG(DBG_APP, 0, ("verifying $(nm) ", PLOG_S(nm), m->file));
        if( (rc = KDirectoryOpenFileRead(d->dir, &kf, "%s", m->file)) == 0 ) {
            const KFile* kfmd5;
            if( (rc = KFileMakeMD5Read(&kfmd5, kf, m->digest)) == 0 ) {
                char buf[10240];
                size_t nr;
                if( (rc = KFileRead(kfmd5, ~0, buf, sizeof(buf), &nr)) != 0 ) {
                    PLOGERR(klogErr, (klogErr, rc, "MD5 check failed for file $(name)", PLOG_S(name), m->file));
                }
                KFileRelease(kfmd5);
            } else {
                KFileRelease(kf);
            }
        }
    } else if( strcmp(&m->file[strlen(m->file) - 5 ], "/lock") != 0 &&
              strcmp(&m->file[strlen(m->file) - 7 ], "/sealed") != 0) {
        rc = RC(rcExe, rcTable, rcValidating, rcConstraint, rcInconsistent);
        PLOGERR(klogWarn, (klogWarn, rc, "missing MD5 for file $(name)", PLOG_S(name), m->file));
    }
    d->rc = rc ? rc : d->rc;
    if( d->rc == 0 && (d->rc = Quitting()) != 0 ) {
        return true;
    }
    return false;
}

static
rc_t CheckMD5(const char* path)
{
    rc_t rc = 0;
    const KDirectory* dir = NULL;
    
    PLOGMSG(klogInfo, (klogInfo, "verifying MD5 for '$(path)'", PLOG_S(path), path));
    if( (rc = KDirectoryNativeDir((KDirectory**)&dir)) == 0 ) {
        uint32_t type = KDirectoryPathType(dir, "%s", path) & ~kptAlias;
        
        if( type == kptFile ) {
            const KDirectory* adir = NULL;
            
            rc = KDirectoryOpenSraArchiveRead(dir, &adir, true, "%s", path);
            if (rc == 0 ) {
                KDirectoryRelease(dir);
                dir = adir;
                path = ".";
            }
        } else if( type != kptDir ) {
            rc = RC(rcExe, rcTable, rcValidating, rcArc, rcUnknown);
        }
        if( rc == 0 ) {
            CheckMD5_Data data;
            BSTreeInit(&data.files);
            data.md5 = false;
            if( (rc = KDirectoryVisit(dir, true, CheckMD5Visitor, &data, "%s", path)) == 0 ) {
                CheckMD5_WalkData wd;
                wd.dir = dir;
                wd.rc = 0;
                BSTreeDoUntil(&data.files, false, CheckMD5_Walk, &wd);
                rc = wd.rc;
                LOGERR(rc ? klogErr : klogInfo, rc, "verifying MD5");
            }
        }
        KDirectoryRelease(dir);
    }
    return rc;
}


static char const* const defaultLogLevel = 
#if _DEBUGGING
"debug5";
#else
"info";
#endif

/*******************************************************************************
 * Usage
 *******************************************************************************/
static void usage(const char *progName)
{
    const char* p = strrchr(progName, '/');
    p = p ? p + 1 : progName;
    printf("\nUsage: %s [options] table\n\n", p);
    printf(
           "    -5, --md5        Check components md5s if present, fail otherwise, unless other checks are requested (default: yes)\n"
           "    -b, --blob-crc   Check blobs CRC32 (default: no)\n"
/*           "    -i, --index      Check 'skey' index (default: no)\n"*/
           "\n"
           "    -h, --help       This help\n"
           "    -v, --version    Program version\n"
           "\n");
}

rc_t CC KMain ( int argc, char *argv [] )
{
    rc_t rc = 0;
    int i;
    const char* arg;
    const char* src_path = "missing parameter";
    bool md5_chk = true, md5_chk_explicit = false;
    bool blob_crc = false;
    bool index_chk = false;

    KLogLevelSet(klogInfo);
    
    for(i = 1; rc == 0 && i < argc; i++) {
        if( argv[i][0] != '-' ) {
            src_path = argv[i];
            break;
        } else if(strcmp(argv[i], "-5") == 0 || strcmp(argv[i], "--md5") == 0 ) {
            md5_chk_explicit = true;
        } else if(strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--blob-crc") == 0 ) {
            blob_crc = true;
/*        } else if(strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--index") == 0 ) {
            index_chk = true;
*/
        } else if(strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0 ) {
            usage(argv[0]);
            return 0;
        } else if(strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0 ) {
            printf("Version: %u.%u.%u\n", KAppVersion() >> 24, (KAppVersion() >> 16) & 0xFF, KAppVersion() & 0xFFFF);
            return 0;
        } else {
            usage(argv[0]);
            rc = RC(rcExe, rcArgv, rcReading, rcParam, rcUnknown);
        }
    }
    if(i != argc - 1) {
        usage(argv[0]);
        rc = RC(rcExe, rcArgv, rcReading, rcParam, rcUnexpected);
    }
    
    if( rc != 0 ) {
    }
    else if( src_path == NULL ) {
        rc = RC(rcExe, rcArgv, rcReading, rcParam, rcNotFound);
    }
    else {
        if( blob_crc || index_chk ) {
            /* if blob or index is requested, md5 is off unless explicitly requested */
            md5_chk = md5_chk_explicit;
        }
        if( md5_chk ) {
            rc = CheckMD5(src_path);
            if( GetRCObject(rc) == rcConstraint && GetRCState(rc) == rcInconsistent ) {
                blob_crc = true;
                rc = 0;
            }
        }
        if (rc == 0 && blob_crc) {
            rc = CheckTable(src_path);
        }
        if (rc == 0 && index_chk) {
            /* rc = ValidateSKeyIndex(tbl); */
        }
    }
    if( rc == 0 ) {
        LOGMSG(klogInfo, "check ok");
    } else {
        PLOGERR(klogErr, (klogErr, rc, "check failed: '$(table)'", PLOG_S(table), src_path));
    }
    return rc;
}
