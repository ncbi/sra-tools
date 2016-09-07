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
MODULE = tools/copycat

INT_TOOLS =

EXT_TOOLS = copycat

ALL_TOOLS = \
	$(INT_TOOLS) \
	$(EXT_TOOLS)

include $(TOP)/build/Makefile.env

#-------------------------------------------------------------------------------
# outer targets
#
ifeq (1,$(HAVE_MAGIC))

all std: makedirs
	@ $(MAKE_CMD) $(TARGDIR)/$@-cmn

$(ALL_TOOLS): makedirs
	@ $(MAKE_CMD) $(BINDIR)/$@

else

all std: $(ALL_TOOLS)

$(ALL_TOOLS):
	@ echo "NOTE - $(@F) cannot be built:"                          \
	       "It requires our internal library 'libkff'"              \
	       "which requires 'libmagic' and its development headers."
endif

.PHONY: all std $(ALL_TOOLS)

#-------------------------------------------------------------------------------
# clean
#
clean: stdclean

.PHONY: clean

#-------------------------------------------------------------------------------
# copycat
#  Copy files from one directory to another while cataloging the contents.
#  XML files can be redirected as well.
#
COPYCAT_SRC = \
	copycat \
	ccfileformat \
	cccat \
	cctree \
	cctree-dump \
	cctar  \
	ccsra \
	ccsubchunk \
	ccfile

COPYCAT_OBJ = \
	$(addsuffix .$(OBJX),$(COPYCAT_SRC))

COPYCAT_LIB = \
	-skapp \
	-stk-version \
	-sncbi-vdb \
	-skff \
	-smagic-static


$(BINDIR)/copycat: $(COPYCAT_OBJ)
	$(LD) --exe --vers $(SRCDIR)/../../shared/toolkit.vers -o $@ $^ $(COPYCAT_LIB)
	@ cp $(SRCDIR)/magic $(BINDIR)
