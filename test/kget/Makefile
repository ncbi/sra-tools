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

CONFIGTOUSE ?= NCBI_SETTINGS
DIRTOTEST ?= $(BINDIR)
TOOLTOTEST = $(DIRTOTEST)/kget
ifneq ("$(wildcard $(TOOLTOTEST))","")
    BIN_EXISTS = 1
else
    BIN_EXISTS = 0
endif

.PHONY: $(TEST_TOOLS)

announce:
	@ echo Testing $(DIRTOTEST) CONFIGTOUSE=$(CONFIGTOUSE)

#-------------------------------------------------------------------------------
# runtests: 
#
ifdef PYTHON
ifeq (0,$(BIN_EXISTS))
runtests: no-test
else
runtests: announce download
endif

else
runtests: ;

endif

no-test:
	@ echo $(TOOLTOTEST) does not exist. Test skipped.

download:
	@ PATH=$(DIRTOTEST):$(PATH) VDB_CONFIG=`pwd` \
		$(CONFIGTOUSE)=/ NCBI_VDB_RELIABLE=y $(PYTHON) test_kget.py #expect rc=0

#-------------------------------------------------------------------------------
# slowtests: match output vs wget
#

slowtests: announce diff-vs-wget

ACCESSION = SRR072810
NAME = $(ACCESSION).wget
HOST = sra-download-internal.ncbi.nlm.nih.gov
URL=$(shell ${DIRTOTEST}/srapath ${ACCESSION})

clean:
	rm -f $(ACCESSION)*

diff-vs-wget: clean
	@ if echo $(URL) | grep -vq /sdlr/sdlr.fcgi?jwt= ; \
	  then \
	     $(DIRTOTEST)/kget --reliable -c ./$(ACCESSION).cachetee \
	                    $(URL) $(ACCESSION).dat --progress && \
	     wget --no-check-certificate $(URL) -O ./$(NAME) && \
	     diff ./$(NAME) ./$(ACCESSION).dat && \
	     rm -f $(ACCESSION)* ; \
	  else \
	     echo kget test when CE is required is skipped ; \
	  fi


.PHONY: 
