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
#include <klib/log.h>
#include <klib/out.h>
#include <klib/container.h>
#include <kfs/directory.h>
#include <kfs/buffile.h>
#include <kfs/gzip.h>
#include <kfs/bzip.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <os-native.h>
#include <sysalloc.h>

#include "factory.h"
#include "debug.h"

#define DUMPER_MAX_KEY_LENGTH 63
#define DUMPER_MAX_TREE_DEPTH 100
#define DUMPER_MAX_OPEN_FILES 100

#define OUTPUT_BUFFER_SIZE ( 128 * 1024 )

uint32_t nreads_max = 0;
uint32_t quality_N_limit = 0;
bool g_legacy_report = false;

typedef struct SRASplitterFile_struct {
    SLNode dad;
    char* key;
    KDirectory* dir;
    char* name;
    KFile* file;
    time_t opened;
    uint64_t pos;
    /* keep track of number of spots written to file */
    spotid_t curr_spot;
    uint64_t spot_qty;
} SRASplitterFile;

typedef struct SRASplitterFiler_struct {
    /* TBD - reorder structure to avoid premature ageing of compiler and CPU */
    char* prefix;
    KFile* kf_stdout;
    bool key_as_dir;
    bool keep_empty;
    bool do_gzip;
    bool do_bzip2;
    const char* arc_extension;
    KDirectory* dir;

    /* TBD - this should be a BSTree */
    SLList files;

    /* list of keys to construct a path */
    int path_tail; /* count of elements in path array */
    int path_len; /* cumulative length of path in array */
    const char* path[DUMPER_MAX_TREE_DEPTH];
    char key_buf[DUMPER_MAX_TREE_DEPTH * (DUMPER_MAX_KEY_LENGTH + 3) + 10];
    /* holds opened files */
    SRASplitterFile* open[DUMPER_MAX_OPEN_FILES];
    /* keep track of number of spots written to file */
    spotid_t curr_spot;
    uint64_t spot_qty;
} SRASplitterFiler;

SRASplitterFiler* g_filer = NULL;

static
void CC SRASplitterFiler_WhackFile( SLNode *node, void *data )
{
    SRASplitterFile* file = (SRASplitterFile*)node;
    bool* d = (bool*)data;

    SRA_DUMP_DBG(5, ("Close file: '%s%s'\n", file->key, g_filer->arc_extension));
    if( file->spot_qty == 0 ) {
        /* truncate file which didn't get actual spots written */
        KFileSetSize(file->file, 0);
    }
    KFileRelease(file->file);
    if( !*d ) {
        uint64_t sz = ~0;
        if( KDirectoryFileSize(file->dir, &sz, "%s%s", file->name, g_filer->arc_extension) == 0 ) {
            if( file->spot_qty == 0 || sz == 0 ) {
                SRA_DUMP_DBG(5, ("Delete empty file: '%s%s'\n", file->key, g_filer->arc_extension));
                KDirectoryRemove(file->dir, false, "%s%s", file->name, g_filer->arc_extension);
            }
        }
    }
    if( file->dir != g_filer->dir ) {
        KDirectoryRelease(file->dir);
    }
    free(file->key);
    free(file->name);
    free(file);
}

static
void CC SRASplitterFiler_StatFile( SLNode *node, void *data )
{
    SRASplitterFile* file = (SRASplitterFile*)node;
    uint64_t* d = (uint64_t*)data;

    if( *d < file->spot_qty ) {
        *d = file->spot_qty;
    }
}

void SRASplitterFiler_Release(void)
{
    if( g_filer != NULL ) {
        SLListWhack(&g_filer->files, SRASplitterFiler_WhackFile, &g_filer->keep_empty);
        KFileRelease(g_filer->kf_stdout);
        KDirectoryRelease(g_filer->dir);
        free(g_filer->prefix);
        free(g_filer);
        g_filer = NULL;
    }
}

void SRASplitterFactory_FilerReport(uint64_t* total, uint64_t* biggest_file)
{
    if( g_filer != NULL ) {
        if( total != NULL ) {
            *total = g_filer->spot_qty;
        }
        if( biggest_file != NULL ) {
            SLListForEach(&g_filer->files, SRASplitterFiler_StatFile, biggest_file);
        }
    }
}

static
rc_t SRASplitterFiler_PushKey(const char* key)
{
    if( g_filer == NULL || key == NULL ) {
        return RC(rcExe, rcFile, rcAttaching, rcParam, rcNull);
    }
    if( g_filer->path_tail == sizeof(g_filer->path) - 1 ) {
        return RC(rcExe, rcFile, rcAttaching, rcDirEntry, rcTooLong);
    }
    if( g_filer->key_as_dir ) {
        /* skip initial non-letters */
        while( !isalnum(*key) && *key != '\0' ) {
            ++key;
        }
    }
    g_filer->path[g_filer->path_tail++] = key;
    g_filer->path_len += strlen(key) + 1;
    return 0;
}

static
rc_t SRASplitterFiler_PopKey(void)
{
    if( g_filer->path_tail == 0 ) {
        return RC(rcExe, rcFile, rcDetaching, rcDirEntry, rcTooShort);
    }
    g_filer->path_len -= strlen(g_filer->path[--g_filer->path_tail]) + 1;
    return 0;
}

static
rc_t SRASplitterFiler_OpenFile(SRASplitterFile* file, bool initial)
{
    rc_t rc = 0;

    if( file == NULL || (initial && file->file != NULL) ) {
        rc = RC(rcExe, rcFile, rcOpening, rcParam, rcInvalid);
    } else if( initial || file->file == NULL ) {
        int i, vacancy = -1;
        time_t oldest = 0;

        for(i = 0; i < DUMPER_MAX_OPEN_FILES; i++) {
            if(g_filer->open[i] == NULL ) {
                vacancy = i;
                break;
            }
            if(g_filer->open[i]->opened < oldest || oldest == 0 ) {
                oldest = g_filer->open[i]->opened;
                vacancy = i;
            }
        }
        if( g_filer->open[vacancy] != NULL ) {
            SRA_DUMP_DBG(5, ("Close file[%i]: %lu '%s%s'\n", vacancy,
                g_filer->open[vacancy]->opened, g_filer->open[vacancy]->key, g_filer->arc_extension));
            KFileRelease(g_filer->open[vacancy]->file);
            g_filer->open[vacancy]->file = NULL;
            g_filer->open[vacancy] = NULL;
        }
        if( g_filer->kf_stdout ) {
            SRA_DUMP_DBG(5, ("attach to pre-opened stdout: '%s'\n", file->key));
            rc = KFileAddRef(g_filer->kf_stdout);
            file->file = g_filer->kf_stdout;
        } else if( initial ) {
            SRA_DUMP_DBG(5, ("Create file: '%s%s'\n", file->key, g_filer->arc_extension));
            if( (rc = KDirectoryCreateFile(file->dir, &file->file, false, 0664, kcmInit,
                                           "%s%s", file->name, g_filer->arc_extension)) == 0 ) {
                if( g_filer->do_gzip ) {
                    KFile* gz;
                    if( (rc = KFileMakeGzipForWrite(&gz, file->file)) == 0 ) {
                        KFileRelease(file->file);
                        file->file = gz;
                    }
                } else if( g_filer->do_bzip2 ) {
                    KFile* bz;
                    if( (rc = KFileMakeBzip2ForWrite(&bz, file->file)) == 0 ) {
                        KFileRelease(file->file);
                        file->file = bz;
                    }
                }
            }
        } else if( file->file == NULL ) {
            SRA_DUMP_DBG(5, ("Reopen file: '%s%s'\n", file->key, g_filer->arc_extension));
            /* position is rememebered since last time */
            if( (rc = KDirectoryOpenFileWrite(file->dir, &file->file, false,
                                              "%s%s", file->name, g_filer->arc_extension)) == 0 ) {
#if ! SUPPORT_MULTI_SESSION_GZIP_FILES
                if( g_filer->do_gzip || g_filer->do_bzip2 ) {
                    /* compressed files cannot (currently) be re-opened until we support multi-session compression */
                    rc = RC(rcExe, rcFile, rcOpening, rcConstraint, rcViolated);
                }
#else
                if( g_filer->do_gzip ) {
                    KFile* gz;
                    if( (rc = KFileMakeGzipForAppend(&gz, file->file)) == 0 ) {
                        KFileRelease(file->file);
                        file->file = gz;
                    }
                } else if( g_filer->do_bzip2 ) {
                    KFile* bz;
                    if( (rc = KFileMakeBzip2ForWrite(&bz, file->file)) == 0 ) {
                        KFileRelease(file->file);
                        file->file = bz;
                    }
                }
#endif
            }
        }
#if OUTPUT_BUFFER_SIZE
        if( rc == 0 && !g_filer->kf_stdout ) {
            /* attach buffer */
            KFile *buf = NULL;
            if( (rc = KBufFileMakeWrite(&buf, file->file, false, OUTPUT_BUFFER_SIZE)) == 0 ) {
                KFileRelease(file->file);
                file->file = buf;
            } else {
#if _DEBUGGING
                /* we only want to see this in debug */
                PLOGERR(klogErr, (klogErr, rc, "creating buffer for file '$(s)$(e)'",
                    PLOG_2(PLOG_S(s),PLOG_S(e)), file->key, g_filer->arc_extension));
#else
                rc = 0;
#endif
            }
        }
#endif
        if( rc == 0 ) {
            g_filer->open[vacancy] = file;
            file->opened = time(NULL);
            SRA_DUMP_DBG(5, ("Opened file[%i]: %lu '%s%s'\n",
                vacancy, file->opened, file->key, g_filer->arc_extension));
        }
    }
    return rc;
}

static
bool CC SRASplitterFiler_GetCurrFile_FindByKey( SLNode *node, void *data )
{
    SRASplitterFile** d = (SRASplitterFile**)data;
    SRASplitterFile* file = (SRASplitterFile*)node;

    if( strcmp(file->key, g_filer->key_buf) == 0 ) {
        *d = file;
        return true;
    }
    return false;
}

static
rc_t SRASplitterFiler_FixFSName(const char* name, char** fname)
{
    /* replace invalid chars with '_' in name */
    char* nn = strdup(name);

    if( nn == NULL ) {
        return RC(rcExe, rcFile, rcOpening, rcMemory, rcExhausted);
    }
    *fname = nn;
    while(*nn != '\0' ) {
        if( strchr("\\/?:*(){}[]^%\"\'<>|`", *nn) != NULL ) {
            *nn = '_';
        }
        nn++;
/*TODO: use it!*/
    }
    return 0;
}

static
rc_t SRASplitterFiler_GetCurrFile(const SRASplitterFile** out_file)
{
    rc_t rc = 0;
    int i;
    char* key = g_filer->key_buf; /* shortcut */
    SRASplitterFile* file = NULL;

    if( out_file == NULL ) {
        return RC(rcExe, rcFile, rcOpening, rcParam, rcInvalid);
    } else if( g_filer->kf_stdout ) {
        strcpy(key, "stdout");
    } else {
        /* prepare the key
           if key_as_dir true, key will be prefix/path[i]/(path[i+1]..)/suffix
           otherwise key will be prefix_path[i](_path[i+1]..)_?suffix
         */
        key[0] = '\0';
        for(i = 0; i < g_filer->path_tail; i++ ) {
            if( g_filer->path[i][0] == '\0' ) {
                continue;
            }
            if( g_filer->key_as_dir ) {
                if( i != 0 ) {
                    strcat(key, "/");
                }
                strcat(key, g_filer->path[i]);
            } else {
                if( i != 0 && isalnum(g_filer->path[i][0]) ) {
                    strcat(key, "_");
                }
                strcat(key, g_filer->path[i]);
            }
        }
    }
    if( !SLListDoUntil( &g_filer->files, SRASplitterFiler_GetCurrFile_FindByKey, &file ) ) {
        SRA_DUMP_DBG(5, ("New file: '%s'\n", key));
        file = calloc(1, sizeof(*file));
        key = strdup(key);
        if( file == NULL || key == NULL ) {
            free(file);
            free(key);
            rc = RC(rcExe, rcFile, rcResolving, rcMemory, rcExhausted);
        } else {
            file->key = key;
            if( g_filer->key_as_dir ) {
                KDirectory* sub = g_filer->dir;
                for(i = 0; rc == 0 && i < (g_filer->path_tail - 1); i++ ) {
                    if( g_filer->path[i][0] != '\0' ) {
                        char* ndir = NULL;
                        if( (rc = SRASplitterFiler_FixFSName(g_filer->path[i], &ndir)) == 0 ) {
                            if( (rc = KDirectoryCreateDir(sub, 0775, kcmCreate, "%s", ndir)) == 0 ||
                                (GetRCObject(rc) == ( enum RCObject )rcDirectory && GetRCState(rc) == rcExists) ) {
                                if( (rc = KDirectoryOpenDirUpdate(sub, &file->dir, true, "%s", ndir)) == 0 ) {
                                    KDirectoryRelease(i == 0 ? NULL : sub);
                                    sub = file->dir;
                                }
                            }
                            free(ndir);
                        }
                    }
                }
                rc = SRASplitterFiler_FixFSName(&file->key[strlen(file->key) - strlen(g_filer->path[g_filer->path_tail - 1])], &file->name);
            } else {
                file->dir = g_filer->dir;
                rc = SRASplitterFiler_FixFSName(file->key, &file->name);
            }
            if( rc == 0 && (rc = SRASplitterFiler_OpenFile(file, true)) == 0 ) {
                SLListPushTail(&g_filer->files, &file->dad);
            } else {
                SRASplitterFiler_WhackFile(&file->dad, &g_filer->keep_empty);
            }
        }
    } else {
        SRA_DUMP_DBG(5, ("Curr file key '%s': '%s'\n", key, file->name));
        rc = SRASplitterFiler_OpenFile(file, false);
    }
    *out_file = rc ? NULL : file;
    return rc;
}

rc_t SRASplitterFactory_FilerPrefix(const char* prefix)
{
    rc_t rc = 0;

    if( g_filer == NULL ) {
        rc = RC(rcExe, rcFile, rcUpdating, rcSelf, rcNotOpen);
    } else if( prefix == NULL || strcmp(prefix, g_filer->prefix) != 0 ) {
        if( (rc = SRASplitterFiler_PopKey()) == 0 ) {
            free(g_filer->prefix);
            g_filer->prefix = strdup(prefix ? prefix : "");
            if( g_filer->prefix == NULL ) {
                rc = RC(rcExe, rcFile, rcConstructing, rcMemory, rcExhausted);
            } else {
                rc = SRASplitterFiler_PushKey(g_filer->prefix);
            }
        }
    }
    return rc;
}

rc_t SRASplitterFactory_FilerInit(bool to_stdout, bool gzip, bool bzip2, bool key_as_dir, bool keep_empty, const char* path, ...)
{
    rc_t rc = 0;

    if( g_filer != NULL ) {
        rc = RC(rcExe, rcFile, rcConstructing, rcSelf, rcExists);
    } else if( gzip && bzip2 ) {
        rc = RC(rcExe, rcFile, rcConstructing, rcParam, rcAmbiguous);
    } else if( (g_filer = calloc(1, sizeof(*g_filer))) == NULL ) {
        rc = RC(rcExe, rcFile, rcConstructing, rcMemory, rcExhausted);
    } else {
        g_filer->key_as_dir = to_stdout ? false : key_as_dir;
        g_filer->keep_empty = to_stdout ? true : keep_empty;
        g_filer->do_gzip = gzip;
        g_filer->do_bzip2 = bzip2;
        g_filer->arc_extension = gzip ? ".gz" : (bzip2 ? ".bz2" : "");
        SLListInit(&g_filer->files);
        /* push empty prefix */
        g_filer->prefix = strdup("");
        if( (rc = SRASplitterFiler_PushKey(g_filer->prefix)) == 0 &&
            (rc = KDirectoryNativeDir(&g_filer->dir)) == 0 ) {
            if( to_stdout ) {
                if( (rc = KFileMakeStdOut(&g_filer->kf_stdout)) == 0 ) {
                    KFile *buf = NULL;
                    if( gzip ) {
                        KFile* gz;
                        if( (rc = KFileMakeGzipForWrite(&gz, g_filer->kf_stdout)) == 0 ) {
                            KFileRelease(g_filer->kf_stdout);
                            g_filer->kf_stdout = gz;
                        }
                    } else if( bzip2 ) {
                        KFile* bz;
                        if( (rc = KFileMakeBzip2ForWrite(&bz, g_filer->kf_stdout)) == 0 ) {
                            KFileRelease(g_filer->kf_stdout);
                            g_filer->kf_stdout = bz;
                        }
                    }
#if OUTPUT_BUFFER_SIZE
                    if ( rc == 0 ) {
                        if( (rc = KBufFileMakeWrite(&buf, g_filer->kf_stdout, false, OUTPUT_BUFFER_SIZE)) == 0 ) {
                            KFileRelease(g_filer->kf_stdout);
                            g_filer->kf_stdout = buf;
                        } else {
#if _DEBUGGING
                            /* we only want to see this in debug */
                            LOGERR(klogErr, rc, "creating buffer for stdout");
#else
                            rc = 0;
#endif
                        }
                    }
#endif
                }
            } else if( path != NULL ) {
                va_list args;
                va_start(args, path);
                if( (rc = KDirectoryVCreateDir(g_filer->dir, 0775, kcmCreate | kcmParents, path, args)) == 0 ||
                    (GetRCObject(rc) == ( enum RCObject )rcDirectory && GetRCState(rc) == rcExists) ) {
                    KDirectory* sub = NULL;
                    va_end(args);
                    va_start(args, path);
                    if( (rc = KDirectoryVOpenDirUpdate(g_filer->dir, &sub, true, path, args)) == 0 ) {
                        KDirectoryRelease(g_filer->dir);
                        g_filer->dir = sub;
                    }
                }
                va_end(args);
            }
        }
    }
    if( rc != 0 ) {
        SRASplitterFiler_PopKey();
        SRASplitterFiler_Release();
    }
    return rc;
}

/* ### Base splitter code ##################################################### */

/* used to detect correct object pointers */
const uint32_t SRASplitterFactory_MAGIC = 0xFACE3693;

struct SRASplitterFactory {
    uint32_t magic;
    ESRASplitterTypes type;
    const SRASplitterFactory* next;
    SRASplitterFactory_Init_Func* Init;
    SRASplitterFactory_NewObj_Func* NewObj;
    SRASplitterFactory_Release_Func* Release;
};

/* used to detect correct object pointers */
const uint32_t SRASplitter_MAGIC = 0xFACE5325;

typedef struct SRASplitter_Child SRASplitter_Child;

struct SRASplitter {
    uint32_t magic;
    ESRASplitterTypes type;
    const SRASplitterFactory* next_fact;
    SRASplitter_GetKey_Func* GetKey;
    SRASplitter_GetKeySet_Func* GetKeySet;
    SRASplitter_Dump_Func* Dump;
    SRASplitter_Release_Func* Release;
    BSTree children;
    SRASplitter_Child* last_found;
};

struct SRASplitter_Child {
    BSTNode node;
    char key[DUMPER_MAX_KEY_LENGTH + 1];
    bool is_splitter; /* next union selector */
    union {
        /* chained key->splitter for self type of eSplitterRead and eSplitterSpot */
        const SRASplitter* splitter;
        /* file object for self type of eSplitterFormat */
        const SRASplitterFile* file;
    } child;
};

static
rc_t SRASplitter_Child_Make(SRASplitter_Child** child, const char* key)
{
    if( child == NULL || key == NULL ) {
        return RC(rcExe, rcNode, rcAllocating, rcParam, rcNull);
    }
    if( strlen(key) > DUMPER_MAX_KEY_LENGTH ) {
        return RC(rcExe, rcNode, rcAllocating, rcTag, rcTooLong);
    }
    *child = calloc(1, sizeof(**child));
    if( *child == NULL ) {
        return RC(rcExe, rcNode, rcAllocating, rcMemory, rcExhausted);
    }
    strcpy((*child)->key, key);
    return 0;
}

static
rc_t SRASplitter_Child_MakeSplitter(SRASplitter_Child** child, const char* key, const SRASplitter* splitter)
{
    rc_t rc = 0;

    if( splitter == NULL ) {
        rc = RC(rcExe, rcNode, rcAllocating, rcParam, rcNull);
    } else if( (rc = SRASplitter_Child_Make(child, key)) == 0 ) {
        (*child)->is_splitter = true;
        (*child)->child.splitter = splitter;
    }
    return rc;
}

static
rc_t SRASplitter_Child_MakeFile(SRASplitter_Child** child, const char* key, const SRASplitterFile* file)
{
    rc_t rc = 0;

    if( file == NULL ) {
        rc = RC(rcExe, rcNode, rcAllocating, rcParam, rcNull);
    } else if( (rc = SRASplitter_Child_Make(child, key)) == 0 ) {
        (*child)->is_splitter = false;
        (*child)->child.file = file;
    }
    return rc;
}

static
void CC SRASplitter_Child_Whack(BSTNode* node, void* data)
{
    const SRASplitter_Child* n = (const SRASplitter_Child*)node;
    if( n->is_splitter ) {
        rc_t rc = 0;
        if( (rc = SRASplitter_Release(n->child.splitter)) != 0 ) {
            LOGERR(klogErr, rc, "SRASplitter_Release");
        }
    }
    free(node);
}

static
int CC SRASplitter_Child_Cmp(const BSTNode* item, const BSTNode* node)
{
    const SRASplitter_Child* i = (const SRASplitter_Child*)item;
    const SRASplitter_Child* n = (const SRASplitter_Child*)node;
    return strcmp(i->key, n->key);
}

static
int CC SRASplitter_Child_Find(const void* item, const BSTNode* node)
{
    const char* key = (const char*)item;
    const SRASplitter_Child* n = (const SRASplitter_Child*)node;
    return strcmp(key, n->key);
}

static /* not virtual, self is direct pointer to base type here !!! */
rc_t SRASplitter_FindNextSplitter(SRASplitter* self, const char* key)
{
    rc_t rc = 0;

    if( self->last_found == NULL || strcmp(self->last_found->key, key) ) {
        self->last_found = (SRASplitter_Child*)BSTreeFind( &self->children, key, SRASplitter_Child_Find );
        if( self->last_found == NULL ) {
            /* create new child using next factory in chain */
            const SRASplitter* splitter = NULL;
            SRA_DUMP_DBG(5, ("New splitter on key '%s'\n", key));
            if( (rc = SRASplitterFactory_NewObj(self->next_fact, &splitter)) == 0 ) {
                if( (rc = SRASplitter_Child_MakeSplitter(&self->last_found, key, splitter)) == 0 ) {
                    if( (rc = BSTreeInsertUnique(&self->children, &self->last_found->node, NULL, SRASplitter_Child_Cmp)) != 0 ) {
                        SRASplitter_Child_Whack(&self->last_found->node, NULL);
                        self->last_found = NULL;
                    }
                } else {
                    SRASplitter_Release(splitter);
                }
            }
        }
    }
    return rc;
}

static /* not virtual, self is direct pointer to base type here !!! */
rc_t SRASplitter_FindNextFile(SRASplitter* self, const char* key)
{
    rc_t rc = 0;

    if( self->last_found == NULL || strcmp(self->last_found->key, key) != 0 ) {
        self->last_found = (SRASplitter_Child*)BSTreeFind(&self->children, key, SRASplitter_Child_Find);
        if( self->last_found == NULL ) {
            /* create new child using global filer */
            const SRASplitterFile* file = NULL;
            SRA_DUMP_DBG(5, ("New file on key '%s'\n", key));
            if( (rc = SRASplitterFiler_GetCurrFile(&file)) == 0 ) {
                if( (rc = SRASplitter_Child_MakeFile(&self->last_found, key, file)) == 0 ) {
                    if( (rc = BSTreeInsertUnique(&self->children, &self->last_found->node, NULL, SRASplitter_Child_Cmp)) != 0 ) {
                        SRASplitter_Child_Whack(&self->last_found->node, NULL);
                        self->last_found = NULL;
                    }
                }
            }
        }
    }
    if( rc == 0 ) {
        /* make sure file is opened */
        rc = SRASplitterFiler_OpenFile((SRASplitterFile*)(self->last_found->child.file), false);
    }
    return rc;
}

rc_t SRASplitter_Make(const SRASplitter** cself, size_t type_size,
                      SRASplitter_GetKey_Func* getkey,
                      SRASplitter_GetKeySet_Func* get_keyset,
                      SRASplitter_Dump_Func* dump,
                      SRASplitter_Release_Func* release)
{
    SRASplitter* self = NULL;

    if( cself == NULL ) {
        return RC(rcExe, rcType, rcAllocating, rcParam, rcNull);
    }
    self = calloc(1, type_size + sizeof(*self));
    if( self == NULL ) {
        return RC(rcExe, rcType, rcAllocating, rcMemory, rcExhausted);
    }
    self->magic = SRASplitter_MAGIC;
    BSTreeInit(&self->children);
    self->GetKey = getkey;
    self->GetKeySet = get_keyset;
    self->Dump = dump;
    self->Release = release;

    /* shift pointer to after hidden structure */
    self++;
    *cself = self;
    return 0;
}

static
rc_t SRASplitter_ResolveSelf(const SRASplitter* self, enum RCContext ctx, SRASplitter** resolved)
{
    if( self == NULL || resolved == NULL ) {
        return RC(rcExe, rcType, ctx, rcSelf, rcNull);
    }
    *resolved = (SRASplitter*)--self;
    /* just to validate that it is full instance */
    if( (*resolved)->magic != SRASplitter_MAGIC ) {
        *resolved = NULL;
        return RC(rcExe, rcType, ctx, rcMemory, rcCorrupt);
    }
    return 0;
}

rc_t SRASplitter_AddSpot( const SRASplitter * cself, spotid_t spot, readmask_t * readmask )
{
    SRASplitter * self = NULL;
    rc_t rc = SRASplitter_ResolveSelf( cself, rcExecuting, &self );
    if ( rc == 0 )
    {
        if ( self->type == eSplitterRead )
        {
            const SRASplitter_Keys* keys = NULL;
            uint32_t key_qty = 0;
            int32_t i, j, k;
            make_readmask( used_readmasks );

            clear_readmask( used_readmasks );
            rc = self->GetKeySet( cself, &keys, &key_qty, spot, readmask );

            for ( i = 0; rc == 0 && i < key_qty; i++ )
            {
                if ( keys[ i ].key != NULL && !isset_readmask( used_readmasks, i ) )
                {
                    make_readmask( local_readmask );
                    copy_readmask( keys[ i ].readmask, local_readmask );
                    /* merge readmasks from duplicate keys in array */
                    for ( j = i + 1; j < key_qty; j++ )
                    {
                        if ( keys[ i ].key == keys[ j ].key || strcmp( keys[ i ].key, keys[ j ].key ) == 0 )
                        {
                            set_readmask( used_readmasks, j );
                            for ( k = 0; k < nreads_max; k++ )
                            {
                                local_readmask[ k ] |= keys[ j ].readmask[ k ];
                            }
                        }
                    }
                    /* leave reads only allowed by previous object in chain */
#ifndef EUGNES_LOOP_FIX
                    for ( j = 0, k = 0; k < nreads_max; k++ )
                    {
                        local_readmask[ k ] &= readmask[ k ];
                        if ( isset_readmask( local_readmask, k ) )
                        {
                            j++;
                        }
                    }
                    if ( j > 0 )
#endif
                    {
                        rc = SRASplitter_FindNextSplitter( self, keys[ i ].key );
                        if ( rc == 0 )
                        {
                            /* push spot to next splitter in chain */
                            rc = SRASplitterFiler_PushKey( self->last_found->key );
                            if ( rc == 0 )
                            {
                                /* here comes RECURSION!!! */
                                rc_t rc2;
                                rc = SRASplitter_AddSpot( self->last_found->child.splitter, spot, local_readmask );
                                rc2 = SRASplitterFiler_PopKey();
                                rc = rc ? rc : rc2;
                            }
                        }
                    }
                }
            }
        }
        else if ( self->type == eSplitterSpot )
        {
            const char* key = NULL;
            make_readmask( new_readmask );
            copy_readmask( readmask, new_readmask );
            rc = self->GetKey( cself, &key, spot, new_readmask );
            if ( rc == 0 && key != NULL )
            {
                int32_t j, k;
                /* leave reads only allowed by previous object in chain */
#ifndef EUGNES_LOOP_FIX
                for( j = 0, k = 0; k < nreads_max; k++ )
                {
                    readmask[ k ] &= new_readmask[ k ];
                    if ( isset_readmask( readmask, k ) )
                    {
                        j++;
                    }
                }
                if ( j > 0 )
#endif
                {
                    rc = SRASplitter_FindNextSplitter( self, key );
                    if ( rc == 0 )
                    {
                        /* push spot to next splitter in chain */
                        rc = SRASplitterFiler_PushKey( self->last_found->key );
                        if ( rc == 0 )
                        {
                            /* here comes RECURSION!!! */
                            rc_t rc2;
                            rc = SRASplitter_AddSpot( self->last_found->child.splitter, spot, readmask );
                            rc2 = SRASplitterFiler_PopKey();
                            rc = rc ? rc : rc2;
                        }
                    }
                }
            }
        }
        else if ( self->type == eSplitterFormat )
        {
            rc = self->Dump( cself, spot, readmask );
        }
        else
        {
            rc = RC( rcExe, rcFile, rcExecuting, rcInterface, rcUnsupported );
        }
    }
    return rc;
}


rc_t SRASplitter_Release(const SRASplitter* cself)
{
    rc_t rc = 0;
    SRASplitter* self = NULL;

    if( cself != NULL ) {
        if( (rc = SRASplitter_ResolveSelf(cself, rcExecuting, &self)) == 0 ) {
            if( self->Release ) {
                rc = self->Release(cself);
            }
            BSTreeWhack( &self->children, SRASplitter_Child_Whack, NULL );
            free(self);
        }
    }
    return rc;
}

rc_t SRASplitter_FileActivate(const SRASplitter* cself, const char* key)
{
    rc_t rc = 0, rc2 = 0;
    SRASplitter* self = NULL;

    if( (rc = SRASplitter_ResolveSelf(cself, rcExecuting, &self)) == 0 ) {
        if( (rc = SRASplitterFiler_PushKey(key)) == 0 ) {
            /* sets self->last_found */
            rc = SRASplitter_FindNextFile(self, key);
            rc2 = SRASplitterFiler_PopKey();
            rc = rc ? rc : rc2;
        }
    }
    return rc;
}

rc_t SRASplitter_FileWrite( const SRASplitter* cself, spotid_t spot, const void* buf, size_t size )
{
    SRASplitter* self = NULL;
    
    rc_t rc = SRASplitter_ResolveSelf( cself, rcWriting, &self );
    if ( rc == 0 )
    {
        if ( self->last_found == NULL )
        {
            rc = RC( rcExe, rcFile, rcWriting, rcDirEntry, rcUnknown );
        }
        else if ( buf != NULL && size > 0 )
        {
            size_t writ = 0;
            SRASplitterFile* f = ( SRASplitterFile* )( self->last_found->child.file );
            rc = KFileWrite( f->file, f->pos, buf, size, &writ );
            if ( rc == 0 )
            {
                f->pos += writ;
                if ( f->curr_spot != spot && spot != 0 )
                {
                     f->curr_spot = spot;
                     f->spot_qty = f->spot_qty + 1;
                }
                if ( g_filer->curr_spot != spot && spot != 0 )
                {
                    g_filer->curr_spot = spot;
                    g_filer->spot_qty = g_filer->spot_qty + 1;
                }
            }
        }
    }
    return rc;
}

rc_t SRASplitter_FileWritePos( const SRASplitter* cself, spotid_t spot, 
                               uint64_t pos, const void* buf, size_t size )
{
    SRASplitter* self = NULL;

    rc_t rc = SRASplitter_ResolveSelf( cself, rcWriting, &self );
    if ( rc == 0 )
    {
        if ( self->last_found == NULL )
        {
            rc = RC( rcExe, rcFile, rcWriting, rcDirEntry, rcUnknown );
        }
        else if ( buf != NULL && size > 0 )
        {
            const SRASplitterFile* f = self->last_found->child.file;
            /* remember last position */
            uint64_t old_pos = f->pos;
            ( ( SRASplitterFile* ) f )->pos = pos;
            /* write to requested position */
            rc = SRASplitter_FileWrite( cself, spot, buf, size );
            if ( f->pos < old_pos )
            {
                /* revert to last position if wrote less than it was */
                ( ( SRASplitterFile* ) f )->pos = old_pos;
            }
        }
    }
    return rc;
}

/* ### Common splitter factory code ##################################################### */

rc_t SRASplitterFactory_Make(const SRASplitterFactory** cself, ESRASplitterTypes type, size_t type_size,
                             SRASplitterFactory_Init_Func* init, SRASplitterFactory_NewObj_Func* newObj,
                             SRASplitterFactory_Release_Func* release)
{
    SRASplitterFactory* self = NULL;

    if( cself == NULL || newObj == NULL ) {
        return RC(rcExe, rcType, rcAllocating, rcParam, rcNull);
    }
    self = calloc(1, type_size + sizeof(*self));
    if( self == NULL ) {
        return RC(rcExe, rcType, rcAllocating, rcMemory, rcExhausted);
    }
    self->magic = SRASplitterFactory_MAGIC;
    self->type = type;
    self->Init = init;
    self->NewObj = newObj;
    self->Release = release;

    /* shift pointer to after hidden structure */
    self++;
    *cself = self;
    return 0;
}

static
rc_t SRASplitterFactory_ResolveSelf(const SRASplitterFactory* self, enum RCContext ctx, SRASplitterFactory** resolved)
{
    if( self == NULL || resolved == NULL ) {
        return RC(rcExe, rcType, ctx, rcSelf, rcNull);
    }
    *resolved = (SRASplitterFactory*)--self;
    /* just to validate that it is full instance */
    if( (*resolved)->magic != SRASplitterFactory_MAGIC ) {
        *resolved = NULL;
        return RC(rcExe, rcType, ctx, rcMemory, rcCorrupt);
    }
    return 0;
}

void SRASplitterFactory_Release(const SRASplitterFactory* cself)
{
    rc_t rc = 0;
    SRASplitterFactory* self = NULL;

    if( cself != NULL ) {
        if( (rc = SRASplitterFactory_ResolveSelf(cself, rcReleasing, &self)) == 0 ) {
            SRASplitterFactory_Release(self->next);
            if( self->Release != NULL ) {
                self->Release(cself);
            }
            free(self);
        }
    }
    if( rc != 0 ) {
        LOGERR(klogErr, rc, "SRASplitterFactory");
    }
}

ESRASplitterTypes SRASplitterFactory_GetType(const SRASplitterFactory* cself)
{
    rc_t rc = 0;
    SRASplitterFactory* self = NULL;

    if( (rc = SRASplitterFactory_ResolveSelf(cself, rcClassifying, &self)) == 0 ) {
        return self->type;
    }
    LOGERR(klogErr, rc, "SRASplitterFactory");
    return 0;
}

rc_t SRASplitterFactory_AddNext(const SRASplitterFactory* cself, const SRASplitterFactory* next)
{
    rc_t rc = 0;
    SRASplitterFactory* self = NULL;

    if( (rc = SRASplitterFactory_ResolveSelf(cself, rcAttaching, &self)) == 0 ) {
        if( self->type == eSplitterFormat && next != NULL ) {
            /* formatter must be last in chain */
            rc = RC(rcExe, rcType, rcAttaching, rcConstraint, rcViolated);
        } else {
            SRASplitterFactory_Release(self->next);
            self->next = next;
        }
    }
    return rc;
}

rc_t SRASplitterFactory_Init(const SRASplitterFactory* cself)
{
    rc_t rc = 0;
    SRASplitterFactory* self = NULL;

    if( (rc = SRASplitterFactory_ResolveSelf(cself, rcConstructing, &self)) == 0 ) {
        if( self->Init ) {
            rc = self->Init(cself);
        }
        if( rc == 0 ) {
            if( self->next != NULL ) {
                rc = SRASplitterFactory_Init(self->next);
            } else if( self->type != eSplitterFormat ) {
                /* formatter must be last in chain */
                rc = RC(rcExe, rcType, rcConstructing, rcConstraint, rcViolated);
            }
        }
    }
    return rc;
}

rc_t SRASplitterFactory_NewObj(const SRASplitterFactory* cself, const SRASplitter** splitter)
{
    rc_t rc = 0;
    SRASplitterFactory* self = NULL;

    if( (rc = SRASplitterFactory_ResolveSelf(cself, rcConstructing, &self)) == 0 ) {
        if( (rc = self->NewObj(cself, splitter)) == 0 ) {
            SRASplitter* sp = NULL;
            if( (rc = SRASplitter_ResolveSelf(*splitter, rcConstructing, &sp)) == 0 ) {
                sp->type = self->type;
                if( (self->type == eSplitterSpot   && (!sp->GetKey ||  sp->GetKeySet ||  sp->Dump) ) ||
                    (self->type == eSplitterRead   && ( sp->GetKey || !sp->GetKeySet ||  sp->Dump) ) ||
                    (self->type == eSplitterFormat && ( sp->GetKey ||  sp->GetKeySet || !sp->Dump) ) ) {
                    SRASplitter_Release(*splitter);
                    *splitter = NULL;
                    rc = RC(rcExe, rcType, rcAllocating, rcInterface, rcInvalid);
                } else if(self->type != eSplitterFormat) {
                    sp->next_fact = self->next;
                }
            }
        }
    }
    return rc;
}
