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
MODULE = tools/sam-events

INT_TOOLS = \

EXT_TOOLS = \
	sam-events \
	alookup \
	t2 \
	t3
    
ALL_TOOLS = \
	$(INT_TOOLS) \
	$(EXT_TOOLS)

include $(TOP)/build/Makefile.env

#-------------------------------------------------------------------------------
# outer targets
#
all std: makedirs
	@ $(MAKE_CMD) $(TARGDIR)/$@-cmn

$(ALL_TOOLS): makedirs
	@ $(MAKE_CMD) $(BINDIR)/$@

.PHONY: all std $(ALL_TOOLS)

#-------------------------------------------------------------------------------
# clean
#
clean: stdclean

.PHONY: clean

#-------------------------------------------------------------------------------
# the tool: sam-events
#

TOOL_SRC = \
	common \
	alignment_iter \
	alig_consumer \
	alig_consumer2 \
	slice_2_rowrange \
	slice \
	fasta-file \
	cigar2events \
	expandCIGAR \
	allele_dict \
	mdb \
	midl \
	allele_lookup \
	sam-events
    
TOOL_OBJ = \
	$(addsuffix .$(OBJX),$(TOOL_SRC))

TOOL_LIB = \
	-skapp \
	-stk-version \
	-sncbi-vdb \
	-ssam-extract \
	-lm

$(BINDIR)/sam-events: $(TOOL_OBJ)
	$(LP) --exe --vers $(SRCDIR)/../../shared/toolkit.vers -o $@ $^ $(TOOL_LIB)


#-------------------------------------------------------------------------------
# helper tool: alookup.c
# ( lookup of an allel in a lmdb-cache )

LOOKUP_SRC = \
	common \
	mdb \
	midl \
	allele_lookup \
	alookup

LOOKUP_OBJ = \
	$(addsuffix .$(OBJX),$(LOOKUP_SRC))

LOOKUP_LIB = \
	-skapp \
	-stk-version \
	-sncbi-vdb \
	-lm \
	-lsqlite3

$(BINDIR)/alookup: $(LOOKUP_OBJ)
	$(LP) --exe --vers $(SRCDIR)/../../shared/toolkit.vers -o $@ $^ $(LOOKUP_LIB)

#-------------------------------------------------------------------------------
# helper tool: t2.c
# ( test multi-threaded approach )

T2_SRC = \
	common \
	alignment_iter \
	alig_consumer \
	slice_2_rowrange \
	slice \
	fasta-file \
	cigar2events \
	expandCIGAR \
	allele_dict \
	mdb \
	midl \
	allele_lookup \
	t2

T2_OBJ = \
	$(addsuffix .$(OBJX),$(T2_SRC))

T2_LIB = \
	-skapp \
	-stk-version \
	-sncbi-vdb \
	-ssam-extract \
	-lm

$(BINDIR)/t2: $(T2_OBJ)
	$(LP) --exe --vers $(SRCDIR)/../../shared/toolkit.vers -o $@ $^ $(T2_LIB)

    
#-------------------------------------------------------------------------------
# helper tool: t3.c
# ( slice-to-row-range test )

T3_SRC = \
	common \
	slice \
	slice_2_rowrange \
	t3

T3_OBJ = \
	$(addsuffix .$(OBJX),$(T3_SRC))

T3_LIB = \
	-skapp \
	-stk-version \
	-sncbi-vdb \
	-lm

$(BINDIR)/t3: $(T3_OBJ)
	$(LP) --exe --vers $(SRCDIR)/../../shared/toolkit.vers -o $@ $^ $(T3_LIB)
    