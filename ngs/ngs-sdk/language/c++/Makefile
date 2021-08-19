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

TOP ?= $(abspath ../../../..)
include $(TOP)/build/Makefile.config
INCDIRS += -I$(TOP)/ngs/ngs-sdk
INCDIRS += -I$(TOP)/ngs/ngs-sdk/ngs/$(OSINC)/$(ARCH)

INTLIBS = \
	ngs-bind-c++

EXTLIBS = \

TARGETS =      \
	$(INTLIBS) \
	$(EXTLIBS)

all std: $(TARGETS)

clean:
	rm -rf $(OBJDIR)
	rm -f $(addprefix $(ILIBDIR)/$(LPFX),$(addsuffix *,$(INTLIBS)))
	rm -f $(addprefix $(LIBDIR)/$(LPFX),$(addsuffix *,$(EXTLIBS)))

.PHONY: default all std clean $(TARGETS)

#-------------------------------------------------------------------------------
# ngs-bind-c++
#
ngs-bind-c++: $(ILIBDIR) $(OBJDIR) $(ILIBDIR)/$(LPFX)ngs-bind-c++.$(LIBX)

BIND_SRC =              \
	ReadCollection      \
	ReferenceSequence   \
	ReferenceIterator   \
	Reference           \
	PileupIterator      \
	Pileup              \
	PileupEventIterator \
	PileupEvent         \
	AlignmentIterator   \
	Alignment           \
	ReadGroupIterator   \
	ReadGroup           \
	ReadIterator        \
	Read                \
	FragmentIterator    \
	Fragment            \
	Statistics          \
	StringRef

BIND_OBJ = \
	$(addprefix $(OBJDIR)/,$(addsuffix .$(LOBX),$(BIND_SRC)))

$(ILIBDIR)/$(LPFX)ngs-bind-c++.$(LIBX): $(BIND_OBJ)
	$(AR) $@ $^
