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

#include <loader/mmarray.h>

#include <sysalloc.h>
#include <stdlib.h>

#include <klib/rc.h>

#include <kfs/mmap.h>
#include <kfs/file.h>

#define MMA_NUM_CHUNKS_BITS (24u)
#define MMA_NUM_SUBCHUNKS_BITS ((32u)-(MMA_NUM_CHUNKS_BITS))
#define MMA_SUBCHUNK_SIZE (1u << MMA_NUM_CHUNKS_BITS)
#define MMA_SUBCHUNK_COUNT (1u << MMA_NUM_SUBCHUNKS_BITS)

typedef struct MMArray {
    KFile *fp;
    size_t elemSize;
    uint64_t fsize;
    struct mma_map_s {
        struct mma_submap_s {
            uint8_t *base;
            KMMap *mmap;
        } submap[MMA_SUBCHUNK_COUNT];
    } map[NUM_ID_SPACES];
} MMArray;

rc_t MMArrayMake(struct MMArray **rslt, KFile *fp, uint32_t elemSize)
{
    MMArray *const self = calloc(1, sizeof(*self));

    if (self == NULL)
        return RC(rcExe, rcMemMap, rcConstructing, rcMemory, rcExhausted);
    self->elemSize = (elemSize + 3) & ~(3u); /** align to 4 byte **/
    self->fp = fp;
    KFileAddRef(fp);
    *rslt = self;
    return 0;
}

#define PERF 0

rc_t MMArrayGet(struct MMArray *const self, void **const value, uint64_t const element)
{
    unsigned const bin_no = element >> 32;
    unsigned const subbin = ((uint32_t)element) >> MMA_NUM_CHUNKS_BITS;
    unsigned const in_bin = (uint32_t)element & (MMA_SUBCHUNK_SIZE - 1);

    if (bin_no >= sizeof(self->map)/sizeof(self->map[0]))
        return RC(rcExe, rcMemMap, rcConstructing, rcId, rcExcessive);
    
    if (self->map[bin_no].submap[subbin].base == NULL) {
        size_t const chunk = MMA_SUBCHUNK_SIZE * self->elemSize;
        size_t const fsize = self->fsize + chunk;
        rc_t rc = KFileSetSize(self->fp, fsize);
        
        if (rc == 0) {
            KMMap *mmap;
            
            self->fsize = fsize;
            rc = KMMapMakeRgnUpdate(&mmap, self->fp, self->fsize, chunk);
            if (rc == 0) {
                void *base;
                
                rc = KMMapAddrUpdate(mmap, &base);
                if (rc == 0) {
#if PERF
                    static unsigned mapcount = 0;

                    (void)PLOGMSG(klogInfo, (klogInfo, "Number of mmaps: $(cnt)", "cnt=%u", ++mapcount));
#endif
                    self->map[bin_no].submap[subbin].mmap = mmap;
                    self->map[bin_no].submap[subbin].base = base;

                    goto GET_MAP;
                }
                KMMapRelease(mmap);
            }
        }
        return rc;
    }
GET_MAP:
    *value = &self->map[bin_no].submap[subbin].base[(size_t)in_bin * self->elemSize];
    return 0;
}

void MMArrayWhack(struct MMArray *self)
{
    unsigned i;

    for (i = 0; i != sizeof(self->map)/sizeof(self->map[0]); ++i) {
        unsigned j;
        
        for (j = 0; j != sizeof(self->map[0].submap)/sizeof(self->map[0].submap[0]); ++j) {
            if (self->map[i].submap[j].mmap)
                KMMapRelease(self->map[i].submap[j].mmap);
            self->map[i].submap[j].mmap = NULL;
            self->map[i].submap[j].base = NULL;
        }
    }
    KFileRelease(self->fp);
    free(self);
}


