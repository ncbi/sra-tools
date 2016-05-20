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
MODULE = tools/sra-load

INT_TOOLS =

EXT_TOOLS = \
	fastq-load \
	illumina-load \
	srf-load \
	abi-load \
	sff-load \
	helicos-load \
	pacbio-loadxml

ALL_TOOLS = \
	$(INT_TOOLS) \
	$(EXT_TOOLS)

include $(TOP)/build/Makefile.env

#-------------------------------------------------------------------------------
# outer targets
#
ifeq (1,$(HAVE_XML2))

all std: makedirs
	@ $(MAKE_CMD) $(TARGDIR)/$@-cmn

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
# clean
#
clean: stdclean

.PHONY: clean

#-------------------------------------------------------------------------------
# loader
#
LOADER_SRC = \
	pstring \
	common-xml \
	experiment-xml \
	run-xml \
	loader-file \
	loader-fmt \
	sra-writer \
	loader

LOADER_OBJ = \
	$(addsuffix .$(OBJX),$(LOADER_SRC))

LIBXML = -lkxfs -lkxml -lxml2

LOADER_LIB = \
	-lkapp \
	-stk-version \
	-lload \
	-sncbi-wvdb \
	$(LIBXML) \
	-lm


#------------------------------------------------------------------------------
# sff-load
#
SFF_LOAD_SRC = \
	$(LOADER_SRC) \
	writer-454 \
	writer-ion-torrent \
	sff-fmt

SFF_LOAD_OBJ = \
	$(addsuffix .$(OBJX),$(SFF_LOAD_SRC))

$(BINDIR)/sff-load: $(SFF_LOAD_OBJ)
	$(LD) --exe --vers $(SRCDIR)/../../shared/toolkit.vers -o $@ $^ $(LOADER_LIB)

#------------------------------------------------------------------------------
# fastq-load
#
FASTQ_LOAD_SRC = \
	$(LOADER_SRC) \
	writer-illumina \
	writer-454 \
	writer-ion-torrent \
	fastq-fmt

FASTQ_LOAD_OBJ = \
	$(addsuffix .$(OBJX),$(FASTQ_LOAD_SRC))

$(BINDIR)/fastq-load: $(FASTQ_LOAD_OBJ)
	$(LD) --exe --vers $(SRCDIR)/../../shared/toolkit.vers -o $@ $^ $(LOADER_LIB)

#------------------------------------------------------------------------------
# srf-load
#
SRF_LOAD_SRC = \
	$(LOADER_SRC) \
	srf \
	srf-fmt \
	writer-illumina \
	ztr-illumina \
	srf-illumina \
	writer-absolid \
	ztr-absolid \
	ztr-huffman \
	srf-absolid

SRF_LOAD_OBJ = \
	$(addsuffix .$(OBJX),$(SRF_LOAD_SRC))

$(BINDIR)/srf-load: $(SRF_LOAD_OBJ)
	$(LD) --exe --vers $(SRCDIR)/../../shared/toolkit.vers -o $@ $^ $(LOADER_LIB)

#------------------------------------------------------------------------------
# abi-load
#
ABSOLID_LOAD_SRC = \
	$(LOADER_SRC) \
	writer-absolid \
	absolid-fmt

ABSOLID_LOAD_OBJ = \
	$(addsuffix .$(OBJX),$(ABSOLID_LOAD_SRC))

$(BINDIR)/abi-load: $(ABSOLID_LOAD_OBJ)
	$(LD) --exe --vers $(SRCDIR)/../../shared/toolkit.vers -o $@ $^ $(LOADER_LIB)

#------------------------------------------------------------------------------
# illumina-load
#
ILLUMINA_LOAD_SRC = \
	$(LOADER_SRC) \
	writer-illumina \
	illumina-fmt

ILLUMINA_LOAD_OBJ = \
	$(addsuffix .$(OBJX),$(ILLUMINA_LOAD_SRC))

$(BINDIR)/illumina-load: $(ILLUMINA_LOAD_OBJ)
	$(LD) --exe --vers $(SRCDIR)/../../shared/toolkit.vers -o $@ $^ $(LOADER_LIB)

#------------------------------------------------------------------------------
# helicos-load
#
HELICOS_LOAD_SRC = \
	$(LOADER_SRC) \
	writer-helicos \
	helicos-fmt

HELICOS_LOAD_OBJ = \
	$(addsuffix .$(OBJX),$(HELICOS_LOAD_SRC))

$(BINDIR)/helicos-load: $(HELICOS_LOAD_OBJ)
	$(LD) --exe --vers $(SRCDIR)/../../shared/toolkit.vers -o $@ $^ $(LOADER_LIB)

#------------------------------------------------------------------------------
# pacbio-loadxml
#
PACBIO_LOADXML_SRC = \
	$(LOADER_SRC) \
	pacbio-fmt

PACBIO_LOADXML_OBJ = \
	$(addsuffix .$(OBJX),$(PACBIO_LOADXML_SRC))

$(BINDIR)/pacbio-loadxml: $(PACBIO_LOADXML_OBJ)
	$(LD) --exe --vers $(SRCDIR)/../../shared/toolkit.vers -o $@ $^ $(LOADER_LIB)
