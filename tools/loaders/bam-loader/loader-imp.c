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

/* #include "bam-load.vers.h" */
#ifdef __cplusplus
extern "C" {
#endif

#include <klib/callconv.h>
#include <klib/data-buffer.h>
#include <klib/text.h>
#include <klib/log.h>
#include <klib/out.h>
#include <klib/status.h>
#include <klib/rc.h>
#include <klib/sort.h>
#include <klib/printf.h>

#include <kfs/directory.h>
#include <kfs/file.h>
#include <kdb/btree.h>
#include <kdb/manager.h>
#include <kdb/database.h>
#include <kdb/table.h>
#include <kdb/meta.h>

#include <vdb/manager.h>
#include <vdb/schema.h>
#include <vdb/database.h>
#include <vdb/table.h>
#include <vdb/cursor.h>
#include <vdb/vdb-priv.h>
#include <insdc/insdc.h>
#include <insdc/sra.h>
#include <align/dna-reverse-cmpl.h>
#include <align/align.h>

#include <kapp/main.h>
#include <kapp/args.h>
#include <kapp/log-xml.h>

#include <kproc/queue.h>
#include <kproc/thread.h>
#include <kproc/timeout.h>
#include <os-native.h>

#include <loader/loader-file.h>
#include <loader/loader-meta.h>
#include <loader/progressbar.h>

#include <sysalloc.h>
#include <atomic32.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <limits.h>
#include <time.h>
#include <zlib.h>
#include "bam.h"
#include "Globals.h"
#include "sequence-writer.h"
#include "reference-writer.h"
#include "alignment-writer.h"
#include "mem-bank.h"
#include "low-match-count.h"
#include "bam-alignment.h"
#ifdef __cplusplus
}
#endif

#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_sinks.h>
#include <spdlog/sinks/null_sink.h>

#include <spdlog/stopwatch.h>

#include <fstream>
#include "data_frame.hpp"
#include <taskflow/taskflow.hpp>
#include <taskflow/algorithm/sort.hpp>
#include <bm/bmsparsevec_algo.h>
#include <bm/bmtimer.h>
#include "hashing.hpp"
#include <set>

#ifdef __linux__
#include <sys/resource.h>
#endif
#include <tsl/array_map.h>
#include "spot_assembly.hpp"
#define JSON_HAS_CPP_14
#include "json.hpp"

#define NEW_QUEUE
#if defined(NEW_QUEUE)
    #include "rwqueue/readerwriterqueue.h"
#endif


//#define HAS_CTX_VALUE 1
//#define NO_METADATA 1

using namespace std;
using namespace moodycamel;
using json = nlohmann::json;

#define NUM_ID_SPACES (256u)
#if defined(HAS_CTX_VALUE)
static constexpr unsigned MAX_GROUP_BITS = 32;
#else
static constexpr unsigned MAX_GROUP_BITS = 24;
#endif
static constexpr unsigned GROUPID_SHIFT = (64 - MAX_GROUP_BITS);
static constexpr uint64_t KEYID_MASK = ~(~(uint64_t)0 << GROUPID_SHIFT);
static constexpr unsigned MAX_GROUPS_ALLOWED = NUM_ID_SPACES;//(1u << MAX_GROUP_BITS);


#define MMA_NUM_CHUNKS_BITS (20u)
#define MMA_NUM_SUBCHUNKS_BITS ((32u)-(MMA_NUM_CHUNKS_BITS))
#define MMA_SUBCHUNK_SIZE (1u << MMA_NUM_CHUNKS_BITS)
#define MMA_SUBCHUNK_COUNT (1u << MMA_NUM_SUBCHUNKS_BITS)


/**
 * Returns the current resident memory use measured in Kb
 */
static
unsigned long getCurrentRSS() {
    unsigned long rss = 0;
    std::ifstream in("/proc/self/status");
    if(in.is_open()) {
        std::string line;
        while(std::getline(in, line)) {
            if(line.substr(0, 6) == "VmHWM:") {
                std::istringstream iss(line.substr(6));
                iss >> rss;
                break; // No need to read further
            }
        }
    }
    return rss; // Size is in kB
}

#if defined(HAS_CTX_VALUE)

typedef struct {
    int fd;
    size_t elemSize;
    off_t fsize;
    uint8_t *current;
    struct mma_map_s {
        struct mma_submap_s {
            uint8_t *base;
        } submap[MMA_SUBCHUNK_COUNT];
    } map[NUM_ID_SPACES];
} MMArray;

typedef struct {
    uint32_t primaryId[2];
    uint32_t spotId;
    uint32_t fragmentId;
    uint8_t  fragment_len[2]; /*** lowest byte of fragment length to prevent different sizes of primary and secondary alignments **/
    uint8_t  platform;
    uint8_t  pId_ext[2];
    uint8_t  spotId_ext;
    uint8_t  alignmentCount[2]; /* 0..254; 254: saturated max; 255: special meaning "too many" */
    uint8_t  unmated: 1,
             pcr_dup: 1,
             unaligned_1: 1,
             unaligned_2: 1,
             hardclipped: 1,
             primary_is_set: 1;
} ctx_value_t;

#define CTX_VALUE_SET_P_ID(O,N,V) do { int64_t tv = (V); (O).primaryId[N] = (uint32_t)tv; (O).pId_ext[N] = tv >> 32; } while(0);
#define CTX_VALUE_GET_P_ID(O,N) ((((int64_t)((O).pId_ext[N])) << 32) | (O).primaryId[N])

#define CTX_VALUE_SET_S_ID(O,V) do { int64_t tv = (V); (O).spotId = (uint32_t)tv; (O).spotId_ext = tv >> 32; } while(0);
#define CTX_VALUE_GET_S_ID(O) ((((int64_t)(O).spotId_ext) << 32) | (O).spotId)

#endif


struct u40_t
{
    vector<uint32_t> values;
    vector<uint8_t> ext;
    uint64_t get(size_t index) const {
        uint64_t v = ext[index];
        v <<= 32;
        v |= values[index];
        return v;
    }
};


struct FragmentInfo {
    uint64_t ti;
    uint32_t readlen;
    uint8_t  aligned;
    uint8_t  is_bad;
    uint8_t  orientation;
    uint8_t  readNo;
    uint8_t  sglen;
    uint8_t  lglen;
    uint8_t  cskey;
};



/**
 * @brief Data returned by bam_read threads
 *
 */
struct queue_rec_t
{
    BAM_Alignment* alignment{nullptr};  ///< BAM Alignment
    metadata_t* metadata{nullptr};      ///< Pointer to metadata
    uint32_t row_id{0};                 ///< Corresponding metadata row
};

struct context_t {
    array<const KLoadProgressbar*, 4> progress = {nullptr, nullptr, nullptr, nullptr};
    MemBank *frags = nullptr;
    uint64_t spotId = 0;
    uint64_t readCount = 0;
    uint64_t primaryId  = 0;
    uint64_t secondId  = 0;
    uint64_t alignCount  = 0;
    unsigned pass;
    bool isColorSpace;
    BAM_FilePosition m_fileOffset = 0;    ///< Position in the current BAM file
    BAM_FilePosition m_HeaderOffset = 0; 
    uint64_t m_inputSize = 0;             ///< Total size in bytes of all input files (can be 0 for stdin inputs)
    uint64_t m_processedSize = 0;         ///< Number of already processed bytes
    atomic<uint64_t> m_BankedSpots{0};               
    atomic<uint64_t> m_BankedSize{0};
    atomic<uint64_t> m_SpotSize{0};
    atomic<uint64_t> m_reference_len{0};
    atomic<uint64_t> m_estimated_spot_count{0};
    atomic<uint64_t> maxSpotLength{0};
    bool has_far_reads{false};
    size_t m_ReferenceSize{0};
    size_t m_ReferenceCount{0};
    json  mTelemetry;    
    size_t m_estimatedBatchSize = 0;      ///< Estimated size of the search batch
    bool m_calcBatchSize = true;          ///< Flag to indicate whether the batch needs to be calculated
    unique_ptr<tf::Executor> m_executor;  ///< Taskflow executor
#if defined(HAS_CTX_VALUE)
     MMArray *id2value;
#endif
    bool m_isSingleGroup{false};          ///< All reads belong to a single group (case when the number of groups in the header exceeds the allowed number)

    unsigned m_emptyGroupIndex = (unsigned)-1;      /**< When m_isSingleGroup is set, the index indicates that single group index,
                                               important when there are multiple inputs and one of them is determined to be singleGroup one*/
    using group_name_t = char;
    using group_index_t = uint32_t;
    tsl::array_map<group_name_t, group_index_t> m_group_map;  ///< group name to group index

    vector<unique_ptr<spot_assembly>> m_read_groups; ///< list of read groups
    shared_ptr<spot_name_filter> m_key_filter;   ///< Bloom filter (all spot names in scope)
    vector<u40_t> m_spot_id_buffer;              ///< Temporary buffer for spot name extraction

    // reset everything but spotId and spot assembly related fields
    void reset_for_remap() {
        for (const auto& p : progress) {
            if (p != nullptr)
               KLoadProgressbar_Release(p, true);
        }
        frags = nullptr;
        primaryId  = 0;
        secondId  = 0;
        alignCount  = 0;
        pass = 0;
        isColorSpace = false;
    }

    /**
     * @brief Set bloom filter based on the estimated number of spots
     * >9e9 spots - sha256
     * >2e9 - sha242
     * >1e9 - sha1
     * otherwise default: fnv + murmur
     *
     * @param num_spots
     */
    void set_key_filter(size_t num_spots) {
        assert(num_spots > 0);
        static bool is_set = false;
        if (is_set)
            return;
        if (num_spots > 3e9 && dynamic_cast<sha256_filter*>(m_key_filter.get()) == 0) {
            spdlog::stopwatch sw;
            m_key_filter.reset(new sha256_filter);
            for(auto& gr : m_read_groups) {
                gr->visit_spots([this](const char* spot_name) {
                    m_key_filter->seen_before(spot_name, strlen(spot_name));
                });
            }
            spdlog::info("SHA256 bloom filter rebuilt in {:.3}", sw);

        } else if (num_spots > 2e9 && dynamic_cast<sha224_filter*>(m_key_filter.get()) == 0) {
            spdlog::stopwatch sw;
            m_key_filter.reset(new sha224_filter);
            for(auto& gr : m_read_groups) {
                gr->visit_spots([this](const char* spot_name) {
                    m_key_filter->seen_before(spot_name, strlen(spot_name));
                });
            }
            spdlog::info("SHA224 bloom filter rebuilt in {:.3}", sw);

        } else if (num_spots > 1e9 && dynamic_cast<sha1_filter*>(m_key_filter.get()) == 0) {
            spdlog::stopwatch sw;
            m_key_filter.reset(new sha1_filter);
            for(auto& gr : m_read_groups) {
                gr->visit_spots([this](const char* spot_name) {
                    m_key_filter->seen_before(spot_name, strlen(spot_name));
                });
            }
            spdlog::info("SHA1 bloom filter rebuilt in {:.3}", sw);
        }
        is_set = true;
    }
    spot_assembly& add_read_group() {
        m_read_groups.emplace_back(make_unique<spot_assembly>(*m_executor, m_key_filter, m_read_groups.size(), G.searchBatchSize));
        return *m_read_groups.back().get();
    }

    /**
     * @brief Release memory spot assembly memory (volume's data, index. scanner). It doesn't touch metadata
     *
     */

    void release_search_memory() {
        m_group_map.clear();
        m_group_map.shrink_to_fit();
        m_key_filter.reset();
        for (auto&& s : m_read_groups) {
            s->release_search_memory();
        }
    }
/*
#if defined(HAS_CTX_VALUE)
    template<typename F>
    void visit_keyId(F&& f) {
        unsigned group_id = 0;
        for (auto& gr : m_read_groups) {
            gr->visit_keyId(f, group_id, GROUPID_SHIFT, metadata_t::e_fragmentId);
            ++group_id;
        }
    }
#endif
*/
    /**
     * @brief Extracts all spot_ids from metadata into m_spot_id_buffer and purges metadata spotId columns
     *
     */
    void extract_spotid() {
        m_spot_id_buffer.clear();
        int sz = m_read_groups.size();
        m_spot_id_buffer.resize(sz);
        tf::Taskflow taskflow;
        taskflow.for_each_index(0, sz,  1, [&](int i) {
            m_read_groups[i]->extract_64bit_column(metadata_t::e_spotId, m_spot_id_buffer[i].values, m_spot_id_buffer[i].ext, true);
        });
        m_executor->run(taskflow).wait();
    }

    /**
     * @brief Purges metadata column
     *
     * @tparam T
     * @param col_index
     */

    template<typename T>
    void clear_column(unsigned col_index) {
        for (auto& rg : m_read_groups) {
            rg->template clear_column<T>(col_index);
        }
    }


    /**
     * @brief Packs all groups that exceed batch_size limit
     *        groups with less then 1M spots ignored
     *
     * @param batch_size
     */
    void pack_read_groups(size_t batch_size)
    {
        size_t total_sz = 0;
        unsigned num_candidates = 0;
        for (auto& rg : m_read_groups) {
            if (rg->m_curr_row > 1e6) {
                ++num_candidates;
            }
            total_sz += rg->m_curr_row;
        }
        if (num_candidates > 0) {
            int batch_half = batch_size / 2;
            auto limit = ((num_candidates * batch_half) + batch_half) / num_candidates;
            for (auto& rg : m_read_groups) {
                if (rg->m_curr_row >= limit) {
                    total_sz -= rg->m_curr_row;
                    rg->pack_batch();
                }
            }
        }

        // Keep packing while total group_size exceeds batch_size * 2
        while (total_sz >= batch_size * 2) {
            int max_index = 0;
            size_t max_value = 0;
            for (size_t index = 0; index < m_read_groups.size(); ++index) {
                if (m_read_groups[index]->m_curr_row > max_value) {
                    max_value = m_read_groups[index]->m_curr_row;
                    max_index = index;
                }
            }
            total_sz -= max_value;
            m_read_groups[max_index]->pack_batch();
        }

    }
};





#if 0
static char const *Print_ctx_value_t(ctx_value_t const *const self)
{
    static char buffer[16384];
    rc_t rc = string_printf(buffer, sizeof(buffer), NULL, "pid: { %lu, %lu }, sid: %lu, fid: %u, alc: { %u, %u }, flg: %x", CTX_VALUE_GET_P_ID(*self, 0), CTX_VALUE_GET_P_ID(*self, 1), CTX_VALUE_GET_S_ID(*self), self->fragmentId, self->alignmentCount[0], self->alignmentCount[1], *(self->alignmentCount + sizeof(self->alignmentCount)/sizeof(self->alignmentCount[0])));

    if (rc)
        return 0;
    return buffer;
}
#endif

#if defined(HAS_CTX_VALUE)

static rc_t MMArrayMake(MMArray **rslt, int fd, uint32_t elemSize)
{
    MMArray *const self = (MMArray *)calloc(1, sizeof(*self));

    if (self == NULL)
        return RC(rcExe, rcMemMap, rcConstructing, rcMemory, rcExhausted);
    self->elemSize = (elemSize + 3) & ~(3u); /** align to 4 byte **/
    self->fd = fd;
    *rslt = self;
    return 0;
}

#define PERF 0
#define PROT 0

static rc_t MMArrayGet(MMArray *const self, void **const value, uint64_t const element)
{
    size_t const chunk = MMA_SUBCHUNK_SIZE * self->elemSize;
    unsigned const bin_no = element >> 32;
    unsigned const subbin = ((uint32_t)element) >> MMA_NUM_CHUNKS_BITS;
    unsigned const in_bin = (uint32_t)element & (MMA_SUBCHUNK_SIZE - 1);

    if (bin_no >= sizeof(self->map)/sizeof(self->map[0]))
        return RC(rcExe, rcMemMap, rcConstructing, rcId, rcExcessive);

    if (self->map[bin_no].submap[subbin].base == NULL) {
        off_t const cur_fsize = self->fsize;
        off_t const new_fsize = cur_fsize + chunk;

        if (ftruncate(self->fd, new_fsize) != 0)
            return RC(rcExe, rcFile, rcResizing, rcSize, rcExcessive);
        else {
            void *const base = mmap(NULL, chunk, PROT_READ|PROT_WRITE,
                                    MAP_FILE|MAP_SHARED, self->fd, cur_fsize);

            self->fsize = new_fsize;
            if (base == MAP_FAILED) {
                PLOGMSG(klogErr, (klogErr, "Failed to construct map for bin $(bin), subbin $(subbin)", "bin=%u,subbin=%u", bin_no, subbin));
                return RC(rcExe, rcMemMap, rcConstructing, rcMemory, rcExhausted);
            }
            else {
#if PERF
                static unsigned mapcount = 0;

                (void)PLOGMSG(klogInfo, (klogInfo, "Number of mmaps: $(cnt)", "cnt=%u", ++mapcount));
#endif
                self->map[bin_no].submap[subbin].base = (uint8_t*)base;
            }
        }
    }
    uint8_t *const next = self->map[bin_no].submap[subbin].base;
#if PROT
    if (next != self->current) {
        void *const current = self->current;

        if (current)
            mprotect(current, chunk, PROT_NONE);

        mprotect(self->current = next, chunk, PROT_READ|PROT_WRITE);
    }
#endif
    *value = &next[(size_t)in_bin * self->elemSize];
    return 0;
}

#if 0
static rc_t MMArrayGetRead(MMArray *const self, void const **const value, uint64_t const element)
{
    unsigned const bin_no = element >> 32;
    unsigned const subbin = ((uint32_t)element) >> MMA_NUM_CHUNKS_BITS;
    unsigned const in_bin = (uint32_t)element & (MMA_SUBCHUNK_SIZE - 1);

    if (bin_no >= sizeof(self->map)/sizeof(self->map[0]))
        return RC(rcExe, rcMemMap, rcConstructing, rcId, rcExcessive);

    if (self->map[bin_no].submap[subbin].base == NULL)
        return RC(rcExe, rcMemMap, rcReading, rcId, rcInvalid);

    uint8_t *const next = self->map[bin_no].submap[subbin].base;
#if PROT
    size_t const chunk = MMA_SUBCHUNK_SIZE * self->elemSize;
    if (next != self->current) {
        void *const current = self->current;

        if (current)
            mprotect(current, chunk, PROT_NONE);

        mprotect(self->current = next, chunk, PROT_READ);
    }
#endif
    *value = &next[(size_t)in_bin * self->elemSize];
    return 0;
}
#endif

static void MMArrayLock(MMArray *const self)
{
#if PROT
    size_t const chunk = MMA_SUBCHUNK_SIZE * self->elemSize;
    void *const current = self->current;

    self->current = NULL;
    if (current)
        mprotect(current, chunk, PROT_NONE);
#endif
}

static void MMArrayClear(MMArray *self)
{
    size_t const chunk = MMA_SUBCHUNK_SIZE * self->elemSize;
    unsigned i;

    for (i = 0; i != sizeof(self->map)/sizeof(self->map[0]); ++i) {
        unsigned j;

        for (j = 0; j != sizeof(self->map[0].submap)/sizeof(self->map[0].submap[0]); ++j) {
            if (self->map[i].submap[j].base) {
#if PROT
                mprotect(self->map[i].submap[j].base, chunk, PROT_READ|PROT_WRITE);
#endif
                memset(self->map[i].submap[j].base, 0, chunk);
#if PROT
                mprotect(self->map[i].submap[j].base, chunk, PROT_NONE);
#endif
            }
        }
    }
#if PROT
    self->current = NULL;
#endif
}

static void MMArrayWhack(MMArray *self)
{
    if ( self == NULL )
    {
        return;
    }

    size_t const chunk = MMA_SUBCHUNK_SIZE * self->elemSize;
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
#endif


static rc_t GetKeyIDOld(context_t *const ctx,
            queue_rec_t& queue_rec,
            char const key[], char const name[], unsigned const namelen)
{
    unsigned const keylen = strlen(key);
    assert(!ctx->m_read_groups.empty());
    auto& rs = *ctx->m_read_groups[ctx->m_emptyGroupIndex];
    BAM_Alignment& rec = *queue_rec.alignment;

    if (memcmp(key, name, keylen) == 0) {
        // qname starts with read group; no append
        auto& r = rs.find(name, namelen);
        rec.keyId = r.pos;
        rec.wasInserted = r.wasInserted;
        queue_rec.metadata = r.metadata;
        queue_rec.row_id = r.row_id;
    } else {
        char sbuf[4096];
        char *buf = sbuf;
        char *hbuf = NULL;
        size_t bsize = sizeof(sbuf);
        size_t actsize;

        if (keylen + namelen + 2 > bsize) {
            hbuf = (char*)malloc(bsize = keylen + namelen + 2);
            if (hbuf == NULL)
                return RC(rcExe, rcName, rcAllocating, rcMemory, rcExhausted);
            buf = hbuf;
        }
        string_printf(buf, bsize, &actsize, "%s\t%.*s", key, (int)namelen, name);

        auto& r = rs.find(buf, actsize);
        rec.keyId = r.pos;
        rec.wasInserted = r.wasInserted;
        queue_rec.metadata = r.metadata;
        queue_rec.row_id = r.row_id;

        if (hbuf)
            free(hbuf);
    }
    return 0;
}


#define USE_ILLUMINA_NAMING_CORRECTION 1

static size_t GetFixedNameLength(char const name[], size_t const namelen)
{
#if USE_ILLUMINA_NAMING_CORRECTION
    /*** Check for possible fixes to illumina names ****/
    size_t newlen=namelen;
    /*** First get rid of possible "/1" "/2" "/3" at the end - violates SAM spec **/
    if(newlen > 2  && name[newlen-2] == '/' &&  (name[newlen-1] == '1' || name[newlen-1] == '2' || name[newlen-1] == '3')){
        newlen -=2;
    }
    if(newlen > 2 && name[newlen-2] == '#' &&  (name[newlen-1] == '0')){ /*** Now, find "#0" ***/
        newlen -=2;
    } else if(newlen>10){ /*** find #ACGT ***/
        int i=newlen;
        for(i--;i>4;i--){ /*** stopping at 4 since the rest of record should still contain :x:y ***/
            char a=toupper(name[i]);
            if(a != 'A' && a != 'C' && a !='G' && a !='T'){
                break;
            }
        }
        if (name[i] == '#'){
            switch (newlen-i) { /** allowed values for illumina barcodes :5,6,8 **/
                case 5:
                case 6:
                case 8:
                    newlen=i;
                    break;
                default:
                    break;
            }
        }
    }
    if(newlen < namelen){ /*** check for :x:y at the end now - to make sure it is illumina **/
        int i=newlen;
        for(i--;i>0 && isdigit(name[i]);i--){}
        if(name[i]==':'){
            for(i--;i>0 && isdigit(name[i]);i--){}
            if(name[i]==':' && newlen > 0){ /*** some name before :x:y should still exist **/
                /*** looks like illumina ***/
                return newlen;
            }
        }
    }
#endif
    return namelen;
}

static
INSDC_SRA_platform_id GetINSDCPlatform(BAM_File const *bam, char const name[]) {
    if (name) {
        BAMReadGroup const *rg = NULL;

        BAM_FileGetReadGroupByName(bam, name, &rg);
        if (rg)
            return rg->platformId;
    }
    return SRA_PLATFORM_UNDEFINED;
}




static
rc_t GetKeyID(context_t *const ctx,
              const BAM_File* bam,
              queue_rec_t& queue_rec,
              char const key[],
              char const name[],
              size_t const o_namelen)
{
    static size_t key_count = 0;
    static size_t spot_count = 0;
    static size_t last_spot_count = 0;
    static int mem_prediction_err_count = 0;
    BAM_Alignment& rec = *queue_rec.alignment;
    size_t group_id = ctx->m_read_groups.size();
    size_t const namelen = GetFixedNameLength(name, o_namelen);
    if (ctx->m_isSingleGroup) {
        GetKeyIDOld(ctx, queue_rec, key, name, namelen);
        if (++key_count % 10000000 == 0) {
            auto& spot_assembly = *ctx->m_read_groups.front();
            spdlog::info("Group: '{}', batch memory {:L}, filter memory {:L}", key, spot_assembly.memory_used(), spot_assembly.m_key_filter->memory_used());
        }

    } else {
        auto [it, inserted] = ctx->m_group_map.emplace(key, group_id);
        if (!inserted) {
            group_id = it.value();
        } else {
            // Created new read group
            if (group_id >= MAX_GROUPS_ALLOWED) {
                (void)PLOGMSG(klogErr, (klogErr, "too many read groups: max is $(max)", "max=%d", (int) NUM_ID_SPACES));
                return RC(rcExe, rcTree, rcAllocating, rcConstraint, rcViolated);
            }
            ctx->add_read_group().m_platform = GetINSDCPlatform(bam, key);
        }
        auto& spot_assembly = *ctx->m_read_groups[group_id];
        auto& r = spot_assembly.find(name, namelen);
        rec.wasInserted = r.wasInserted;
        queue_rec.metadata = r.metadata;
        queue_rec.row_id = r.row_id;
        rec.platform = spot_assembly.m_platform;
        rec.keyId = (((uint64_t)group_id) << GROUPID_SHIFT) | r.pos;
        if (++key_count % 10000000 == 0) {
            spdlog::info("Group: '{}', batch memory {:L}, filter memory {:L}", key, spot_assembly.memory_used(), spot_assembly.m_key_filter->memory_used());
        }
    }
    spot_count += rec.wasInserted ? 1 : 0;

    if (spot_count % 1000000 == 0 && spot_count && spot_count != last_spot_count) {
        if (ctx->m_inputSize > 0) {
            last_spot_count = spot_count;
            BAM_FilePosition end_pos = 0;
            BAM_FileGetPosition(bam, &end_pos);
            end_pos >>= 16;
            size_t chunk_len = end_pos - ctx->m_fileOffset;
            ctx->m_processedSize += chunk_len;
            ctx->m_fileOffset = end_pos;
            if (ctx->m_processedSize) {
                int pct = round(100. * (float)ctx->m_processedSize/ctx->m_inputSize);
                // Estimated batch size until 10% of input size is processed
                if (pct <= 10) {
                    size_t num_chunks = (ctx->m_inputSize / ctx->m_processedSize) + 1;
                    size_t num_spots = num_chunks * spot_count;
                    ctx->m_estimated_spot_count = num_spots;
                    if (spot_count % 10000000 == 0) {
                        ctx->set_key_filter(num_spots);
                        ctx->m_estimatedBatchSize = min<int>(G.searchBatchSize, num_spots/(ctx->m_executor->num_workers() - 1));
                        ctx->m_estimatedBatchSize = max<int>(G.minBatchSize, ctx->m_estimatedBatchSize);
                        spdlog::info("Current spot_count: {:L}, estimated spot count {:L}, estimated batch size: {:L}", spot_count, num_spots, ctx->m_estimatedBatchSize);
                    }
                }
                if (pct >= 10 && pct <= 50) {
                    size_t est_mem_cnt = 1;
                    int last_pct = 0;
                    int last_est_mem = 0;
                    const auto& prev_items = ctx->mTelemetry["estimates"];
                    if (prev_items.size() > 0) {
                        est_mem_cnt += prev_items.size();
                        auto& prev = prev_items[to_string(prev_items.size())];
                        last_pct = prev["pct-done"].get<int>();
                        last_est_mem = prev["est-max-mem-kb"].get<int>();
                    }
                    if ((pct - last_pct) > 1) {
                        size_t sa_mem_used = 0;
                        for (const auto& sa : ctx->m_read_groups) {
                            sa_mem_used += sa->memory_used();
                        }
                        size_t est_mem = ((float)(sa_mem_used * ctx->m_estimated_spot_count)/ctx->spotId)  + ctx->m_ReferenceSize + ctx->m_key_filter->memory_used();
                        size_t est_mem_gb = est_mem/(1024 * 1024 * 1024);
                        (void)PLOGMSG(klogInfo, (klogInfo, "Estimate mem $(m1), limit mem: $(m2)", "m1=%lu,m2=%lu", est_mem_gb, G.LOADER_MEM_LIMIT_GB));
                        json& j = ctx->mTelemetry["estimates"][to_string(est_mem_cnt)];
                        j["est-max-mem-kb"] = est_mem/1024;
                        j["est-spot-count"] = ctx->m_estimated_spot_count.load();
                        j["spot-assembly-mem-kb"] = sa_mem_used/1024;
                        j["reference-mem-kb"] = ctx->m_reference_len.load()/1024;
                        j["pending-spot-size"] = ctx->m_BankedSize.load();
                        j["pending-spot-count"] = ctx->m_BankedSpots.load();
                        j["spot-count"] = ctx->spotId;
                        j["spot-size"] = ctx->m_SpotSize.load();
                        j["max-mem-kb"] = getCurrentRSS();
                        j["pct-done"] = pct;
                        if (G.LOADER_MEM_LIMIT_GB > 0 && float(est_mem_gb)/G.LOADER_MEM_LIMIT_GB > 1.25 && est_mem_cnt > 1) {

                            int x1 = last_pct;
                            int x2 = pct;
                            if (x2 - x1 <= 0) {
                                PLOGMSG(klogErr, (klogErr, "SRAE-68: Estimated memory usage $(pred_m) GB exceeds loader memory limit $(limit_m) GB at $(pct)%", "pred_m=%lu,limit_m=%lu,pct=%u", est_mem_gb, G.LOADER_MEM_LIMIT_GB, pct));
                                return RC(rcExe, rcNoTarg, rcProjecting, rcMemory, rcExcessive);                        
                            }
                            int y1 = last_est_mem;
                            int y2 = est_mem/1024;

                            float m = (float)(y2 - y1)/(x2 - x1);
                            float b = y1 - m * x1;
                            float projected_mem_kb = m * 50 + b; // projected memory at 50%
                            j["projected-mem-kb"] = round(projected_mem_kb * 100) / 100;
                            if (projected_mem_kb > 0 && projected_mem_kb/(G.LOADER_MEM_LIMIT_GB * 1024 * 1024) > 1.25) {
                                if (++mem_prediction_err_count > 3) {
                                    PLOGMSG(klogErr, (klogErr, "SRAE-68: Estimated memory usage $(pred_m) GB exceeds loader memory limit $(limit_m) GB at $(pct)%", "pred_m=%lu,limit_m=%lu,pct=%u", est_mem_gb, G.LOADER_MEM_LIMIT_GB, pct));
                                    return RC(rcExe, rcNoTarg, rcProjecting, rcMemory, rcExcessive);                        
                                } else {
                                    spdlog::warn("Estimated memory usage {:L} GB exceeds loader memory limit {:L} GB at {:L}%, count {}", est_mem_gb, G.LOADER_MEM_LIMIT_GB, pct, mem_prediction_err_count);
                                }
                            } else {
                                mem_prediction_err_count = 0;
                            }
                        }
                    }
                }
            }
        } else if (G.LOADER_MEM_LIMIT_GB > 0) {
            size_t curr_mem_gb = getCurrentRSS()/(1024 * 1024);
            // input is stdin. we can't make RAM  estimate so we bail out if current memory exceeds the limit
            if (float(curr_mem_gb)/G.LOADER_MEM_LIMIT_GB > 1.25) {
                PLOGMSG(klogErr, (klogErr, "SRAE-74: Memory usage $(pred_m) GB exceeds loader memory limit $(limit_m) GB", "pred_m=%lu,limit_m=%lu,pct=%u", curr_mem_gb, G.LOADER_MEM_LIMIT_GB));
                return RC(rcExe, rcNoTarg, rcProjecting, rcMemory, rcExcessive);                        
            }
        }
        if (spot_count % 10000000 == 0)
            ctx->pack_read_groups(ctx->m_estimatedBatchSize);
    }

    return 0;
}

#if defined HAS_CTX_VALUE
static rc_t OpenMMapFile(context_t *const ctx, KDirectory *const dir)
{
    int fd;
    char fname[4096];
    rc_t rc = string_printf(fname, sizeof(fname), NULL, "%s/id2value.%u", G.tmpfs, G.pid);

    if (rc)
        return rc;

    fd = open(fname, O_RDWR|O_TRUNC|O_CREAT, S_IRUSR|S_IWUSR);
    if (fd < 0)
        return RC(rcExe, rcFile, rcCreating, rcFile, rcNotFound);
    unlink(fname);
    return MMArrayMake(&ctx->id2value, fd, sizeof(ctx_value_t));
}

#endif
static rc_t TmpfsDirectory(KDirectory **const rslt)
{
    KDirectory *dir;
    rc_t rc = KDirectoryNativeDir(&dir);
    if (rc == 0) {
        rc = KDirectoryOpenDirUpdate(dir, rslt, false, "%s", G.tmpfs);
        KDirectoryRelease(dir);
    }
    return rc;
}

static rc_t SetupContext(context_t *ctx, unsigned numfiles)
{
    rc_t rc = 0;

    // memset(ctx, 0, sizeof(*ctx));

    if (G.mode == mode_Archive) {
        KDirectory *dir;
        size_t fragSize[2];

        fragSize[1] = (G.cache_size / 8);
        fragSize[0] = fragSize[1] * 4;

        rc = TmpfsDirectory(&dir);
#if defined HAS_CTX_VALUE
        if (rc == 0)
            rc = OpenMMapFile(ctx, dir);
#endif
        if (rc == 0)
            rc = MemBankMake(&ctx->frags, dir, G.pid, fragSize);
        KDirectoryRelease(dir);
    }
    else if (G.mode == mode_Remap) {
        ctx->reset_for_remap();
    }

    rc = KLoadProgressbar_Make(&ctx->progress[0], 0); if (rc) return rc;
    rc = KLoadProgressbar_Make(&ctx->progress[1], 0); if (rc) return rc;
    rc = KLoadProgressbar_Make(&ctx->progress[2], 0); if (rc) return rc;
    rc = KLoadProgressbar_Make(&ctx->progress[3], 0); if (rc) return rc;

    KLoadProgressbar_Append(ctx->progress[0], 100 * numfiles);
    ctx->m_estimatedBatchSize = G.searchBatchSize;
    ctx->m_key_filter.reset(new fnv_murmur_filter);
    ctx->m_executor.reset(new tf::Executor(G.numThreads));
    return rc;
}

static void ContextReleaseMemBank(context_t *ctx)
{
    MemBankRelease(ctx->frags);
    ctx->frags = NULL;
}

static void ContextRelease(context_t *ctx, bool continuing)
{
    KLoadProgressbar_Release(ctx->progress[0], true);
    KLoadProgressbar_Release(ctx->progress[1], true);
    KLoadProgressbar_Release(ctx->progress[2], true);
    KLoadProgressbar_Release(ctx->progress[3], true);
#ifdef HAS_CTX_VALUE
    if (!continuing)
        MMArrayWhack(ctx->id2value);
    else
        MMArrayClear(ctx->id2value);
#endif

}

static
void COPY_QUAL(uint8_t D[], uint8_t const S[], unsigned const L, bool const R)
{
    if (R) {
        unsigned i;
        unsigned j;

        for (i = 0, j = L - 1; i != L; ++i, --j)
            D[i] = S[j];
    }
    else
        memmove(D, S, L);
}

static
void COPY_READ(INSDC_dna_text D[], INSDC_dna_text const S[], unsigned const L, bool const R)
{
    static INSDC_dna_text const complement[] = {
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 , '.',  0 ,
        '0', '1', '2', '3',  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 , 'T', 'V', 'G', 'H',  0 ,  0 , 'C',
        'D',  0 ,  0 , 'M',  0 , 'K', 'N',  0 ,
         0 ,  0 , 'Y', 'S', 'A', 'A', 'B', 'W',
         0 , 'R',  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 , 'T', 'V', 'G', 'H',  0 ,  0 , 'C',
        'D',  0 ,  0 , 'M',  0 , 'K', 'N',  0 ,
         0 ,  0 , 'Y', 'S', 'A', 'A', 'B', 'W',
         0 , 'R',  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0
    };
    if (R) {
        unsigned i;
        unsigned j;

        for (i = 0, j = L - 1; i != L; ++i, --j)
            D[i] = complement[((uint8_t const *)S)[j]];
    }
    else
        memmove(D, S, L);
}

static KFile *MakeDeferralFile() {
    if (G.deferSecondary) {
        char tmplate[4096];
        int fd;
        KFile *f;
        KDirectory *d;
        size_t nwrit;

        KDirectoryNativeDir(&d);
        string_printf(tmplate, sizeof(tmplate), &nwrit, "%s/defer.XXXXXX", G.tmpfs);
        fd = mkstemp(tmplate);
        KDirectoryOpenFileWrite(d, &f, true, tmplate);
        KDirectoryRelease(d);
        close(fd);
        unlink(tmplate);
        return f;
    }
    return NULL;
}


static rc_t OpenBAM(const BAM_File **bam, VDatabase *db, const char bamFile[])
{
    rc_t rc = 0;
    KFile *defer = MakeDeferralFile();

    if (strcmp(bamFile, "/dev/stdin") == 0) {
        rc = BAM_FileMake(bam, defer, G.headerText, "/dev/stdin");
    }
    else {
        rc = BAM_FileMake(bam, defer, G.headerText, "%s", bamFile);
    }
    KFileRelease(defer); /* it was retained by BAM file */

    if (rc) {
        (void)PLOGERR(klogErr, (klogErr, rc, "Failed to open '$(file)'", "file=%s", bamFile));
    }
    else if (db) {
        KMetadata *dbmeta;

        rc = VDatabaseOpenMetadataUpdate(db, &dbmeta);
        if (rc == 0) {
            KMDataNode *node;

            rc = KMetadataOpenNodeUpdate(dbmeta, &node, "BAM_HEADER");
            KMetadataRelease(dbmeta);
            if (rc == 0) {
                char const *header;
                size_t size;

                rc = BAM_FileGetHeaderText(*bam, &header, &size);
                if (rc == 0) {
                    rc = KMDataNodeWrite(node, header, size);
                }
                KMDataNodeRelease(node);
            }
        }
    }

    return rc;
}

static rc_t CollectReferences(context_t *ctx, unsigned bamFiles, char const *bamFile[])
{
    rc_t rc = 0;
    map<string, size_t> references;     ///< name, count

    for (unsigned i = 0; i < bamFiles && rc == 0; ++i) {
        if (strcmp(bamFile[i], "/dev/stdin") == 0) 
            continue;
        const BAM_File *bam = nullptr;
        rc = OpenBAM(&bam, NULL, bamFile[i]);
        if (rc) return rc;
        uint32_t n;
        BAM_FileGetRefSeqCount(bam, &n);
        BAMRefSeq const *refSeq;
        for (unsigned k = 0; k != n; ++k) {
            BAM_FileGetRefSeq(bam, k, &refSeq);
            references.emplace(string(refSeq->name), refSeq->length);
        }
        BAM_FileRelease(bam);
    }
    ctx->m_ReferenceSize = 0;
    ctx->m_ReferenceCount = references.size();
    for (const auto& it : references)
        ctx->m_ReferenceSize += it.second;
    return rc;
}

static rc_t VerifyReferences(BAM_File const *bam, Reference const *ref)
{
    rc_t rc = 0;
    uint32_t n;
    unsigned i;

    BAM_FileGetRefSeqCount(bam, &n);
    for (i = 0; i != n; ++i) {
        BAMRefSeq const *refSeq;

        BAM_FileGetRefSeq(bam, i, &refSeq);
        if (G.refFilter && strcmp(refSeq->name, G.refFilter) != 0)
            continue;

        rc = ReferenceVerify(ref, refSeq->name, refSeq->length, refSeq->checksum);
        if (rc) {
            if (GetRCObject(rc) == rcId && GetRCState(rc) == rcUndefined) {
                (void)PLOGMSG(klogInfo, (klogInfo, "Reference: '$(name)' is unmapped", "name=%s", refSeq->name));
            }
            else if (GetRCObject(rc) == rcChecksum && GetRCState(rc) == rcUnequal) {
#if NCBI
                (void)PLOGMSG(klogWarn, (klogWarn, "Reference: '$(name)', Length: $(len); checksums do not match", "name=%s,len=%u", refSeq->name, (unsigned)refSeq->length));
#endif
            }
            else if (GetRCObject(rc) == rcSize && GetRCState(rc) == rcUnequal) {
                (void)PLOGMSG(klogWarn, (klogWarn, "Reference: '$(name)', Length: $(len); lengths do not match", "name=%s,len=%u", refSeq->name, (unsigned)refSeq->length));
            }
            else if (GetRCObject(rc) == rcSize && GetRCState(rc) == rcEmpty) {
                (void)PLOGMSG(klogWarn, (klogWarn, "Reference: '$(name)', Length: $(len); fasta file is empty", "name=%s,len=%u", refSeq->name, (unsigned)refSeq->length));
            }
            else if (GetRCObject(rc) == rcId && GetRCState(rc) == rcNotFound) {
                (void)PLOGMSG(klogWarn, (klogWarn, "Reference: '$(name)', Length: $(len); no match found", "name=%s,len=%u", refSeq->name, (unsigned)refSeq->length));
            }
            else {
                (void)PLOGERR(klogWarn, (klogWarn, rc, "Reference: '$(name)', Length: $(len); error", "name=%s,len=%u", refSeq->name, (unsigned)refSeq->length));
            }
        }
        else if (G.onlyVerifyReferences) {
            (void)PLOGMSG(klogInfo, (klogInfo, "Reference: '$(name)', Length: $(len); match found", "name=%s,len=%u", refSeq->name, (unsigned)refSeq->length));
        }
    }
    return 0;
}

static uint8_t GetMapQ(BAM_Alignment const *rec)
{
    uint8_t mapQ;

    BAM_AlignmentGetMapQuality(rec, &mapQ);
    return mapQ;
}

#if 0
static bool EditAlignedQualities(uint8_t qual[], bool const hasMismatch[], unsigned readlen)
{
    unsigned i;
    bool changed = false;

    for (i = 0; i < readlen; ++i) {
        uint8_t const q_0 = qual[i];
        uint8_t const q_1= hasMismatch[i] ? G.alignedQualValue : q_0;

        if (q_0 != q_1) {
            changed = true;
            break;
        }
    }
    if (!changed)
        return false;
    for (i = 0; i < readlen; ++i) {
        uint8_t const q_0 = qual[i];
        uint8_t const q_1= hasMismatch[i] ? G.alignedQualValue : q_0;

        qual[i] = q_1;
    }
    return true;
}
#endif

#if 0
static bool EditUnalignedQualities(uint8_t qual[], bool const hasMismatch[], unsigned readlen)
{
    unsigned i;
    bool changed = false;

    for (i = 0; i < readlen; ++i) {
        uint8_t const q_0 = qual[i];
        uint8_t const q_1 = (q_0 & 0x7F) | (hasMismatch[i] ? 0x80 : 0);

        if (q_0 != q_1) {
            changed = true;
            break;
        }
    }
    if (!changed)
        return false;
    for (i = 0; i < readlen; ++i) {
        uint8_t const q_0 = qual[i];
        uint8_t const q_1 = (q_0 & 0x7F) | (hasMismatch[i] ? 0x80 : 0);

        qual[i] = q_1;
    }
    return true;
}
#endif


static
rc_t CheckLimitAndLogError(void)
{
    unsigned const count = ++G.errCount;
    if (G.maxErrCount > 0 && count > G.maxErrCount) {
        (void)PLOGERR(klogErr, (klogErr, SILENT_RC(rcAlign, rcFile, rcReading, rcError, rcExcessive), "Number of errors $(cnt) exceeds limit of $(max): Exiting", "cnt=%u,max=%u", count, G.maxErrCount));
        return RC(rcAlign, rcFile, rcReading, rcError, rcExcessive);
    }
    return 0;
}

static
void RecordNoMatch(char const readName[], char const refName[], uint32_t const refPos)
{
    if (G.noMatchLog) {
        static uint64_t lpos = 0;
        char logbuf[256];
        size_t len;

        if (string_printf(logbuf, sizeof(logbuf), &len, "%s\t%s\t%u\n", readName, refName, refPos) == 0) {
            KFileWrite(G.noMatchLog, lpos, logbuf, len, NULL);
            lpos += len;
        }
    }
}

static LowMatchCounter *lmc = NULL;

static
rc_t LogNoMatch(char const readName[], char const refName[], unsigned rpos, unsigned matches)
{
    rc_t const rc = CheckLimitAndLogError();
    static unsigned count = 0;

    if (lmc == NULL)
        lmc = LowMatchCounterMake();
    assert(lmc != NULL);
    LowMatchCounterAdd(lmc, refName);

    ++count;
    if (rc) {
        (void)PLOGMSG(klogInfo, (klogInfo, "This is the last warning; this class of warning occurred $(occurred) times",
                                 "occurred=%u", count));
        (void)PLOGMSG(klogErr, (klogErr, "Spot '$(name)' contains too few ($(count)) matching bases to reference '$(ref)' at $(pos)",
                                 "name=%s,ref=%s,pos=%u,count=%u", readName, refName, rpos, matches));
        return rc;
    }
    if (G.maxWarnCount_NoMatch == 0 || count < G.maxWarnCount_NoMatch)
        (void)PLOGMSG(klogWarn, (klogWarn, "Spot '$(name)' contains too few ($(count)) matching bases to reference '$(ref)' at $(pos)",
                                 "name=%s,ref=%s,pos=%u,count=%u", readName, refName, rpos, matches));
    return 0;
}

struct rlmc_context {
    KMDataNode *node;
    unsigned node_number;
    rc_t rc;
};

static void RecordLowMatchCount(void *Ctx, char const name[], unsigned const count)
{
    struct rlmc_context *const ctx = (rlmc_context *)Ctx;

    if (ctx->rc == 0) {
        KMDataNode *sub = NULL;

        ctx->rc = KMDataNodeOpenNodeUpdate(ctx->node, &sub, "LOW_MATCH_COUNT_%u", ++ctx->node_number);
        if (ctx->rc == 0) {
            uint32_t const count_temp = count;
            ctx->rc = KMDataNodeWriteAttr(sub, "REFNAME", name);
            if (ctx->rc == 0)
                ctx->rc = KMDataNodeWriteB32(sub, &count_temp);

            KMDataNodeRelease(sub);
        }
    }
}

static rc_t RecordLowMatchCounts(KMDataNode *const node)
{
    struct rlmc_context ctx;

    assert(lmc != NULL);
    if (node) {
        ctx.node = node;
        ctx.node_number = 0;
        ctx.rc = 0;

        LowMatchCounterEach(lmc, &ctx, RecordLowMatchCount);
    }
    return ctx.rc;
}

#if 0
static
rc_t LogDupConflict(char const readName[])
{
    rc_t const rc = CheckLimitAndLogError();
    static unsigned count = 0;

    ++count;
    if (rc) {
        (void)PLOGMSG(klogInfo, (klogInfo, "This is the last warning; this class of warning occurred $(occurred) times",
                                 "occurred=%u", count));
        (void)PLOGERR(klogWarn, (klogWarn, SILENT_RC(rcApp, rcFile, rcReading, rcData, rcInconsistent),
                                 "Spot '$(name)' is both a duplicate and NOT a duplicate!",
                                 "name=%s", readName));
    }
    else if (G.maxWarnCount_DupConflict == 0 || count < G.maxWarnCount_DupConflict)
        (void)PLOGERR(klogWarn, (klogWarn, SILENT_RC(rcApp, rcFile, rcReading, rcData, rcInconsistent),
                                 "Spot '$(name)' is both a duplicate and NOT a duplicate!",
                                 "name=%s", readName));
    return rc;
}
#endif

static char const *const CHANGED[] = {
    "FLAG changed",
    "QUAL changed",
    "SEQ changed",
    "record made unaligned",
    "record made unfragmented",
    "mate alignment lost",
    "record discarded",
    "reference name changed",
    "CIGAR changed"
};

#define FLAG_CHANGED (0)
#define QUAL_CHANGED (1)
#define SEQ_CHANGED (2)
#define MAKE_UNALIGNED (3)
#define MAKE_UNFRAGMENTED (4)
#define MATE_LOST (5)
#define DISCARDED (6)
#define REF_NAME_CHANGED (7)
#define CIGAR_CHANGED (8)

static char const *const REASONS[] = {
/* FLAG changed */
    "0x400 and 0x200 both set",                 /*  0 */
    "conflicting PCR Dup flags",                /*  1 */
    "primary alignment already exists",         /*  2 */
    "was already recorded as unaligned",        /*  3 */
/* QUAL changed */
    "original quality used",                    /*  4 */
    "unaligned colorspace",                     /*  5 */
    "aligned bases",                            /*  6 */
    "unaligned bases",                          /*  7 */
    "reversed",                                 /*  8 */
/* unaligned */
    "low MAPQ",                                 /*  9 */
    "low match count",                          /* 10 */
    "missing alignment info",                   /* 11 */
    "missing reference position",               /* 12 */
    "invalid alignment info",                   /* 13 */
    "invalid reference position",               /* 14 */
    "invalid reference",                        /* 15 */
    "unaligned reference",                      /* 16 */
    "unknown reference",                        /* 17 */
    "hard-clipped colorspace",                  /* 18 */
/* unfragmented */
    "missing fragment info",                    /* 19 */
    "too many fragments",                       /* 20 */
/* mate info lost */
    "invalid mate reference",                   /* 21 */
    "missing mate alignment info",              /* 22 */
    "unknown mate reference",                   /* 23 */
/* discarded */
    "conflicting PCR duplicate",                /* 24 */
    "conflicting fragment info",                /* 25 */
    "reference is skipped",                     /* 26 */
/* reference name changed */
    "reference was named more than once",       /* 27 */
/* CIGAR changed */
    "alignment overhanging end of reference",   /* 28 */
/* discarded */
    "hard-clipped secondary alignment",         /* 29 */
    "low-matching secondary alignment",         /* 30 */
};

static struct {
    unsigned what, why;
} const CHANGES[] = {
    {FLAG_CHANGED,  0},
    {FLAG_CHANGED,  1},
    {FLAG_CHANGED,  2},
    {FLAG_CHANGED,  3},
    {QUAL_CHANGED,  4},
    {QUAL_CHANGED,  5},
    {QUAL_CHANGED,  6},
    {QUAL_CHANGED,  7},
    {QUAL_CHANGED,  8},
    {SEQ_CHANGED,  8},
    {MAKE_UNALIGNED,  9},
    {MAKE_UNALIGNED, 10},
    {MAKE_UNALIGNED, 11},
    {MAKE_UNALIGNED, 12},
    {MAKE_UNALIGNED, 13},
    {MAKE_UNALIGNED, 14},
    {MAKE_UNALIGNED, 15},
    {MAKE_UNALIGNED, 16},
    {MAKE_UNALIGNED, 17},
    {MAKE_UNALIGNED, 18},
    {MAKE_UNFRAGMENTED, 19},
    {MAKE_UNFRAGMENTED, 20},
    {MATE_LOST, 21},
    {MATE_LOST, 22},
    {MATE_LOST, 23},
    {DISCARDED, 24},
    {DISCARDED, 25},
    {DISCARDED, 26},
    {DISCARDED, 17},
    {REF_NAME_CHANGED, 27},
    {CIGAR_CHANGED, 28},
    {DISCARDED, 29},
    {DISCARDED, 30},
};

#define NUMBER_OF_CHANGES ((unsigned)(sizeof(CHANGES)/sizeof(CHANGES[0])))
static unsigned change_counter[NUMBER_OF_CHANGES];

static void LOG_CHANGE(unsigned const change)
{
    ++change_counter[change];
}

static void PrintChangeReport(void)
{
    unsigned i;

    for (i = 0; i != NUMBER_OF_CHANGES; ++i) {
        if (change_counter[i] > 0) {
            char const *const what = CHANGED[CHANGES[i].what];
            char const *const why  = REASONS[CHANGES[i].why];

            PLOGMSG(klogInfo, (klogInfo, "$(what) $(times) times because $(reason)", "what=%s,reason=%s,times=%u", what, why, change_counter[i]));
        }
    }
}

static rc_t RecordChange(KMDataNode *const node,
                         char const node_name[],
                         unsigned const node_number,
                         char const what[],
                         char const why[],
                         unsigned const count)
{
    KMDataNode *sub = NULL;
    rc_t const rc_sub = KMDataNodeOpenNodeUpdate(node, &sub, "%s_%u", node_name, node_number);

    if (rc_sub) return rc_sub;
    {
        uint32_t const count_temp = count;
        rc_t const rc_attr1 = KMDataNodeWriteAttr(sub, "change", what);
        rc_t const rc_attr2 = KMDataNodeWriteAttr(sub, "reason", why);
        rc_t const rc_value = KMDataNodeWriteB32(sub, &count_temp);

        KMDataNodeRelease(sub);
        if (rc_attr1) return rc_attr1;
        if (rc_attr2) return rc_attr2;
        if (rc_value) return rc_value;

        return 0;
    }
}

static rc_t RecordChanges(KMDataNode *const node, char const name[])
{
    if (node) {
        unsigned i;
        unsigned j = 0;

        for (i = 0; i != NUMBER_OF_CHANGES; ++i) {
            if (change_counter[i] > 0) {
                char const *const what = CHANGED[CHANGES[i].what];
                char const *const why  = REASONS[CHANGES[i].why];
                rc_t const rc = RecordChange(node, name, ++j, what, why, change_counter[i]);

                if (rc) return rc;
            }
        }
    }
    return 0;
}

#define FLAG_CHANGED_400_AND_200   do { LOG_CHANGE( 0); } while(0)
#define FLAG_CHANGED_PCR_DUP       do { LOG_CHANGE( 1); } while(0)
#define FLAG_CHANGED_PRIMARY_DUP   do { LOG_CHANGE( 2); } while(0)
#define FLAG_CHANGED_WAS_UNALIGNED do { LOG_CHANGE( 3); } while(0)
#define QUAL_CHANGED_OQ            do { LOG_CHANGE( 4); } while(0)
#define QUAL_CHANGED_UNALIGNED_CS  do { LOG_CHANGE( 5); } while(0)
#define QUAL_CHANGED_ALIGNED_EDIT  do { LOG_CHANGE( 6); } while(0)
#define QUAL_CHANGED_UNALIGN_EDIT  do { LOG_CHANGE( 7); } while(0)
#define QUAL_CHANGED_REVERSED      do { LOG_CHANGE( 8); } while(0)
#define SEQ__CHANGED_REV_COMP      do { LOG_CHANGE( 9); } while(0)
#define UNALIGNED_LOW_MAPQ         do { LOG_CHANGE(10); } while(0)
#define UNALIGNED_LOW_MATCH_COUNT  do { LOG_CHANGE(11); } while(0)
#define UNALIGNED_MISSING_INFO     do { LOG_CHANGE(12); } while(0)
#define UNALIGNED_MISSING_REF_POS  do { LOG_CHANGE(13); } while(0)
#define UNALIGNED_INVALID_INFO     do { LOG_CHANGE(14); } while(0)
#define UNALIGNED_INVALID_REF_POS  do { LOG_CHANGE(15); } while(0)
#define UNALIGNED_INVALID_REF      do { LOG_CHANGE(16); } while(0)
#define UNALIGNED_UNALIGNED_REF    do { LOG_CHANGE(17); } while(0)
#define UNALIGNED_UNKNOWN_REF      do { LOG_CHANGE(18); } while(0)
#define UNALIGNED_HARD_CLIPPED_CS  do { LOG_CHANGE(19); } while(0)
#define UNFRAGMENT_MISSING_INFO    do { LOG_CHANGE(20); } while(0)
#define UNFRAGMENT_TOO_MANY        do { LOG_CHANGE(21); } while(0)
#define MATE_INFO_LOST_INVALID     do { LOG_CHANGE(22); } while(0)
#define MATE_INFO_LOST_MISSING     do { LOG_CHANGE(23); } while(0)
#define MATE_INFO_LOST_UNKNOWN_REF do { LOG_CHANGE(24); } while(0)
#define DISCARD_PCR_DUP            do { LOG_CHANGE(25); } while(0)
#define DISCARD_BAD_FRAGMENT_INFO  do { LOG_CHANGE(26); } while(0)
#define DISCARD_SKIP_REFERENCE     do { LOG_CHANGE(27); } while(0)
#define DISCARD_UNKNOWN_REFERENCE  do { LOG_CHANGE(28); } while(0)
#define RENAMED_REFERENCE          do { LOG_CHANGE(29); } while(0)
#define OVERHANGING_ALIGNMENT      do { LOG_CHANGE(30); } while(0)
#define DISCARD_HARDCLIP_SECONDARY do { LOG_CHANGE(31); } while(0)
#define DISCARD_BAD_SECONDARY      do { LOG_CHANGE(32); } while(0)

static bool isHardClipped(unsigned const ops, uint32_t const cigar[/* ops */])
{
    unsigned i;

    for (i = 0; i < ops; ++i) {
        uint32_t const op = cigar[i];
        int const code = op & 0x0F;

        if (code == 5)
            return true;
    }
    return false;
}

static rc_t FixOverhangingAlignment(KDataBuffer *cigBuf, uint32_t *opCount, uint32_t refPos, uint32_t refLen, uint32_t readlen)
{
    uint32_t const *cigar = (uint32_t*)cigBuf->base;
    uint32_t refend = refPos;
    uint32_t seqpos = 0;
    uint32_t i;

    for (i = 0; i < *opCount; ++i) {
        uint32_t const op = cigar[i];
        uint32_t const len = op >> 4;
        uint32_t const code = op & 0x0F;

        switch (code) {
        case 0: /* M */
        case 7: /* = */
        case 8: /* X */
            seqpos += len;
            refend += len;
            break;
        case 2: /* D */
        case 3: /* N */
            refend += len;
            break;
        case 1: /* I */
        case 4: /* S */
        case 9: /* B */
            seqpos += len;
        default:
            break;
        }
        if (refend > refLen) {
            uint32_t const chop = refend - refLen;
            uint32_t const newlen = len - chop;
            uint32_t const left = seqpos - chop;
            if (left * 2 > readlen) {
                uint32_t const clip = readlen - left;
                rc_t rc;

                *opCount = i + 2;
                rc = KDataBufferResize(cigBuf, *opCount);
                if (rc) return rc;
                ((uint32_t *)cigBuf->base)[i  ] = (newlen << 4) | code;
                ((uint32_t *)cigBuf->base)[i+1] = (clip << 4) | 4;
                OVERHANGING_ALIGNMENT;
                break;
            }
        }
    }
    return 0;
}

static context_t GlobalContext;
#ifdef NEW_QUEUE
static ReaderWriterQueue<queue_rec_t> rw_queue{1024};
atomic<bool> rw_done{false};
#else
static KQueue *bamq;
#endif
static KThread *bamread_thread;

static rc_t BAM_FileReadDetached(BAM_File const *self, BAM_Alignment **rec)
{
    BAM_Alignment const *crec = NULL;
    rc_t const rc = BAM_FileRead2(self, &crec);
    if (rc == 0) {
        if ((*rec = BAM_AlignmentDetach(crec)) != NULL)
            return 0;
        return RC(rcAlign, rcFile, rcReading, rcMemory, rcExhausted);
    }
    BAM_AlignmentRelease(crec);
    return rc;
}

static rc_t run_bamread_thread(const KThread *self, void *const file)
{
    rc_t rc = 0;
    size_t NR = 0;
    auto bam = (const BAM_File*)file;
    while (rc == 0) {
        if (rw_done)
            break;
        BAM_Alignment *rec = NULL;
        ++NR;
        rc = BAM_FileReadDetached(bam, &rec);
        if ((int)GetRCObject(rc) == rcRow && (int)GetRCState(rc) == rcEmpty) {
            rc = CheckLimitAndLogError();
            continue;
        }
        if ((int)GetRCObject(rc) == rcRow && (int)GetRCState(rc) == rcNotFound) {
            /* EOF */
            rc = 0;
            --NR;
            break;
        }
        if (rc) break;
#if defined(NEW_QUEUE)
        queue_rec_t queue_rec;
#else
        queue_rec_t* queue_rec = new queue_rec_t;
#endif

        {
            static char const dummy[] = "";
            char const *spotGroup;
            char const *name;
            size_t namelen;

            BAM_AlignmentGetReadName2(rec, &name, &namelen);
            BAM_AlignmentGetReadGroupName(rec, &spotGroup);
#if defined(NEW_QUEUE)
            queue_rec.alignment = rec;
            queue_rec.metadata = nullptr;
            rc = GetKeyID(&GlobalContext, bam, queue_rec, spotGroup ? spotGroup : dummy, name, namelen);
#else
            queue_rec->alignment = rec;
            queue_rec->metadata = nullptr;
            rc = GetKeyID(&GlobalContext, bam, *queue_rec, spotGroup ? spotGroup : dummy, name, namelen);
#endif
            if (rc) break;
        }

        for ( ; ; ) {
#ifdef NEW_QUEUE
            if (rw_queue.try_enqueue(std::move(queue_rec))) {
                break;
            }
            if (rw_done)
                break;
            std::this_thread::yield();    
#else
            timeout_t tm;
            TimeoutInit(&tm, 1000);
            rc = KQueuePush(bamq, queue_rec, &tm);
            if (rc == 0 || (int)GetRCObject(rc) != rcTimeout)
                break;
#endif
        }
    }

#ifndef NEW_QUEUE
    KQueueSeal(bamq);
#else
    rw_done.store(true);
#endif
    if (rc) {
        (void)LOGERR(klogErr, rc, "bamread_thread done");
    }
    else {
        (void)PLOGMSG(klogInfo, (klogInfo, "bamread_thread done; read $(NR) records", "NR=%lu", NR));
    }
    return rc;
}

/* call on main thread only */
#ifdef NEW_QUEUE
static queue_rec_t const getNextRecord(BAM_File const *const bam, rc_t *const rc)
#else
static queue_rec_t* const getNextRecord(BAM_File const *const bam, rc_t *const rc)
#endif
{
#ifdef NEW_QUEUE
    queue_rec_t queue_rec = {nullptr, nullptr};
#else
    queue_rec_t* queue_rec = nullptr;
#endif

#ifndef NEW_QUEUE
    if (bamq == NULL) {
        *rc = KQueueMake(&bamq, 4096);
        if (*rc) return queue_rec;
        *rc = KThreadMake(&bamread_thread, run_bamread_thread, (void *)bam);
        if (*rc) {
            KQueueRelease(bamq);
            bamq = NULL;
            return queue_rec;
        }
    }
#endif
    static size_t dequeued = 0;
    while (*rc == 0 && (*rc = Quitting()) == 0) {
        //BAM_Alignment const *rec = NULL;
#ifdef NEW_QUEUE
        if (rw_queue.try_dequeue(queue_rec)) {
            ++dequeued;
            return queue_rec;
        }
        if (rw_done.load())
            break;
        std::this_thread::yield();
#else
        timeout_t tm;
        TimeoutInit(&tm, 10000);
        *rc = KQueuePop(bamq, (void **)&queue_rec, &tm);
        if (*rc == 0) {
            return queue_rec; // this is the normal return
        }

        if ((int)GetRCObject(*rc) == rcTimeout)
            *rc = 0;
        else {
            if ((int)GetRCObject(*rc) == rcData && (int)GetRCState(*rc) == rcDone)
                (void)LOGMSG(klogDebug, "KQueuePop Done");
            else
                (void)PLOGERR(klogWarn, (klogWarn, *rc, "KQueuePop Error", NULL));
        }
#endif
    }
    spdlog::info("Dequeued: {:L}", dequeued);
    rw_done = true;
    {
        rc_t rc2 = 0;
        KThreadWait(bamread_thread, &rc2);
        if (rc2 != 0)
            *rc = rc2; // return the rc from the reader thread
    }
    KThreadRelease(bamread_thread);
    bamread_thread = NULL;
#ifndef NEW_QUEUE
    KQueueRelease(bamq);
    bamq = NULL;
#endif
    return queue_rec;
}

static void getSpotGroup(BAM_Alignment const *const rec, char spotGroup[])
{
    char const *rgname;

    BAM_AlignmentGetReadGroupName(rec, &rgname);
    if (rgname)
        strcpy(spotGroup, rgname);
    else
        spotGroup[0] = '\0';
}

static char const *getLinkageGroup(BAM_Alignment const *const rec)
{
    static char linkageGroup[1024];
    char const *BX = NULL;
    char const *CB = NULL;
    char const *UB = NULL;

    linkageGroup[0] = '\0';
    BAM_AlignmentGetLinkageGroup(rec, &BX, &CB, &UB);
    if (BX == NULL) {
        if (CB != NULL && UB != NULL) {
            unsigned const cblen = strlen(CB);
            unsigned const ublen = strlen(UB);
            if (cblen + ublen + 8 < sizeof(linkageGroup)) {
                memmove(&linkageGroup[        0], "CB:", 3);
                memmove(&linkageGroup[        3], CB, cblen);
                memmove(&linkageGroup[cblen + 3], "|UB:", 4);
                memmove(&linkageGroup[cblen + 7], UB, ublen + 1);
            }
        }
    }
    else {
        unsigned const bxlen = strlen(BX);
        if (bxlen + 1 < sizeof(linkageGroup))
            memmove(linkageGroup, BX, bxlen + 1);
    }
    return linkageGroup;
}

static rc_t ProcessBAM(char const bamFile[], context_t *ctx, VDatabase *db,
                        /* data outputs */
                       Reference *ref, Sequence *seq, Alignment *align,
                       /* output parameters */
                       bool *had_alignments, bool *had_sequences)
{

    const BAM_File *bam;
    const BAM_Alignment *rec;
#if defined(NEW_QUEUE)
    queue_rec_t queue_rec;
#else
    queue_rec_t* queue_rec;
#endif
    KDataBuffer buf;
    KDataBuffer fragBuf;
    KDataBuffer cigBuf;
    rc_t rc;
    const BAMRefSeq *refSeq = NULL;
    int32_t lastRefSeqId = -1;
    bool wasRenamed = false;
    size_t rsize;
    uint64_t keyId = 0;
    uint64_t reccount = 0;
    char spotGroup[512];
    size_t namelen;
    float progress = 0.0;
    unsigned warned = 0;
    int skipRefSeqID = -1;
    int unmapRefSeqId = -1;
    uint64_t recordsRead = 0;
    uint64_t recordsProcessed = 0;
    uint64_t filterFlagConflictRecords=0; /*** counts number of conflicts between flags 0x400 and 0x200 ***/
#define MAX_WARNINGS_FLAG_CONFLICT 10000 /*** maximum errors to report ***/

    bool isColorSpace = false;
    bool isNotColorSpace = G.noColorSpace;
    char alignGroup[32];
    size_t alignGroupLen;
    AlignmentRecord data;
    KDataBuffer seqBuffer;
    KDataBuffer qualBuffer;
    SequenceRecord srec;
    SequenceRecordStorage srecStorage;

    /* setting up buffers */
    memset(&data, 0, sizeof(data));
    memset(&srec, 0, sizeof(srec));

    srec.ti             = srecStorage.ti;
    srec.readStart      = srecStorage.readStart;
    srec.readLen        = srecStorage.readLen;
    srec.orientation    = srecStorage.orientation;
    srec.is_bad         = srecStorage.is_bad;
    srec.alignmentCount = srecStorage.alignmentCount;
    srec.aligned        = srecStorage.aligned;
    srec.cskey          = srecStorage. cskey;

    rc = OpenBAM(&bam, db, bamFile);
    if (rc) return rc;
    if (!G.noVerifyReferences && ref != NULL) {
        rc = VerifyReferences(bam, ref);
        if (G.onlyVerifyReferences) {
            BAM_FileRelease(bam);
            return rc;
        }
    }
    BAM_FileGetPosition(bam, &ctx->m_fileOffset);
    ctx->m_fileOffset >>= 16;
    ctx->m_HeaderOffset = ctx->m_fileOffset;

    {
        uint32_t rgcount;

        BAM_FileGetReadGroupCount(bam, &rgcount);
        ctx->m_isSingleGroup = rgcount >= MAX_GROUPS_ALLOWED; // TODO
        if (ctx->m_isSingleGroup && ctx->m_emptyGroupIndex == (unsigned)-1) {
            ctx->add_read_group();
            ctx->m_emptyGroupIndex = ctx->m_read_groups.size() - 1;
        }

        for (unsigned rgi = 0; rgi != rgcount; ++rgi) {
            BAMReadGroup const *rg;
            BAM_FileGetReadGroup(bam, rgi, &rg);
            if (rg && rg->platformId == SRA_PLATFORM_CAPILLARY) {
                G.hasTI = true;
                break;
            }
        }
    }

    if (strcmp(bamFile, "/dev/stdin") == 0) {
        uint32_t n;
        BAM_FileGetRefSeqCount(bam, &n);
        ctx->m_ReferenceSize = 0;
        ctx->m_ReferenceCount = n;
        BAMRefSeq const *refSeq;
        for (unsigned i = 0; i != n; ++i) {
            BAM_FileGetRefSeq(bam, i, &refSeq);
            ctx->m_ReferenceSize += refSeq->length;
        }
    }

    /* setting up more buffers */
    rc = KDataBufferMake(&cigBuf, 32, 0);
    if (rc)
        return rc;

    rc = KDataBufferMake(&fragBuf, 8, 1024);
    if (rc)
        return rc;

    rc = KDataBufferMake(&buf, 16, 0);
    if (rc)
        return rc;

    rc = KDataBufferMake(&seqBuffer, 8, 4096);
    if (rc)
        return rc;

    rc = KDataBufferMake(&qualBuffer, 8, 4096);
    if (rc)
        return rc;

    if (rc == 0) {
        (void)PLOGMSG(klogInfo, (klogInfo, "Loading '$(file)'", "file=%s", bamFile));
    }
    spdlog::stopwatch sw;
    uint64_t primaryId[2];
    std::optional<uint8_t> opt_frag_len[2];
    std::optional<bool> opt_pcr_dup;
    std::optional<bool> opt_is_primary;
    std::optional<bool> opt_is_unmated;

#ifdef NEW_QUEUE
    //while (rw_queue.pop()); // clear queue
    rw_done = false;
    auto _rc = KThreadMake(&bamread_thread, run_bamread_thread, (void *)bam);
    if (_rc) {
        return 0;
    }
#endif
    size_t new_spots = 0;
    string prev_rec;

#if defined(NEW_QUEUE)
    while (true) {
        queue_rec = getNextRecord(bam, &rc);
        rec = queue_rec.alignment;
#else
    while ((queue_rec = getNextRecord(bam, &rc)) != NULL) {
        rec = queue_rec->alignment;
#endif
        if (rec == nullptr)
            break;

        bool aligned;
        uint32_t readlen;
        uint16_t flags;
        int64_t rpos=0;
        char *seqDNA;
#ifdef HAS_CTX_VALUE
        ctx_value_t *value;
#endif
        bool wasInserted;
        int32_t refSeqId=-1;
        uint8_t *qual;
        bool mated;
        const char *name;
        char cskey = 0;
        bool originally_aligned;
        bool isPrimary;
        uint32_t opCount;
        bool hasCG = false;
        uint64_t ti = 0;
        uint32_t csSeqLen = 0;
        int lpad = 0;
        int rpad = 0;
        bool hardclipped = false;
        bool revcmp = false;
        unsigned readNo = 0;
        bool wasPromoted = false;
        char const *barCode = NULL;
        char const *linkageGroup;

        keyId = rec->keyId;
        wasInserted = rec->wasInserted;
        if (wasInserted)
            ++new_spots;
#ifndef NO_METADATA
        //uint64_t row_id = keyId & 0xffffffffff; //(64 - MAX_GROUP_BITS) bits
#if defined(NEW_QUEUE)
        auto& metadata = *queue_rec.metadata;
        const uint64_t row_id = queue_rec.row_id;
        if (metadata.need_optimize) {
            spdlog::stopwatch sw;
            metadata.memory_used = metadata.Optimize();
            spdlog::info("Metadata memory {:L}", metadata.memory_used);
            spdlog::info("Metadata Optimize {:.3} sec", sw);
            metadata.need_optimize = false;
        }
#else
        auto& metadata = *queue_rec->metadata;
        const uint64_t row_id = queue_rec->row_id;
#endif
        primaryId[0] = 0;
        primaryId[1] = 0;
        opt_pcr_dup.reset();
        opt_is_primary.reset();
        opt_is_unmated.reset();
        opt_frag_len[0].reset();
        opt_frag_len[1].reset();
#endif
        ++ctx->readCount;
        if (ctx->readCount % 10000000 == 0) {

            {
                float const new_value = BAM_FileGetProportionalPosition(bam) * 100.0;
                float const delta = new_value - progress;
                if (delta > 1.0) {
                    KLoadProgressbar_Process(ctx->progress[0], delta, false);
                    progress = new_value;
                }
            }
            spdlog::info("Keys {:L}, time: {:.3} sec, memory: {:L}", recordsRead, sw, getCurrentRSS());
            sw.reset();
            /*
            for (auto& gr : ctx->m_read_groups) {
                int idx = 0;
                for (auto& b : gr->m_batches) {
                    if (b->m_data_ready && b->m_data_saved == false) {
                        string fname = fmt::format("{}.{}.batch", gr->m_group_id, idx);
                        bm::file_save_svector(*b->m_data, fname);
                        b->m_data_saved = true;
                    }
                    ++idx;
                }
            }
            */
        }
        BAM_AlignmentGetReadName2(rec, &name, &namelen);
#ifdef HAS_CTX_VALUE
        rc = MMArrayGet(ctx->id2value, (void **)&value, keyId);
        if (rc) {
            (void)PLOGERR(klogErr, (klogErr, rc, "MMArrayGet: failed on id '$(id)'", "id=%u", keyId));
            goto LOOP_END;
        }
#endif

        linkageGroup = getLinkageGroup(rec);

        if (!G.noColorSpace) {
            if (BAM_AlignmentHasColorSpace(rec)) {
                if (isNotColorSpace) {
MIXED_BASE_AND_COLOR:
                    rc = RC(rcApp, rcFile, rcReading, rcData, rcInconsistent);
                    (void)PLOGERR(klogErr, (klogErr, rc, "File '$(file)' contains base space and color space", "file=%s", bamFile));
                    goto LOOP_END;
                }
                /* COLORSPACE is disabled!
                 * ctx->isColorSpace = isColorSpace = true; */
            }
            else if (isColorSpace)
                goto MIXED_BASE_AND_COLOR;
            else
                isNotColorSpace = true;
        }
        BAM_AlignmentGetFlags(rec, &flags);

        originally_aligned = (flags & BAMFlags_SelfIsUnmapped) == 0;
        aligned = originally_aligned;

        mated = false;
        if (flags & BAMFlags_WasPaired) {
            if ((flags & BAMFlags_IsFirst) != 0)
                readNo |= 1;
            if ((flags & BAMFlags_IsSecond) != 0)
                readNo |= 2;
            switch (readNo) {
            case 1:
            case 2:
                mated = true;
                break;
            case 0:
                if ((warned & 1) == 0) {
                    (void)LOGMSG(klogWarn, "Spots without fragment info have been encountered");
                    warned |= 1;
                }
                UNFRAGMENT_MISSING_INFO;
                break;
            case 3:
                if ((warned & 2) == 0) {
                    (void)LOGMSG(klogWarn, "Spots with more than two fragments have been encountered");
                    warned |= 2;
                }
                UNFRAGMENT_TOO_MANY;
                break;
            }
        }
        if (!mated)
            readNo = 1;

        isPrimary = (flags & (BAMFlags_IsNotPrimary|BAMFlags_IsSupplemental)) == 0 ? true : false;
#ifndef NO_METADATA
        if (G.deferSecondary && !isPrimary && aligned) {
            if (wasInserted) {
                isPrimary = true;
                wasPromoted = true;
            } else {
                primaryId[readNo - 1] = metadata.get<u64_t>(metadata_t::E_PRIM_ID[readNo - 1]).get_no_check(row_id);
                if (primaryId[readNo - 1] == 0) {
                    /* promote to primary alignment */
                    isPrimary = true;
                    wasPromoted = true;
                }
            }
        }

#else
        if (G.deferSecondary && !isPrimary && aligned && CTX_VALUE_GET_P_ID(*value, readNo - 1) == 0) {
            /* promote to primary alignment */
            isPrimary = true;
            wasPromoted = true;
        }
#endif

        if (ctx->m_isSingleGroup)
            getSpotGroup(rec, spotGroup);

        if (wasInserted) {
            if (G.mode == mode_Remap) {
                (void)PLOGERR(klogErr, (klogErr, rc = RC(rcApp, rcFile, rcReading, rcData, rcInconsistent),
                                         "Spot '$(name)' is a new spot, not a remapping",
                                         "name=%s", name));
                goto LOOP_END;
            }
            /* first time spot is seen                    */
            /* need to make sure that every goto LOOP_END */
            /* above this point is with rc != 0           */
            /* else this structure won't get initialized  */
#ifndef NO_METADATA
            opt_is_unmated = !mated;
            if (!mated)
                metadata.get<bit_t>(metadata_t::e_unmated).set(row_id);
            if (isPrimary || G.assembleWithSecondary || G.deferSecondary) {
                opt_pcr_dup = flags & BAMFlags_IsDuplicate;
                if (flags & BAMFlags_IsDuplicate)
                    metadata.get<bit_t>(metadata_t::e_pcr_dup).set(row_id);
                if (ctx->m_isSingleGroup)
                    metadata.get<u16_t>(metadata_t::e_platform).set(row_id, GetINSDCPlatform(bam, spotGroup));
                metadata.get<bit_t>(metadata_t::e_primary_is_set).set(row_id);
                opt_is_primary = true;
            }
#endif
#ifdef HAS_CTX_VALUE
            memset(value, 0, sizeof(*value));
            value->unmated = !mated;
            if (isPrimary || G.assembleWithSecondary || G.deferSecondary) {
                value->pcr_dup = (flags & BAMFlags_IsDuplicate) == 0 ? 0 : 1;
                value->platform = rec->platform;//GetINSDCPlatform(bam, spotGroup);
                //assert(value->platform == rec.platform);
                value->primary_is_set = 1;
            }
#endif


        }
        if (!isPrimary && G.noSecondary)
            goto LOOP_END;

        rc = BAM_AlignmentCGReadLength(rec, &readlen);
        if (rc != 0 && GetRCState(rc) != rcNotFound) {
            // FATAL ERROR, DATA ERROR, NOT FIXABLE
            (void)LOGERR(klogErr, rc, "Invalid CG data");
            goto LOOP_END;
        }
        if (rc == 0) {
            hasCG = true;
            BAM_AlignmentGetCigarCount(rec, &opCount);
            rc = KDataBufferResize(&cigBuf, opCount * 2 + 5);
            if (rc) {
                // FATAL ERROR, OUT OF MEMORY
                (void)LOGERR(klogErr, rc, "Failed to resize CIGAR buffer");
                goto LOOP_END;
            }
            rc = AlignmentRecordInit(&data, readlen);
            if (rc == 0)
                rc = KDataBufferResize(&buf, readlen);
            if (rc) {
                // FATAL ERROR, OUT OF MEMORY
                (void)LOGERR(klogErr, rc, "Failed to resize record buffer");
                goto LOOP_END;
            }

            seqDNA = (char*)buf.base;
            qual = (uint8_t *)&seqDNA[readlen];

            rc = BAM_AlignmentGetCGSeqQual(rec, seqDNA, qual);
            if (rc == 0) {
                rc = BAM_AlignmentGetCGCigar(rec, (uint32_t*)cigBuf.base, cigBuf.elem_count, &opCount);
            }
            if (rc) {
                // FATAL ERROR, DATA ERROR, NOT FIXABLE
                (void)LOGERR(klogErr, rc, "Failed to read CG data");
                goto LOOP_END;
            }
            data.data.align_group.elements = 0;
            data.data.align_group.buffer = alignGroup;
            if (BAM_AlignmentGetCGAlignGroup(rec, alignGroup, sizeof(alignGroup), &alignGroupLen) == 0)
                data.data.align_group.elements = alignGroupLen;
        }
        else {
            /* normal flow i.e. NOT CG */
            uint32_t const *tmp;

            /* resize buffers */
            BAM_AlignmentGetReadLength(rec, &readlen);
            BAM_AlignmentGetRawCigar(rec, &tmp, &opCount);
            rc = KDataBufferResize(&cigBuf, opCount);
            assert(rc == 0);
            if (rc) {
                // FATAL ERROR, OUT OF MEMORY
                (void)LOGERR(klogErr, rc, "Failed to resize CIGAR buffer");
                goto LOOP_END;
            }
            memmove(cigBuf.base, tmp, opCount * sizeof(uint32_t));

            hardclipped = isHardClipped(opCount, (const uint32_t*)cigBuf.base);
            if (hardclipped) {
                if (isPrimary && !wasPromoted) {
                    /* when we promote a secondary to primary and it is hardclipped, we want to "fix" it */
                    if (!G.acceptHardClip) {
                        // FATAL ERROR, DATA ERROR, CAN BE FORCED WITH COMMAND LINE OPTION
                        rc = RC(rcApp, rcFile, rcReading, rcConstraint, rcViolated);
                        (void)PLOGERR(klogErr, (klogErr, rc, "File '$(file)' contains hard clipped primary alignments", "file=%s", bamFile));
                        goto LOOP_END;
                    }
                }
                else if (!G.acceptHardClip) { /* convert to soft clip */
                    uint32_t *const cigar = (uint32_t*)cigBuf.base;
                    uint32_t const lOp = cigar[0];
                    uint32_t const rOp = cigar[opCount - 1];

                    lpad = (lOp & 0xF) == 5 ? (lOp >> 4) : 0;
                    rpad = (rOp & 0xF) == 5 ? (rOp >> 4) : 0;

                    if (lpad + rpad == 0) {
                        // FATAL ERROR, DATA ERROR
                        rc = RC(rcApp, rcFile, rcReading, rcData, rcInvalid);
                        (void)PLOGERR(klogErr, (klogErr, rc, "File '$(file)' contains invalid CIGAR", "file=%s", bamFile));
                        goto LOOP_END;
                    }
                    if (lpad != 0) {
                        uint32_t const new_lOp = (((uint32_t)lpad) << 4) | 4;
                        cigar[0] = new_lOp;
                    }
                    if (rpad != 0) {
                        uint32_t const new_rOp = (((uint32_t)rpad) << 4) | 4;
                        cigar[opCount - 1] = new_rOp;
                    }
                }
            }

            if (G.deferSecondary && !isPrimary) {
                /*** try to see if hard-clipped secondary alignment can be salvaged **/
                auto l = readlen + lpad + rpad;
                if (l < 256) {

#ifndef NO_METADATA
                    uint8_t frag_len = 0;
                    if (wasInserted == false) {
                        frag_len = metadata.get<u16_t>(metadata_t::E_FRAG_LEN[readNo - 1]).get_no_check(row_id);
                        opt_frag_len[readNo - 1] = frag_len;
                    }

#if defined HAS_CTX_VALUE
                    if (frag_len != value->fragment_len[readNo - 1]) {
                        spdlog::error("Inconsistent fragment_len");
                        throw runtime_error("Inconsistent fragment_len");
                    }
#endif
#else
                    auto frag_len = value->fragment_len[readNo -1];
#endif
                    if ( l < frag_len) {
                        rc = KDataBufferResize(&cigBuf, opCount + 1);
                        assert(rc == 0);
                        if (rc) {
                            // FATAL ERROR, OUT OF MEMORY
                            (void)LOGERR(klogErr, rc, "Failed to resize CIGAR buffer");
                            goto LOOP_END;
                        }
                        if (rpad > 0 && lpad == 0) {
                            uint32_t *const cigar = (uint32_t*)cigBuf.base;
                            lpad = frag_len - readlen - rpad;
                            memmove(cigar + 1, cigar, opCount * sizeof(*cigar));
                            cigar[0] = (uint32_t)((lpad << 4) | 4);
                        }
                        else {
                            uint32_t *const cigar = (uint32_t*)cigBuf.base;
                            rpad += frag_len - readlen - lpad;
                            cigar[opCount] = (uint32_t)((rpad << 4) | 4);
                        }
                        opCount++;
                    }
                }
            }
            rc = AlignmentRecordInit(&data, readlen + lpad + rpad);
            assert(rc == 0);
            if (rc == 0)
                rc = KDataBufferResize(&buf, readlen + lpad + rpad);
            assert(rc == 0);
            if (rc) {
                // FATAL ERROR, OUT OF MEMORY
                (void)LOGERR(klogErr, rc, "Failed to resize record buffer");
                goto LOOP_END;
            }

            seqDNA = (char*)buf.base;
            qual = (uint8_t *)&seqDNA[(readlen | csSeqLen) + lpad + rpad];
            memset(seqDNA, 'N', (readlen | csSeqLen) + lpad + rpad);
            memset(qual, 0, (readlen | csSeqLen) + lpad + rpad);

            BAM_AlignmentGetSequence(rec, seqDNA + lpad);
            if (G.useQUAL) {
                uint8_t const *squal;

                BAM_AlignmentGetQuality(rec, &squal);
                memmove(qual + lpad, squal, readlen);
            }
            else {
                uint8_t const *squal;
                uint8_t qoffset = 0;
                unsigned i;

                rc = BAM_AlignmentGetQuality2(rec, &squal, &qoffset);
                if (rc) {
                    // FATAL ERROR; DATA INCONSISTENT
                    (void)PLOGERR(klogErr, (klogErr, rc, "Spot '$(name)': length of original quality does not match sequence", "name=%s", name));
                    goto LOOP_END;
                }
                if (qoffset) {
                    for (i = 0; i != readlen; ++i)
                        qual[i + lpad] = squal[i] - qoffset;
                    QUAL_CHANGED_OQ;
                }
                else
                    memmove(qual + lpad, squal, readlen);
            }
            readlen = readlen + lpad + rpad;
            data.data.align_group.elements = 0;
            data.data.align_group.buffer = alignGroup;
        }
        if (G.hasTI) {
            rc = BAM_AlignmentGetTI(rec, &ti);
            if (rc)
                ti = 0;
            rc = 0;
        }

        rc = KDataBufferResize(&seqBuffer, readlen);
        if (rc) {
            // FATAL ERROR, OUT OF MEMORY
            (void)LOGERR(klogErr, rc, "Failed to resize record buffer");
            goto LOOP_END;
        }
        rc = KDataBufferResize(&qualBuffer, readlen);
        if (rc) {
            // FATAL ERROR, OUT OF MEMORY
            (void)LOGERR(klogErr, rc, "Failed to resize record buffer");
            goto LOOP_END;
        }
        AR_REF_ORIENT(data) = (flags & BAMFlags_SelfIsReverse) == 0 ? false : true;

        rpos = -1;
        if (aligned) {
            BAM_AlignmentGetPosition(rec, &rpos);
            BAM_AlignmentGetRefSeqId(rec, &refSeqId);
            if (refSeqId != lastRefSeqId) {
                refSeq = NULL;
                BAM_FileGetRefSeqById(bam, refSeqId, &refSeq);
            }
        }

        revcmp = (isColorSpace && !aligned) ? false : AR_REF_ORIENT(data);
        (void)PLOGMSG(klogDebug, (klogDebug, "Read '$(name)' is $(or) at $(ref):$(pos)", "name=%s,or=%s,ref=%s,pos=%i", name, revcmp ? "reverse" : "forward", refSeq ? refSeq->name : "(none)", rpos));
        COPY_READ((INSDC_dna_text*)seqBuffer.base, seqDNA, readlen, revcmp);
        COPY_QUAL((uint8_t*)qualBuffer.base, qual, readlen, revcmp);

        AR_MAPQ(data) = GetMapQ(rec);
        if (!isPrimary && AR_MAPQ(data) < G.minMapQual)
            goto LOOP_END;

        if (aligned && align == NULL) {
            // FATAL ERROR, COMMAND AND DATA ARE INCONSISTENT
            rc = RC(rcApp, rcFile, rcReading, rcData, rcInconsistent);
            (void)PLOGERR(klogErr, (klogErr, rc, "File '$(file)' contains aligned records", "file=%s", bamFile));
            goto LOOP_END;
        }
        while (aligned) {
            if (rpos >= 0 && refSeqId >= 0) {
                if (refSeqId == skipRefSeqID) {
                    DISCARD_SKIP_REFERENCE;
                    goto LOOP_END;
                }
                if (refSeqId == unmapRefSeqId) {
                    aligned = false;
                    UNALIGNED_UNALIGNED_REF;
                    break;
                }
                unmapRefSeqId = -1;
                if (refSeq == NULL) {
                    // NOT FATAL ERROR, DATA ERROR, LIKELY IMPOSSIBLE 
                    rc = SILENT_RC(rcApp, rcFile, rcReading, rcData, rcInconsistent);
                    (void)PLOGERR(klogWarn, (klogWarn, rc, "File '$(file)': Spot '$(name)' refers to an unknown Reference number $(refSeqId)", "file=%s,refSeqId=%i,name=%s", bamFile, (int)refSeqId, name));
                    rc = CheckLimitAndLogError();
                    DISCARD_UNKNOWN_REFERENCE;
                    goto LOOP_END;
                }
                else {
                    bool shouldUnmap = false;

                    if (G.refFilter && strcmp(G.refFilter, refSeq->name) != 0) {
                        (void)PLOGMSG(klogInfo, (klogInfo, "Skipping Reference '$(name)'", "name=%s", refSeq->name));
                        skipRefSeqID = refSeqId;
                        DISCARD_SKIP_REFERENCE;
                        goto LOOP_END;
                    }
/*
                    if (ctx->references.count(refSeq->name) == 0) {
                        //spdlog::info("Reference: '{}', length: {}", refSeq->name, refSeq->length);
                        ctx->references.insert(string(refSeq->name));
                        ctx->m_reference_len += refSeq->length;
                    }
 */                 
                    bool is_new = false;
                    rc = ReferenceSetFile(ref, refSeq->name, refSeq->length, refSeq->checksum, &shouldUnmap, &wasRenamed, &is_new);
                    if (rc == 0) {
                        if (is_new)
                            ctx->m_reference_len += refSeq->length;
                        lastRefSeqId = refSeqId;
                        if (shouldUnmap) {
                            aligned = false;
                            unmapRefSeqId = refSeqId;
                            UNALIGNED_UNALIGNED_REF;
                        }
                        break;
                    }
                    if (GetRCObject(rc) == rcConstraint && GetRCState(rc) == rcViolated) {
                        int const level = G.limit2config ? klogWarn : klogErr;

                        // NOT FATAL BY DEFAULT, CAN BE FATAL, DATA ERROR, CONFIGURATION ERROR
                        (void)PLOGMSG(level, (level, "Could not find a Reference to match { name: '$(name)', length: $(rlen) }", "name=%s,rlen=%u", refSeq->name, (unsigned)refSeq->length));
                    }
                    else if (!G.limit2config) {
                        // NOT FATAL BY DEFAULT, CAN BE FATAL, DATA ERROR, CONFIGURATION ERROR
                        (void)PLOGERR(klogErr, (klogErr, rc, "File '$(file)': Spot '$(sname)' refers to an unknown Reference '$(rname)'", "file=%s,rname=%s,sname=%s", bamFile, refSeq->name, name));
                    }
                    if (G.limit2config) {
                        rc = 0;
                        UNALIGNED_UNKNOWN_REF;
                    }
                    goto LOOP_END;
                }
            }
            else if (refSeqId < 0) {
                // FATAL IF TOO MANY, DATA ERROR, INCONSISTENT DATA
                (void)PLOGMSG(klogWarn, (klogWarn, "Spot '$(name)' was marked aligned, but reference id = $(id) is invalid", "name=%.*s,id=%i", namelen, name, refSeqId));
                if ((rc = CheckLimitAndLogError()) != 0) goto LOOP_END;
                UNALIGNED_INVALID_REF;
            }
            else {
                // FATAL IF TOO MANY, DATA ERROR, POSSIBLE CONFIGURATION ERROR
                (void)PLOGMSG(klogWarn, (klogWarn, "Spot '$(name)' was marked aligned, but reference position = $(pos) is invalid", "name=%.*s,pos=%i", namelen, name, rpos));
                if ((rc = CheckLimitAndLogError()) != 0) goto LOOP_END;
                UNALIGNED_INVALID_REF_POS;
            }

            aligned = false;
        }
        if (!aligned && (G.refFilter != NULL || G.limit2config)) {
            assert(!"this shouldn't happen");
            goto LOOP_END;
        }

        AR_KEY(data) = keyId;
        AR_READNO(data) = readNo;

        if (wasInserted) {
        }
        else if (isPrimary || G.assembleWithSecondary || G.deferSecondary) {
            /* other times */
            int o_pcr_dup = 0;
            int const n_pcr_dup = (flags & BAMFlags_IsDuplicate) == 0 ? 0 : 1;

#ifndef NO_METADATA
            if (!opt_is_primary.has_value())
                opt_is_primary = metadata.get<bit_t>(metadata_t::e_primary_is_set).test(row_id);
            if (!opt_is_primary.value()) {
                metadata.get<bit_t>(metadata_t::e_primary_is_set).set(row_id);
                o_pcr_dup = n_pcr_dup;
                opt_is_primary = true;
            } else {
                if (!opt_pcr_dup.has_value())
                    opt_pcr_dup = metadata.get<bit_t>(metadata_t::e_pcr_dup).test(row_id);
                o_pcr_dup = opt_pcr_dup.value() ? 1 : 0;
            }
            if (!opt_pcr_dup.has_value() || opt_pcr_dup.value() != (o_pcr_dup & n_pcr_dup)) {
                metadata.get<bit_t>(metadata_t::e_pcr_dup).set(row_id, o_pcr_dup & n_pcr_dup);
                opt_pcr_dup = o_pcr_dup & n_pcr_dup;
            }

#endif
#if defined(HAS_CTX_VALUE)
            if (!value->primary_is_set) {
                o_pcr_dup = n_pcr_dup;
                value->primary_is_set = 1;
            } else {
                o_pcr_dup = value->pcr_dup;
            }
            value->pcr_dup = o_pcr_dup & n_pcr_dup;
#endif
            if (o_pcr_dup != (o_pcr_dup & n_pcr_dup)) {
                FLAG_CHANGED_PCR_DUP;
            }
#ifndef NO_METADATA
            auto v_unmated = opt_is_unmated.value_or(metadata.get<bit_t>(metadata_t::e_unmated).test(row_id));
#ifdef HAS_CTX_VALUE
            if (v_unmated != value->unmated) {
                spdlog::error("Inconsistent unmated");
                throw runtime_error("Inconsistent unmated");
            }

#endif
#else
            auto v_unmated = value->unmated;
#endif
            if (mated && v_unmated) {
                // FATAL IF TOO MANY, DATA ERROR, INCONSISTENT DATA
                (void)PLOGERR(klogWarn, (klogWarn, SILENT_RC(rcApp, rcFile, rcReading, rcData, rcInconsistent),
                                         "Spot '$(name)', which was first seen without mate info, now has mate info",
                                         "name=%s", name));
                rc = CheckLimitAndLogError();
                DISCARD_BAD_FRAGMENT_INFO;
                goto LOOP_END;
            }
            else if (!mated && !v_unmated) {
                // FATAL IF TOO MANY, DATA ERROR, INCONSISTENT DATA
                (void)PLOGERR(klogWarn, (klogWarn, SILENT_RC(rcApp, rcFile, rcReading, rcData, rcInconsistent),
                                         "Spot '$(name)', which was first seen with mate info, now has no mate info",
                                         "name=%s", name));
                rc = CheckLimitAndLogError();
                DISCARD_BAD_FRAGMENT_INFO;
                goto LOOP_END;
            }
        }
        if (isPrimary) {
            switch (readNo) {
            case 1: {
#if !defined NO_METADATA
                if (!primaryId[0])
                    primaryId[0] = wasInserted ? 0 : metadata.get<u64_t>(metadata_t::E_PRIM_ID[0]).get_no_check(row_id);
                auto v = primaryId[0];
#ifdef HAS_CTX_VALUE
                if (v != CTX_VALUE_GET_P_ID(*value, 0)) {
                    spdlog::error("Inconsistent primaryId");
                    throw runtime_error("Inconsistent primaryId");
                }
#endif
#else
                auto v = CTX_VALUE_GET_P_ID(*value, 0);
#endif
                if (v != 0) {
                    isPrimary = false;
                    FLAG_CHANGED_PRIMARY_DUP;
                }
                else if (aligned) {
#if !defined NO_METADATA
                    auto v_unaligned_1 = wasInserted ? 0 : metadata.get<bit_t>(metadata_t::e_unaligned_1).test(row_id);
#ifdef HAS_CTX_VALUE
                    if (v_unaligned_1 != value->unaligned_1) {
                        spdlog::error("Inconsistent v_unaligned_1");
                        throw runtime_error("Inconsistent v_unaligned_1");
                    }
#endif
#else
                    auto v_unaligned_1 = value->unaligned_1;
#endif
                    if (v_unaligned_1) {
                        // NOT FATAL, DATA ERROR, INCONSISTENT DATA
                        (void)PLOGMSG(klogWarn, (klogWarn, "Read 1 of spot '$(name)', which was unmapped, is now being mapped at position $(pos) on reference '$(ref)'; this alignment will be considered as secondary", "name=%s,ref=%s,pos=%u", name, refSeq->name, rpos));
                        isPrimary = false;
                        FLAG_CHANGED_WAS_UNALIGNED;
                    }
                }
                break;
            }
            case 2:
            {
#ifndef NO_METADATA
                if (!primaryId[1])
                    primaryId[1] = wasInserted ? 0 : metadata.get<u64_t>(metadata_t::E_PRIM_ID[1]).get_no_check(row_id);
                auto v = primaryId[1];
#ifdef HAS_CTX_VALUE
                if (v != CTX_VALUE_GET_P_ID(*value, 1)) {
                    spdlog::error("Inconsistent primaryId[1]");
                    throw runtime_error("Inconsistent primaryId[1]");
                }

#endif
#else
                auto v = CTX_VALUE_GET_P_ID(*value, 1);
#endif
                if (v != 0) {
                    isPrimary = false;
                    FLAG_CHANGED_PRIMARY_DUP;
                }
                else if (aligned) {
#ifndef NO_METADATA
                    auto v_unaligned_2 = wasInserted ? 0 : metadata.get<bit_t>(metadata_t::e_unaligned_2).test(row_id);
#ifdef HAS_CTX_VALUE
                    if (v_unaligned_2 != value->unaligned_2) {
                        spdlog::error("Inconsistent v_unaligned_2");
                        throw runtime_error("Inconsistent v_unaligned_2");
                    }

#endif
#else
                    auto v_unaligned_2 = value->unaligned_2;
#endif
                    if (v_unaligned_2) {
                        // NOT FATAL, DATA ERROR, INCONSISTENT DATA
                        (void)PLOGMSG(klogWarn, (klogWarn, "Read 2 of spot '$(name)', which was unmapped, is now being mapped at position $(pos) on reference '$(ref)'; this alignment will be considered as secondary", "name=%s,ref=%s,pos=%u", name, refSeq->name, rpos));
                        isPrimary = false;
                        FLAG_CHANGED_WAS_UNALIGNED;
                    }
                }
                break;
            }
            default:
                break;
            }
        }
        if (hardclipped) {
#ifndef NO_METADATA
            metadata.get<bit_t>(metadata_t::e_hardclipped).set(row_id);
#endif
#if defined(HAS_CTX_VALUE)
            value->hardclipped = 1;
#endif
        }
#if 0 /** EY TO REVIEW **/
        if (!isPrimary && value->hardclipped) {
            DISCARD_HARDCLIP_SECONDARY;
            goto LOOP_END;
        }
#endif

        /* input is clean */
        ++recordsProcessed;

        data.isPrimary = isPrimary;
        if (aligned) {
            uint32_t matches = 0;
            uint32_t misses = 0;
            uint8_t rna_orient = ' ';

            FixOverhangingAlignment(&cigBuf, &opCount, rpos, refSeq->length, readlen);
            BAM_AlignmentGetRNAStrand(rec, &rna_orient);
            {
                int const intronType = rna_orient == '+' ? NCBI_align_ro_intron_plus :
                                       rna_orient == '-' ? NCBI_align_ro_intron_minus :
                                                   hasCG ? NCBI_align_ro_complete_genomics :
                                                           NCBI_align_ro_intron_unknown;
                rc = ReferenceRead(ref, &data, rpos, (const uint32_t*)cigBuf.base, opCount, seqDNA, readlen, intronType, &matches, &misses);
            }
            if (rc == 0) {
                int const i = readNo - 1;
                int const clipped_rl = readlen < 255 ? readlen : 255;
                if (i >= 0 && i < 2) {
#if !defined NO_METADATA
                    int const rl = wasInserted ? 0 : opt_frag_len[i].value_or(metadata.get<u16_t>(metadata_t::E_FRAG_LEN[i]).get_no_check(row_id));
#ifdef HAS_CTX_VALUE
                    if (rl != value->fragment_len[i]) {
                        spdlog::error("Inconsistent fragment_len");
                        throw runtime_error("Inconsistent fragment_len");
                    }
#endif
#else
                    int const rl = value->fragment_len[i];
#endif

                    if (rl == 0) {
#if !defined NO_METADATA
                        metadata.get<u16_t>(metadata_t::E_FRAG_LEN[i]).set(row_id, clipped_rl);
#endif
#ifdef HAS_CTX_VALUE
                        value->fragment_len[i] = clipped_rl;
#endif
                    }
                    else if (rl != clipped_rl) {
                        if (isPrimary) {
                            // FATAL ERROR, DATA ERROR, INCONSISTENT DATA
                            rc = RC(rcApp, rcFile, rcReading, rcConstraint, rcViolated);
                            (void)PLOGERR(klogErr, (klogErr, rc, "Primary alignment for '$(name)' has different length ($(len)) than previously recorded non-primary alignment. Try to defer non-primary alignment processing.",
                                                    "name=%s,len=%d", name, readlen));
                        }
                        else {
                            // FATAL IF TOO MANY, DATA ERROR, INCONSISTENT DATA
                            rc = SILENT_RC(rcApp, rcFile, rcReading, rcConstraint, rcViolated);
                            (void)PLOGERR(klogWarn, (klogWarn, rc, "Non-primary alignment for '$(name)' has different length ($(len)) than previously recorded primary alignment; discarding non-primary alignment.",
                                                     "name=%s,len=%d", name, readlen));
                            DISCARD_BAD_SECONDARY;
                            rc = CheckLimitAndLogError();
                        }
                        goto LOOP_END;
                    }
                }
            }
            if (rc == 0 && (matches < G.minMatchCount || (matches == 0 && !G.acceptNoMatch))) {
                if (isPrimary) {
                    if (misses > matches) {
                        RecordNoMatch(name, refSeq->name, rpos);
                        rc = LogNoMatch(name, refSeq->name, (unsigned)rpos, (unsigned)matches);
                        if (rc)
                            goto LOOP_END;
                    }
                }
                else {
                    // WARNING
                    (void)PLOGMSG(klogWarn, (klogWarn, "Spot '$(name)' contains too few ($(count)) matching bases to reference '$(ref)' at $(pos); discarding non-primary alignment",
                                             "name=%s,ref=%s,pos=%u,count=%u", name, refSeq->name, (unsigned)rpos, (unsigned)matches));
                    DISCARD_BAD_SECONDARY;
                    rc = 0;
                    goto LOOP_END;
                }
            }
            if (rc) {
                aligned = false;

                if (((int)GetRCObject(rc)) == ((int)rcData) && GetRCState(rc) == rcNotAvailable) {
                    /* because of code above converting hard clips to soft clips, this should be unreachable */
                    abort();
                }
                else if (((int)GetRCObject(rc)) == ((int)rcData)) {
                    // FATAL IF TOO MANY, DATA ERROR
                    UNALIGNED_INVALID_INFO;
                    (void)PLOGERR(klogWarn, (klogWarn, rc, "Spot '$(name)': bad alignment to reference '$(ref)' at $(pos)", "name=%s,ref=%s,pos=%u", name, refSeq->name, rpos));
                    /* Data errors may get reset; alignment will be unmapped at any rate */
                    rc = CheckLimitAndLogError();
                }
                else {
                    // FATAL IF TOO MANY, DATA ERROR
                    UNALIGNED_INVALID_REF_POS;
                    (void)PLOGERR(klogWarn, (klogWarn, rc, "Spot '$(name)': error reading reference '$(ref)' at $(pos)", "name=%s,ref=%s,pos=%u", name, refSeq->name, rpos));
                    rc = CheckLimitAndLogError();
                }
                if (rc) goto LOOP_END;
            }
        }

        if (!aligned && isPrimary) {
            switch (readNo) {
            case 1:
#if !defined NO_METADATA
                metadata.get<bit_t>(metadata_t::e_unaligned_1).set(row_id);
#endif
#ifdef HAS_CTX_VALUE
                value->unaligned_1 = 1;
#endif
                break;
            case 2:
#if !defined NO_METADATA
                metadata.get<bit_t>(metadata_t::e_unaligned_2).set(row_id);
#endif
#ifdef HAS_CTX_VALUE
                value->unaligned_2 = 1;
#endif
                break;
            default:
                break;
            }
        }
        if (isPrimary && aligned) {
#if !defined NO_METADATA
            if (!primaryId[readNo-1])
                primaryId[readNo-1] = wasInserted ? 0 : metadata.get<u64_t>(metadata_t::E_PRIM_ID[readNo-1]).get_no_check(row_id);
            auto v = primaryId[readNo-1];
#ifdef HAS_CTX_VALUE
            if (v != CTX_VALUE_GET_P_ID(*value, readNo - 1)) {
                spdlog::error("Inconsistent CTX_VALUE_GET_P_ID");
                throw runtime_error("Inconsistent CTX_VALUE_GET_P_ID");
            }
#endif
#else
            auto v = CTX_VALUE_GET_P_ID(*value, readNo - 1);
#endif
            switch (readNo) {
            case 1:
                if (v == 0) {
                    data.alignId = ++ctx->primaryId;
#ifndef NO_METADATA
                    metadata.get<u64_t>(metadata_t::E_PRIM_ID[0]).set(row_id, data.alignId);
#endif
#if defined(HAS_CTX_VALUE)
                    CTX_VALUE_SET_P_ID(*value, 0, data.alignId);
#endif
                }
                break;
            case 2:
                if (v == 0) {
                    data.alignId = ++ctx->primaryId;
#ifndef NO_METADATA
                    metadata.get<u64_t>(metadata_t::E_PRIM_ID[1]).set(row_id, data.alignId);
#endif
#if defined(HAS_CTX_VALUE)
                    CTX_VALUE_SET_P_ID(*value, 1, data.alignId);
#endif
                }
                break;
            default:
                break;
            }
        }
        if (G.mode == mode_Archive)
            goto WRITE_SEQUENCE;
        else
            goto WRITE_ALIGNMENT;
        if (0) {
WRITE_SEQUENCE:
#ifndef NO_METADATA
//            int64_t const spotId = metadata.Uint64(e_spotId).get_no_check(row_id);
            int64_t const spotId = wasInserted ? 0 : metadata.get<u64_t>(metadata_t::e_spotId).get_no_check(row_id);
#ifdef HAS_CTX_VALUE
            if (spotId != CTX_VALUE_GET_S_ID(*value)) {
                spdlog::error("Inconsistent spotId");
                throw runtime_error("Inconsistent spotId");
            }
#endif
#else
            int64_t const spotId = CTX_VALUE_GET_S_ID(*value);
#endif
            if (mated) {
                bool const spotHasBeenWritten = (spotId != 0);
                if (spotHasBeenWritten == false) {

#ifndef NO_METADATA
                    uint32_t fragmentId = wasInserted ? 0 : metadata.get<u32_t>(metadata_t::e_fragmentId).get_no_check(row_id);
#ifdef HAS_CTX_VALUE
                    if (value->fragmentId != fragmentId) {
                        spdlog::error("Inconsistent fragmentId");
                        throw runtime_error("Inconsistent fragmentId");
                    }
#endif
#else
                    uint32_t fragmentId = value->fragmentId;
#endif
                    bool const spotHasFragmentInfo = (fragmentId != 0);
                    bool const spotIsFirstSeen = spotHasFragmentInfo ? false : true;


                    if (spotIsFirstSeen) {

                        if (!isPrimary) {
                            if ( (!G.assembleWithSecondary || hardclipped) && !G.deferSecondary ) {
                                goto WRITE_ALIGNMENT;
                            }
                            (void)PLOGMSG(klogDebug, (klogDebug, "Spot '$(name)' (id $(id)) is being constructed from non-primary alignment information", "id=%lx,name=%s", keyId, name));
                        }
                        /* start spot assembly */
                        unsigned sz;
                        FragmentInfo fi;
                        int32_t mate_refSeqId = -1;
                        int64_t pnext = 0;

                        if (ctx->m_isSingleGroup == false) // otherwise spotGroup was captured
                            getSpotGroup(rec, spotGroup);
                        BAM_AlignmentGetBarCode(rec, &barCode);
                        if (barCode) {
                            if (spotGroup[0] != '\0' && rec->platform == SRA_PLATFORM_UNDEFINED) {
                                /* don't use bar code */
                            }
                            else {
                                unsigned const sglen = strlen(barCode);
                                if (sglen + 1 < sizeof(spotGroup))
                                    memmove(spotGroup, barCode, sglen + 1);
                            }
                        }

                        memset(&fi, 0, sizeof(fi));

                        fi.aligned = isPrimary ? aligned : 0;
                        fi.ti = ti;
                        fi.orientation = AR_REF_ORIENT(data);
                        fi.readNo = readNo;
                        fi.sglen = strlen(spotGroup);
                        fi.lglen = strlen(linkageGroup);

                        fi.readlen = readlen;
                        fi.cskey = cskey;
                        fi.is_bad = (flags & BAMFlags_IsLowQuality) != 0;
                        sz = sizeof(fi) + 2*fi.readlen + fi.sglen + fi.lglen;
                        if (align) {
                            BAM_AlignmentGetMateRefSeqId(rec, &mate_refSeqId);
                            BAM_AlignmentGetMatePosition(rec, &pnext);
                        }
                        if(align && mate_refSeqId == refSeqId && pnext > 0 && pnext!=rpos /*** weird case in some bams**/){
                            rc = MemBankAlloc(ctx->frags, &fragmentId, sz, 0, false);
                        } else {
                            rc = MemBankAlloc(ctx->frags, &fragmentId, sz, 0, true);
                        }
#ifndef NO_METADATA
                        metadata.get<u32_t>(metadata_t::e_fragmentId).set(row_id, fragmentId);
#endif
#if defined (HAS_CTX_VALUE)
                        value->fragmentId = fragmentId;
#endif

                        if (rc) {
                            // FATAL ERROR, OUT OF MEMORY
                            (void)LOGERR(klogErr, rc, "KMemBankAlloc failed");
                            goto LOOP_END;
                        }
                        /*printf("IN:%10d\tcnt2=%ld\tcnt1=%ld\n",value->fragmentId,fcountBoth,fcountOne);*/

                        rc = KDataBufferResize(&fragBuf, sz);
                        if (rc) {
                            // FATAL ERROR, OUT OF MEMORY
                            (void)LOGERR(klogErr, rc, "Failed to resize fragment buffer");
                            goto LOOP_END;
                        }
                        {{
                            uint8_t *dst = (uint8_t*) fragBuf.base;

                            memmove(dst,&fi,sizeof(fi));
                            dst += sizeof(fi);
                            memmove(dst, seqBuffer.base, readlen);
                            dst += readlen;
                            memmove(dst, qualBuffer.base, readlen);
                            dst += fi.readlen;
                            memmove(dst, spotGroup, fi.sglen);
                            dst += fi.sglen;
                            memmove(dst, linkageGroup, fi.lglen);
                            dst += fi.lglen;
                        }}
                        rc = MemBankWrite(ctx->frags, fragmentId, 0, fragBuf.base, sz, &rsize);
                        if (rc) {
                            // FATAL ERROR, RUNTIME ERROR, LIKELY IMPOSSIBLE
                            (void)PLOGERR(klogErr, (klogErr, rc, "KMemBankWrite failed writing fragment $(id)", "id=%u", fragmentId));
                            goto LOOP_END;
                        }
                        if (revcmp) {
                            QUAL_CHANGED_REVERSED;
                            SEQ__CHANGED_REV_COMP;
                        }
                        ++ctx->m_BankedSpots;
                        ctx->m_BankedSize += sz;
                        ctx->m_SpotSize += 2*fi.readlen + fi.sglen + fi.lglen;
                        if (ctx->m_BankedSpots > 10)
                            ctx->has_far_reads = true;
                    }
                    else if (spotHasFragmentInfo) {
                        /* continue spot assembly */
                        FragmentInfo *fip;
                        size_t banked_size = 0;
                        {
                            size_t size2;

                            rc = MemBankSize(ctx->frags, fragmentId, &banked_size);
                            if (rc) {
                                // FATAL ERROR, INTERNAL CONSISTENCY ERROR
                                (void)PLOGERR(klogErr, (klogErr, rc, "KMemBankSize failed on fragment $(id)", "id=%u", fragmentId));
                                goto LOOP_END;
                            }

                            rc = KDataBufferResize(&fragBuf, banked_size);
                            fip = (FragmentInfo *)fragBuf.base;
                            if (rc) {
                                // FATAL ERROR, OUT OF MEMORY
                                (void)PLOGERR(klogErr, (klogErr, rc, "Failed to resize fragment buffer", ""));
                                goto LOOP_END;
                            }

                            rc = MemBankRead(ctx->frags, fragmentId, 0, fragBuf.base, banked_size, &size2);
                            if (rc) {
                                // FATAL ERROR, INTERNAL CONSISTENCY ERROR
                                (void)PLOGERR(klogErr, (klogErr, rc, "KMemBankRead failed on fragment $(id)", "id=%u", fragmentId));
                                goto LOOP_END;
                            }
                            assert(banked_size == size2);
                        }
                        if (readNo == fip->readNo) {
                            /* is a repeat of the same read; do nothing */
                        }
                        else {
                            /* mate found; finish spot assembly */
                            unsigned read1 = 0;
                            unsigned read2 = 1;
                            char const *const seq1 = (const char *)&fip[1];
                            char const *const qual1 = (const char *)(seq1 + fip->readlen);
                            char const *const sg1 = (const char *)(qual1 + fip->readlen);
                            char const *const bx1 = (const char *)(sg1 + fip->sglen);
                            
                            ctx->m_SpotSize += 2*fip->readlen + fip->sglen + fip->lglen;


                            if (!isPrimary) {
                                if ((!G.assembleWithSecondary || hardclipped) && !G.deferSecondary ) {
                                    goto WRITE_ALIGNMENT;
                                }
                                (void)PLOGMSG(klogDebug, (klogDebug, "Spot '$(name)' (id $(id)) is being constructed from non-primary alignment information", "id=%lx,name=%s", keyId, name));
                            }
                            rc = KDataBufferResize(&seqBuffer, readlen + fip->readlen);
                            if (rc) {
                                // FATAL ERROR, OUT OF MEMORY
                                (void)LOGERR(klogErr, rc, "Failed to resize record buffer");
                                goto LOOP_END;
                            }
                            rc = KDataBufferResize(&qualBuffer, readlen + fip->readlen);
                            if (rc) {
                                // FATAL ERROR, OUT OF MEMORY
                                (void)LOGERR(klogErr, rc, "Failed to resize record buffer");
                                goto LOOP_END;
                            }
                            if (readNo < fip->readNo) {
                                read1 = 1;
                                read2 = 0;
                            }

                            memset(&srecStorage, 0, sizeof(srecStorage));
                            srec.numreads = 2;
                            srec.readLen[read1] = fip->readlen;
                            srec.readLen[read2] = readlen;
                            srec.readStart[1] = srec.readLen[0];
                            {
                                char const *const s1 = seq1;
                                char const *const s2 = (const char*)seqBuffer.base;
                                char *const d = (char*)seqBuffer.base;
                                char *const d1 = d + srec.readStart[read1];
                                char *const d2 = d + srec.readStart[read2];

                                srec.seq = (char*)seqBuffer.base;
                                if (d2 != s2) {
                                    memmove(d2, s2, readlen);
                                }
                                memmove(d1, s1, fip->readlen);
                            }
                            {
                                char const *const s1 = qual1;
                                char const *const s2 = (const char*)qualBuffer.base;
                                char *const d = (char*)qualBuffer.base;
                                char *const d1 = d + srec.readStart[read1];
                                char *const d2 = d + srec.readStart[read2];

                                srec.qual = (uint8_t*)qualBuffer.base;
                                if (d2 != s2) {
                                    memmove(d2, s2, readlen);
                                }
                                memmove(d1, s1, fip->readlen);
                            }

                            srec.ti[read1] = fip->ti;
                            srec.ti[read2] = ti;

                            srec.aligned[read1] = fip->aligned;
                            srec.aligned[read2] = isPrimary ? aligned : 0;

                            srec.is_bad[read1] = fip->is_bad;
                            srec.is_bad[read2] = (flags & BAMFlags_IsLowQuality) != 0;

                            srec.orientation[read1] = fip->orientation;
                            srec.orientation[read2] = AR_REF_ORIENT(data);

                            srec.cskey[read1] = fip->cskey;
                            srec.cskey[read2] = cskey;

                            srec.keyId = keyId;

                            srec.spotGroup = sg1;
                            srec.spotGroupLen = fip->sglen;

                            srec.linkageGroup = bx1;
                            srec.linkageGroupLen = fip->lglen;

                            srec.seq = (char*)seqBuffer.base;
                            srec.qual = (uint8_t*)qualBuffer.base;
    #ifndef NO_METADATA
                            auto v_pcr_dup = opt_pcr_dup.value_or(metadata.get<bit_t>(metadata_t::e_pcr_dup).test(row_id));
    #ifdef HAS_CTX_VALUE
                            if (v_pcr_dup != value->pcr_dup) {
                                spdlog::error("Inconsistent pcr_dup");
                                throw runtime_error("Inconsistent pcr_dup");
                            }
    #endif
    #else
                            auto v_pcr_dup = value->pcr_dup;
    #endif
                            rc = SequenceWriteRecord(seq, &srec, isColorSpace, v_pcr_dup, rec->platform);
                            if (rc) {
                                // FATAL ERROR, VDB I/O ERROR
                                (void)LOGERR(klogErr, rc, "SequenceWriteRecord failed");
                                goto LOOP_END;
                            }
                            ++ctx->spotId;
    #ifndef NO_METADATA
                            //*ctx->m_fs << "mate:" << keyId << '\t' << ctx->spotId << '\t' << row_id << endl;
                            //ctx->key4spot[ctx->spotId] = keyId;
                            metadata.get<u64_t>(metadata_t::e_spotId).set(row_id, ctx->spotId);
    #endif
    #if defined (HAS_CTX_VALUE)
                            CTX_VALUE_SET_S_ID(*value, ctx->spotId);
    #endif
                            rc = MemBankFree(ctx->frags, fragmentId);
                            if (rc) {
                                // FATAL ERROR, RUNTIME ERROR, LIKELY IMPOSSIBLE
                                (void)PLOGERR(klogErr, (klogErr, rc, "KMemBankFree failed on fragment $(id)", "id=%u", fragmentId));
                                goto LOOP_END;
                            }

                            --ctx->m_BankedSpots;
                            ctx->m_BankedSize -= banked_size;

    #ifndef NO_METADATA
                            //fragment_buffer.set_bit_no_check(row_id);
                            metadata.get<u32_t>(metadata_t::e_fragmentId).set(row_id, 0);
    #endif
    #if defined (HAS_CTX_VALUE)
                            value->fragmentId = 0;
    #endif

                            if (revcmp) {
                                QUAL_CHANGED_REVERSED;
                                SEQ__CHANGED_REV_COMP;
                            }
                            if (v_pcr_dup && (srec.is_bad[0] || srec.is_bad[1])) {
                                FLAG_CHANGED_400_AND_200;
                                filterFlagConflictRecords++;
                                if (filterFlagConflictRecords < MAX_WARNINGS_FLAG_CONFLICT) {
                                    // WARNING
                                    (void)PLOGMSG(klogWarn, (klogWarn, "Spot '$(name)': both 0x400 and 0x200 flag bits set, only 0x400 will be saved", "name=%s", name));
                                }
                                else if (filterFlagConflictRecords == MAX_WARNINGS_FLAG_CONFLICT) {
                                    // WARNING
                                    (void)PLOGMSG(klogWarn, (klogWarn, "Last reported warning: Spot '$(name)': both 0x400 and 0x200 flag bits set, only 0x400 will be saved", "name=%s", name));
                                }
                            }
                        }
                    }
                    else {
                        // FATAL INTERNAL CONSISTENCY ERROR
                        (void)PLOGMSG(klogErr, (klogErr, "Spot '$(name)' has caused the loader to enter an illogical state", "name=%s", name));
                        assert("this should never happen");
                        abort();
                    }
                }
                else {
                    // Spot has been written already
                }
            }
            else if (spotId == 0) {
                /* new unmated fragment - no spot assembly */
                if (!isPrimary) {
                    if ((!G.assembleWithSecondary || hardclipped) && !G.deferSecondary ) {
                        goto WRITE_ALIGNMENT;
                    }
                    (void)PLOGMSG(klogDebug, (klogDebug, "Spot '$(name)' (id $(id)) is being constructed from non-primary alignment information", "id=%lx,name=%s", keyId, name));
                }
                if (ctx->m_isSingleGroup == false) // otherwise spotGroup was captured
                    getSpotGroup(rec, spotGroup);

                BAM_AlignmentGetBarCode(rec, &barCode);
                if (barCode) {
                    if (spotGroup[0] != '\0' && rec->platform == SRA_PLATFORM_UNDEFINED) {
                        /* don't use bar code */
                    }
                    else {
                        unsigned const sglen = strlen(barCode);
                        if (sglen + 1 < sizeof(spotGroup))
                            memmove(spotGroup, barCode, sglen + 1);
                    }
                }
                ctx->m_SpotSize += readlen*2 + strlen(spotGroup) + strlen(linkageGroup);

                memset(&srecStorage, 0, sizeof(srecStorage));
                srec.numreads = 1;

                srec.readLen[0] = readlen;
                srec.ti[0] = ti;
                srec.aligned[0] = isPrimary ? aligned : 0;
                srec.is_bad[0] = (flags & BAMFlags_IsLowQuality) != 0;
                srec.orientation[0] = AR_REF_ORIENT(data);
                srec.cskey[0] = cskey;

                srec.keyId = keyId;

                srec.spotGroup = spotGroup;
                srec.spotGroupLen = strlen(spotGroup);

                srec.linkageGroup = linkageGroup;
                srec.linkageGroupLen = strlen(linkageGroup);

                srec.seq = (char*)seqBuffer.base;
                srec.qual = (uint8_t*)qualBuffer.base;
#ifndef NO_METADATA
                auto v_pcr_dup = opt_pcr_dup.value_or(metadata.get<bit_t>(metadata_t::e_pcr_dup).test(row_id));
#ifdef HAS_CTX_VALUE
                if (v_pcr_dup != value->pcr_dup) {
                    spdlog::error("Inconsistent pcr_dup");
                    throw runtime_error("Inconsistent pcr_dup");
                }
#endif
#else
                auto v_pcr_dup = value->pcr_dup;
#endif
                rc = SequenceWriteRecord(seq, &srec, isColorSpace, v_pcr_dup, rec->platform);
                if (rc) {
                    // FATAL ERROR, VDB I/O ERROR
                    (void)PLOGERR(klogErr, (klogErr, rc, "SequenceWriteRecord failed", NULL));
                    goto LOOP_END;
                }
                ++ctx->spotId;
#ifndef NO_METADATA
//                *ctx->m_fs << "no mate:" << keyId << '\t' << ctx->spotId << '\t' << row_id << endl;
//                ctx->key4spot[ctx->spotId] = keyId;
                metadata.get<u64_t>(metadata_t::e_spotId).set(row_id, ctx->spotId);
                //fragment_buffer.set_bit_no_check(row_id);
                metadata.get<u32_t>(metadata_t::e_fragmentId).set(row_id, 0);
#endif
#if defined (HAS_CTX_VALUE)
                CTX_VALUE_SET_S_ID(*value, ctx->spotId);
                value->fragmentId = 0;
#endif

                if (v_pcr_dup && srec.is_bad[0]) {
                    FLAG_CHANGED_400_AND_200;
                    filterFlagConflictRecords++;
                    if (filterFlagConflictRecords < MAX_WARNINGS_FLAG_CONFLICT) {
                        // WARNING
                        (void)PLOGMSG(klogWarn, (klogWarn, "Spot '$(name)': both 0x400 and 0x200 flag bits set, only 0x400 will be saved", "name=%s", name));
                    }
                    else if (filterFlagConflictRecords == MAX_WARNINGS_FLAG_CONFLICT) {
                        // WARNING
                        (void)PLOGMSG(klogWarn, (klogWarn, "Last reported warning: Spot '$(name)': both 0x400 and 0x200 flag bits set, only 0x400 will be saved", "name=%s", name));
                    }
                }
                if (revcmp) {
                    QUAL_CHANGED_REVERSED;
                    SEQ__CHANGED_REV_COMP;
                }
            }
        }
WRITE_ALIGNMENT:
        if (aligned) {
            if (mated && !isPrimary) {
                int32_t bam_mrid;
                int64_t mpos;
                int64_t mrid = 0;
                int64_t tlen;

                BAM_AlignmentGetMatePosition(rec, &mpos);
                BAM_AlignmentGetMateRefSeqId(rec, &bam_mrid);
                BAM_AlignmentGetInsertSize(rec, &tlen);

                if (mpos >= 0 && bam_mrid >= 0 && tlen != 0) {
                    BAMRefSeq const *mref;

                    BAM_FileGetRefSeq(bam, bam_mrid, &mref);
                    if (mref) {
                        rc_t rc_temp = ReferenceGet1stRow(ref, &mrid, mref->name);
                        if (rc_temp == 0) {
                            data.mate_ref_pos = mpos;
                            data.template_len = tlen;
                            data.mate_ref_orientation = (flags & BAMFlags_MateIsReverse) ? 1 : 0;
                        }
                        else {
                            // WARNING
                            (void)PLOGERR(klogWarn, (klogWarn, rc_temp, "Failed to get refID for $(name)", "name=%s", mref->name));
                            MATE_INFO_LOST_UNKNOWN_REF;
                        }
                        data.mate_ref_id = mrid;
                    }
                    else {
                        MATE_INFO_LOST_INVALID;
                    }
                }
                else if (mpos >= 0 || bam_mrid >= 0 || tlen != 0) {
                    MATE_INFO_LOST_MISSING;
                }
            }

            if (wasRenamed) {
                RENAMED_REFERENCE;
            }
#ifndef NO_METADATA
            auto v_aln_count = wasInserted ? 0 : metadata.get<u16_t>(metadata_t::E_ALN_COUNT[readNo - 1]).get_no_check(row_id);
#ifdef HAS_CTX_VALUE
            if (v_aln_count != value->alignmentCount[readNo - 1]) {
                spdlog::error("Inconsistent alignmentCount");
                throw runtime_error("Inconsistent alignmentCount");
            }
#endif
            if (v_aln_count < 254) {
#ifdef HAS_CTX_VALUE
                ++value->alignmentCount[readNo - 1];
#endif
                metadata.get<u16_t>(metadata_t::E_ALN_COUNT[readNo - 1]).inc(row_id);
            }
#else
            auto v_aln_count = value->alignmentCount[readNo - 1];
            if (v_aln_count < 254) {
                ++value->alignmentCount[readNo - 1];
            }
#endif
            ++ctx->alignCount;

            if (linkageGroup[0] != '\0') {
                AR_LINKAGE_GROUP(data).elements = strlen(linkageGroup);
                AR_LINKAGE_GROUP(data).buffer = linkageGroup;
            }

            rc = AlignmentWriteRecord(align, &data);
            if (rc == 0) {
                if (!isPrimary)
                    data.alignId = ++ctx->secondId;

                rc = ReferenceAddAlignId(ref, data.alignId, isPrimary);
                if (rc) {
                    // FATAL ERROR, VDB I/O ERROR                   
                    (void)PLOGERR(klogErr, (klogErr, rc, "ReferenceAddAlignId failed", NULL));
                }
                else {
                    *had_alignments = true;
                }
            }
            else {
                // FATAL ERROR, VDB I/O ERROR
                (void)PLOGERR(klogErr, (klogErr, rc, "AlignmentWriteRecord failed", NULL));
            }
        }
        /**************************************************************/

    LOOP_END:
        BAM_AlignmentRelease(rec);
#if !defined(NEW_QUEUE)
        delete queue_rec;
#endif
        ++reccount;

        if (G.maxAlignCount > 0 && reccount >= G.maxAlignCount)
            break;
        if (rc == 0)
            *had_sequences = true;
        else
            break;
    }
    spdlog::info("New spots: {:L}, recordRead: {:L}, recordsProcess: {:L}", new_spots, recordsRead, recordsProcessed);
    /*
    for (const auto& it : ctx->spots) {
            spdlog::info("Orhpan spot: {}", it);
    }
*/

    //ctx->m_fs->close();
#ifndef NEW_QUEUE
    if (bamread_thread != NULL && bamq != NULL) {
        KQueueSeal(bamq);
        for ( ; ; ) {
            timeout_t tm;
            void *rr = NULL;
            rc_t rc2;

            TimeoutInit(&tm, 1000);
            rc2 = KQueuePop(bamq, &rr, &tm);
            if (rc2) break;
            BAM_AlignmentRelease((BAM_Alignment *)rr);
        }
        KThreadWait(bamread_thread, NULL);
    }
#else
    rw_done.store(true);
    KThreadWait(bamread_thread, NULL);
    {
        queue_rec_t queue_rec;
        while (rw_queue.try_dequeue(queue_rec)) {
            //spdlog::info("There still recs, dude!");
            BAM_AlignmentRelease(queue_rec.alignment);
        }
    }

#endif
    KThreadRelease(bamread_thread);
#ifndef NEW_QUEUE
    KQueueRelease(bamq);
#endif
    if (rc) {
        if (   (GetRCModule(rc) == rcCont && (int)GetRCObject(rc) == rcData && GetRCState(rc) == rcDone)
            || (GetRCModule(rc) == rcAlign && GetRCObject(rc) == rcRow && GetRCState(rc) == rcNotFound))
        {
            (void)PLOGMSG(klogInfo, (klogInfo, "EOF '$(file)'; processed $(proc)", "file=%s,read=%lu,proc=%lu", bamFile, (unsigned long)recordsRead, (unsigned long)recordsProcessed));
            rc = 0;
        }
        else {
            (void)PLOGERR(klogInfo, (klogInfo, rc, "Error '$(file)'; read $(read); processed $(proc)", "file=%s,read=%lu,proc=%lu", bamFile, (unsigned long)recordsRead, (unsigned long)recordsProcessed));
        }
    }
    if (filterFlagConflictRecords > 0) {
        (void)PLOGMSG(klogWarn, (klogWarn, "$(cnt1) out of $(cnt2) records contained warning : both 0x400 and 0x200 flag bits set, only 0x400 will be saved", "cnt1=%lu,cnt2=%lu", filterFlagConflictRecords,recordsProcessed));
    }
    if (rc == 0 && recordsProcessed == 0) {
        (void)LOGMSG(klogWarn, (G.limit2config || G.refFilter != NULL) ?
                     "All records from the file were filtered out" :
                     "The file contained no records that were processed.");
        rc = RC(rcAlign, rcFile, rcReading, rcData, rcEmpty);
    }

    BAM_FileRelease(bam);
#ifdef HAS_CTX_VALUE
    MMArrayLock(ctx->id2value);
#endif
    KDataBufferWhack(&seqBuffer);
    KDataBufferWhack(&qualBuffer);
    KDataBufferWhack(&buf);
    KDataBufferWhack(&fragBuf);
    KDataBufferWhack(&cigBuf);
    KDataBufferWhack(&data.buffer);
    return rc;
}

#if defined(HAS_CTX_VALUE) && defined(NO_METADATA)
/*
static rc_t WriteSoloFragments(context_t *ctx, Sequence *seq)
{
    uint32_t i;
    unsigned j;
    uint64_t idCount = 0;
    rc_t rc;
    KDataBuffer fragBuf;
    SequenceRecordStorage srecStorage;
    SequenceRecord srec;

    ++ctx->pass;
    memset(&srec, 0, sizeof(srec));

    srec.ti             = srecStorage.ti;
    srec.readStart      = srecStorage.readStart;
    srec.readLen        = srecStorage.readLen;
    srec.orientation    = srecStorage.orientation;
    srec.is_bad         = srecStorage.is_bad;
    srec.alignmentCount = srecStorage.alignmentCount;
    srec.aligned        = srecStorage.aligned;
    srec.cskey          = srecStorage. cskey;

    rc = KDataBufferMake(&fragBuf, 8, 0);
    if (rc) {
        (void)LOGERR(klogErr, rc, "KDataBufferMake failed");
        return rc;
    }
//    for (idCount = 0, j = 0; j < ctx->keyToID.key2id_count; ++j) {
//        idCount += ctx->keyToID.idCount[j];
//    }
//    KLoadProgressbar_Append(ctx->progress[ctx->pass - 1], idCount);
    ctx->visit_keyId([&](uint64_t keyId) {
        ctx_value_t *value;
        size_t rsize;
        size_t sz;
        char const *src;
        FragmentInfo const *fip;

        rc = MMArrayGet(ctx->id2value, (void **)&value, keyId);
        if (rc)
            return;
        KLoadProgressbar_Process(ctx->progress[ctx->pass - 1], 1, false);
        if (value->fragmentId == 0)
            return;

        rc = MemBankSize(ctx->frags, value->fragmentId, &sz);
        if (rc) {
            (void)LOGERR(klogErr, rc, "KMemBankSize failed");
            return;
        }
        rc = KDataBufferResize(&fragBuf, (size_t)sz);
        if (rc) {
            (void)LOGERR(klogErr, rc, "KDataBufferResize failed");
            return;
        }
        rc = MemBankRead(ctx->frags, value->fragmentId, 0, fragBuf.base, sz, &rsize);
        if (rc) {
            (void)LOGERR(klogErr, rc, "KMemBankRead failed");
            return;
        }
        assert( rsize == sz );

        fip = (const FragmentInfo*)fragBuf.base;
        src = (char const *)&fip[1];

        memset(&srecStorage, 0, sizeof(srecStorage));
        if (value->unmated) {
            srec.numreads = 1;
            srec.readLen[0] = fip->readlen;
            srec.ti[0] = fip->ti;
            srec.aligned[0] = fip->aligned;
            srec.is_bad[0] = fip->is_bad;
            srec.orientation[0] = fip->orientation;
            srec.cskey[0] = fip->cskey;
        }
        else {
            unsigned const read = ((fip->aligned && CTX_VALUE_GET_P_ID(*value, 0) == 0) || value->unaligned_2) ? 1 : 0;

            srec.numreads = 2;
            srec.readLen[read] = fip->readlen;
            srec.readStart[1] = srec.readLen[0];
            srec.ti[read] = fip->ti;
            srec.aligned[read] = fip->aligned;
            srec.is_bad[read] = fip->is_bad;
            srec.orientation[read] = fip->orientation;
            srec.cskey[0] = srec.cskey[1] = 'N';
            srec.cskey[read] = fip->cskey;
        }
        srec.seq = (char *)src;
        srec.qual = (uint8_t *)(src + fip->readlen);
        srec.spotGroup = (char *)(src + 2 * fip->readlen);
        srec.spotGroupLen = fip->sglen;
        srec.linkageGroup = (char *)(src + 2 * fip->readlen * fip->sglen);
        srec.linkageGroupLen = fip->lglen;
        srec.keyId = keyId;
        assert(false);
        rc = SequenceWriteRecord(seq, &srec, ctx->isColorSpace, value->pcr_dup, value->platform);
        if (rc) {
            // FATAL ERROR, VDB I/O ERROR
            (void)LOGERR(klogErr, rc, "SequenceWriteRecord failed");
            return;
        }
        rc = KMemBankFree(frags, id);
        CTX_VALUE_SET_S_ID(*value, ++ctx->spotId);
    });
    MMArrayLock(ctx->id2value);
    KDataBufferWhack(&fragBuf);
    return rc;
}

static rc_t SequenceUpdateAlignInfo(context_t *ctx, Sequence *seq)
{
    rc_t rc = 0;
    uint64_t row;
    uint64_t keyId;

    ++ctx->pass;
    KLoadProgressbar_Append(ctx->progress[ctx->pass - 1], ctx->spotId + 1);

    for (row = 1; row <= ctx->spotId; ++row) {
        ctx_value_t *value;

        rc = SequenceReadKey(seq, row, &keyId);
        if (rc) {
            (void)PLOGERR(klogErr, (klogErr, rc, "Failed to get key for row $(row)", "row=%u", (unsigned)row));
            break;
        }
        rc = MMArrayGet(ctx->id2value, (void **)&value, keyId);
        if (rc) {
            (void)PLOGERR(klogErr, (klogErr, rc, "Failed to read info for row $(row), index $(idx)", "row=%u,idx=%u", (unsigned)row, (unsigned)keyId));
            break;
        }
        if (G.mode == mode_Remap) {
            CTX_VALUE_SET_S_ID(*value, row);
        }
        if (row != CTX_VALUE_GET_S_ID(*value)) {
            rc = RC(rcApp, rcTable, rcWriting, rcData, rcUnexpected);
            (void)PLOGMSG(klogErr, (klogErr, "Unexpected spot id $(spotId) for row $(row), index $(idx)", "spotId=%u,row=%u,idx=%u", (unsigned)CTX_VALUE_GET_S_ID(*value), (unsigned)row, (unsigned)keyId));
            break;
        }
        {{
            int64_t primaryId[2];
            int const logLevel = klogWarn; //G.assembleWithSecondary ? klogWarn : klogErr;

            primaryId[0] = CTX_VALUE_GET_P_ID(*value, 0);
            primaryId[1] = CTX_VALUE_GET_P_ID(*value, 1);

            if (primaryId[0] == 0 && value->alignmentCount[0] != 0) {
                rc = RC(rcApp, rcTable, rcWriting, rcConstraint, rcViolated);
                (void)PLOGERR(logLevel, (logLevel, rc, "Spot id $(id) read 1 never had a primary alignment", "id=%lx", keyId));
            }
            if (!value->unmated && primaryId[1] == 0 && value->alignmentCount[1] != 0) {
                rc = RC(rcApp, rcTable, rcWriting, rcConstraint, rcViolated);
                (void)PLOGERR(logLevel, (logLevel, rc, "Spot id $(id) read 2 never had a primary alignment", "id=%lx", keyId));
            }
            if (rc != 0 && logLevel == klogErr)
                break;

            rc = SequenceUpdateAlignData(seq, row, value->unmated ? 1 : 2,
                                         primaryId,
                                         value->alignmentCount);
        }}
        if (rc) {
            (void)LOGERR(klogErr, rc, "Failed updating Alignment data in sequence table");
            break;
        }
        KLoadProgressbar_Process(ctx->progress[ctx->pass - 1], 1, false);
    }
    MMArrayLock(ctx->id2value);
    return rc;
}

static rc_t AlignmentUpdateSpotInfo(context_t *ctx, Alignment *align)
{
    rc_t rc;
    uint64_t keyId;

    ++ctx->pass;

    KLoadProgressbar_Append(ctx->progress[ctx->pass - 1], ctx->alignCount);

    rc = AlignmentStartUpdatingSpotIds(align);
    while (rc == 0 && (rc = Quitting()) == 0) {
        ctx_value_t *value;

        rc = AlignmentGetSpotKey(align, &keyId);
        if (rc) {
            if (GetRCObject(rc) == rcRow && GetRCState(rc) == rcNotFound)
                rc = 0;
            break;
        }
        //assert(keyId >> 32 < ctx->keyToID.key2id_count);
        //assert((uint32_t)keyId < ctx->keyToID.idCount[keyId >> 32]);
        rc = MMArrayGet(ctx->id2value, (void **)&value, keyId);
        if (rc == 0) {
            int64_t const spotId = CTX_VALUE_GET_S_ID(*value);

            if (spotId == 0) {
                rc = RC(rcApp, rcTable, rcWriting, rcConstraint, rcViolated);
                (void)PLOGERR(klogErr, (klogErr, rc, "Spot '$(id)' was never assigned a spot id, probably has no primary alignments", "id=%lx", keyId));
                break;
            }
#ifndef NO_METADATA
            uint32_t group_id = keyId >> GROUPID_SHIFT;
            auto [metadata, row_id] = ctx->m_read_groups[group_id]->metadata_by_key(keyId & KEYID_MASK);

            auto spot_id = metadata->get<u64_t>(metadata_t::e_spotId).get(row_id);
            if (spot_id != spotId) {
                spdlog::error("Conflict: {} != {}", spot_id, spotId);
            }
#endif
            rc = AlignmentWriteSpotId(align, spotId);
        }
        KLoadProgressbar_Process(ctx->progress[ctx->pass - 1], 1, false);
    }
    MMArrayLock(ctx->id2value);
    return rc;
}
*/
#else

static rc_t WriteSoloFragments(context_t *ctx, Sequence *seq)
{
    spdlog::stopwatch sw;
    rc_t rc;
    KDataBuffer fragBuf;
    SequenceRecordStorage srecStorage;
    SequenceRecord srec;

    ++ctx->pass;
    memset(&srec, 0, sizeof(srec));

    srec.ti             = srecStorage.ti;
    srec.readStart      = srecStorage.readStart;
    srec.readLen        = srecStorage.readLen;
    srec.orientation    = srecStorage.orientation;
    srec.is_bad         = srecStorage.is_bad;
    srec.alignmentCount = srecStorage.alignmentCount;
    srec.aligned        = srecStorage.aligned;
    srec.cskey          = srecStorage. cskey;

    rc = KDataBufferMake(&fragBuf, 8, 0);
    if (rc) {
        // FATAL ERROR, OUT OF MEMORY
        (void)LOGERR(klogErr, rc, "KDataBufferMake failed");
        return rc;
    }

    uint64_t idCount = 0;
    for(const auto& rg : ctx->m_read_groups)
        idCount += rg->m_total_spots;
    KLoadProgressbar_Append(ctx->progress[ctx->pass - 1], idCount);
    unsigned group_id = 0;
    for (auto& gr : ctx->m_read_groups) {
        gr->visit_metadata([&](metadata_t& metadata, unsigned group_id, size_t offset) {
        size_t row_id = 0;
        auto& fragCol = metadata.get<u32_t>(metadata_t::e_fragmentId);
        auto fragment_it = fragCol.begin();
        while (fragment_it.valid()) {
            KLoadProgressbar_Process(ctx->progress[ctx->pass - 1], 1, false);
            uint64_t keyId = ((uint64_t)group_id << GROUPID_SHIFT) | (offset + row_id);
#ifdef HAS_CTX_VALUE
            ctx_value_t *value;
            MMArrayGet(ctx->id2value, (void **)&value, keyId);
            if (fragment_it.value() != value->fragmentId) {
                spdlog::error("Solo fragment mismatch: {} != {}", fragment_it.value(), value->fragmentId);
            }
#endif
            if (fragment_it.value() != 0) {

                size_t rsize;
                size_t sz;
                char const *src;
                FragmentInfo const *fip;
                rc = MemBankSize(ctx->frags, fragment_it.value(), &sz);
                if (rc) {
                    // FATAL ERROR, INTERNAL CONSISTENCY ERROR
                    (void)LOGERR(klogErr, rc, "KMemBankSize failed");
                    break;
                }
                rc = KDataBufferResize(&fragBuf, (size_t)sz);
                if (rc) {
                    // FATAL ERROR, OUT OF MEMORY
                    (void)LOGERR(klogErr, rc, "KDataBufferResize failed");
                    break;
                }
                rc = MemBankRead(ctx->frags, fragment_it.value(), 0, fragBuf.base, sz, &rsize);
                if (rc) {
                    // FATAL ERROR, INTERNAL CONSISTENCY ERROR
                    (void)LOGERR(klogErr, rc, "KMemBankRead failed");
                    break;
                }
                assert( rsize == sz );

                fip = (const FragmentInfo*)fragBuf.base;
                src = (char const *)&fip[1];

                memset(&srecStorage, 0, sizeof(srecStorage));

                if (metadata.get<bit_t>(metadata_t::e_unmated).test(row_id)) {  //value->unmated
                    srec.numreads = 1;
                    srec.readLen[0] = fip->readlen;
                    srec.ti[0] = fip->ti;
                    srec.aligned[0] = fip->aligned;
                    srec.is_bad[0] = fip->is_bad;
                    srec.orientation[0] = fip->orientation;
                    srec.cskey[0] = fip->cskey;
                } else {
                    unsigned const read = ((fip->aligned && metadata.get<u64_t>(metadata_t::E_PRIM_ID[0]).get_no_check(row_id) == 0) || metadata.get<bit_t>(metadata_t::e_unaligned_2).test(row_id))
                        ? 1 : 0;
                    srec.numreads = 2;
                    srec.readLen[read] = fip->readlen;
                    srec.readStart[1] = srec.readLen[0];
                    srec.ti[read] = fip->ti;
                    srec.aligned[read] = fip->aligned;
                    srec.is_bad[read] = fip->is_bad;
                    srec.orientation[read] = fip->orientation;
                    srec.cskey[0] = srec.cskey[1] = 'N';
                    srec.cskey[read] = fip->cskey;
                }
                srec.seq = (char *)src;
                srec.qual = (uint8_t *)(src + fip->readlen);
                srec.spotGroup = (char *)(src + 2 * fip->readlen);
                srec.spotGroupLen = fip->sglen;
                srec.linkageGroup = (char *)(src + 2 * fip->readlen * fip->sglen);
                srec.linkageGroupLen = fip->lglen;
                srec.keyId = keyId;
                INSDC_SRA_platform_id platform_id = ctx->m_isSingleGroup ?
                    metadata.get<u16_t>(metadata_t::e_platform).get(row_id) : ctx->m_read_groups[group_id]->m_platform;
                rc = SequenceWriteRecord(seq, &srec, ctx->isColorSpace, metadata.get<bit_t>(metadata_t::e_pcr_dup).test(row_id), platform_id);
                if (rc) {
                    // FATAL ERROR, VDB I/O ERROR
                    (void)LOGERR(klogErr, rc, "SequenceWriteRecord failed");
                    break;
                }
                assert(metadata.get<u64_t>(metadata_t::e_spotId).get_no_check(row_id) == 0);
                metadata.get<u64_t>(metadata_t::e_spotId).set(row_id, ++ctx->spotId);
#ifdef HAS_CTX_VALUE
                CTX_VALUE_SET_S_ID(*value, ctx->spotId);
#endif
            }
            fragment_it.advance();
            ++row_id;
        }
        }, group_id);
        ++group_id;
    }
/*
    ctx->visit_metadata([&](metadata_t& metadata, unsigned group_id, size_t offset) {
        size_t row_id = 0;
        auto& fragCol = metadata.get<u32_t>(metadata_t::e_fragmentId);
        auto fragment_it = fragCol.begin();
        while (fragment_it.valid()) {
            KLoadProgressbar_Process(ctx->progress[ctx->pass - 1], 1, false);
            uint64_t keyId = ((uint64_t)group_id << GROUPID_SHIFT) | (offset + row_id);
#ifdef HAS_CTX_VALUE
            ctx_value_t *value;
            MMArrayGet(ctx->id2value, (void **)&value, keyId);
            if (fragment_it.value() != value->fragmentId) {
                spdlog::error("Solo fragment mismatch: {} != {}", fragment_it.value(), value->fragmentId);
            }
#endif
            if (fragment_it.value() != 0) {

                size_t rsize;
                size_t sz;
                char const *src;
                FragmentInfo const *fip;
                rc = MemBankSize(ctx->frags, fragment_it.value(), &sz);
                if (rc) {
                    (void)LOGERR(klogErr, rc, "KMemBankSize failed");
                    break;
                }
                rc = KDataBufferResize(&fragBuf, (size_t)sz);
                if (rc) {
                    (void)LOGERR(klogErr, rc, "KDataBufferResize failed");
                    break;
                }
                rc = MemBankRead(ctx->frags, fragment_it.value(), 0, fragBuf.base, sz, &rsize);
                if (rc) {
                    (void)LOGERR(klogErr, rc, "KMemBankRead failed");
                    break;
                }
                assert( rsize == sz );

                fip = (const FragmentInfo*)fragBuf.base;
                src = (char const *)&fip[1];

                memset(&srecStorage, 0, sizeof(srecStorage));

                if (metadata.get<bit_t>(metadata_t::e_unmated).test(row_id)) {  //value->unmated
                    srec.numreads = 1;
                    srec.readLen[0] = fip->readlen;
                    srec.ti[0] = fip->ti;
                    srec.aligned[0] = fip->aligned;
                    srec.is_bad[0] = fip->is_bad;
                    srec.orientation[0] = fip->orientation;
                    srec.cskey[0] = fip->cskey;
                } else {
                    unsigned const read = ((fip->aligned && metadata.get<u64_t>(metadata_t::E_PRIM_ID[0]).get_no_check(row_id) == 0) || metadata.get<bit_t>(metadata_t::e_unaligned_2).test(row_id))
                        ? 1 : 0;
                    srec.numreads = 2;
                    srec.readLen[read] = fip->readlen;
                    srec.readStart[1] = srec.readLen[0];
                    srec.ti[read] = fip->ti;
                    srec.aligned[read] = fip->aligned;
                    srec.is_bad[read] = fip->is_bad;
                    srec.orientation[read] = fip->orientation;
                    srec.cskey[0] = srec.cskey[1] = 'N';
                    srec.cskey[read] = fip->cskey;
                }
                srec.seq = (char *)src;
                srec.qual = (uint8_t *)(src + fip->readlen);
                srec.spotGroup = (char *)(src + 2 * fip->readlen);
                srec.spotGroupLen = fip->sglen;
                srec.linkageGroup = (char *)(src + 2 * fip->readlen * fip->sglen);
                srec.linkageGroupLen = fip->lglen;
                srec.keyId = keyId;
                INSDC_SRA_platform_id platform_id = ctx->m_isSingleGroup ?
                    metadata.get<u16_t>(metadata_t::e_platform).get(row_id) : ctx->m_read_groups[group_id]->m_platform;
                rc = SequenceWriteRecord(seq, &srec, ctx->isColorSpace, metadata.get<bit_t>(metadata_t::e_pcr_dup).test(row_id), platform_id);
                if (rc) {
                    (void)LOGERR(klogErr, rc, "SequenceWriteRecord failed");
                    break;
                }
                assert(metadata.get<u64_t>(metadata_t::e_spotId).get_no_check(row_id) == 0);
                metadata.get<u64_t>(metadata_t::e_spotId).set(row_id, ++ctx->spotId);
#ifdef HAS_CTX_VALUE
                CTX_VALUE_SET_S_ID(*value, ctx->spotId);
#endif
            }
            fragment_it.advance();
            ++row_id;
        }
    });
*/
    KDataBufferWhack(&fragBuf);

    ctx->clear_column<u32_t>(metadata_t::e_fragmentId);
    ctx->clear_column<u16_t>(metadata_t::e_fragment_len1);
    ctx->clear_column<u16_t>(metadata_t::e_fragment_len2);
    ctx->clear_column<u16_t>(metadata_t::e_platform);
    ctx->clear_column<bit_t>(metadata_t::e_pcr_dup);

    spdlog::info("Solo fragments: {:.3} sec, memory: {:L}", sw, getCurrentRSS());
    return rc;
}


static rc_t SequenceUpdateAlignInfo(context_t *ctx, Sequence *seq)
{
    spdlog::stopwatch sw;
    rc_t rc = 0;
    uint64_t row;
    uint64_t keyId;
    ++ctx->pass;

    if (G.mode != mode_Remap) {
        spdlog::info("Extraction start, memory: {:L}", getCurrentRSS());
        ctx->extract_spotid();
        spdlog::info("Extraction stop: {:.3} sec, memory: {:L}", sw, getCurrentRSS());
        sw.reset();
    }
    size_t row_offset = 1;
    KLoadProgressbar_Append(ctx->progress[ctx->pass - 1], ctx->spotId + 1);
    struct key_batch_t {
        vector<uint64_t> keys;
        vector<uint8_t> alignmentCount;//(BUFFER_SIZE * 2);
        vector<int64_t> primaryId;//(BUFFER_SIZE * 2);
        vector<uint8_t> unmated;//(BUFFER_SIZE * 2);
        size_t offset = 0;
    };
    atomic<bool> queue_done{false};
    atomic<bool> exit_on_error{false};
    atomic<bool> gather_done{false};
    atomic<size_t> num_gathered = 0;
    atomic<size_t> num_updated = 0;

    size_t batches_processed = 0;
    size_t batches_gathered = 0;
    size_t batches_updated = 0;
    ReaderWriterQueue<key_batch_t> gather_queue{12};
    ReaderWriterQueue<key_batch_t> update_queue{4};

    constexpr int BUFFER_SIZE = 10e6;
    mutex metadata_mutex;  // protects metadata in Remap mode
    int const logLevel = klogWarn; /*G.assembleWithSecondary ? klogWarn : klogErr;*/

    /* Two tasks and two queues
     * Main thread puts keyIds into gather_queue
     * Gather task works on gather_queue, extracts metadata and pus them into update_queue
     * Update task works on update queue and updates VDB
     *
     */
    size_t expected_batches = (ctx->spotId + BUFFER_SIZE - 1) / BUFFER_SIZE;
    auto gather_task = ctx->m_executor->async([&]() {
        key_batch_t batch;
        while (exit_on_error == false) {
            if (gather_queue.try_dequeue(batch)) {

                if ( batch.keys.size() == 0 )
                {   // empty batch signals the end of processing
                    update_queue.enqueue(batch); // signal the update thread to exit
                    break;
                }

                ++batches_gathered;
                (void)PLOGMSG(klogInfo, (klogInfo, "batches_gathered: $(NR) records expected $(e)", "NR=%lu,e=%lu", batches_gathered, expected_batches));

                spdlog::stopwatch sw;
                auto sz = batch.keys.size();
                int page_size = sz/ctx->m_executor->num_workers();
                int num_pages = page_size ? sz/page_size + 1 : 1;
                if (num_pages == 1)
                    page_size = sz;
                batch.alignmentCount.resize(sz * 2);
                batch.primaryId.resize(sz * 2);
                batch.unmated.resize(sz);
                tf::Taskflow taskflow;
                taskflow.for_each_index(0, num_pages, 1, [&](int index) {
                    size_t row_b = index * page_size;
                    size_t row_e = min<size_t>(row_b + page_size, batch.keys.size());
                    for (; row_b < row_e; ++row_b) {
                        ++num_gathered;
                        auto keyId = batch.keys[row_b];
                        uint32_t group_id = keyId >> GROUPID_SHIFT;
                        uint64_t row_id = keyId & KEYID_MASK;
                        auto [metadata, local_row_id] = ctx->m_read_groups[group_id]->metadata_by_key(row_id);
                        batch.primaryId[row_b * 2] = (int64_t)metadata->get<u64_t>(metadata_t::E_PRIM_ID[0]).get_no_check(local_row_id);
                        batch.primaryId[row_b * 2 + 1] = (int64_t)metadata->get<u64_t>(metadata_t::E_PRIM_ID[1]).get_no_check(local_row_id);
                        batch.alignmentCount[row_b * 2] = (uint8_t)metadata->get<u16_t>(metadata_t::E_ALN_COUNT[0]).get_no_check(local_row_id);
                        batch.alignmentCount[row_b * 2 + 1] = (uint8_t)metadata->get<u16_t>(metadata_t::E_ALN_COUNT[1]).get_no_check(local_row_id);
                        batch.unmated[row_b] = metadata->get<bit_t>(metadata_t::e_unmated).test(local_row_id) ? 1 : 0;
                        if (G.mode == mode_Remap) {
                            const lock_guard<mutex> lock(metadata_mutex);
                            metadata->get<u64_t>(metadata_t::e_spotId).set(local_row_id, row_b + row_offset);
                        }
                    }
                });
                ctx->m_executor->run(taskflow).wait();
                while (!update_queue.try_enqueue(std::move(batch))) {
                    if (exit_on_error)
                        break;
                };
            } else if (batches_gathered >= expected_batches) {
                (void)PLOGMSG(klogInfo, (klogInfo, "batches_gathered >= expected_batches $(NR)", "NR=%lu", batches_gathered));
                break;
            }
        }
        gather_done = true;
    });

    auto update_task = ctx->m_executor->async([&]() {
        key_batch_t batch;
        rc = 0;
        while (true) {
            if (update_queue.try_dequeue(batch)) {

                if ( batch.keys.size() == 0 )
                {   // empty batch signals the end of processing
                    break;
                }

                ++batches_updated;
                (void)PLOGMSG(klogInfo, (klogInfo, "batches_updated: $(NR) records expected $(e)", "NR=%lu,e=%lu", batches_updated, expected_batches));

                spdlog::stopwatch sw;
                for (size_t i = 0; i < batch.keys.size(); ++i) {
                    ++num_updated;
                    auto i_row = i + batch.offset;
                    if (batch.primaryId[i * 2] == 0 && batch.alignmentCount[i * 2] != 0) {
                        rc = RC(rcApp, rcTable, rcWriting, rcConstraint, rcViolated);
                        // WARNING
                        (void)PLOGERR(logLevel, (logLevel, rc, "Spot id $(id) read 1 never had a primary alignment", "id=%lx", batch.keys[i]));
                    }
                    bool is_unmated = batch.unmated[i];
                    if (!is_unmated && batch.primaryId[i * 2 + 1] == 0 && batch.alignmentCount[i * 2 + 1] != 0) {
                        rc = RC(rcApp, rcTable, rcWriting, rcConstraint, rcViolated);
                        // WARNING
                        (void)PLOGERR(logLevel, (logLevel, rc, "Spot id $(id) read 2 never had a primary alignment", "id=%lx", batch.keys[i]));
                    }
                    if (rc != 0 && logLevel == klogErr) {
                        exit_on_error = true;
                        break;
                    }
                    rc = SequenceUpdateAlignData(seq, i_row, is_unmated ? 1 : 2, &batch.primaryId[i * 2], &batch.alignmentCount[i * 2]);
                    if (rc) {
                        exit_on_error = true;
                        // FATAL ERROR, VDB I/O ERROR
                        (void)LOGERR(klogErr, rc, "Failed updating Alignment data in sequence table");
                        break;
                    }
                }
                //spdlog::info("Finished updating batch {} in {:3} sec", batch.keys.size(), sw);

            } else if (batches_updated >= expected_batches) {
                (void)PLOGMSG(klogInfo, (klogInfo, "batches_updated >= expected_batches $(NR)", "NR=%lu", batches_updated));
                break;
            }
        }
    });

    vector<uint64_t> keys;
    keys.reserve(BUFFER_SIZE);
    (void)PLOGMSG(klogInfo, (klogInfo, "BUFFER_SIZE $(b)", "b=%lu", BUFFER_SIZE));

    for (row = 1; row <= ctx->spotId; ++row) {
        rc = SequenceReadKey(seq, row, &keyId);
        if (rc) {
            // FATAL ERROR, VDB I/O ERROR
            (void)PLOGERR(klogErr, (klogErr, rc, "Failed to get key for row $(row)", "row=%u", (unsigned)row));
            break;
        }
        if (G.mode != mode_Remap) {
            auto spotId = ctx->m_spot_id_buffer[uint32_t(keyId >> GROUPID_SHIFT)].get(keyId & KEYID_MASK);
            if (row != spotId) {
                //if (row != metadata->get<u64_t>(metadata_t::e_spotId).get_no_check(local_row_id)) {
                //  auto spotId = metadata->get<u64_t>(metadata_t::e_spotId).get_no_check(local_row_id);
                rc = RC(rcApp, rcTable, rcWriting, rcData, rcUnexpected);
                // FATAL ERROR, INTERNAL CONSISTENCY ERROR
                (void)PLOGMSG(klogErr, (klogErr, "Unexpected spot id $(spotId) for row $(row), index $(idx)", "spotId=%u,row=%u,idx=%u", (unsigned)spotId, (unsigned)row, (unsigned)keyId));
                break;
            }
        }
        keys.push_back(keyId);
        if (keys.size() == BUFFER_SIZE) {
            key_batch_t batch;
            batch.offset = row_offset;
            row_offset += keys.size();
            batch.keys = std::move(keys);
            keys.clear();
            keys.reserve(BUFFER_SIZE);
            (void)PLOGMSG(klogInfo, (klogInfo, "enqueue row $(r)", "r=%lu", row));
            while (gather_queue.try_enqueue(std::move(batch)) == false) {
                if (exit_on_error)
                    break;
            };
            ++batches_processed;
            (void)PLOGMSG(klogInfo, (klogInfo, "enqueue batches_processed $(NR)", "NR=%lu", batches_processed));
            if (exit_on_error)
                break;
            KLoadProgressbar_Process(ctx->progress[ctx->pass - 1], 1, false);
        }
    }

    if (!keys.empty() && exit_on_error == false) {
        key_batch_t batch;
        batch.offset = row_offset;
        row_offset += keys.size();
        batch.keys = std::move(keys);
        keys.clear();
        while (gather_queue.try_enqueue(std::move(batch)) == false) {
            if (exit_on_error)
                break;
        };
        ++batches_processed;
    }

    // once done, enqueue an empty batch to signal that there will be no more batches
    gather_queue.enqueue( key_batch_t() );

    (void)LOGMSG(klogInfo, "enqueue done");

    //while (gather_queue.peek() != nullptr)
    //   std::this_thread::sleep_for(std::chrono::milliseconds(100));
    assert(gather_task.valid());
    queue_done = true;
    gather_task.get();

    assert(update_task.valid());
    update_task.get();

    assert(exit_on_error || num_gathered == num_updated);
    assert(exit_on_error || batches_processed == expected_batches);
    assert(exit_on_error || batches_gathered == expected_batches);
    assert(exit_on_error || batches_updated == expected_batches);

    spdlog::info("Gathered: {:L}, updated: {:L}", num_gathered, num_updated);
    spdlog::info("Queued: {:L}, Dequeued: {:L}", batches_gathered, batches_updated);
    spdlog::info("Align Info: {:.3} sec, memory: {:L}", sw, getCurrentRSS());
    return rc;
}


static rc_t AlignmentUpdateSpotInfo(context_t *ctx, Alignment *align)
{
    spdlog::stopwatch sw;
    rc_t rc;
    uint64_t keyId;
    ++ctx->pass;
    KLoadProgressbar_Append(ctx->progress[ctx->pass - 1], ctx->alignCount);
    rc = AlignmentStartUpdatingSpotIds(align);
    if (ctx->m_spot_id_buffer.empty())
        ctx->extract_spotid();

    while (rc == 0 && (rc = Quitting()) == 0) {
        rc = AlignmentGetSpotKey(align, &keyId);
        if (rc) {
            if (GetRCObject(rc) == rcRow && GetRCState(rc) == rcNotFound)
                rc = 0;
            break;
        }
        uint32_t group_id = keyId >> GROUPID_SHIFT;
        auto row_id = keyId & KEYID_MASK;
        auto spotId = ctx->m_spot_id_buffer[group_id].get(row_id);
        if (spotId == 0) {
            rc = RC(rcApp, rcTable, rcWriting, rcConstraint, rcViolated);
            // FATAL ERROR, DATA ERROR, CAN BE FIXED WITH COMMAND LINE OPTIONS
            (void)PLOGERR(klogErr, (klogErr, rc, "Spot '$(id)' was never assigned a spot id, probably has no primary alignments", "id=%lx", keyId));
            break;
        }
        rc = AlignmentWriteSpotId(align, spotId);
        KLoadProgressbar_Process(ctx->progress[ctx->pass - 1], 1, false);
    }
    spdlog::info("Align Update Spot Info: {:.3} sec", sw);
    return rc;
}
#endif

static rc_t ArchiveBAM(VDBManager *mgr, VDatabase *db,
                       unsigned bamFiles, char const *bamFile[],
                       unsigned seqFiles, char const *seqFile[],
                       bool *has_alignments,
                       bool continuing)
{
    std::locale::global(std::locale("en_US.UTF-8")); // enable comma as thousand separator
    if (G.hasExtraLogging) {
        auto logger = spdlog::stderr_logger_mt("stderr"); // send log to stderr
        spdlog::set_default_logger(logger);
    } else {
        auto logger = spdlog::null_logger_mt("null"); // send log to stderr
        spdlog::set_default_logger(logger);
    }
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v"); // default logging pattern (datetime, error level, error text)
    spdlog::info("SIMD code = {}", bm::simd_version());
    spdlog::info("Num threads  = {}", G.numThreads);
    spdlog::info("Search batch size = {}", G.searchBatchSize);

    rc_t rc = 0;
    rc_t rc2;
    Reference ref;
    Sequence seq;
    Alignment *align;
    static context_t *ctx = &GlobalContext;

    rc = CollectReferences(ctx, bamFiles, bamFile);
    if (rc)
        return rc;

    bool has_sequences = false;
    unsigned i;

    *has_alignments = false;
    rc = ReferenceInit(&ref, mgr, db);
    if (rc)
        return rc;

    if (G.onlyVerifyReferences) {
        for (i = 0; i < bamFiles && rc == 0; ++i) {
            rc = ProcessBAM(bamFile[i], NULL, db, &ref, NULL, NULL, NULL, NULL);
        }
        ReferenceWhack(&ref, false);
        return rc;
    }
    SequenceInit(&seq, db);
    align = AlignmentMake(db);
    ctx->m_inputSize = 0;
    for (i = 0; i < bamFiles && rc == 0; ++i) {
        if (strcmp(bamFile[i], "/dev/stdin") == 0) {
            ctx->m_inputSize = 0;
            break;
        }
        FILE *in = fopen(bamFile[i], "rb");
        if (in) {
            fseek(in, 0, SEEK_END);
            ctx->m_inputSize += ftell(in);
            fclose(in);
        }
    }
    spdlog::info("Number of files: {},  Total size: {:L}", bamFiles, ctx->m_inputSize);
    ctx->m_calcBatchSize = ctx->m_inputSize > 0;

    rc = SetupContext(ctx, bamFiles + seqFiles);
    if (rc)
        return rc;
    ctx->pass = 1;
    spdlog::info("ArchiveBAM start, memory: {:L}", getCurrentRSS());

    for (i = 0; i < bamFiles && rc == 0; ++i) {
        bool this_has_alignments = false;
        bool this_has_sequences = false;

        rc = ProcessBAM(bamFile[i], ctx, db, &ref, &seq, align, &this_has_alignments, &this_has_sequences);
        *has_alignments |= this_has_alignments;
        has_sequences |= this_has_sequences;
    }
    for (i = 0; i < seqFiles && rc == 0; ++i) {
        bool this_has_alignments = false;
        bool this_has_sequences = false;

        rc = ProcessBAM(seqFile[i], ctx, db, &ref, &seq, align, &this_has_alignments, &this_has_sequences);
        *has_alignments |= this_has_alignments;
        has_sequences |= this_has_sequences;
    }
    spdlog::info("Processing done, memory: {:L}, spotCount: {:L}", getCurrentRSS(), ctx->spotId);
    if (!continuing) {
        ctx->mTelemetry["spot-count"] = ctx->spotId;
        ctx->mTelemetry["read-count"] = ctx->readCount;
        ctx->mTelemetry["spot-size-kb"] = ctx->m_SpotSize/1024;
        ctx->mTelemetry["reference-size_kb"] = ctx->m_ReferenceSize/1024;
        ctx->mTelemetry["reference-count"] = ctx->m_ReferenceCount;
        ctx->mTelemetry["input-size-kb"] = ctx->m_inputSize/1024;
        ctx->mTelemetry["number-read-groups"] = ctx->m_read_groups.size();

        if (ctx->has_far_reads)
            ctx->mTelemetry["has-far-reads"] = 1;
        if (*has_alignments == false)
            ctx->mTelemetry["is-unaligned"] = 1;
        if (ref.out_of_order)
            ctx->mTelemetry["is-unsorted"] = 1;

        ctx->release_search_memory();
        // Clear the metadata columns that we don't need anymore
        ctx->clear_column<bit_t>(metadata_t::e_unaligned_1);
        ctx->clear_column<bit_t>(metadata_t::e_unaligned_2);
        ctx->clear_column<bit_t>(metadata_t::e_hardclipped);
        ctx->clear_column<bit_t>(metadata_t::e_primary_is_set);

        spdlog::info("Spot assembly memory release, memory: {:L}", getCurrentRSS());
    }

    if (has_sequences) {
        if (rc == 0 && (rc = Quitting()) == 0) {
            if (G.mode == mode_Archive) {
                (void)LOGMSG(klogInfo, "Writing unpaired sequences");
                rc = WriteSoloFragments(ctx, &seq);
                ContextReleaseMemBank(ctx);
            }
            if (rc == 0) {
                rc = SequenceDoneWriting(&seq);
                if (rc == 0) {
                    (void)LOGMSG(klogInfo, "Updating sequence alignment info");
                    rc = SequenceUpdateAlignInfo(ctx, &seq);
                }
            }
        }
    } else {

        // Clear the metadata columns that we don't need anymore
        ctx->clear_column<u64_t>(metadata_t::e_primaryId1);
        ctx->clear_column<u64_t>(metadata_t::e_primaryId2);
        ctx->clear_column<u32_t>(metadata_t::e_fragmentId);

        ctx->clear_column<u16_t>(metadata_t::e_fragment_len1);
        ctx->clear_column<u16_t>(metadata_t::e_fragment_len2);
        ctx->clear_column<u16_t>(metadata_t::e_alignmentCount1);
        ctx->clear_column<u16_t>(metadata_t::e_alignmentCount2);
        ctx->clear_column<u16_t>(metadata_t::e_platform);
        ctx->clear_column<bit_t>(metadata_t::e_pcr_dup);

    }

    if (*has_alignments && rc == 0 && (rc = Quitting()) == 0) {
        (void)LOGMSG(klogInfo, "Writing alignment spot ids");
        rc = AlignmentUpdateSpotInfo(ctx, align);
    }
    spdlog::info("Before Whacking, memory: {:L}", getCurrentRSS());
    for (auto& b : ctx->m_spot_id_buffer) {
        b.values.clear();
        b.values.shrink_to_fit();
        b.ext.clear();
        b.ext.shrink_to_fit();
    }
    ctx->m_read_groups.clear();
    ctx->m_executor.reset(nullptr);

    rc2 = AlignmentWhack(align, *has_alignments && rc == 0 && (rc = Quitting()) == 0);
    if (rc == 0)
        rc = rc2;

    rc2 = ReferenceWhack(&ref, *has_alignments && rc == 0 && (rc = Quitting()) == 0);
    if (rc == 0)
        rc = rc2;

    SequenceWhack(&seq, rc == 0);

    ContextRelease(ctx, continuing);

    if (rc == 0) {
        (void)LOGMSG(klogInfo, "Successfully loaded all files");
    }

    spdlog::info("ArchiveBAM, memory: {:L}", getCurrentRSS());
    
    return rc;
}

rc_t WriteLoaderSignature(KMetadata *meta, char const progName[])
{
    KMDataNode *node;
    rc_t rc = KMetadataOpenNodeUpdate(meta, &node, "/");

    if (rc == 0) {
        rc = KLoaderMeta_Write(node, progName, __DATE__, "BAM", KAppVersion());
        KMDataNodeRelease(node);
    }
    if (rc) {
        (void)LOGERR(klogErr, rc, "Cannot update loader meta");
    }
    return rc;
}

rc_t OpenPath(char const path[], KDirectory **dir)
{
    KDirectory *p;
    rc_t rc = KDirectoryNativeDir(&p);

    if (rc == 0) {
        rc = KDirectoryOpenDirUpdate(p, dir, false, "%s", path);
        KDirectoryRelease(p);
    }
    return rc;
}

static
rc_t ConvertDatabaseToUnmapped(VDatabase *db)
{
    VTable* tbl;
    rc_t rc = VDatabaseOpenTableUpdate(db, &tbl, "SEQUENCE");
    if (rc == 0)
    {
        VTableRenameColumn(tbl, false, "CMP_ALTREAD", "ALTREAD");
        VTableRenameColumn(tbl, false, "CMP_READ", "READ");
        VTableRenameColumn(tbl, false, "CMP_ALTCSREAD", "ALTCSREAD");
        VTableRenameColumn(tbl, false, "CMP_CSREAD", "CSREAD");
        rc = VTableRelease(tbl);
    }
    return rc;
}

rc_t run(char const progName[],
         unsigned bamFiles, char const *bamFile[],
         unsigned seqFiles, char const *seqFile[],
         bool continuing)
{
    VDBManager *mgr;
    rc_t rc;
    rc_t rc2;
    char const *db_type = G.expectUnsorted ? "NCBI:align:db:alignment_unsorted" : "NCBI:align:db:alignment_sorted";

    rc = VDBManagerMakeUpdate(&mgr, NULL);
    if (rc) {
        (void)LOGERR (klogErr, rc, "failed to create VDB Manager!");
    }
    else {
        bool has_alignments = false;

        /* VDBManagerDisableFlushThread(mgr); */
        rc = VDBManagerDisablePagemapThread(mgr);
        if (rc == 0)
        {
            if (G.onlyVerifyReferences) {
                rc = ArchiveBAM(mgr, NULL, bamFiles, bamFile, 0, NULL, &has_alignments, continuing);
            }
            else {
                VSchema *schema;

                rc = VDBManagerMakeSchema(mgr, &schema);
                if (rc) {
                    (void)LOGERR (klogErr, rc, "failed to create schema");
                }
                else {
                    (void)(rc = VSchemaAddIncludePath(schema, "%s", G.schemaIncludePath));
                    rc = VSchemaParseFile(schema, "%s", G.schemaPath);
                    if (rc) {
                        (void)PLOGERR(klogErr, (klogErr, rc, "failed to parse schema file $(file)", "file=%s", G.schemaPath));
                    }
                    else {
                        VDatabase *db;

                        rc = VDBManagerCreateDB(mgr, &db, schema, db_type,
                                                kcmInit + kcmMD5, "%s", G.outpath);
                        VSchemaRelease(schema);
                        if (rc == 0) {
                            rc = ArchiveBAM(mgr, db, bamFiles, bamFile, seqFiles, seqFile, &has_alignments, continuing);
                            if (rc == 0)
                                PrintChangeReport();
                            if (rc == 0 && !has_alignments) {
                                rc = ConvertDatabaseToUnmapped(db);
                            }
                            else if (rc == 0 && lmc != NULL) {
                                VTable *tbl = NULL;
                                KTable *ktbl = NULL;
                                KMetadata *meta = NULL;
                                KMDataNode *node = NULL;

                                VDatabaseOpenTableUpdate(db, &tbl, "REFERENCE");
                                VTableOpenKTableUpdate(tbl, &ktbl);
                                VTableRelease(tbl);

                                KTableOpenMetadataUpdate(ktbl, &meta);
                                KTableRelease(ktbl);

                                KMetadataOpenNodeUpdate(meta, &node, "LOW_MATCH_COUNT");
                                KMetadataRelease(meta);

                                RecordLowMatchCounts(node);

                                KMDataNodeRelease(node);

                                LowMatchCounterFree(lmc);
                                lmc = NULL;
                            }
                            VDatabaseRelease(db);

                            if (rc == 0 && G.globalMode == mode_Remap && !continuing) {
                                VTable *tbl = NULL;

                                VDBManagerOpenDBUpdate(mgr, &db, NULL, G.firstOut);
                                VDatabaseOpenTableUpdate(db, &tbl, "SEQUENCE");
                                VDatabaseRelease(db);
                                VTableDropColumn(tbl, "TMP_KEY_ID");
                                VTableDropColumn(tbl, "READ");
                                VTableDropColumn(tbl, "ALTREAD");
                                VTableRelease(tbl);
                            }

                            if (rc == 0) {
                                KMetadata *meta = NULL;

                                {
                                    KDBManager *kmgr = NULL;

                                    rc = VDBManagerOpenKDBManagerUpdate(mgr, &kmgr);
                                    if (rc == 0) {
                                        KDatabase *kdb;

                                        rc = KDBManagerOpenDBUpdate(kmgr, &kdb, "%s", G.outpath);
                                        if (rc == 0) {
                                            rc = KDatabaseOpenMetadataUpdate(kdb, &meta);
                                            KDatabaseRelease(kdb);
                                        }
                                        KDBManagerRelease(kmgr);
                                    }
                                }
                                if (rc == 0) {
                                    rc = WriteLoaderSignature(meta, progName);
                                    if (rc == 0) {
                                        KMDataNode *changes = NULL;

                                        rc = KMetadataOpenNodeUpdate(meta, &changes, "CHANGES");
                                        if (rc == 0)
                                            RecordChanges(changes, "CHANGE");
                                        KMDataNodeRelease(changes);
                                    }
                                    KMetadataRelease(meta);
                                }
                            }
                        }
                    }
                }
            }
        }
        rc2 = VDBManagerRelease(mgr);
        if (rc2)
            (void)LOGERR(klogWarn, rc2, "Failed to release VDB Manager");
        if (rc == 0)
            rc = rc2;
    }

    if (G.telemetryPath) try {
        GlobalContext.mTelemetry["max-mem-kb"] = getCurrentRSS();
        std::ofstream f(G.telemetryPath, std::ios::out);
        f << GlobalContext.mTelemetry.dump(4, ' ', true) << endl;
    } catch(std::exception const& e) {
        (void)PLOGMSG(klogErr, (klogWarn, "Failed to write telemetry", "%s", e.what()));
    }

    return rc;
}
