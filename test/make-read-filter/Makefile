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

default: runtests

TOP ?= $(abspath ../..)

MODULE = test/make-read-filter

include $(TOP)/build/Makefile.env

TMPDIR ?= /tmp
TEMPDIR ?= $(TMPDIR)

$(TEMPDIR)/tmp.mkfg:
	@ echo '/LIBS/GUID = "c1d99592-6ab7-41b2-bfd0-8aeba5ef8498"' >$@; \
	  echo '/sra/quality_type = "raw_scores"' >>$@

mkfg: $(TEMPDIR)/tmp.mkfg

exe: $(BINDIR)/make-read-filter $(BINDIR)/vdb-dump $(BINDIR)/kar

test-data.kar: generate-test-data.py | mkfg exe
	NCBI_SETTINGS=$(TEMPDIR)/tmp.mkfg \
	VDB_LIBRARY_PATH=$(VDB_LIBDIR) \
	python3 $^ $(TEMPDIR)/test-data && \
	$(BINDIR)/kar -d $(TEMPDIR)/test-data -fc $@ && \
	rm -rf $(TEMPDIR)/test-data expected

runtests: test-data.kar | mkfg exe
	NCBI_SETTINGS=$(TEMPDIR)/tmp.mkfg \
	$(BINDIR)/kar -d $(TEMPDIR)/test-data -x test-data.kar && \
	$(BINDIR)/make-read-filter --temp $(TEMPDIR) $(TEMPDIR)/test-data && \
	$(BINDIR)/vdb-dump $(TEMPDIR)/test-data >actual && \
	rm -rf $(TEMPDIR)/test-data && \
	diff expected actual

.PHONY: runtests
.INTERMEDIATE: $(TEMPDIR)/tmp.mkfg
