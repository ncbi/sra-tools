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
*  A couple of function to ease migration from libsra to libvdb use.
*  Maybe could be improved/optimized/removed later.
*/

#include "sra-stat.h" /* VTableMakeSingleFileArchive_ */

#include <kdb/database.h> /* KDatabase */
#include <kdb/kdb-priv.h> /* KDatabaseOpenDirectoryRead */
#include <kdb/table.h> /* KTableRelease */

#include <kfs/directory.h> /* KDirectory */
#include <kfs/file.h> /* KFile */
#include <kfs/toc.h> /* KDirectoryOpenTocFileRead */

#include <klib/debug.h> /* DBGMSG */
#include <klib/log.h> /* LOGERR */
#include <klib/rc.h>
#include <klib/vector.h> /* Vector */

#include <vdb/cursor.h> /* VCursor */
#include <vdb/database.h> /* VDatabase */
#include <vdb/table.h> /* VTableOpenParentRead */
#include <vdb/vdb-priv.h> /* VDatabaseOpenKDatabaseRead */

#include <string.h> /* memcmp */

#define DISP_RC(rc, msg) (void)((rc == 0) ? 0 : LOGERR(klogInt, rc, msg))

#define RELEASE(type, obj) do { rc_t rc2 = type##Release(obj); \
    if (rc2 && !rc) { rc = rc2; } obj = NULL; } while (false)

/* sfa_sort
 *  reorders list
 */
enum sfa_path_type_id {
    sfa_not_set = -1,
    sfa_exclude,
    sfa_non_column,
    sfa_required,
    sfa_preferred,
    sfa_optional
};
#if _DEBUGGING
static const char* sfa_path_type_id[] = {
    "not_set",
    "exclude",
    "non_column",
    "required",
    "preferred",
    "optional"
};
#endif

#define MATCH( ptr, str ) \
    ( (memcmp(ptr, str, sizeof(str) - 2) == 0 && \
       ((ptr)[sizeof(str) - 2] == '\0' || (ptr)[sizeof(str) - 2] == '/')) \
        ? (ptr) += sizeof(str) - (((ptr)[sizeof(str) - 2] == '/') ? 1 : 2) \
        : (const char*) 0)

static
enum sfa_path_type_id CC sfa_path_type_tbl( const char *path )
{
    /* use first character as distinguisher for match */
    switch ( path [ 0 ] )
    {
    case 'c':
        /* perhaps it's a column */
        if ( MATCH ( path, "col/" ) )
        {
            switch ( path [ 0 ] )
            {
            case 'D':
                if ( MATCH ( path, "DELETION_QV/" ) )
                    return sfa_optional;
                if ( MATCH ( path, "DELETION_TAG/" ) )
                    return sfa_optional;
                break;
            case 'H':
                if ( MATCH ( path, "HOLE_NUMBER/" ) )
                    return sfa_optional;
                if ( MATCH ( path, "HOLE_STATUS/" ) )
                    return sfa_optional;
                break;
            case 'I':
                if ( MATCH ( path, "INTENSITY/" ) )
                    return sfa_optional;
                if ( MATCH ( path, "INSERTION_QV/" ) )
                    return sfa_optional;
                break;
            case 'N':
                if ( MATCH ( path, "NAME_FMT/" ) )
                    return sfa_preferred;
                if ( MATCH ( path, "NAME/" ) )
                    return sfa_preferred;
                if ( MATCH ( path, "NOISE/" ) )
                    return sfa_optional;
                if ( MATCH ( path, "NUM_PASSES/" ) )
                    return sfa_optional;
                break;
            case 'P':
                if ( MATCH ( path, "POSITION/" ) )
                    return sfa_optional;
                if ( MATCH ( path, "PRE_BASE_FRAMES/" ) )
                    return sfa_optional;
                if ( MATCH ( path, "PULSE_INDEX/" ) )
                    return sfa_optional;
                break;
            case 'Q':
                if ( MATCH ( path, "QUALITY2/" ) )
                    return sfa_optional;
                break;
            case 'S':
                if ( MATCH ( path, "SIGNAL/" ) )
                    return sfa_optional;
                if ( MATCH ( path, "SPOT_NAME/" ) )
                    return sfa_preferred;
                if ( MATCH ( path, "SUBSTITUTION_QV/" ) )
                    return sfa_optional;
                if ( MATCH ( path, "SUBSTITUTION_TAG/" ) )
                    return sfa_optional;
                break;
            case 'W':
                if ( MATCH ( path, "WIDTH_IN_FRAMES/" ) )
                    return sfa_optional;
                break;
            case 'X':
            case 'Y':
                if ( path [ 1 ] == '/' )
                    return sfa_preferred;
                break;
            }
        }
        return sfa_required;

    case 'i':
        /* look for skey index */
        if ( MATCH ( path, "idx/skey" ) )
            if ( path [ 0 ] == 0 || strcmp ( path, ".md5" ) == 0 )
                return sfa_preferred;
        if ( MATCH ( path, "idx/fuse-" ) )
            return sfa_exclude;
        break;

    case 's':
        /* look for old skey index */
        if ( MATCH ( path, "skey" ) )
            if ( path [ 0 ] == 0 || strcmp ( path, ".md5" ) == 0 )
                return sfa_preferred;
        break;
    }
    /* anything not recognized is non-column required */
    return sfa_non_column;
}

static
enum sfa_path_type_id CC sfa_path_type_db ( const char *path )
{
    /* use first character as distinguisher for match */
    switch ( path [ 0 ] )
    {
    case 't':
        /* perhaps it's a table */
        if ( MATCH ( path, "tbl/" ) )
        {
            switch ( path [ 0 ] )
            {
            case 0:
                return sfa_non_column;
            case 'S':
                if ( MATCH ( path, "SEQUENCE/" ) )
                    return sfa_path_type_tbl(path);
                break;
            case 'C':
                if ( MATCH ( path, "CONSENSUS/" ) )
                    return sfa_path_type_tbl(path);
                break;
            case 'P':
                if ( MATCH ( path, "PRIMARY_ALIGNMENT/" ) )
                    return sfa_path_type_tbl(path);
                break;
            case 'R':
                if ( MATCH ( path, "REFERENCE/" ) )
                    return sfa_path_type_tbl(path);
                break;
            }
            /* all other tables are optional */
            return sfa_optional;
        }
    }
    /* anything not recognized is non-column required */
    return sfa_non_column;
}

typedef enum sfa_path_type_id (CC *sfa_path_type_func)( const char *path );

typedef
struct to_nv_data_struct
{
    const KDirectory * d;
    Vector * v;
    rc_t rc;
    sfa_path_type_func path_type;
} to_nv_data;

/* sfa_filter
 *  if a name is found in list, exclude it
 */
#define DEBUG_SORT(msg) DBGMSG (DBG_APP, DBG_FLAG(DBG_APP_0), msg)

typedef struct reorder_t_struct {
    const char * path;
    uint64_t     size;
    enum sfa_path_type_id type_id;
} reorder_t;

static
void CC to_nv (void * _item, void * _data)
{
    const char* path = _item;
    to_nv_data* data = _data;
    reorder_t* obj;

    if (data->rc == 0)
    {
        obj = malloc (sizeof (*obj));
        if (obj == NULL)
            data->rc
            = RC(rcSRA, rcVector, rcConstructing, rcMemory, rcExhausted);
        else
        {
            rc_t rc = KDirectoryFileSize (data->d, &obj->size, "%s", path);
            if (rc == 0)
            {
                obj->path = path;
                obj->type_id = data->path_type(path);
                rc = VectorAppend (data->v, NULL, obj);
            }

            if (rc)
            {
                free (obj);
                data->rc = rc;
            }
        }
    }
}

static
int CC sfa_path_cmp ( const void **_a, const void **_b, void * ignored )
{
    const reorder_t * a = *_a;
    const reorder_t * b = *_b;
    int ret;

    DEBUG_SORT(("%s enter\t%s %s %lu \t%s %s %lu", __func__, 
                a->path, sfa_path_type_id[a->type_id + 1], a->size,
                b->path, sfa_path_type_id[b->type_id + 1], b->size));

    ret = a->type_id - b->type_id;
    if (ret == 0)
    {
        if (a->size > b->size)
            ret = 1;
        else if (a->size < b->size)
            ret = -1;
        else
            ret = strcmp (a->path, b->path);
    }
    DEBUG_SORT(("\t%d\n", ret));
    return ret;
}

static
void CC item_whack (void * item, void * ignored)
{
    free (item);
}

static
rc_t CC sfa_sort(const KDirectory *dir, Vector *v, sfa_path_type_func func) {
    /* assume "v" is a vector of paths - hopefully relative to "dir" */
    Vector nv;
    to_nv_data data;
    uint32_t base;

    DEBUG_SORT(("%s enter\n", __func__));

    base = VectorStart (v);
    VectorInit (&nv, base, VectorLength (v));
    data.d = dir;
    data.v = &nv;
    data.rc = 0;
    data.path_type = func;

    VectorForEach (v, false, to_nv, &data);

    if(data.rc == 0) {
        uint32_t idx = 0;
        uint32_t limit = VectorLength (v) + base;

        VectorReorder(&nv, sfa_path_cmp, NULL);

        for (idx = base; idx < limit; ++idx) {
            const reorder_t * tmp;
            void * ignore;

            tmp = VectorGet (&nv, idx);
            data.rc = VectorSwap (v, idx + base, tmp->path, &ignore);
            if(data.rc) {
                break;
            }
        }
    }
    VectorWhack (&nv, item_whack, NULL);
    DEBUG_SORT(("%s exit %d %R\n", __func__, data.rc, data.rc));
    return data.rc;
}

static rc_t CC sfa_sort_db(const KDirectory *dir, Vector *v) {
    return sfa_sort(dir, v, sfa_path_type_db);
}

static
bool CC sfa_filter(const KDirectory *dir, const char *leaf, void* func)
{
    bool ret = true;
    sfa_path_type_func f = (sfa_path_type_func)func;
    enum sfa_path_type_id type = f(leaf);

    ret = type >= sfa_non_column;
    DEBUG_SORT(("%s: %s %s %s\n",
        __func__, leaf, sfa_path_type_id[type + 1], ret ? "keep" : "drop"));
    return ret;
}

static
bool CC sfa_filter_light(const KDirectory *dir, const char *leaf, void* func)
{
    bool ret = true;
    sfa_path_type_func f = (sfa_path_type_func)func;

    enum sfa_path_type_id type = f(leaf);

    ret = type >= sfa_non_column && type < sfa_optional;
    DEBUG_SORT(("%s: %s %s %s\n",
        __func__, leaf, sfa_path_type_id[type + 1], ret ? "keep" : "drop"));
    return ret;
}

static rc_t CC sfa_sort_tbl(const KDirectory *dir, Vector *v) {
    return sfa_sort(dir, v, sfa_path_type_tbl);
}

rc_t CC VTableMakeSingleFileArchive(const VTable *self,
    const KFile **sfa, bool  lightweight)
{
    const VDatabase *db = NULL;
    rc_t rc = VTableOpenParentRead(self, &db);

    assert(sfa);

    *sfa = NULL;

    if (rc == 0 && db != NULL) {
        const KDatabase *kdb = NULL;
        const KDirectory *db_dir = NULL;
        rc = VDatabaseOpenKDatabaseRead(db, &kdb);
        DISP_RC(rc, "VDatabaseOpenKDatabaseRead");

        if (rc == 0) {
            rc = KDatabaseOpenDirectoryRead(kdb, &db_dir);
            DISP_RC(rc, "KDatabaseOpenDirectoryRead");
        }

        if (rc == 0) {
            rc = KDirectoryOpenTocFileRead(db_dir, sfa, sraAlign4Byte,
                lightweight ? sfa_filter_light : sfa_filter,
                (void *)sfa_path_type_db, sfa_sort_db);
        }

        RELEASE(KDirectory, db_dir);
        RELEASE(KDatabase, kdb);
    }
    else {
        const KTable *ktbl = NULL;
        rc = VTableOpenKTableRead(self, &ktbl);
        if (rc == 0) {
            const KDirectory *tbl_dir = NULL;
            rc = KTableGetDirectoryRead(ktbl, &tbl_dir);
            if (rc == 0) {
                rc = KDirectoryOpenTocFileRead(tbl_dir, sfa, sraAlign4Byte,
                    lightweight ? sfa_filter_light : sfa_filter,
                    (void *)sfa_path_type_tbl, sfa_sort_tbl);
                KDirectoryRelease( tbl_dir );
                KTableRelease(ktbl);
            }
        }
    }

    RELEASE(VDatabase, db);

    return rc;
}

rc_t CC VCursorColumnRead(const VCursor *self, int64_t id,
    uint32_t idx, const void **base, bitsz_t *offset, bitsz_t *size)
{
    uint32_t elem_bits = 0, elem_off = 0, elem_cnt = 0;
    rc_t rc = VCursorCellDataDirect(self, id, idx,
        &elem_bits, base, &elem_off, &elem_cnt);

    assert(offset && size);

    if (rc == 0) {
        *offset = elem_off * elem_bits;
        *size   = elem_cnt * elem_bits;
    }
    else {
        *offset = *size = 0;
    }

    return rc;
}
