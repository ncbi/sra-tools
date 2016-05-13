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

TOP ?= $(abspath ..)

MODULE = shared

INT_LIBS = \
	libtk-version \

ALL_LIBS = \
	$(INT_LIBS)

ALL_TOOLS =      \
	$(INT_TOOLS) \
	$(EXT_TOOLS)
	
include $(TOP)/build/Makefile.env

#-------------------------------------------------------------------------------
# outer targets
#
all std: makedirs
	@ $(MAKE_CMD) $(TARGDIR)/std

$(INT_LIBS): makedirs
	@ $(MAKE_CMD) $(ILIBDIR)/$@


.PHONY: all std $(ALL_LIBS)

#-------------------------------------------------------------------------------
# std
#
$(TARGDIR)/std: \
	$(addprefix $(ILIBDIR)/,$(INT_LIBS))

.PHONY: $(TARGDIR)/std

#-------------------------------------------------------------------------------
# clean
#
clean: stdclean

.PHONY: clean

#-------------------------------------------------------------------------------
# toolkit-version
#
$(ILIBDIR)/libtk-version: $(addprefix $(ILIBDIR)/libtk-version.,$(ILIBEXT))

TK_VERSION_SRC = \
    toolkit.vers

TK_VERSION_OBJ = \
	$(addsuffix .$(LOBX),$(TK_VERSION_SRC))

$(ILIBDIR)/libtk-version.$(LIBX): $(TK_VERSION_OBJ)
	$(LD) --slib -o $@ $^ $(KFS_LIB)


