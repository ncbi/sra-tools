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

#include <ngs-bam/ngs-bam.hpp>
#include "bam.hpp"

#include <ngs/ReadCollection.hpp>
#include <ngs/adapter/ReadCollectionItf.hpp>
#include <ngs/adapter/AlignmentItf.hpp>
#include <ngs/adapter/ReferenceItf.hpp>
#include <ngs/adapter/StringItf.hpp>

class ReadCollection : public ngs_adapt::ReadCollectionItf
{
    class Alignment;
    class AlignmentNone;
    class AlignmentSlice;
    class Reference;

    BAMFile file;
    std::string const path;         /* path used to open the BAM file       */
public:
    ReadCollection(std::string const &filepath)
    : path(filepath)
    , file(filepath)
    {};

    ngs_adapt::StringItf *getName() const;
    ngs_adapt::ReadGroupItf *getReadGroups() const;
    bool hasReadGroup(char const spec[]) const;
    ngs_adapt::ReadGroupItf *getReadGroup(char const spec[]) const;
    ngs_adapt::ReferenceItf *getReferences() const;
    bool hasReference(char const spec[]) const;
    ngs_adapt::ReferenceItf *getReference(char const spec[]) const;
    ngs_adapt::AlignmentItf *getAlignment(char const spec[]) const;
    ngs_adapt::AlignmentItf *getAlignments(bool const want_primary,
                                           bool const want_secondary) const;
    uint64_t getAlignmentCount(bool const want_primary,
                               bool const want_secondary) const;
    ngs_adapt::AlignmentItf *getAlignmentRange(uint64_t const first,
                                               uint64_t const count,
                                               bool const want_primary,
                                               bool const want_secondary ) const;
    uint64_t getReadCount(bool const want_full,
                          bool const want_partial,
                          bool const want_unaligned) const;
    ngs_adapt::ReadItf *getRead(char const spec[]) const;
    ngs_adapt::ReadItf *getReads(bool const want_full,
                                 bool const want_partial,
                                 bool const want_unaligned) const;
    ngs_adapt::ReadItf *getReadRange(uint64_t const first,
                                     uint64_t const count,
                                     bool const want_full,
                                     bool const want_partial,
                                     bool const want_unaligned) const;

    void Seek(BAMFilePosType const new_pos) {
    	file.Seek(new_pos.fpos(), new_pos.bpos());
    }
    BAMRecord const *ReadBAMRecord() {
        return file.Read();
    }
    HeaderRefInfo const &getRefInfo(unsigned const i) const {
        return file.getRefInfo(i);
    }
};

// base class for the Alignment types
// defines the default behavior of the Alignment types
// by design, this class doesn't actually do anything but throw errors
// it is the type that is returned when there can't be any alignments
// for example, if want_primary and want_secondary are both false
class ReadCollection::AlignmentNone : public ngs_adapt::AlignmentItf
{
    virtual ngs_adapt::StringItf *getCigar(bool const clipped, char const OPCODE[]) const {
        throw std::runtime_error("no rows");
    }
public:
    ngs_adapt::StringItf *getFragmentId() const {
        throw std::runtime_error("not available");
    }
    ngs_adapt::StringItf *getFragmentBases(uint64_t offset, uint64_t length) const {
        throw std::runtime_error("no rows");
    }
    ngs_adapt::StringItf *getFragmentQualities(uint64_t offset, uint64_t length) const {
        throw std::runtime_error("no rows");
    }
    ngs_adapt::StringItf *getAlignmentId() const {
        throw std::runtime_error("not available");
    }
    ngs_adapt::StringItf *getReferenceSpec() const {
        throw std::runtime_error("no rows");
    }
    int32_t getMappingQuality() const {
        throw std::runtime_error("no rows");
    }
    ngs_adapt::StringItf *getReferenceBases() const {
        throw std::runtime_error("not available");
    }
    ngs_adapt::StringItf *getReadGroup() const {
        throw std::runtime_error("no rows");
    }
    ngs_adapt::StringItf *getReadId() const {
        throw std::runtime_error("no rows");
    }
    ngs_adapt::StringItf *getClippedFragmentBases() const {
        throw std::runtime_error("not available");
    }
    ngs_adapt::StringItf *getClippedFragmentQualities() const {
        throw std::runtime_error("not available");
    }
    ngs_adapt::StringItf *getAlignedFragmentBases() const {
        throw std::runtime_error("not available");
    }
    ngs_adapt::StringItf *getAlignedFragmentQualities() const {
        throw std::runtime_error("not available");
    }
    bool isPrimary() const {
        throw std::runtime_error("no rows");
    }
    int64_t getAlignmentPosition() const {
        throw std::runtime_error("no rows");
    }
    uint64_t getReferencePositionProjectionRange ( int64_t ref_pos ) const {
        throw std::runtime_error("no rows");
    }
    uint64_t getAlignmentLength() const {
        throw std::runtime_error("no rows");
    }
    ngs_adapt::StringItf *getShortCigar(bool clipped) const {
        return getCigar(clipped, "MIDNSHPMM???????");
    }
    ngs_adapt::StringItf *getLongCigar(bool clipped) const {
        return getCigar(clipped, "MIDNSHP=X???????");
    }
    char getRNAOrientation () const {
        throw std::runtime_error("no rows");
    }
    bool getIsReversedOrientation() const {
        throw std::runtime_error("no rows");
    }
    int32_t getSoftClip(uint32_t edge) const {
        throw std::runtime_error("not available");
    }
    uint64_t getTemplateLength() const {
        throw std::runtime_error("no rows");
    }
    bool hasMate() const {
        throw std::runtime_error("no rows");
    }
    ngs_adapt::StringItf *getMateAlignmentId() const {
        throw std::runtime_error("not available");
    }
    ngs_adapt::AlignmentItf *getMateAlignment() const {
        throw std::runtime_error("not available");
    }
    ngs_adapt::StringItf *getMateReferenceSpec() const {
        throw std::runtime_error("no rows");
    }
    bool getMateIsReversedOrientation() const {
        throw std::runtime_error("no rows");
    }
    bool nextAlignment() {
        return false;
    }
    bool nextFragment() {
        throw std::runtime_error("not available");
    }
};

class ReadCollection::Alignment : public ReadCollection::AlignmentNone
{
    mutable std::string seqBuffer;
    mutable std::string qualBuffer;
    mutable std::string cigarBuffer;
protected:
    ReadCollection *parent;
    BAMRecord const *current;
    bool want_primary;
    bool want_secondary;

    ngs_adapt::StringItf *getCigar(bool const clipped, char const OPCODE[]) const;

    bool shouldSkip() const {
        int const flag = current->flag();

        if ((flag & 0x0004) != 0)
            return true;

        if ((flag & 0x0900) == 0 && !want_primary)
            return true;

        if ((flag & 0x0900) != 0 && !want_secondary)
            return true;

        return false;
    }

public:
    Alignment(ReadCollection const *Parent, bool WantPrimary, bool WantSecondary) {
        parent = static_cast<ReadCollection *>(Parent->Duplicate());
        want_primary = WantPrimary;
        want_secondary = WantSecondary;
        current = 0;
    }
    virtual ~Alignment() {
        if (current)
            delete current;
        parent->Release();
    }

    ngs_adapt::StringItf *getFragmentBases(uint64_t offset, uint64_t length) const;
    ngs_adapt::StringItf *getFragmentQualities(uint64_t offset, uint64_t length) const;
    ngs_adapt::StringItf *getReferenceSpec() const;
    int32_t getMappingQuality() const;
    ngs_adapt::StringItf *getReadGroup() const;
    ngs_adapt::StringItf *getReadId() const;
    bool isPrimary() const;
    int64_t getAlignmentPosition() const;
    uint64_t getAlignmentLength() const;
    bool getIsReversedOrientation() const;
    uint64_t getTemplateLength() const;
    bool hasMate() const;
    ngs_adapt::StringItf *getMateReferenceSpec() const;
    bool getMateIsReversedOrientation() const;
    bool nextAlignment();
};

class ReadCollection::AlignmentSlice : public ReadCollection::Alignment
{
    unsigned refID;
    unsigned beg;
    unsigned end;
    BAMFilePosTypeList const slice;
    BAMFilePosTypeList::const_iterator cur;
public:
    AlignmentSlice(ReadCollection const *Parent,
                   bool const WantPrimary,
                   bool const WantSecondary,
                   BAMFilePosTypeList const &Slice,
                   unsigned const Beg,
                   unsigned const End)
    : Alignment(Parent, WantPrimary, WantSecondary)
    , slice(Slice)
    , beg(Beg)
    , end(End)
    , cur(Slice.begin())
    {
        parent->Seek(*cur++);
    }

    bool nextAlignment() {
        for ( ; ; ) {
            if (!Alignment::nextAlignment())
                return false;
            if (current->isSelfMapped()) {
                unsigned const POS   = current->pos();
                unsigned const REFID = current->refID();

                if (REFID != refID || POS >= end)
                    return false;

                unsigned const REFLEN = current->refLen();
                if (POS + REFLEN > beg)
                    return true;
            }
        }
    }
};

class ReadCollection::Reference : public ngs_adapt::ReferenceItf
{
    ReadCollection *parent;
    unsigned cur;
    unsigned max;
    int state;

public:
    Reference(ReadCollection const *const Parent,
              unsigned const current,
              unsigned const references,
              int const initState)
    : parent(static_cast<ReadCollection *>(Parent->Duplicate()))
    , cur(current)
    , max(references)
    , state(initState)
    {}

    ~Reference() {
        parent->Release();
    }

    ngs_adapt::StringItf *getCommonName() const {
        if (state == 2)
            throw std::runtime_error("no current row");

        HeaderRefInfo const &ri = parent->getRefInfo(cur);
        std::string const &RNAME = ri.getName();

        return new ngs_adapt::StringItf(RNAME.data(), RNAME.size());
    }
    ngs_adapt::StringItf *getCanonicalName() const {
        throw std::runtime_error("not available");
    }
    // TODO: rename to isCircular
    bool getIsCircular() const {
        throw std::runtime_error("not available");
    }
     bool getIsLocal () const {
        throw std::runtime_error("not available");
    }
    uint64_t getLength() const {
        if (state == 2)
            throw std::runtime_error("no current row");

        HeaderRefInfo const &ri = parent->getRefInfo(cur);

        return ri.getLength();
    }
    ngs_adapt::StringItf *getReferenceBases(uint64_t const offset, uint64_t const length) const {
        throw std::runtime_error("not available");
    }
    ngs_adapt::StringItf *getReferenceChunk(uint64_t const offset, uint64_t const length) const {
        throw std::runtime_error("not available");
    }
    uint64_t getAlignmentCount ( bool wants_primary, bool wants_secondary ) const {
        throw std::runtime_error("not available");
    }
    ngs_adapt::AlignmentItf *getAlignment(char const id[]) const {
        throw std::runtime_error("not available");
    }
    ngs_adapt::AlignmentItf *getAlignments(bool const want_primary, bool const want_secondary) const {
        return getAlignmentSlice(0, getLength(), want_primary, want_secondary);
    }
    ngs_adapt::AlignmentItf *getAlignmentSlice(int64_t const Start, uint64_t const length, bool const want_primary, bool const want_secondary) const {
        if (state == 2)
            throw std::runtime_error("no current row");

        HeaderRefInfo const &ri = parent->getRefInfo(cur);
        unsigned const len = ri.getLength();
        if (Start >= len)
            return new ReadCollection::AlignmentNone();

        unsigned const start = Start < 0 ? 0 : Start;
        uint64_t const End = (Start < 0 ? 0 : Start) + length;
        unsigned const end = End > len ? len : End;
        BAMFilePosTypeList const &slice = ri.slice(start, end);

        if (slice.size() == 0)
            return new ReadCollection::AlignmentNone();

        return new ReadCollection::AlignmentSlice(parent, want_primary, want_secondary,
                                                  slice, start, end);
    }
    ngs_adapt::AlignmentItf * getFilteredAlignmentSlice ( int64_t start, uint64_t length, uint32_t flags, int32_t map_qual ) const {
        throw std::runtime_error("not available");
    }
    ngs_adapt::PileupItf *getPileups(bool const want_primary, bool const want_secondary) const {
        throw std::runtime_error("not available");
    }
    ngs_adapt::PileupItf *getFilteredPileups(uint32_t flags, int32_t map_qual) const {
        throw std::runtime_error("not available");
    }
    ngs_adapt::PileupItf *getPileupSlice(int64_t const start, uint64_t const length, bool const want_primary, bool const want_secondary) const {
        throw std::runtime_error("not available");
    }
    ngs_adapt::PileupItf *getFilteredPileupSlice(int64_t const start, uint64_t const length, uint32_t flags, int32_t map_qual) const {
        throw std::runtime_error("not available");
    }
    bool nextReference() {
        switch (state) {
            case 0:
                if (cur < max) {
                    state = 1;
                    return true;
                }
                else {
                    state = 2;
                    return false;
                }
            case 1:
                ++cur;
                if (cur < max)
                    return true;
                state = 2;
            case 2:
                return false;
            default:
                throw std::runtime_error("no more rows available");
        }
    }
};

ngs_adapt::StringItf *ReadCollection::getName() const
{
    unsigned const sep = path.rfind('/');

    if (sep == path.npos)
        return new ngs_adapt::StringItf(path.data(), path.size());
    else {
        char const *name = path.data() + sep + 1;
        unsigned const len = path.size() - sep - 1;
        return new ngs_adapt::StringItf(name, len);
    }
}

ngs_adapt::ReadGroupItf *ReadCollection::getReadGroups() const
{
    throw std::logic_error("unimplemented");
}

bool ReadCollection::hasReadGroup(char const spec[]) const
{
    return false;
}

ngs_adapt::ReadGroupItf *ReadCollection::getReadGroup(char const spec[]) const
{
    throw std::logic_error("unimplemented");
}

ngs_adapt::ReferenceItf *ReadCollection::getReferences() const
{
    return new Reference(this, 0, file.countOfReferences(), 0);
}

bool ReadCollection::hasReference(char const spec[]) const
{
    int const i = file.getReferenceIndexByName(spec);
    return i >= 0;
}

ngs_adapt::ReferenceItf *ReadCollection::getReference(char const spec[]) const
{
    int const i = file.getReferenceIndexByName(spec);

    if (i < 0)
        return NULL;
    else
        return new Reference(this, i, 0, 3);
}

ngs_adapt::AlignmentItf *ReadCollection::getAlignment(char const spec[]) const
{
    throw std::logic_error("unimplemented");
}

ngs_adapt::AlignmentItf *ReadCollection::getAlignments(bool const want_primary,
                                                       bool const want_secondary) const
{
    if (!want_secondary && !want_primary)
        return new AlignmentNone();
    const_cast<BAMFile *>(&file)->Rewind();
    return new Alignment(this, want_primary, want_secondary);
}

uint64_t ReadCollection::getAlignmentCount(bool const want_primary,
                                           bool const want_secondary) const
{
    throw std::logic_error("unimplemented");
}

ngs_adapt::AlignmentItf *ReadCollection::getAlignmentRange(uint64_t const first,
                                                           uint64_t const count,
                                                           bool const want_primary,
                                                           bool const want_secondary ) const
{
    throw std::logic_error("unimplemented");
}

uint64_t ReadCollection::getReadCount(bool const want_full,
                                      bool const want_partial,
                                      bool const want_unaligned) const
{
    throw std::logic_error("unimplemented");
}

ngs_adapt::ReadItf *ReadCollection::getRead(char const spec[]) const
{
    throw std::logic_error("unimplemented");
}

ngs_adapt::ReadItf *ReadCollection::getReads(bool const want_full,
                                             bool const want_partial,
                                             bool const want_unaligned) const
{
    throw std::logic_error("unimplemented");
}

ngs_adapt::ReadItf *ReadCollection::getReadRange(uint64_t const first,
                                                 uint64_t const count,
                                                 bool const want_full,
                                                 bool const want_partial,
                                                 bool const want_unaligned) const
{
    throw std::logic_error("unimplemented");
}

ngs_adapt::StringItf *ReadCollection::Alignment::getFragmentBases(uint64_t const Offset, uint64_t const Length) const
{
    uint64_t const End = Offset + Length;
    unsigned const seqLen = current->l_seq();
    unsigned const offset = Offset < seqLen ? Offset : seqLen;
    unsigned const seqEnd = End < seqLen ? End : seqLen;

    seqBuffer.resize(0);
    seqBuffer.reserve(seqLen);

    for (unsigned i = offset; i < seqEnd; ++i)
        seqBuffer.append(1, current->seq(i));

    return new ngs_adapt::StringItf(seqBuffer.data(), seqBuffer.size());
}

ngs_adapt::StringItf *ReadCollection::Alignment::getFragmentQualities(uint64_t const Offset, uint64_t const Length) const
{
    uint64_t const End = Offset + Length;
    unsigned const seqLen = current->l_seq();
    unsigned const offset = Offset < seqLen ? Offset : seqLen;
    unsigned const seqEnd = End < seqLen ? End : seqLen;
    bool notFF = false;
    uint8_t const *qual = current->qual();

    qualBuffer.resize(0);
    qualBuffer.reserve(seqLen);

    for (unsigned i = offset; i < seqEnd; ++i) {
        int const qv = qual[i];

        notFF |= (qv != 0xFF);
        qualBuffer.append(1, (char)((qv > 63 ? 63 : qv) + 33));
    }
    return new ngs_adapt::StringItf(qualBuffer.data(), notFF ? qualBuffer.size() : 0);
}

ngs_adapt::StringItf *ReadCollection::Alignment::getReferenceSpec() const
{
    int const refID = current->refID();
    HeaderRefInfo const &ri = parent->getRefInfo(refID);
    std::string const &RNAME = ri.getName();
    return new ngs_adapt::StringItf(RNAME.data(), RNAME.size());
}

int32_t ReadCollection::Alignment::getMappingQuality() const
{
    return current->mq();
}

ngs_adapt::StringItf *ReadCollection::Alignment::getReadGroup() const
{
    for (BAMRecord::OptionalField::const_iterator i = current->begin(); i != current->end(); ++i) {
        char const *tag = i->getTag();
        if (tag[0] == 'R' && tag[1] == 'G' && i->getValueType() == 'Z') {
            return new ngs_adapt::StringItf(i->getRawValue(), i->getElementSize());
        }
    }
    return NULL;
}

ngs_adapt::StringItf *ReadCollection::Alignment::getReadId() const
{
    char const *const QNAME = current->readname();
    size_t const len = strnlen(QNAME, current->l_read_name());
    return new ngs_adapt::StringItf(QNAME, len);
}

bool ReadCollection::Alignment::isPrimary() const
{
    return (current->flag() & 0x0900) == 0;
}

int64_t ReadCollection::Alignment::getAlignmentPosition() const
{
    return current->pos();
}

uint64_t ReadCollection::Alignment::getAlignmentLength() const {
    return current->refLen();
}

ngs_adapt::StringItf *ReadCollection::Alignment::getCigar(bool const clipped, char const OPCODE[]) const
{
    current->cigarString(cigarBuffer, clipped, OPCODE);
    return new ngs_adapt::StringItf(cigarBuffer.data(), cigarBuffer.size());
}

bool ReadCollection::Alignment::getIsReversedOrientation() const
{
    return (current->flag() & 0x0010) != 0;
}

// TODO: return type should be int64_t
uint64_t ReadCollection::Alignment::getTemplateLength() const
{
    return current->tlen();
}

bool ReadCollection::Alignment::hasMate() const
{
    int const FLAG = current->flag();

    return (FLAG & 0x0001) != 0 && (FLAG & 0x00C0) != 0 && (FLAG & 0x00C0) != 0x00C0;
}

ngs_adapt::StringItf *ReadCollection::Alignment::getMateReferenceSpec() const
{
    int const refID = current->next_refID();

    if (refID < 0)
        return new ngs_adapt::StringItf("", 0);

    HeaderRefInfo const &ri = parent->getRefInfo(refID);
    std::string const &RNEXT = ri.getName();
    return new ngs_adapt::StringItf(RNEXT.data(), RNEXT.size());
}

// TODO: rename to isMateReversedOrientation
bool ReadCollection::Alignment::getMateIsReversedOrientation() const
{
    return (current->flag() & 0x0020) != 0;
}

bool ReadCollection::Alignment::nextAlignment()
{
    do {
        if (current) {
            delete current;
            current = 0;
        }
        current = parent->ReadBAMRecord();
        if (!current)
            return false;
    } while (shouldSkip());
    return true;
}

ngs::ReadCollection NGS_BAM::openReadCollection(std::string const &path)
{
    ngs_adapt::ReadCollectionItf *const self = new ReadCollection(path);
    NGS_ReadCollection_v1 *const c_obj = self->Cast();
    ngs::ReadCollectionItf *const ngs_itf = ngs::ReadCollectionItf::Cast(c_obj);

    return ngs::ReadCollection(ngs_itf);
}
