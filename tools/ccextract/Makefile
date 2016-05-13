# ==============================================================================
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
MODULE = tools/ccextract

include $(TOP)/build/Makefile.env

INT_TOOLS =

EXT_TOOLS = ccextract


ALL_TOOLS = \
	$(INT_TOOLS) \
	$(EXT_TOOLS)

#-------------------------------------------------------------------------------
# outer targets
#
ifeq (1,$(HAVE_XML2))

all std: makedirs
	@ $(MAKE_CMD) $(TARGDIR)/std

$(ALL_TOOLS): makedirs
	@ $(MAKE_CMD) $(BINDIR)/$@

else

all std: $(ALL_TOOLS)

$(ALL_TOOLS):
	@ echo "NOTE - $(@F) cannot be built:"                          \
	       "It requires our internal library 'libkxml'"             \
	       "which requires 'libxml2' and its development headers."
endif

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
# ccextract
#  Copy files from one directory to another while cataloging the contents.
#  XML files can be redirected as well.
#
CCEXTRACT_SRC = \
	ccextract

CCEXTRACT_OBJ = \
	$(addsuffix .$(OBJX),$(CCEXTRACT_SRC))

CCEXTRACT_LIB = \
	-lkapp \
	-stk-version \
    -lkxfs \
    -lkxml \
	-sncbi-vdb

$(BINDIR)/ccextract: $(CCEXTRACT_OBJ)
	$(LD) --exe --vers $(SRCDIR)/../../shared/toolkit.vers -o $@ $^ $(CCEXTRACT_LIB)
