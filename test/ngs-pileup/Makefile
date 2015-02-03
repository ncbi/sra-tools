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

MODULE = test/ngs-pileup

TEST_TOOLS = \
    test-ngs-pileup

include $(TOP)/build/Makefile.env

$(TEST_TOOLS): makedirs
	@ $(MAKE_CMD) $(TEST_BINDIR)/$@

.PHONY: $(TEST_TOOLS)

clean: stdclean

#-------------------------------------------------------------------------------
# test-ngs-pileup
#
TEST_NGS_PILEUP_SRC = \
	test-ngs-pileup 

TEST_NGS_PILEUP_OBJ = \
	$(addsuffix .$(OBJX),$(TEST_NGS_PILEUP_SRC))
    
TEST_NGS_PILEUP_LIB =   \
	-sngs-c++           \
	-sncbi-ngs-c++      \
	-sncbi-vdb-static   \
	-skapp              \
    -sktst              \

$(TEST_BINDIR)/test-ngs-pileup: $(TEST_NGS_PILEUP_OBJ)
	$(LP) --exe -o $@ $^ $(TEST_NGS_PILEUP_LIB)

valgrind_ngs_pileup: test-ngs-pileup
	valgrind --ncbi --suppressions=$(SRCDIR)/valgrind.suppress $(TEST_BINDIR)/test-ngs-pileup 
    
#-------------------------------------------------------------------------------
# slowtests: match output vs sra-pileup
#

slowtests: diff-vs-sra-pileup

diff-vs-sra-pileup:
	#@ ./runtestcase.sh $(BINDIR) $(SRCDIR) 1.0 SRR833251
	#@ ./runtestcase.sh $(BINDIR) $(SRCDIR) 2.0 SRR1166393
	#@ ./runtestcase.sh $(BINDIR) $(SRCDIR) 3.0 ERR334733                # ILLUMINA
	#@ ./runtestcase.sh $(BINDIR) $(SRCDIR) 4.0 SRR1068006
	#@ ./runtestcase.sh $(BINDIR) $(SRCDIR) 5.0 SRR619510                # ABSOLID REF_ROWS=36172
	#@ ./runtestcase.sh $(BINDIR) $(SRCDIR) 6.0 SRR1251988               # SEC_ROWS=198
	#@ ./runtestcase.sh $(BINDIR) $(SRCDIR) 7.0 SRR1186012               # ION_TORRENT
	@ ./runtestcase.sh $(BINDIR) $(SRCDIR) 7.0 SRR556739 -r chrY # COMPLETE_GENOMICS
	#@ ./runtestcase.sh $(BINDIR) $(SRCDIR) x.0 SRR556739 -r chrM # COMPLETE_GENOMICS, circular reference

#alignment selection:
#TODO: multiple references in one accession
#TODO: multiple accessions with overlapping alignments
#TODO: filter by reference (-r ref) using canonic name
#TODO: filter by reference (-r ref) using common name 
#TODO: filter by multiple references (-r ref1 -r ref2)
#TODO: filter by position on the reference (-r ref:from-to)
#TODO: filter by multiple positions on the same reference (-r ref:from1-to1 -r ref:from2-to2)
#TODO: primary alignment table only (-t p)
#TODO: secondary alignment table only (-t s)
#TODO: evidence  table only (-t e)
#TODO: include duplicates (-d 1)
#TODO: do not include duplicates (-d 0) NB: broken in sra-pileup
#TODO: minimum mapq value (-q min)

#output control:
#TODO: omit qualities (-n)

#output file control:
#TODO: redirect output to a file (-o)
#TODO: bzip2 output (--bzip2)
#TODO: gzip output (--gzip)

# hidden options:
#TODO: output alignment information (-i)
# any more?

#not sure if these are needed:
#  -p|--spotgroups <spotgroups-modes>  divide by spotgroups 
#  -e|--seqname                     use original seq-name 
#  --minmismatch                    min percent of mismatches used in function 
#                                   mismatch, default is 5% 
#  --merge-dist                     If adjacent slices are closer than this,  
#                                   they are merged and skiplist is created.  
#                                   a value of zero disables the feature, 
#                                   default is 10000 
#  --function ref                   list references 
#  --function ref-ex                list references coverage 
#  --function count                 sort pileup with counters 
#  --function stat                  strand/tlen statistic 
#  --function mismatch              only lines with mismatch 
#  --function index                 list deletion counts 
#  --function varcount              variation counters:  ref-name, ref-pos, 
#                                   ref-base, coverage,  mismatch A, mismatch C, 
#                                   mismatch G, mismatch T, deletes, inserts, ins 
#                                   after A, ins after C, ins after G, ins 
#                                   after T 
#  --function deletes               list deletions greater then 20 
#
#Grouping of accessions into artificial spotgroups:
#  sra-pileup SRRXXXXXX=a SRRYYYYYY=b SRRZZZZZZ=a



.PHONY: diff-vs-sra-pileup
