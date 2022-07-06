-------------------------------------------------------------------
    do not include in the build-system
-------------------------------------------------------------------

the files here are the result of an investigation into the possibility
to produce SAM-output from a cSRA without a vdb-cache-file in approx.
the same time as sam-dump with a vdb-cache-file.

The answer is yes, it is possible with the help of some lookup-files.

files:
--------------------------------------------------------------------

full.sh             ... execute all 4 steps ( 2 of them in parallel )
                        this is to be executed to perform the equivalent
                        of sam-dump ( but without headers, and only aligned data )
                        
step[1|2|3|4].sh    ... bash-script for the each of the 4 steps
step[1|2|3|4].cpp   ... source-code for the binary in each step

tools.hpp           ... common code used in the .cpp files
mmap.hpp            ...     --- " ---
rr_lookup.hpp       ...     --- " ---

extract_row_count.cpp   ... does what it's name says, used by step1.sh

explain_flags.cpp   ... helper binary to explain the bits of a decimal SAM-flag value
sam-cmp.cpp         ... helper to compare 1 column between 2 SAM-files

sam-format-computation.*    ... graphical explanation of the algorithm to
                                produce SAM with lookup-tables
                                
acc.sh              ... helper script to download accession if not present

prefetch.sh         ... helper script to locate the prefetch-binary
vdb-dump.sh         ... helper script to locate the vdb-dump-binary
