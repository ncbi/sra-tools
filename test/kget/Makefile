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

MODULE = test/kget

TEST_TOOLS = \

include $(TOP)/build/Makefile.env

.PHONY: $(TEST_TOOLS)

#-------------------------------------------------------------------------------
# runtests: 
#
runtests: download

download:
	export PATH=$(BINDIR):$$PATH; export VDB_CONFIG=`pwd` ; \
		python test_kget.py # expect rc = 0 

#-------------------------------------------------------------------------------
# slowtests: match output vs wget
#

slowtests: diff-vs-wget

ACCESSION = SRR072810
URL = http://ftp-trace.ncbi.nlm.nih.gov/sra/sra-instant/reads/ByRun/sra/SRR/SRR072/SRR072810/SRR072810.sra

clean:
	rm -f $(ACCESSION)*

diff-vs-wget: clean
	$(BINDIR)/kget --reliable -c ./$(ACCESSION).cachetee $(URL) $(ACCESSION).dat --progress
	wget $(URL)
	diff $(ACCESSION).sra ./$(ACCESSION).dat
	#diff $(ACCESSION).sra ./$(ACCESSION).cachetee
	rm -f $(ACCESSION)*


.PHONY: 
