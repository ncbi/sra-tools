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


# compilers
CC = cc -c
CP = c++ -c

# linkers
LD = $(TOP)/build/ld.sun.cc.sh cc
LP = $(TOP)/build/ld.sun.cc.sh c++

# tool options
ifeq (prof, $(BUILD))
	PROF := -xpg
endif

ifeq (dbg, $(BUILD))
	OPT := -g
	NOPT := -g
	PED := -xc99=all -Xc -v
else
	OPT := -xO3
endif

BSTATIC = -Bstatic
BDYNAMIC = -Bdynamic
BEGIN_WHOLE_ARCHIVE = 
END_WHOLE_ARCHIVE = 

## build rules

# assembly
%.o: %.s
	$(CC) -o $@ $<

# executable image
%.o: %.c
	$(CC) -o $@ $(OPT) -D_LOGGING $(CFLAGS) -xMD $<
%.o: %.cpp
	$(CP) -o $@ $(OPT) -D_LOGGING $(CPFLAGS) -xMD $<
%.o: %.cxx
	$(CP) -o $@ $(OPT) -D_LOGGING $(CPFLAGS) -xMD $<

# non-optimized executable image
%.nopt.o: %.c
	$(CC) -o $@ $(NOPT) -D_LOGGING $(CFLAGS) -xMD $<
%.nopt.o: %.cpp
	$(CP) -o $@ $(NOPT) -D_LOGGING $(CPFLAGS) -xMD $<
%.nopt.o: %.cxx
	$(CP) -o $@ $(NOPT) -D_LOGGING $(CPFLAGS) -xMD $<

# relocatable image
%.pic.o: %.c
	$(CC) -o $@ -kPIC $(OPT) $(CFLAGS) -xMD $<
%.pic.o: %.cpp
	$(CP) -o $@ -kPIC $(OPT) $(CPFLAGS) -xMD $<
%.pic.o: %.cxx
	$(CP) -o $@ -kPIC $(OPT) $(CPFLAGS) -xMD $<

# non-optimized relocatable image
%.nopt.pic.o: %.c
	$(CC) -o $@ -kPIC $(NOPT) $(CFLAGS) -xMD $<
%.nopt.pic.o: %.cpp
	$(CP) -o $@ -kPIC $(NOPT) $(CPFLAGS) -xMD $<
%.nopt.pic.o: %.cxx
	$(CP) -o $@ -kPIC $(NOPT) $(CPFLAGS) -xMD $<


# non-optimized relocatable image, byte swapping
%.swap.nopt.pic.o: %.c
	$(CC) -o $@ -DSWAP_PERSISTED -kPIC $(NOPT) $(CFLAGS) -xMD $<
%.swap.nopt.pic.o: %.cpp
	$(CP) -o $@ -DSWAP_PERSISTED -kPIC $(NOPT) $(CPFLAGS) -xMD $<
%.swap.nopt.pic.o: %.cxx
	$(CP) -o $@ -DSWAP_PERSISTED -kPIC $(NOPT) $(CPFLAGS) -xMD $<

# relocatable image with kapp logging
%.log.pic.o: %.c
	$(CC) -o $@ -kPIC $(OPT) -D_LOGGING $(CFLAGS) -xMD $<
%.log.pic.o: %.cpp
	$(CP) -o $@ -kPIC $(OPT) -D_LOGGING $(CPFLAGS) -xMD $<
%.log.pic.o: %.cxx
	$(CP) -o $@ -kPIC $(OPT) -D_LOGGING $(CPFLAGS) -xMD $<

# non-optimized relocatable image with kapp logging
%.nopt.log.pic.o: %.c
	$(CC) -o $@ -kPIC $(NOPT) -D_LOGGING $(CFLAGS) -xMD $<
%.log.nopt.pic.o: %.c
	$(CC) -o $@ -kPIC $(NOPT) -D_LOGGING $(CFLAGS) -xMD $<
%.log.nopt.pic.o: %.cpp
	$(CP) -o $@ -kPIC $(NOPT) -D_LOGGING $(CPFLAGS) -xMD $<
%.log.nopt.pic.o: %.cxx
	$(CP) -o $@ -kPIC $(NOPT) -D_LOGGING $(CPFLAGS) -xMD $<


# non-optimized relocatable image with kapp logging, byte swapping
%.swap.nopt.log.pic.o: %.c
	$(CC) -o $@ -DSWAP_PERSISTED -kPIC $(NOPT) -D_LOGGING $(CFLAGS) -xMD $<
%.swap.log.nopt.pic.o: %.c
	$(CC) -o $@ -DSWAP_PERSISTED -kPIC $(NOPT) -D_LOGGING $(CFLAGS) -xMD $<
%.swap.log.nopt.pic.o: %.cpp
	$(CP) -o $@ -DSWAP_PERSISTED -kPIC $(NOPT) -D_LOGGING $(CPFLAGS) -xMD $<
%.swap.log.nopt.pic.o: %.cxx
	$(CP) -o $@ -DSWAP_PERSISTED -kPIC $(NOPT) -D_LOGGING $(CPFLAGS) -xMD $<

# assembly language output
%.s: %.c
	$(CC) -S -o $@ $(OPT) $(CFLAGS) -xMD $<
%.s: %.cpp
	$(CP) -S -o $@ $(OPT) $(CPFLAGS) -xMD $<
%.s: %.cxx
	$(CP) -S -o $@ $(OPT) $(CPFLAGS) -xMD $<

%.nopt.s: %.c
	$(CC) -S -o $@ $(NOPT) $(CFLAGS) -xMD $<
%.nopt.s: %.cpp
	$(CP) -S -o $@ $(NOPT) $(CPFLAGS) -xMD $<
%.nopt.s: %.cxx
	$(CP) -S -o $@ $(NOPT) $(CPFLAGS) -xMD $<

%.pic.s: %.c
	$(CC) -S -o $@ -kPIC $(OPT) $(CFLAGS) -xMD $<
%.pic.s: %.cpp
	$(CP) -S -o $@ -kPIC $(OPT) $(CPFLAGS) -xMD $<
%.pic.s: %.cxx
	$(CP) -S -o $@ -kPIC $(OPT) $(CPFLAGS) -xMD $<

%.nopt.pic.s: %.c
	$(CC) -S -o $@ -kPIC $(NOPT) $(CFLAGS) -xMD $<
%.nopt.pic.s: %.cpp
	$(CP) -S -o $@ -kPIC $(NOPT) $(CPFLAGS) -xMD $<
%.nopt.pic.s: %.cxx
	$(CP) -S -o $@ -kPIC $(NOPT) $(CPFLAGS) -xMD $<
