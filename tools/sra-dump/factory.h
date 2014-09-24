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
#ifndef _h_tools_dump_factory
#define _h_tools_dump_factory

#include <klib/rc.h>
#include <kfs/file.h>
#include <klib/data-buffer.h>
#include <sra/sradb.h>

#define DATABUFFER_INC_SZ 4096

/* KDataBuffer is used as byte buffer-only here! */
#define IF_BUF(expr, buf, writ) \
    while( (rc = (expr)) != 0 && \
           (GetRCObject(rc) == rcMemory || GetRCObject(rc) == ( enum RCObject )rcBuffer) && \
           (GetRCState(rc) == rcInsufficient || GetRCState(rc) == rcExhausted) ) { \
        SRA_DUMP_DBG(10, ("\n%s grow buffer from %u to %u\n", __func__, buf->elem_count, writ + DATABUFFER_INC_SZ)); \
        if( (rc = KDataBufferResize(buf, writ + DATABUFFER_INC_SZ)) != 0 ) { \
            break; \
        } \
    } \
    if( rc == 0 )

typedef enum {
    eSplitterSpot = 1,
    eSplitterRead,
    eSplitterFormat /* in splitters chain this type must be the last instance and must write to file(s) */
} ESRASplitterTypes;

typedef struct SRASplitter SRASplitter;

#define NREADS_MAX (8*1024)
typedef uint8_t readmask_t; 
extern uint32_t nreads_max;

/* new hack to realize changed requirement for quality-filter */
#define DEF_QUAL_N_LIMIT 10
extern uint32_t quality_N_limit;

extern bool g_legacy_report;

#define make_readmask(m) readmask_t m[NREADS_MAX]
#define copy_readmask(src,dst) (memcpy(dst, src, sizeof(readmask_t) * nreads_max))
#define reset_readmask(m) (memset(m, ~0, sizeof(readmask_t) * nreads_max))
#define clear_readmask(m) (memset(m,  0, sizeof(readmask_t)  * nreads_max))
#define set_readmask(m,id) (id < nreads_max ? (m)[id] = 1 : 0)
#define unset_readmask(m,id) (id < nreads_max ? (m)[id] = 0 : 0)
#define get_readmask(m,id) (id < nreads_max ? ((m)[id]) : 0)
#define isset_readmask(m,id) (id < nreads_max ? (m)[id] != 0 : false)

typedef struct SRASplitter_Keys_struct {
    const char* key;
    make_readmask(readmask);
} SRASplitter_Keys;

/* for eSpot splitter: returns pointer to key and (optionally) modified readmask, based on spotid and readmask
   key == NULL stops further processing of the spot, char* key alloc and dealloc must be handled by splitter itself
 */
typedef rc_t (SRASplitter_GetKey_Func)(const SRASplitter* self, const char** key, spotid_t spot, readmask_t* readmask);

/* for eRead splitter:
   returns array of keys and readmasks used array portion count in keys, based on spotid and readmask,
   on return keys shows number of array key[] elements used, keys == 0 stops further processing of the spot,
   elements with key.key == NULL OR key.readmask == 0 are skipped
   key alloc and dealloc must be handled by splitter itself
*/
typedef rc_t (SRASplitter_GetKeySet_Func)(const SRASplitter* self, const SRASplitter_Keys** key, uint32_t* keys, spotid_t spot, const readmask_t* readmask);

/* writes data to file(s) using SRASplitter_File* functions, based on spot and readmask
   do NOT open or create any files directly here, use SRASplitter_FileXXX functions only!
*/
typedef rc_t (SRASplitter_Dump_Func)(const SRASplitter* self, spotid_t spot, const readmask_t* readmask);

/* optional method to free splitter resources, object distruction is handled automatically, do NOT free self! */
typedef rc_t (SRASplitter_Release_Func)(const SRASplitter* self);

/**
  * Base type constructor, must be called when constructing custom splitter type
  */
rc_t SRASplitter_Make(const SRASplitter** self, size_t type_size, /* sizeof(derived object) */
                      /* mandatory only for eSpot splitter, must be NULL for other types */
                      SRASplitter_GetKey_Func*,
                      /* mandatory only for eRead splitter, must be NULL for other types */
                      SRASplitter_GetKeySet_Func*,
                      /* mandatory only for eFormat splitter, must be NULL for other types */
                      SRASplitter_Dump_Func*,
                      /* optional destructor, do NOT free self! */
                      SRASplitter_Release_Func* release);
/**
  * Base type destructor
  */
rc_t SRASplitter_Release(const SRASplitter* self);

/**
  * Add spot to processing chain
  */
rc_t SRASplitter_AddSpot(const SRASplitter* self, spotid_t spot, readmask_t* readmask);

/**
  * File access routines
  */
/* key [IN] - usually file type extension: .sff, .fastq, _nse.txt, etc */
rc_t SRASplitter_FileActivate(const SRASplitter* self, const char* key);
/* CAREFULL!!! these operates on last active file
   spot can be zero to indicate file header/footer
   which no considered as actual data and file treated as empty if no actual spots is written
 */
rc_t SRASplitter_FileWrite( const SRASplitter* cself, spotid_t spot, const void* buf, size_t size );
rc_t SRASplitter_FileWritePos( const SRASplitter* cself, spotid_t spot, uint64_t pos, const void* buf, size_t size );

typedef struct SRASplitterFactory SRASplitterFactory;

typedef rc_t (SRASplitterFactory_Init_Func)(const SRASplitterFactory* self);
typedef rc_t (SRASplitterFactory_NewObj_Func)(const SRASplitterFactory* self, const SRASplitter** splitter);
typedef void (SRASplitterFactory_Release_Func)(const SRASplitterFactory* self);


/**
  * Initialize factories file handler object.
  * Must be called BEFORE 1st call to SRASplitterFactory_Init
  *
  * key_as_dir [IN] - if true, subdirs created for each splitting level: SPOT_GROUP/1/prefix.fastq
  *                   if false, single file is used in split chain: prefix_SPOT_GROUP_1.fastq
  * prefix [IN]     - file name prefix, usually run id (accession)
  * path, ... [IN]  - path to directory where file will reside
  */
rc_t SRASplitterFactory_FilerInit(bool to_stdout, bool gzip, bool bzip2, bool key_as_dir, bool keep_empty, const char* path, ...);
/* this only works correctly on top of the splitter tree !! */
rc_t SRASplitterFactory_FilerPrefix(const char* prefix);
void SRASplitterFactory_FilerReport(uint64_t* total, uint64_t* biggest_file);
void SRASplitterFiler_Release(void);

/**
  * Create factory object
  */
rc_t SRASplitterFactory_Make(const SRASplitterFactory** self, ESRASplitterTypes type, size_t type_size,
                             /* optional method to initialize self */
                             SRASplitterFactory_Init_Func* init,
                             /* mandatory method to spawn new SRASplitter object */
                             SRASplitterFactory_NewObj_Func* newObj,
                             /* optional destructor, should NOT free self!!! */
                             SRASplitterFactory_Release_Func* release);

/**
  * Releases Factory and all chained factories
  */
void SRASplitterFactory_Release(const SRASplitterFactory* self);

/**
  * Get Factory type
  */
ESRASplitterTypes SRASplitterFactory_GetType(const SRASplitterFactory* self);

/**
  * Chain "next" factory after "self", releases old chain if present
  */
rc_t SRASplitterFactory_AddNext(const SRASplitterFactory* self, const SRASplitterFactory* next);

/**
  * Initialize a chain of factories. Factories must be chaind properly before init.
  * Type chain must look like (eSpot|eRead)*, eFormat.
  */
rc_t SRASplitterFactory_Init(const SRASplitterFactory* self);

/**
  * Create new instance of factory kind, point splitter to next factory in chain
  */
rc_t SRASplitterFactory_NewObj(const SRASplitterFactory* self, const SRASplitter** splitter);

#endif /* _h_tools_dump_factory */
