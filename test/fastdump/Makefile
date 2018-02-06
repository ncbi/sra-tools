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

MODULE = test/fastdump

TEST_TOOLS = 

include $(TOP)/build/Makefile.env

.PHONY: $(TEST_TOOLS)

slowtests: fastdump1 fastdump2

ACC = SRR341578
SCRATCH ?= /tmp/$(shell whoami)
THREADS = 4

fastdump1: prepare $(BINDIR)/fastdump $(BINDIR)/vdb-dump
	@ mkdir -p $(SCRATCH)
	@./test_no_1.sh $(ACC) $(SCRATCH)/ $(THREADS) $(BINDIR)

fastdump2: $(BINDIR)/fastdump $(BINDIR)/vdb-dump
	@ mkdir -p $(SCRATCH)
	@./test_no_2.sh $(ACC) $(SCRATCH)/ $(THREADS) $(BINDIR)

prepare:
	@ export BINDIR=$(BINDIR) ; export VDB_INCDIR=$(VDB_INCDIR) ; \
        ./copy-default-kfg.sh
