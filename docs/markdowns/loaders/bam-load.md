# :fontawesome-solid-square-binary: bam-load

!!! note
    It is an NCBI internal tool used for processing submitted files into SRA format.

The bam-load program is the only binary for processing the SAM/BAM input to SRA. BAM is the second most popular file type submitted to SRA.

# Program Description
bam-load takes as input both SAM and BAM files. It follows the SAM specification. SAM is a text format, tab delimited fields with one record per line. BAM is a binary format, a sequence of small gzip'ped blocks (decompressed size <64K) each with a header and checksum. bam-load uses zlib to decompress; zlib validates the block checksums, and bam-load validates the block headers. As it is a binary format, BAM parsing is straight forward. For SAM, the spec is actually tighter than hts-lib has implemented, so bam-load parsing rules get relaxed when we find situations where hts-lib accepts an input that bam-load rejects.

There are additional rules and restrictions that bam-load has. SAM/BAM are flat data files; the output of bam-load is a database of cross-referenced tables. So bam-load has referential integrity rules. Like latf-load, bam-load does spot assembly. In addition, bam-load tracks alignments for each spot and takes pains to ensure that the records it outputs are correctly cross-referenced. In order to do that, bam-load must make changes to the data.

bam-load data changes:
The user can tell bam-load to discard all secondary alignments.

SEQ/QUAL orientation: SAM stores reads in reference orientation. SRA stores reads in the original (as read from the machine) orientation. If needed, SEQ will get reverse-complemented and QUAL will get reversed.

If a SAM file refers to the same reference by more than one name, bam-load will store the first one used.

A secondary alignment will become a primary alignment if it is the only alignment for a read and is not hard-clipped.

Hard-clipped primary alignments will be counted as an error and discarded unless the user tells bam-load to accept hard clipping.

bam-load tries to fix hard-clipped secondary alignments using information from the primary alignment.

Only the first primary alignment for a read is the primary alignment, any others will become secondary alignments.

Alignments may be considered as unaligned or be discarded entirely:

The user of bam-load can skip loading alignments to some references.
The user of bam-load can set a minimum MAPQ below which the alignment will not be considered an alignment.
There is a user-adjustable minimum matching base count below which the alignment will not be considered an alignment.
An alignment can overhang the end of a reference, according to the SAM spec, these are to be considered as unaligned.
FLAG can be changed for numerous reasons:

SRA has only one reject bit, SAM has two that get OR'ed.
A read can have only one record in SRA but multiple records in SAM which can have conflicting FLAG values, the first occurrence wins or it is an error.
A spot in SRA can have only one record with all of its constituent reads, but SAM will have multiple records per spot. Some of the reads can be missing.
Many of the other changes also effect FLAG.
Usage