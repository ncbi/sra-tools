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

runtests: built-in bogus good

#.PHONY runtests

GOOD =\
	fastq-dump \
	fasterq-dump \
	sam-dump \
	sra-pileup \
	vdb-dump

good: $(GOOD)

actual:
	@ mkdir actual

built-in:
	@echo "running built-in tests" ; SRATOOLS_TESTING=1 $(BINDIR)/sratools

$(GOOD): | actual
	@echo "testing" $@ ; PATH=$(BINDIR):$$PATH SRATOOLS_DRY_RUN=2 SRATOOLS_IMPERSONATE=$@ $(BINDIR)/sratools SRR000001 2>actual/$@.stderr && \
	diff expected/$@.stderr actual/$@.stderr

bogus: | actual
	@echo "testing expected failure for unknown tool" ; SRATOOLS_IMPERSONATE=bogus $(BINDIR)/sratools 2>actual/$@.stderr ; \
	diff expected/$@.stderr actual/$@.stderr
