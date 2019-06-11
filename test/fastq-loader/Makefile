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

#TODO: transfer slowtests/big_loads (see CVS)

default: runtests

TOP ?= $(abspath ../..)

MODULE = test/fastq-loader

TEST_TOOLS = \
	test-id2name \
	wb-test-fastq \
	wb-test-fastq-parse \
	test-fastq-loader

VDB_CONFIG = $(SRCDIR)

include $(TOP)/build/Makefile.env

$(TEST_TOOLS): makedirs
	@ $(MAKE_CMD) $(TEST_BINDIR)/$@

.PHONY: $(TEST_TOOLS)

clean: stdclean

#-------------------------------------------------------------------------------
# FASTQ parsing test
#
FASTQ_PARSE_SRC = \
	wb-test-fastq-parse

FASTQ_PARSE_OBJ = \
	$(addsuffix .$(OBJX),$(FASTQ_PARSE_SRC))

FASTQ_PARSE_LIB = \
	-skapp \
	-sktst \
	-sfastqloader \
	-sload \
	-sncbi-vdb \

$(TEST_BINDIR)/wb-test-fastq-parse: $(FASTQ_PARSE_OBJ)
	$(LP) --exe -o $@ $^ $(FASTQ_PARSE_LIB)

parse: wb-test-fastq-parse
	$(TEST_BINDIR)/wb-test-fastq-parse  2>&1

#-------------------------------------------------------------------------------
# id2name
#
ID2NAME_TEST_SRC = \
	test-id2name

ID2NAME_TEST_OBJ = \
	$(addsuffix .$(OBJX),$(ID2NAME_TEST_SRC))

ID2NAME_TEST_LIB = \
	-skapp \
	-sktst \
	-sfastqloader \
	-sncbi-vdb

$(TEST_BINDIR)/test-id2name: $(ID2NAME_TEST_OBJ)
	$(LP) --exe -o $@ $^ $(ID2NAME_TEST_LIB)

id: test-id2name
	$(TEST_BINDIR)/test-id2name  2>&1

#-------------------------------------------------------------------------------
# white-box test
#
FASTQ_TEST_SRC = \
	wb-test-fastq

FASTQ_TEST_OBJ = \
	$(addsuffix .$(OBJX),$(FASTQ_TEST_SRC))

FASTQ_TEST_LIB = \
	-skapp \
	-sktst \
	-sfastqloader \
	-sload \
	-sncbi-wvdb \

$(TEST_BINDIR)/wb-test-fastq: $(FASTQ_TEST_OBJ)
	$(LP) --exe -o $@ $^ $(FASTQ_TEST_LIB)

wb: wb-test-fastq
	$(TEST_BINDIR)/wb-test-fastq  2>&1

#-------------------------------------------------------------------------------
# test-fastqtest-loader
#
FASTQTEST_LOADER_SRC = \
	test-fastq-loader

FASTQTEST_LOADER_OBJ = \
	$(addsuffix .$(OBJX),$(FASTQTEST_LOADER_SRC))

$(TEST_BINDIR)/test-fastq-loader: $(FASTQTEST_LOADER_OBJ)
	$(LP) --exe -o $@ $^ $(FASTQ_TEST_LIB)

tfl:
	$(MAKE) -C $(OBJDIR) -f $(SRCDIR)/Makefile test-fastq-loader
	$(TEST_BINDIR)/test-fastq-loader

.PHONY: test-fastq-loader

#-------------------------------------------------------------------------------
# scripted tests
#
runtests: wb parse set_schema smalltests

set_schema:
	echo "vdb/schema/paths = \"$(VDB_INCDIR)\"" > tmp.kfg

SMALLRUN = @ NCBI_SETTINGS= VDB_CONFIG=$(SRCDIR) $(SRCDIR)/runtestcase.sh $(BINDIR) $(SRCDIR)

smalltests: $(TEST_TOOLS)
	-rm -rf $(SRCDIR)/actual
#   Small tests
#   Input:
#       Quality encoding: phred/logodds, ascii/decimal
#           1.1 phred 33
	$(SMALLRUN) 1.1 0 $(SRCDIR)/input/1.1.fastq --quality PHRED_33
#           1.2 phred 64
	$(SMALLRUN) 1.2 0 $(SRCDIR)/input/1.2.fastq --quality PHRED_64
#           1.3 phred 33, decimal qualities
	$(SMALLRUN) 1.3 3 $(SRCDIR)/input/1.3.fastq --quality PHRED_33
#           1.4 logodds 64
	$(SMALLRUN) 1.4 0 $(SRCDIR)/input/1.4.fastq --quality LOGODDS
#
#       Single input file
#           2.1 reads only, basespace
	$(SMALLRUN) 2.1 0 $(SRCDIR)/input/2.1.fastq --quality PHRED_33
#           2.1.1 reads only, colorspace
#CS	$(SMALLRUN) 2.1.1 0 $(SRCDIR)/input/2.1.1.fastq --quality PHRED_33
#           2.2 qualities only
	@#-$(SMALLRUN) 2.2 0 $(SRCDIR)/input/2.2.fastq --quality PHRED_33
#           2.3 reads+qualities, qualities before reads
	@#-$(SMALLRUN) 2.3 0 $(SRCDIR)/input/2.3.fastq --quality PHRED_33
#           2.4 reads+qualities, reads before qualities
	@#-$(SMALLRUN) 2.4 0 $(SRCDIR)/input/2.4.fastq --quality PHRED_33
#           2.5 reads+qualities, paired up
	$(SMALLRUN) 2.5 0 $(SRCDIR)/input/2.5.fastq --quality PHRED_33
#           2.6 spot assembly
	$(SMALLRUN) 2.6 0 $(SRCDIR)/input/2.6.fastq --quality PHRED_33
#           2.7 spot assembly, read numbers not specified
	$(SMALLRUN) 2.7 0 $(SRCDIR)/input/2.7.fastq --quality PHRED_33
#           2.8 handling of N in reads, ALTREAD is a static column
#           TODO; investigate intermittent failure (FIXED_SPOT_LEN == 0, not 3)
	@#-$(SMALLRUN) 2.8 0 $(SRCDIR)/input/2.8.fastq --quality PHRED_33
#           2.8.1 handling of N in reads, ALTREAD is a physical column
	$(SMALLRUN) 2.8.1 0 $(SRCDIR)/input/2.8.1.fastq --quality PHRED_33
#           2.9 mismatching length of sequence and quality (qualities shorter)
	$(SMALLRUN) 2.9 3 $(SRCDIR)/input/2.9.fastq --quality PHRED_33
#           2.9.1 mismatching length of sequence and quality (qualities longer)
	$(SMALLRUN) 2.9.1 3 $(SRCDIR)/input/2.9.1.fastq --quality PHRED_33
#
#       Multiple files
#           3.1 reads+qualities together
	$(SMALLRUN) 3.1 0 $(SRCDIR)/input/3.1a.fastq $(SRCDIR)/input/3.1b.fastq --quality PHRED_33
#           3.2 qualities separately from reads, reads file comes first
	@#-$(SMALLRUN) 3.2 0 $(SRCDIR)/input/3.2a.fastq $(SRCDIR)/input/3.2b.fastq --quality PHRED_33
#           3.3 qualities separately from reads, qualities file comes first (reject?)
	@#-$(SMALLRUN) 3.3 0 $(SRCDIR)/input/3.3a.fastq $(SRCDIR)/input/3.3b.fastq --quality PHRED_33
#           3.4 default read number
#	$(SMALLRUN) 3.4 0 -r 1 $(SRCDIR)/input/3.4a.fastq -r 2 $(SRCDIR)/input/3.4b.fastq --quality PHRED_33
#
#   Command line options:
#           4.1 --tmpfs             temporary files created at the specified location
	@echo "running 4.1"; export NCBI_SETTINGS=/; export VDB_CONFIG=`pwd`; \
	 export LD_LIBRARY_PATH=$(LIBDIR);$(BINDIR)/latf-load \
	 $(SRCDIR)/input/4.fastq --tmpfs $(SRCDIR)/actual/ -v \
	 -o actual/4.1.obj --quality PHRED_33 \
	 | grep "Path for scratch files: $(SRCDIR)/actual/" >/dev/null
#           4.2 --qual-quant        quality values are quantized as specified
	$(SMALLRUN) 4.2 0 $(SRCDIR)/input/4.fastq --qual-quant 1:10,10:20,20:30,30:- --quality PHRED_33
#           4.3 --cache-size        handled correctly
	@echo "running 4.3"; export NCBI_SETTINGS=/; export VDB_CONFIG=`pwd`; \
	 export LD_LIBRARY_PATH=$(LIBDIR); \
	 $(BINDIR)/latf-load $(SRCDIR)/input/4.fastq --cache-size 10 -v \
     -o actual/4.1.obj --quality PHRED_33 \
     | grep "Cache size: 10M" >/dev/null
#           4.3.1 --cache-size      rejected if invalid
	$(SMALLRUN) 4.3.1 3 $(SRCDIR)/input/4.fastq --cache-size BIG --quality PHRED_33
#           4.4 --max-rec-count     stops at the specified record #
#CS	$(SMALLRUN) 4.4 0 $(SRCDIR)/input/4.4.fastq --max-rec-count 3 --quality PHRED_33
#           4.5 --max-err-count     stops after the specified # of errors
	$(SMALLRUN) 4.5 3 $(SRCDIR)/input/4.5.fastq --max-err-count 2 --quality PHRED_33
#           4.5.1 --max-err-pct     fails if % of error records is too high
#CS	$(SMALLRUN) 4.5.1 3 $(SRCDIR)/input/4.5.1.fastq --quality PHRED_33  --max-err-count 10 --max-err-pct 20
#           4.6 --platform          saves correct value in the PLATFORM column
	$(SMALLRUN) 4.6 0 $(SRCDIR)/input/4.fastq -p LS454 --quality PHRED_33
#           4.6.1 --platform        rejects invalid values
	$(SMALLRUN) 4.6.1 3 $(SRCDIR)/input/4.fastq -p SOMETHINGELSE --quality PHRED_33
#           4.7 input file missing
	$(SMALLRUN) 4.7 3 $(SRCDIR)/input/not_there --quality PHRED_33
#   Gzipped input
	$(SMALLRUN) 5.0 0 $(SRCDIR)/input/5.0.fastq.gz --quality PHRED_33
#   Misparsed quality
	$(SMALLRUN) 6.0 0 $(SRCDIR)/input/6.0.fastq --quality PHRED_33
#   PACBIO fastq
	$(SMALLRUN) 7.1 0 $(SRCDIR)/input/7.1.fastq --platform=PACBIO --quality PHRED_33
	$(SMALLRUN) 7.2 0 $(SRCDIR)/input/7.2.fastq --platform=PACBIO --quality PHRED_33
	$(SMALLRUN) 7.3 0 $(SRCDIR)/input/7.3.fastq --platform=PACBIO --quality PHRED_33
#   secondary read numbers other than 3
	$(SMALLRUN) 8.0 0 $(SRCDIR)/input/8.0.fastq --quality PHRED_33
#   reject inconsistent secondary read numbers
	$(SMALLRUN) 8.1 3 $(SRCDIR)/input/8.1.fastq --quality PHRED_33
#   newest Illumina tag line variation ("_" instead of " /")
	$(SMALLRUN) 9.0 0 $(SRCDIR)/input/9.0.fastq --quality PHRED_33
#   accepting ascii quality values up to 126
	$(SMALLRUN) 10.0 0 $(SRCDIR)/input/10.0.fastq --quality PHRED_33
    # 2-file fasta, tag line Run.Spot.Read
	$(SMALLRUN) 11.0 0 $(SRCDIR)/input/11.0.1.fasta $(SRCDIR)/input/11.0.2.fasta --quality PHRED_33
    # 2-file fastq, tag line Run.Spot.Read
	$(SMALLRUN) 11.1 0 $(SRCDIR)/input/11.1.1.fastq $(SRCDIR)/input/11.1.2.fastq --quality PHRED_33
    # Illumina, ignore barcodes
	$(SMALLRUN) 12.0 0 $(SRCDIR)/input/12.0.fastq --quality PHRED_33 --ignore-illumina-tags
    # Illumina, use barcodes as spotgroups
	$(SMALLRUN) 12.1 0 $(SRCDIR)/input/12.1.fastq --quality PHRED_33
    # Illumina, use barcodes as spotgroups, alternative format
	$(SMALLRUN) 12.2 0 $(SRCDIR)/input/12.2.fastq --quality PHRED_33
    # Consume our own dog food (13.0.fastq created by fastq-dump )
	$(SMALLRUN) 13.0 0 $(SRCDIR)/input/13.0.fastq --quality PHRED_33
    # Consume our own dog food (13.1.fastq created by fastq-dump -F )
	$(SMALLRUN) 13.1 0 $(SRCDIR)/input/13.1.fastq --quality PHRED_33
    # SRA-2932: 30 is the preferred value for qualities when reading FASTA
	$(SMALLRUN) 14.0 0 $(SRCDIR)/input/14.0.fasta $(SRCDIR)/input/14.0.fasta --quality PHRED_33
    # VDB-3154: buffer insufficient
	$(SMALLRUN) 15.0 0 $(SRCDIR)/input/15.0.fastq --quality PHRED_33
    # VDB-3413: multi-line reads and qualities
	$(SMALLRUN) 16.0 0 $(SRCDIR)/input/16.0.fasta --quality PHRED_33 --platform=PACBIO
	$(SMALLRUN) 16.1 0 $(SRCDIR)/input/16.1.fastq --quality PHRED_33 --platform=PACBIO
	#
	#rm -rf $(SRCDIR)/actual

onetest:
	rm -rf $(SRCDIR)/actual
	$(SMALLRUN) 8.1 3 $(SRCDIR)/input/8.1.fastq --quality PHRED_33

