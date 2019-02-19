## Purpose:
Assemble paired-end reads into fragments, ordered by reference position

## Input:
1. SRA aligned archive
1. SAM/BAM

## Stages:
1. Group all of fragments' alignments by spot name / spot id
1. Remove problem fragments
1. Reorder by aligned position
1. Build contiguous regions from overlapping fragments
1. Assign each fragment to its best-fit contiguous region
1. Create virtual references from split contiguous regions (split means mates aligned to two different references or the mate-pair gap wasn't closed)
1. Reorder fragments to aligned positions on the virtual references

## Output:
A vdb database containing

1. a table of ordered aligned fragments
1. a table describing virtual references
1. a table of unaligned fragments

## Tools:
1. `text2ir` - provides a simple way to load records into an IR table.
    It's expected use is for generating test data from a text file.
    The format is tab-delimited text, the columns are `GROUP, NAME, READNO, SEQUENCE, REFERENCE, STRAND, POSITION, CIGAR`.
    The last 4 columns are required for aligned records and must be removed for unaligned records.
    Example:
    ```
    text2ir < testdata.txt | general-loader --include include --schema ./schema/aligned-ir.schema.text --target test.IR
    ```
1. `sra2ir` - provides a way to load an IR table from an existing SRA run. 
    It can filter by reference and region.
1. `reorder-ir` - clusters IR table by GROUP and NAME, which is needed by `filter-ir`
    Uses a gigaton of virtual memory (maybe).
    Example:
    ```
    reorder-ir test.IR | general-loader --include include --schema ./schema/aligned-ir.schema.text --target test.sorted.IR
    ```
1. `filter-ir` - removes problem fragments
    Moves problem fragments from `RAW` table to `DISCARDED` table.
    Problems are:
    1. fragment contains unaligned reads
    1. fragment contains reads with inconsistent sequence
    1. fragment contains alignments with bad CIGAR strings
    Example:
    ```
    filter-ir test.sorted.IR | general-loader --include include --schema ./schema/aligned-ir.schema.text --target test.filtered.IR
    ```
1. `summarize-pairs` - for generating contiguous regions
    1. `summarize-pairs map` - generates a reduces representation of fragment alignments.
        The output needs to be sorted with
        ```
        sort -k1,1 -k2n,2n -k3n,3n -k4,4 -k5n,5n -k6n,6n
        ```
    1. `summarize-pairs reduce` - generates contiguous regions from overlapping fragment alignments.
        Writes to table `CONTIGS`
        Example:
        ```
        summarize-pairs map test.filtered.IR | sort -k1,1 -k2n,2n -k3n,3n -k4,4 -k5n,5n -k6n,6n | summarize-pairs reduce - | ./general-loader --include include --schema ./schema/aligned-ir.schema.text --target test.contigs
        ```
1. `assemble-fragments` - assigns one alignment to each fragment and writes a fragment alignment.
    Example:
    ```
    assemble-fragments test.filtered.IR test.contigs | ./general-loader --include include --schema ./schema/aligned-ir.schema.text --target test.fragments
    ```

## Building:
This project uses cmake. It will attempt to locate your ncbi-vdb install in the usual places. It will attempt to locate the ncb-vdb header files in some `../ncbi-vdb/interfaces` directory relative to the source.

### Virtual references
> There's no problem in computer science that can't be simplified by yet another indirection

And virtual references are yet another indirection, designed to simplify the problem that
no real DNA fragment can span two chromosomes. Therefore, it must be a fragment of some
non-reference entity built from the two chromosomes to which it aligns. Additionally,
virtual references can simplify the problem of duplicated regions within the reference.
Rather than having secondary alignments, any fragments that align fully within a
duplicated region can be assigned to a virtual reference that is known to occur in more
than one place.
