# ===========================================================================
#
#                            PUBLIC DOMAIN NOTICE
#               National Center for Biotechnology Information
#
#  This software/database is a "United States Government Work" under the
#  terms of the United States Copyright Act.  It was written as part of
#  the author's official duties as a United States Government employee and
#  thus cannot be copyrighted.  This software/database is freely available
#  to the public for use. The National Library of Medicine and the U.S.
#  Government have not placed any restriction on its use or reproduction.
#
#  Although all reasonable efforts have been taken to ensure the accuracy
#  and reliability of the software and data, the NLM and the U.S.
#  Government do not and cannot warrant the performance or results that
#  may be obtained by using this software or data. The NLM and the U.S.
#  Government disclaim all warranties, express or implied, including
#  warranties of performance, merchantability or fitness for any particular
#  purpose.
#
#  Please cite the author in any work or product based on this material.
#
# ===========================================================================

default: std

TOP ?= $(abspath ../../..)
MODULE = ngs/ncbi/ngs

INT_LIBS = \
	libngs

EXT_LIBS = \

ALL_LIBS = \
	$(INT_LIBS) \
	$(EXT_LIBS)

OBJFILES = \
	$(NGS_OBJ)

include $(TOP)/build/Makefile.env

INCDIRS += -I$(TOP)/ngs/ngs-sdk -I$(VDB_SRCDIR)/libs/vdb


#-------------------------------------------------------------------------------
# outer targets
#
all std: makedirs compile
	@ $(MAKE_CMD) $(TARGDIR)/std

$(INT_LIBS): makedirs
	@ $(MAKE_CMD) $(ILIBDIR)/$@

$(EXT_LIBS): makedirs
	@ $(MAKE_CMD) $(LIBDIR)/$@

.PHONY: all std $(ALL_LIBS)

#-------------------------------------------------------------------------------
# std
#
$(TARGDIR)/std: \
	$(addprefix $(ILIBDIR)/,$(INT_LIBS)) \
	$(addprefix $(LIBDIR)/,$(EXT_LIBS))

.PHONY: $(TARGDIR)/std

#-------------------------------------------------------------------------------
# clean
#
clean: stdclean

.PHONY: clean

#-------------------------------------------------------------------------------
# ngs
#  will be included in ncbi-vdb
#
$(ILIBDIR)/libngs: $(addprefix $(ILIBDIR)/libngs.,$(ILIBEXT))

NGS_SRC =                 \
	NCBI-NGS              \
	CSRA1_PileupEvent     \
	CSRA1_Pileup          \
	CSRA1_Alignment       \
	CSRA1_ReferenceWindow \
	CSRA1_Reference       \
	CSRA1_ReadCollection  \
	CSRA1_Read            \
	EBI_ReferenceSequence \
	SRA_Statistics        \
	SRA_DB_ReadCollection \
	SRA_Read              \
	SRA_ReadGroup         \
	SRA_ReadGroupInfo     \
	SRA_ReadCollection    \
	SRA_ReferenceSequence \
	NGS_Statistics        \
	NGS_ReadCollection    \
	NGS_PileupEvent       \
	NGS_Pileup            \
	NGS_ReadGroup         \
	NGS_Alignment         \
	NGS_Reference         \
	NGS_ReferenceSequence \
	NGS_Read              \
	NGS_Fragment          \
	NGS_Cursor            \
	NGS_Refcount          \
	NGS_String            \
	NGS_Id                \
	NGS_ErrBlock          \
	NGS_FragmentBlob      \
	NGS_FragmentBlobIterator    \
    NGS_ReferenceBlob       \
    NGS_ReferenceBlobIterator   \
    VByteBlob               \

$(info $$NGS_OBJ is [${NGS_OBJ}])
$(info $$NGS_LIB is [${NGS_LIB}])
$(info $$ILIBDIR is [${ILIBDIR}])
$(info $$MAKE_CMD is [${MAKE_CMD}])
$(info $$OBJFILES is [${OBJFILES}])
$(info $$TARGDIR is [${TARGDIR}])
$(info $$TOP is [${TOP}])
$(info $$SRCDIR is [${SRCDIR}])
$(info $$OBJDIR is [${OBJDIR}])
	
	
NGS_OBJ = \
	$(addsuffix .$(LOBX),$(NGS_SRC))

NGS_LIB = \

$(ILIBDIR)/libngs.$(LIBX): $(NGS_OBJ)
	$(LD) --slib -o $@ $^ $(NGS_LIB)

stdcompile: makedirs
#	@ $(MAKE_CMD) -j$(MAKE_JOBS) $(TARGDIR)/compile

compile: stdcompile

$(TARGDIR)/compile: $(OBJFILES)

#-------------------------------------------------------------------------------
# fuzz
#
fuzz: std
	cp $(addsuffix .$(LIBX),$(addprefix $(ILIBDIR)/,$(INT_LIBS))) $(FLIBDIR)/
