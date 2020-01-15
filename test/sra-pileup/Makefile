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

MODULE = test/sra-pileup

TEST_TOOLS = 

include $(TOP)/build/Makefile.env

$(TEST_TOOLS): makedirs
	@ $(MAKE_CMD) $(TEST_BINDIR)/$@

runtests: check_exit_code check_skiplist

slowtests: fastq_dump_vs_sam_dump sam_dump_spotgroup_for_all

#-------------------------------------------------------------------------------
# scripted tests
#

check_exit_code:
	@ python $(TOP)/build/check-exit-code.py $(BINDIR)/sra-pileup

check_skiplist:
	@ python check_skiplist.py $(BINDIR)/sra-pileup

ACC = SRR3332402

#-------------------------------------------------------------------------------
# testing if sam-dump and fastq-dump produce the same READ/QUALITY values
#
fastq_dump_vs_sam_dump :
	@ python test_diff_fastq_dump_vs_sam_dump.py -a $(ACC) -f $(BINDIR)/fastq-dump -m $(BINDIR)/sam-dump

#-------------------------------------------------------------------------------
# testing if sam-dump and fastq-dump produce the same READ/QUALITY values
#
sam_dump_spotgroup_for_all :
	@ python test_all_sam_dump_has_spotgroup.py -a $(ACC) -m $(BINDIR)/sam-dump

.PHONY: $(TEST_TOOLS)

clean: stdclean
