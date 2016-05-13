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
MODULE = tools/fuse

include $(TOP)/build/Makefile.env

INT_TOOLS =

EXT_TOOLS = \
	remote-fuser \

ALL_TOOLS = \
	$(INT_TOOLS) \
	$(EXT_TOOLS)

#-------------------------------------------------------------------------------
# outer targets
#
ifeq (1,$(HAVE_XML2))

ifeq (1,$(HAVE_FUSE))

ifeq (64,$(BITS))

all std: makedirs
	@ $(MAKE_CMD) $(TARGDIR)/std

$(ALL_TOOLS): makedirs
	@ $(MAKE_CMD) $(BINDIR)/$@

else

all std: $(ALL_TOOLS)

$(ALL_TOOLS):
	@ echo "NOTE - $(@F) cannot be built: it requires a 64-bit system."

endif

else

all std: $(ALL_TOOLS)

$(ALL_TOOLS):
	@ echo "NOTE - $(@F) cannot be built: " \
		"it requires 'fuse' library and its development headers."

endif

else

all std: $(ALL_TOOLS)

$(ALL_TOOLS):
	@ echo "NOTE - $(@F) cannot be built: " \
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
# sra-fuser
#
SRA_FUSER_SRC = \
        zlib-simple \
        log \
        node \
        tar-list \
        file \
        tar-file \
        tar-node \
        directory \
        formats \
        accessor \
        kfile-accessor \
        xml \
        text-file \
        sra-list \
        sra-directory \
        sra-node \
        sra-fastq \
        sra-sff \
        sra-fuser-sys \
        sra-fuser

SRA_FUSER_OBJ = \
	$(addsuffix .$(OBJX),$(SRA_FUSER_SRC))

LIBXML = -lxml2
ifdef NCBI
CFLAGS += -DSRAFUSER_LOGLOCALTIME
ifeq (linux,$(OS))
   LIBXML = -X$(LIBXML_LPATH) -lxml2-static
endif
ifeq (sun,$(OS))
   LIBXML = -X$(LIBXML_LPATH) -lxml2-static
endif
endif

SRA_FUSER_LIB = \
	-lkapp \
	-stk-version \
	-sncbi-vdb \
	-lfuse \
	-lrt

$(BINDIR)/sra-fuser: $(SRA_FUSER_OBJ)
	$(LD) --exe --vers $(SRCDIR)/../../shared/toolkit.vers -o $@ $^ $(SRA_FUSER_LIB)

#-------------------------------------------------------------------------------
# remote-fuser
#
REMOTE_FUSER_SRC = \
        log \
        node \
        accessor \
        kfile-accessor \
        remote-xml \
        remote-cache \
        remote-link \
        remote-file \
        remote-directory \
        remote-fuser-sys \
        remote-fuser

REMOTE_FUSER_OBJ = \
	$(addsuffix .$(OBJX),$(REMOTE_FUSER_SRC))

LIBXML = -lxml2
ifdef NCBI
CFLAGS += -DSRAFUSER_LOGLOCALTIME
ifeq (linux,$(OS))
   LIBXML = -X$(LIBXML_LPATH) -lxml2-static
endif
ifeq (sun,$(OS))
   LIBXML = -X$(LIBXML_LPATH) -lxml2-static
endif
endif

REMOTE_FUSER_LIB = \
	-lkapp \
	-stk-version \
	-sncbi-vdb \
	-lfuse \
	-lrt

$(BINDIR)/remote-fuser: $(REMOTE_FUSER_OBJ)
	$(LD) --exe --vers $(SRCDIR)/../../shared/toolkit.vers -o $@ $^ $(REMOTE_FUSER_LIB)

#-------------------------------------------------------------------------------
# sra-fuser
#
SRA_MAKEIDX_SRC = \
    zlib-simple \
	sra-makeidx

SRA_MAKEIDX_OBJ = \
	$(addsuffix .$(OBJX),$(SRA_MAKEIDX_SRC))

SRA_MAKEIDX_LIB = \
	-lkapp \
	-stk-version \
	-lncbi-wvdb \
	-ssrareader \

$(BINDIR)/sra-makeidx: $(SRA_MAKEIDX_OBJ)
	$(LD) --exe --vers $(SRCDIR)/../../shared/toolkit.vers -o $@ $^ $(SRA_MAKEIDX_LIB)

