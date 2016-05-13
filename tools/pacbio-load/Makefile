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
MODULE = tools/pacbio-load

include $(TOP)/build/Makefile.env

INT_TOOLS = \

EXT_TOOLS = \
	pacbio-load

ALL_TOOLS = \
	$(INT_TOOLS) \
	$(EXT_TOOLS)


#-------------------------------------------------------------------------------
# where to find hdf5 libs
#
ifdef NCBI
ifeq (dbg,$(BUILD))
	LIBHDF5_LPATH = $(NCBI)/hdf5/Debug/lib
else
	LIBHDF5_LPATH = $(NCBI)/hdf5/Release/lib
endif
endif

#-------------------------------------------------------------------------------
# outer targets
#
ifeq (1,$(HAVE_HDF5))

all std: makedirs
	@ $(MAKE_CMD) $(TARGDIR)/$@

$(ALL_TOOLS): makedirs
	@ $(MAKE_CMD) $(BINDIR)/$@

else

all std: $(ALL_TOOLS)

$(ALL_TOOLS):
	@ echo "NOTE - $(@F) cannot be built:"                          \
	       "It requires our internal library 'libkdf5'"             \
	       "which requires 'libhdf5' and its development headers."
endif

.PHONY: all std $(ALL_TOOLS)

#-------------------------------------------------------------------------------
# all
#
$(TARGDIR)/all: \
	$(addprefix $(BINDIR)/,$(ALL_TOOLS))

.PHONY: $(TARGDIR)/all

#-------------------------------------------------------------------------------
# std
#
$(TARGDIR)/std: \
	$(addprefix $(BINDIR)/,$(EXT_TOOLS))

.PHONY: $(TARGDIR)/std

#-------------------------------------------------------------------------------
# clean
#
clean: stdclean

.PHONY: clean

#-------------------------------------------------------------------------------
# vdb-copy
#  vdb copy tool
#
PACBIO_LOAD_SRC = \
	pl-context \
	pl-tools \
	pl-zmw \
	pl-basecalls_cmn \
	pl-sequence \
	pl-consensus \
	pl-passes \
	pl-metrics \
	pl-regions \
	pl-progress \
	pacbio-load

PACBIO_LOAD_OBJ = \
	$(addsuffix .$(OBJX),$(PACBIO_LOAD_SRC))

PACBIO_LOAD_LIB = \
	-skapp \
	-stk-version \
	-skdf5 \
	-sload \
	-shdf5 \
	-sncbi-wvdb \
	-sm \

ifdef HDF5_LIBDIR
	PACBIO_LOAD_LIB += $(addprefix -L,$(HDF5_LIBDIR))
endif

$(BINDIR)/pacbio-load: $(PACBIO_LOAD_OBJ)
	$(LD) --exe --vers $(SRCDIR)/../../shared/toolkit.vers -o $@ $^ $(PACBIO_LOAD_LIB)
