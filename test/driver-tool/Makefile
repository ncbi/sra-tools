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

MODULE = test/driver-tool

include $(TOP)/build/Makefile.env

DIRTOTEST ?= $(BINDIR)

runtests: \
	announce \
	built-in \
	bogus \
	container \
	good \
	vdbcache \
	split_3 \
	split_e \
	fasta_missing_param \
	fasta_default \
	fasta_0 \
	fasta_number \
	fasta_neg_number \
	NO_SDL

TMPDIR ?= /tmp
TEMPDIR ?= $(TMPDIR)

GOOD =\
	fastq-dump \
	fasterq-dump \
	sam-dump \
	sra-pileup \
	vdb-dump \
	prefetch \
	srapath

CONTAINER =\
	SRP000001 \
	SRX000001 \
	SRS000001 \
	SRA000001 \
	ERP000001 \
	DRX000001

good: $(GOOD)

container: $(CONTAINER)

announce:
	@echo Testing $(DIRTOTEST)

$(TEMPDIR)/tmp.mkfg:
	@ echo '/LIBS/GUID = "c1d99592-6ab7-41b2-bfd0-8aeba5ef8498"' >$@

$(TEMPDIR)/tmp2.mkfg:
	@ echo '/LIBS/GUID = "c1d99592-6ab7-41b2-bfd0-8aeba5ef8498"' >$@ ;\
	  echo '/repository/remote/disabled = "true"' >>$@

mkfg: $(TEMPDIR)/tmp.mkfg

mkfg2: $(TEMPDIR)/tmp2.mkfg

actual:
	@ mkdir actual

built-in:
	@echo "running built-in tests" ; \
	SRATOOLS_TESTING=1 \
	$(BINDIR)/sratools

$(GOOD): | actual mkfg
	@# SRATOOLS_TESTING=2 skip SDL, sub-tool invocation is simulated to always
	@# succeed, but everything up to the exec call is real
	@echo "testing expected output for dry run of" $@ ;\
	NCBI_SETTINGS=$(TEMPDIR)/tmp.mkfg \
	PATH=$(DIRTOTEST):$$PATH \
	SRATOOLS_TESTING=2 \
	SRATOOLS_IMPERSONATE=$@ \
	$(BINDIR)/sratools SRR000001 ERR000001 DRR000001 2>actual/$@.stderr && \
	diff expected/$@.stderr actual/$@.stderr

NO_SDL: | actual mkfg2
	@# SRATOOLS_TESTING=5 and skip SDL via config, sub-tool invocation is
	@# simulated to always succeed, but everything up to the exec call is real
	@echo "testing expected output for dry run with no SDL" ;\
	NCBI_SETTINGS=$(TEMPDIR)/tmp2.mkfg \
	PATH=$(DIRTOTEST):$$PATH \
	SRATOOLS_TESTING=5 \
	SRATOOLS_IMPERSONATE=fastq-dump \
	$(BINDIR)/sratools SRR000001 2>actual/NO_SDL.stderr && \
	diff expected/NO_SDL.stderr actual/NO_SDL.stderr

bogus: | actual mkfg
	@echo "testing expected output for unknown tool" ;\
	NCBI_SETTINGS=$(TEMPDIR)/tmp.mkfg \
	PATH=$(DIRTOTEST):$$PATH \
	SRATOOLS_IMPERSONATE=rcexplain \
	$(BINDIR)/sratools 2>actual/$@.stderr ; \
	diff expected/$@.stderr actual/$@.stderr

$(CONTAINER): | actual mkfg
	@echo "testing expected output for container" $@ ;\
	NCBI_SETTINGS=$(TEMPDIR)/tmp.mkfg \
	PATH=$(DIRTOTEST):$$PATH \
	SRATOOLS_IMPERSONATE=fastq-dump \
	$(BINDIR)/sratools $@ 2>actual/$@.stderr ; \
	diff expected/$@.stderr actual/$@.stderr

vdbcache: | actual mkfg
	@echo "testing expected output for run with vdbcache" ;\
	NCBI_SETTINGS=$(TEMPDIR)/tmp.mkfg \
	PATH=$(DIRTOTEST):$$PATH \
	SRATOOLS_TESTING=5 \
	SRATOOLS_IMPERSONATE=vdb-dump \
	$(BINDIR)/sratools SRR390728 2>actual/$@.stderr ; \
	diff expected/$@.stderr actual/$@.stderr

split_3: | actual mkfg
	@echo "testing expected output for fastq-dump --split-3" ;\
	NCBI_SETTINGS=$(TEMPDIR)/tmp.mkfg \
	PATH=$(DIRTOTEST):$$PATH \
	SRATOOLS_TESTING=2 \
	SRATOOLS_IMPERSONATE=fastq-dump \
	$(BINDIR)/sratools --split-3 SRR390728 2>actual/$@.stderr ; \
	diff expected/$@.stderr actual/$@.stderr

split_e: | actual mkfg
	@echo "testing expected output for fastq-dump --split-e (sic)" ;\
	NCBI_SETTINGS=$(TEMPDIR)/tmp.mkfg \
	PATH=$(DIRTOTEST):$$PATH \
	SRATOOLS_TESTING=2 \
	SRATOOLS_IMPERSONATE=fastq-dump \
	$(BINDIR)/sratools --split-e SRR390728 2>actual/$@.stderr ; \
	diff expected/$@.stderr actual/$@.stderr

fasta_missing_param: | actual mkfg
	@echo "testing expected output for fastq-dump --fasta <run>" ;\
	NCBI_SETTINGS=$(TEMPDIR)/tmp.mkfg \
	PATH=$(DIRTOTEST):$$PATH \
	SRATOOLS_TESTING=2 \
	SRATOOLS_IMPERSONATE=fastq-dump \
	$(BINDIR)/sratools --fasta SRR390728 2>actual/$@.stderr ; \
	diff expected/$@.stderr actual/$@.stderr

fasta_default: | actual mkfg
	@echo "testing expected output for fastq-dump --fasta default <run>" ;\
	NCBI_SETTINGS=$(TEMPDIR)/tmp.mkfg \
	PATH=$(DIRTOTEST):$$PATH \
	SRATOOLS_TESTING=2 \
	SRATOOLS_IMPERSONATE=fastq-dump \
	$(BINDIR)/sratools --fasta default SRR390728 2>actual/$@.stderr ; \
	diff expected/$@.stderr actual/$@.stderr

fasta_0: | actual mkfg
	@echo "testing expected output for fastq-dump --fasta 0 <run>" ;\
	NCBI_SETTINGS=$(TEMPDIR)/tmp.mkfg \
	PATH=$(DIRTOTEST):$$PATH \
	SRATOOLS_TESTING=2 \
	SRATOOLS_IMPERSONATE=fastq-dump \
	$(BINDIR)/sratools --fasta 0 SRR390728 2>actual/$@.stderr ; \
	diff expected/$@.stderr actual/$@.stderr

fasta_number: | actual mkfg
	@echo "testing expected output for fastq-dump --fasta <number> <run>" ;\
	NCBI_SETTINGS=$(TEMPDIR)/tmp.mkfg \
	PATH=$(DIRTOTEST):$$PATH \
	SRATOOLS_TESTING=2 \
	SRATOOLS_IMPERSONATE=fastq-dump \
	$(BINDIR)/sratools --fasta 75 SRR390728 2>actual/$@.stderr ; \
	diff expected/$@.stderr actual/$@.stderr

fasta_neg_number: | actual mkfg
	@echo "testing expected output for fastq-dump --fasta -<number> <run>" ;\
	NCBI_SETTINGS=$(TEMPDIR)/tmp.mkfg \
	PATH=$(DIRTOTEST):$$PATH \
	SRATOOLS_TESTING=2 \
	SRATOOLS_IMPERSONATE=fastq-dump \
	$(BINDIR)/sratools --fasta -75 SRR390728 2>actual/$@.stderr ; \
	diff expected/$@.stderr actual/$@.stderr

.PHONY: runtests bogus container good $(CONTAINER) $(GOOD) vdbcache split_3 split_e NO_SDL
.INTERMEDIATE: $(TEMPDIR)/tmp.mkfg $(TEMPDIR)/tmp2.mkfg
