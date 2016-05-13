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
MODULE = tools/cg-load

include $(TOP)/build/Makefile.env

INT_TOOLS =

EXT_TOOLS = \
	cg-load

ALL_TOOLS = \
	$(INT_TOOLS) \
	$(EXT_TOOLS)

#-------------------------------------------------------------------------------
# outer targets
#
all std: makedirs
	@ $(MAKE_CMD) $(TARGDIR)/std

$(ALL_TOOLS): makedirs
	@ $(MAKE_CMD) $(BINDIR)/$@

.PHONY: all std $(ALL_TOOLS)

#-------------------------------------------------------------------------------
# std
#
$(TARGDIR)/std: \
	$(addprefix $(BINDIR)/,$(ALL_TOOLS))

.PHONY: $(TARGDIR)/std

#-------------------------------------------------------------------------------
# clean
#
clean: stdclean

.PHONY: clean

#-------------------------------------------------------------------------------
# cg-load
#
CGLOAD_SRC = \
	cg-load \
	factory-evidence-intervals \
	factory-evidence-dnbs \
	factory-mappings \
	factory-reads \
	factory-tag-lfr \
	f1_3 \
	f1_5 \
	f1_7 \
	f2_0 \
	f2_2 \
	f2_5 \
	file \
	file-version-factory \
	formats \
	writer-algn \
	writer-evidence-dnbs \
	writer-evidence-intervals \
	writer-seq \

CGLOAD_OBJ = \
	$(addsuffix .$(OBJX),$(CGLOAD_SRC))

CGLOAD_LIB = \
	-lkapp \
	-stk-version \
	-lload \
	-sncbi-wvdb \
	-lm

$(BINDIR)/cg-load: $(CGLOAD_OBJ)
	$(LD) --exe --vers $(SRCDIR)/../../shared/toolkit.vers -o $@ $^ $(CGLOAD_LIB)

