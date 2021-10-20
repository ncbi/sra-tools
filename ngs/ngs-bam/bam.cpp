/* ===========================================================================
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
 */

#include <iostream>
#include <fstream>
#include "bam.hpp"

#define MAX_INDEX_SEQ_LEN ((1u << 29) - 1)
#define MAX_BIN  (37449u)
#define NUMINTV ((MAX_INDEX_SEQ_LEN + 1) >> 14)

class RefIndex {
public:
    BAMFilePosType off_beg, off_end;
    uint64_t n_mapped, n_unmapped;
    BAMFilePosTypeList interval;
    std::vector<BAMFilePosTypeList> bins;
    
private:
    static void CopyWhereLess(BAMFilePosTypeList &dst,
                              BAMFilePosTypeList const &src,
                              BAMFilePosType const maxpos)
    {
        if (maxpos.hasValue()) {
            for (unsigned i = 0; i < src.size(); ++i) {
                BAMFilePosType const pos = src[i];
                
                if (pos < maxpos)
                    dst.push_back(pos);
            }
        }
        else {
            for (unsigned i = 0; i < src.size(); ++i)
                dst.push_back(src[i]);
        }
    }
public:
    char const *LoadIndexIntervals(char const *data, char const *const endp)
    {
        if (data + 4 > endp)
            throw std::runtime_error("insufficient data to load index interval count");
        int32_t const n = LE2Host<int32_t>(data); data += 4;

        char const *const next = data + 8 * n;
        if (next > endp)
            throw std::runtime_error("insufficient data to load index intervals");

        BAMFilePosType last(0);
        
        interval.resize(n);
        for (unsigned i = 0; i < n; ++i) {
            BAMFilePosType const intvl = LE2Host<BAMFilePosType>(data); data += 8;
            
            interval[i] = (intvl == last) ? BAMFilePosType(0) : intvl;
            last = intvl;
        }
        while (interval.size() > 0 && !interval.back().hasValue())
            interval.pop_back();
        
        return next;
    }
    char const *LoadIndexBins(char const *data, char const *const endp)
    {
        if (data + 4 > endp)
            throw std::runtime_error("insufficient data to load index bin count");
        int32_t const n_bin = LE2Host<int32_t>(data); data += 4;
        
        bins.resize(MAX_BIN);
        for (int i = 0; i < n_bin; ++i) {
            if (data + 8 > endp)
                throw std::runtime_error("insufficient data to load index bin size");
            
            uint32_t const bin     = LE2Host<uint32_t>(data + 0);
             int32_t const n_chunk = LE2Host< int32_t>(data + 4);

            data += 8;
            if (data + n_chunk * 16 > endp)
                throw std::runtime_error("insufficient data to load index bin chunks");
            
            if (bin == MAX_BIN && n_chunk == 2) {
                // special doodad
                off_beg    = LE2Host<BAMFilePosType>(data); data += 8;
                off_end    = LE2Host<BAMFilePosType>(data); data += 8;
                n_mapped   = LE2Host<uint64_t>(data); data += 8;
                n_unmapped = LE2Host<uint64_t>(data); data += 8;
            }
            else if (bin < MAX_BIN) {
                BAMFilePosTypeList &this_bin = bins[bin];
                
                this_bin.resize(n_chunk);
                
                for (unsigned k = 0; k < n_chunk; ++k) {
                    BAMFilePosType const beg = LE2Host<BAMFilePosType>(data); data += 8;
                    BAMFilePosType const end = LE2Host<BAMFilePosType>(data); data += 8;
                    
                    (void)end;
                    this_bin[k] = beg;
                }
            }
            else
                data += 16 * n_chunk;
        }
        return data;
    }
    RefIndex()
    {}
    BAMFilePosTypeList slice(unsigned const beg, unsigned const end) const
    {
        unsigned const first[] = { 1, 9, 73, 585, 4681 };
        unsigned const cnt = end - 1 - beg;
        unsigned const maxintvl = (end >> 14) + 1;
        BAMFilePosType const maxpos = maxintvl < interval.size() ? interval[maxintvl] : off_end;
        unsigned int_beg[5], int_cnt[5];
        BAMFilePosTypeList rslt;
        
        for (unsigned i = 0; i < 5; ++i) {
            unsigned const shift = 14 + 3 * (4 - i);
            
            int_beg[i] = (beg >> shift);
        }
        for (unsigned i = 0; i < 5; ++i) {
            unsigned const shift = 14 + 3 * (4 - i);
            
            int_cnt[i] = (cnt >> shift) + 1;
        }
        CopyWhereLess(rslt, bins[0], maxpos);
        for (unsigned i = 0; i < 5; ++i) {
            unsigned const beg = int_beg[i] + first[i];
            unsigned const N = int_cnt[i];
            
            for (unsigned j = 0; j < N; ++j) {
                CopyWhereLess(rslt, bins[beg + j], maxpos);
            }
        }
        std::sort(rslt.begin(), rslt.end());
        return rslt;
    }
};

size_t HeaderRefInfo::LoadIndex(char const data[], char const *const endp)
{
    RefIndex *i = new RefIndex();
    try {
        char const *const next1 = i->LoadIndexBins(data, endp);
        char const *const next2 = i->LoadIndexIntervals(next1, endp);
        
        index = i;
        
        return next2 - data;
    }
    catch (...) {
        delete i;
        throw;
    }
}

void HeaderRefInfo::DropIndex()
{
    if (index) {
        delete index;
        index = 0;
    }
}

BAMFilePosTypeList HeaderRefInfo::slice(unsigned const beg, unsigned const end) const {
    return index ? index->slice(beg, end) : BAMFilePosTypeList();
}

void BAMFile::DumpSAM(std::ostream &oss, BAMRecord const &rec) const
{
    unsigned const seqlen   = rec.l_seq();
    std::string const RNAME = rec.isSelfMapped() ? getRefInfo(rec.refID()).getName() : "*";
    std::string const RNEXT = rec.isMateMapped() ? getRefInfo(rec.next_refID()).getName() : "*";
    int const POS   		= rec.isSelfMapped() ? (rec.pos() + 1) : 0;
    int const PNEXT 		= rec.isMateMapped() ? (rec.next_pos() + 1) : 0;

    oss	        << rec.readname()
 		<< '\t' << rec.flag()
    	<< '\t' << RNAME
    	<< '\t' << POS
        << '\t' << int(rec.mq())
    	<< '\t';
    
    if (rec.isSelfMapped()) {
        unsigned const N = rec.nc();
        
        for (unsigned i = 0; i < N; ++i) {
            uint32_t const cv = rec.cigar(i);
            int const op = cv & 0x0F;
            int len = cv >> 4;
            char buf[16];
            char *cur = buf + sizeof(buf);
            
            *--cur = '\0';
            *--cur = "MIDNSHP=X???????"[op];
            do {
                int const digit = len % 10;
                
                *--cur = digit + '0';
                len /= 10;
            } while (len > 0);
            oss << cur;
        }
    }
    else
        oss << '*';
    
	oss	<< '\t' << RNEXT
    	<< '\t' << PNEXT
        << '\t' << rec.tlen()
    	<< '\t';
    
    if (seqlen > 0) {
        for (int i = 0; i < seqlen; ++i)
            oss << rec.seq(i);
        oss << '\t';
        
        bool allFF = true;
        uint8_t const *const q = rec.qual();
        for (unsigned i = 0; i < seqlen; ++i) {
            uint8_t const qv = q[i];
            
            if (qv != 0xFF) {
                allFF = false;
                break;
            }
        }
        if (allFF)
            oss << '*';
        else {
            for (unsigned i = 0; i < seqlen; ++i) {
                uint8_t const qv = q[i];
                int const offset = ((int)qv) + '!';
                int const out = offset < '~' ? offset : '~';
                
                oss << char(out);
            }
        }
    }
    else
        oss << "*\t*";
    
    for (BAMRecord::OptionalField::const_iterator i = rec.begin(); i != rec.end(); ++i) {
        std::string const tag = std::string(i->getTag(), 2);
        int const elems = i->getElementCount();
        char const type = i->getValueType();
        char const *raw = i->getRawValue();
        
        oss << '\t' << tag << ':' << (elems == 1 ? type : 'B') << ':';
        if (type == 'Z' || type == 'H')
            oss << raw;
        else {
            if (elems > 1)
                oss << type;
            for (int j = 0; j < elems; ++j) {
                if (elems > 1)
                    oss << ',';
                switch (type) {
                    case 'A':
                        oss << *raw;
                        ++raw;
                        break;
                    case 'C':
                        oss << unsigned(*((uint8_t const *)raw));
                        ++raw;
                        break;
                    case 'c':
                        oss << int(*((int8_t const *)raw));
                        ++raw;
                        break;
                    case 'S':
                        oss << unsigned(LE2Host<uint16_t>(raw));
                        raw += 2;
                        break;
                    case 's':
                        oss << int(LE2Host<int16_t>(raw));
                        raw += 2;
                        break;
                    case 'F':
                        oss << float(LE2Host<float>(raw));
                        raw += 4;
                        break;
                    case 'I':
                        oss << unsigned(LE2Host<uint32_t>(raw));
                        raw += 4;
                        break;
                    case 'i':
                        oss << int(LE2Host<int32_t>(raw));
                        raw += 4;
                        break;
                }
            }
        }
    }
}

unsigned BAMFile::FillBuffer(int const n) {
    size_t const nwant = n * IO_BLK_SIZE;
    char *const dst = (char *)(iobuffer + 2 * IO_BLK_SIZE - nwant);
#if USE_STDIO
    size_t const nread = feof(file) ? 0 : fread(dst, 1, nwant, file);
#else
    size_t const nread = file.eof() ? 0 : file.read(dst, nwant).gcount();
    
    if (nread == 0 && !file.eof())
        throw std::runtime_error("read failed");
#endif
    return (unsigned)nread;
}

void BAMFile::ReadZlib(void) {
    zs.next_out  = bambuffer;
    zs.avail_out = sizeof(bambuffer);
    zs.total_out = 0;
    bam_cur      = 0;
    
    if (zs.avail_in == 0) {
    FILL_BUFFER:
#if USE_STDIO
        cpos = ftell(file);
#else
        cpos = file.tellg();
#endif
        zs.avail_in = FillBuffer(2);
        zs.next_in  = iobuffer;
        if (zs.avail_in == 0) /* EOF */
            return;
    }
    
    bpos = cpos + (zs.next_in - iobuffer);
    int const zrc = inflate(&zs, Z_FINISH);
    
    if (zrc == Z_STREAM_END) {
        /* inflateReset clobbers these value but we want them */
        uLong const total_out = zs.total_out;
        uLong const total_in  = zs.total_in;
        
        int const zrc = inflateReset(&zs);
        if (zrc != Z_OK)
            throw std::logic_error("inflateReset didn't return Z_OK");
        
        zs.total_out = total_out;
        zs.total_in  = total_in;

#if 0
        std::cerr << std::dec << "total_in: " << total_in << "; avail_in: " << zs.avail_in << "; IO_BLK_SIZE: " << IO_BLK_SIZE << std::endl;
#endif
        
        if (total_in >= IO_BLK_SIZE && total_in + zs.avail_in == 2 * IO_BLK_SIZE)
        {
            memmove(iobuffer, &iobuffer[sizeof(iobuffer)/2], sizeof(iobuffer)/2);
            cpos += sizeof(iobuffer)/2;
            zs.next_in  -= sizeof(iobuffer)/2;
            zs.avail_in += FillBuffer(1);
        }
        
        return;
    }
    if (zrc != Z_OK && zrc != Z_BUF_ERROR)
        throw std::runtime_error("decompression failed");
    
    if (zs.avail_in == 0)
        goto FILL_BUFFER;
    
    throw std::runtime_error("zs.avail_in != 0");
}

size_t BAMFile::ReadN(size_t N, void *Dst) {
    uint8_t *const dst = reinterpret_cast<uint8_t *>(Dst);
    size_t n = 0;
    
    while (n < N) {
        size_t const avail_out = N - n;
        size_t const avail_in = zs.total_out - bam_cur;
        
        if (avail_in) {
            size_t const copy = avail_out < avail_in ? avail_out : avail_in;
            
            memmove(dst + n, bambuffer + bam_cur, copy);
            bam_cur += copy;
            if (bam_cur == zs.total_out)
                bam_cur = zs.total_out = 0;
            
            n += copy;
            if (n == N)
                break;
        }
        ReadZlib();
        if (zs.total_out == 0)
            break;
    }
    return n;
}

size_t BAMFile::SkipN(size_t N) {
    size_t n = 0;
    
    while (n < N) {
        size_t const avail_out = N - n;
        size_t const avail_in = zs.total_out - bam_cur;
        
        if (avail_in) {
            size_t const copy = avail_out < avail_in ? avail_out : avail_in;
            
            bam_cur += copy;
            n += copy;
            if (n == N)
                break;
        }
        ReadZlib();
        if (zs.total_out == 0)
            break;
    }
    return n;
}

void BAMFile::Seek(size_t const new_bpos, unsigned const new_bam_cur) {
    unsigned const c_offset = new_bpos % IO_BLK_SIZE;
    size_t const new_cpos = new_bpos - c_offset;
    
#if 0
    std::cerr << "seek to " << std::hex << new_bpos << "|" << new_bam_cur << std::endl;
#endif
    
#if USE_STDIO
    if (fseek(file, new_cpos, SEEK_SET))
        throw std::runtime_error("position is invalid");
    cpos = ftell(file);
#else
    file.seekg(new_cpos);
    cpos = file.tellg();
#endif
    zs.avail_in = FillBuffer(2);
    if (zs.avail_in > c_offset) {
        zs.avail_in -= c_offset;
        zs.next_in = iobuffer + c_offset;
        ReadZlib();
        if (zs.total_out > new_bam_cur) {
            bam_cur = new_bam_cur;
            return;
        }
    }
    throw std::runtime_error("position is invalid");
}

template <typename T>
bool BAMFile::Read(size_t count, T *dst) {
    size_t const nwant = count * sizeof(T);
    size_t const nread = ReadN(nwant, reinterpret_cast<void *>(dst));
    
    return nwant == nread;
}

int32_t BAMFile::ReadI32() {
    int32_t value;
    
    if (Read(1, &value))
        return LE2Host<int32_t>(&value);
    throw std::runtime_error("insufficient data while reading bam file");
}

bool BAMFile::ReadI32(int32_t &rslt) {
    int32_t value;
    
    if (Read(1, &value)) {
        rslt = LE2Host<int32_t>(&value);
        return true;
    }
    return false;
}

void BAMFile::InflateInit(void) {
    memset(&zs, 0, sizeof(zs));
    
    int const zrc = inflateInit2(&zs, MAX_WBITS + 16);
    switch (zrc) {
        case Z_OK:
            break;
        case Z_MEM_ERROR:
            throw std::bad_alloc();
            break;
        case Z_VERSION_ERROR:
            throw std::runtime_error(std::string("zlib version is not compatible; need version " ZLIB_VERSION " but have ") + zlibVersion());
            break;
        case Z_STREAM_ERROR:
        default:
            throw std::invalid_argument(zs.msg ? zs.msg : "unknown");
            break;
    }
}

void BAMFile::CheckHeaderSignature(void) {
    static char const sig[] = "BAM\1";
    char actual[4];
    
    if (!Read(4, actual) || memcmp(actual, sig, 4) != 0)
        throw std::runtime_error("Not a BAM file");
}

void BAMFile::ReadHeader(void) {
    CheckHeaderSignature();
    
    {
        int32_t const l_text = ReadI32();
        if (l_text < 0)
            throw std::runtime_error("header text length < 0");
        
        char *const text = new char[l_text];
        if (!Read(l_text, text))
            throw std::runtime_error("file is truncated");
        
        headerText = text;
        delete [] text;
    }
    int32_t const n_ref = ReadI32();
    if (n_ref < 0)
        throw std::runtime_error("header reference count < 0");
    
    references.reserve(n_ref);
    
    for (int i = 0; i < n_ref; ++i) {
        int32_t const l_name = ReadI32();
        
        if (l_name < 0)
            throw std::runtime_error("header reference name length < 0");
        
        char *const name = new char[l_name];
        if (!Read(l_name, name))
            throw std::runtime_error("file is truncated");
        
        int32_t const l_ref = ReadI32();
        if (l_ref < 0)
            throw std::runtime_error("header reference length < 0");
        
        references.push_back(HeaderRefInfo(name, l_ref));
        referencesByName[name] = i;
        
        delete [] name;
    }
}

void BAMFile::LoadIndexData(size_t const fsize, char const data[]) {
    char const *const endp = data + fsize;
    
    if (memcmp(data, "BAI\1", 4) != 0)
        return;
    
    int32_t const n_ref = LE2Host<int32_t>(data + 4);
    if (n_ref != references.size())
        return;
    
    size_t offset = 8;
    
    for (int i = 0; i < n_ref; ++i) {
        size_t const size = references[i].LoadIndex(data + offset, endp);
        
        offset += size;
        if (size == 0) {
            std::cerr << "failed to load index #" << (i + 1) << std::endl;
            for (int j = 0; j < i; ++j)
                references[j].DropIndex();
            return;
        }
    }
}

void BAMFile::LoadIndex(std::string const &filepath) {
    char *data;
    size_t fsize;
    {
        std::string const idxpath(filepath+".bai");
        std::ifstream ifile;
        
        ifile.open(idxpath.c_str(), std::ifstream::in | std::ifstream::binary);
        if (!ifile.is_open())
            return;
        
        std::filebuf *const buf = ifile.rdbuf();
        fsize = buf->pubseekoff(0, ifile.end, ifile.in);
        
        if (fsize < 8)
            return;
        
        buf->pubseekpos(0, ifile.in);
        
        data = new char[fsize];
        buf->sgetn(data, fsize);
    }
    LoadIndexData(fsize, data);
    delete [] data;
}

BAMFile::BAMFile(std::string const &filepath)
{
    InflateInit();
    
#if USE_STDIO
    file = fopen(filepath.c_str(), "rb");
    if (file == NULL)
        throw std::runtime_error(std::string("The file '")+filepath+"' could not be opened");
#else
    file.open(filepath.c_str(), std::ifstream::in | std::ifstream::binary);
    if (!file.is_open())
        throw std::runtime_error(std::string("The file '")+filepath+"' could not be opened");
#endif

    ReadHeader();
    first_bpos = cpos + (zs.next_in - iobuffer);
    first_bam_cur = bam_cur;
    LoadIndex(filepath);
}

BAMFile::~BAMFile()
{
    inflateEnd(&zs);
}

BAMRecord const *BAMFile::Read()
{
    union aligned_BAMRecord {
        SizedRawData raw;
        BAMRecord record;
        struct {
            uint8_t align[16];
        } align;
    };
    int32_t datasize;
    
    if (!ReadI32(datasize)) // assumes cause is EOF
        return 0;
    
    if (datasize < 0)
        throw std::runtime_error("file is corrupt: record size < 0");

    uint32_t const size = (uint32_t)datasize;
    
    union aligned_BAMRecord *data = new aligned_BAMRecord[(size + sizeof(uint32_t) + sizeof(aligned_BAMRecord) - 1)/sizeof(aligned_BAMRecord)];
    data->raw.size = size;
    if (Read(size, data->raw.data))
        return &data->record;

    delete [] data;
    throw std::runtime_error("file is truncated");
}

bool BAMFile::isGoodRecord(BAMRecord const &rec)
{
    if (rec.isTooSmall())
        return false;
    
    unsigned const refs = (unsigned)references.size();
    unsigned const flag = rec.flag();
    int const self_refID = rec.refID();
    if ((flag & 0x40) != 0 && self_refID > 0 && self_refID >= refs)
        return false;

    int const mate_refID = rec.next_refID();
    if ((flag & 0x80) != 0 && mate_refID > 0 && mate_refID >= refs)
        return false;
    
    return true;
}

BAMRecordSource *BAMFile::Slice(const std::string &rname, unsigned start, unsigned last)
{
    int const refID = getReferenceIndexByName(rname);
    
    if (refID < 0)
        return new BAMRecordSource();
    
    HeaderRefInfo const &ri = getRefInfo(refID);

    if (start > 0)
        --start;
    if (last == 0)
        last = ri.length;
    
    if (!ri.index || start >= ri.length)
        return new BAMRecordSource();

    if (last > start + ri.length)
        last = start + ri.length;
    
    BAMFilePosTypeList const &index = ri.slice(start, last);
    
    if (index.size() == 0)
        return new BAMRecordSource();
    
    return new BAMFileSlice(*this, refID, start, last, index);
}
