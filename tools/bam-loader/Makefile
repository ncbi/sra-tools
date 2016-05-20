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

TOP ?= $(abspath ../..)
MODULE = tools/bam-loader

INT_TOOLS = \
	samview

EXT_TOOLS = \
	bam-load 

ALL_TOOLS = \
	$(INT_TOOLS) \
	$(EXT_TOOLS)

include $(TOP)/build/Makefile.env

#-------------------------------------------------------------------------------
# outer targets
#
ifeq (win,$(OS))
all std:
	@ echo "not building bam-load under Windows"
else
all std: makedirs
	@ $(MAKE_CMD) $(TARGDIR)/$@-cmn
endif

$(ALL_TOOLS): makedirs
	@ $(MAKE_CMD) $(BINDIR)/$@

.PHONY: all std $(ALL_TOOLS)

#-------------------------------------------------------------------------------
# clean
#
clean: stdclean

.PHONY: clean

#-------------------------------------------------------------------------------
# bam-load
#
BAMLOAD_SRC = \
	bam-loader \
	bam \
	alignment-writer \
	reference-writer \
	sequence-writer \
	loader-imp \
	mem-bank \
	low-match-count

BAMLOAD_OBJ = \
	$(addsuffix .$(OBJX),$(BAMLOAD_SRC))

BAMLOAD_LIB = \
	-lkapp \
	-stk-version \
	-lload \
	-sncbi-wvdb \
	-lm

$(BINDIR)/bam-load: $(BAMLOAD_OBJ)
	$(LP) --exe --vers $(SRCDIR)/../../shared/toolkit.vers -o $@ $^ $(BAMLOAD_LIB)


#-------------------------------------------------------------------------------
# samview
#
SAMVIEW_SRC = \
	bam \
	samview

SAMVIEW_OBJ = \
	$(addsuffix .$(OBJX),$(SAMVIEW_SRC))

SAMVIEW_LIB = \
	-lkapp \
	-stk-version \
	-sncbi-vdb \
	-lm

$(BINDIR)/samview: $(SAMVIEW_OBJ)
	$(LP) --exe --vers $(SRCDIR)/../../shared/toolkit.vers -o $@ $^ $(SAMVIEW_LIB)

