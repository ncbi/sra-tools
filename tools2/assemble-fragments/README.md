# Purpose:
Assemble paired-end reads into fragments, ordered by reference position

# Input:
1. SRA aligned archive
1. SAM/BAM

# Stages:
1. Group all of fragments' alignments by spot name / spot id
1. Remove problematic fragments
1. Create virtual references
1. Translate alignments to the virtual references
1. Reorder fragments to aligned positions on the virtual references

# Output:
A vdb database containing

1. a table of ordered aligned fragments
1. a table describing virtual references
1. a table of unaligned fragments

# Virtual references
> There's no problem in computer science that can't be simplified by yet another indirection

And virtual references are yet another indirection, designed to simplify the problem that
no real DNA fragment can span two chromosomes. Therefore, it must be a fragment of some
non-reference entity built from the two chromosomes to which it aligns. Additionally,
virtual references can simplify the problem of duplicated regions within the reference.
Rather than having secondary alignments, any fragments that align fully within a
duplicated region can be assigned to a virtual reference that is known to occur in more
than one place.
