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

#include <fcntl.h>
#include <sys/mman.h>
#include <klib/log.h>


#define MMA_NUM_CHUNKS_BITS (20u)
#define MMA_NUM_SUBCHUNKS_BITS ((32u)-(MMA_NUM_CHUNKS_BITS))
#define MMA_SUBCHUNK_SIZE (1u << MMA_NUM_CHUNKS_BITS)
#define MMA_SUBCHUNK_COUNT (1u << MMA_NUM_SUBCHUNKS_BITS)
#define MMA_ELEM_SIZE ((size_t)((sizeof(MMA_ELEM_T) + ((size_t)(3u))) & ~((size_t)(3u))))

typedef struct MMArray {
    struct mma_map_s {
        struct mma_submap_s {
            void *base;
        } submap[MMA_SUBCHUNK_COUNT];
    } map[NUM_ID_SPACES];
    off_t fsize;
    int fd;
} MMArray;

static MMArray *MMArrayMake(rc_t *const prc, int fd)
{
    MMArray *const self = calloc(1, sizeof(*self));

    if (self) {
        self->fd = fd;
        return self;
    }
    *prc = RC(rcExe, rcMemMap, rcConstructing, rcMemory, rcExhausted);
    return NULL;
}

static MMA_ELEM_T *MMArrayGet(MMArray *const self, rc_t *const prc, uint64_t const element)
{
    size_t const chunk = MMA_SUBCHUNK_SIZE * MMA_ELEM_SIZE;
    unsigned const bin_no = element >> 32;
    unsigned const subbin = ((uint32_t)element) >> MMA_NUM_CHUNKS_BITS;
    unsigned const in_bin = (uint32_t)element & (MMA_SUBCHUNK_SIZE - 1);

    while (bin_no < sizeof(self->map)/sizeof(self->map[0])) {
        MMA_ELEM_T *const next = self->map[bin_no].submap[subbin].base;
        if (next != NULL)
            return &next[in_bin];
        else {
            off_t const cur_fsize = self->fsize;
            off_t const new_fsize = cur_fsize + chunk;

            if (ftruncate(self->fd, new_fsize) == 0) {
                void *const base = mmap(NULL, chunk, PROT_READ|PROT_WRITE,
                                        MAP_FILE|MAP_SHARED, self->fd, cur_fsize);

                self->fsize = new_fsize;
                if (base != MAP_FAILED)
                    self->map[bin_no].submap[subbin].base = base;
                else {
                    PLOGMSG(klogErr, (klogErr, "Failed to construct map for bin $(bin), subbin $(subbin)", "bin=%u,subbin=%u", bin_no, subbin));
                    *prc = RC(rcExe, rcMemMap, rcConstructing, rcMemory, rcExhausted);
                    return NULL;
                }
            }
            else {
                *prc = RC(rcExe, rcMemMap, rcResizing, rcSize, rcExcessive);
                return NULL;
            }
        }
    }
    *prc = RC(rcExe, rcMemMap, rcResizing, rcId, rcExcessive);
    return NULL;
}

static void MMArrayWhack(MMArray *self)
{
    size_t const chunk = MMA_SUBCHUNK_SIZE * MMA_ELEM_SIZE;
    unsigned i;

    for (i = 0; i != sizeof(self->map)/sizeof(self->map[0]); ++i) {
        unsigned j;

        for (j = 0; j != sizeof(self->map[0].submap)/sizeof(self->map[0].submap[0]); ++j) {
            if (self->map[i].submap[j].base)
            	munmap(self->map[i].submap[j].base, chunk);
        }
    }
    close(self->fd);
    free(self);
}

#undef MMA_NUM_CHUNKS_BITS
#undef MMA_NUM_SUBCHUNKS_BITS
#undef MMA_SUBCHUNK_SIZE
#undef MMA_SUBCHUNK_COUNT
#undef MMA_ELEM_SIZE
