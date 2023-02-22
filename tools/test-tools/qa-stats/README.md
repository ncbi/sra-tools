# Generate and compare loader QA statistics.

## Purpose:
There are various changes that loaders may legitimately apply to input data.
These changes are expected to vary from one loader (version) to another.
In order to assure that no data was unexpectedly changed, we want to gather
statistics that would be insensitive to the legitimate changes that loaders
may make.

### Legitimate changes that loaders may make:

1. The order of records may change.
2. Read orientation may change.
3. Read number may change.
4. Alignment type (primary vs secondary) may change.

This is not an exhaustive list, but these are the sort of changes for which we
are trying to account.

## Procedure to generate statistics file.

Run `make-input.sh` on the load result and pipe the output into the 
`qa-stats` tool.

E.g.
```
sh make-input.sh new-load | qa-stats > new-load.stats
```

## Procedure to `diff` statistics files.

Run `diff-tool.py` on two putatively equivalent statistics files. The output
will be a list of the statistics that differ. No output means no differences.
The exit code will be 0 if there are no differences, otherwise it will be 1.

E.g., continuing from above:
```
sh make-input.sh original | qa-stats > original.stats
python3 diff-tool.py original.stats new-load.stats
```

## Input formats.

Lines starting with '#' are echoed to `stderr`.
The records may come in any order or grouping.

The main input format:
- is tab-delimited text.
- can be generated trivially with `vdb-dump`.
* Alignment records
    1. The sequence
        - one read.
        - in reference orientation.
        - like in SAM.
    2. Reference name.
    3. Aligned position.
        - 0-based
        - not like in SAM.
    4. Reference strand
        - 5': `0|false`, 3': `1|true`.
    5. CIGAR.
        - like in SAM.
    6. Spot group
        - optional.
* Sequence records
    1. The sequence
        - will be broken up into individual reads.
        - does not need to have any bases for aligned reads.
    2. Read lengths
        - comma separated numeric values.
        - technically, optional.
    3. Read starts
        - comma separated numeric values.
        - if missing, is computed from the read lengths.
    4. Read types and orientations
        - comma separated values from `SRA_READ_TYPE_`.
        - e.g. `SRA_READ_TYPE_TECHNICAL|SRA_READ_TYPE_REVERSE`.
        - default is `SRA_READ_TYPE_BIOLOGICAL|SRA_READ_TYPE_FORWARD`.
    5. Read is aligned
        - comma separated numeric values.
        - 0 means not aligned.
        - default is 0.
    6. Spot group
        - optional.

Sequence records are used to count spots. For unaligned reads, sequence records
are also used to count bases. Alignment records are used to count 
alignments **and** bases. Sequence records are needed for spots
that have alignments, but it is not necessary to include the aligned bases in
their sequence records (they are ignored).
(And thus you can use `CMP_READ` for a big performance gain.)

Additionally, `qa-stats` accepts FASTQ and SAM inputs, but still a work-in-progress.

## Format of the statistics file.

The statistics file is JSON text. For arrays of objects, the distinguishing
member is highlighted. The top level is an object containing:

- `total`:
  - `reads`:
    - `count`: the total number of reads.
    - `biological`: the total number of biological reads.
    - `technical`: the total number of technical reads.
  - `bases`, an array of objects containing:
    - **`base`**: a string describing which bases are counted; `W`: `A` or `T`, `S`: `C` or `G`.
    - `biological`: the count, optional if 0.
    - `technical`: the count, optional if 0.
  - `spots`, an array of objects containing:
    - **`nreads`**: the number of reads in the spot.
    - `total`: the number of spots.
    - `biological`: the number of spots with a biological read.
    - `technical`: the number of spots with a technical read.
  - `spot-layouts`, array of objects containing:
    - **`descriptor`**: a string describing the spot layout (`<length><B|T><F|R>...`).
    - `count`: the number of spots with this layout.
  - `spectra`: number of bases between occurrences of the same event (base or transition).
    - `W`, i.e. `|A-A|` + `|T-T|`
    - `S`, i.e. `|C-C|` + `|G-G|`
    - `S-W`, i.e. transitions between `C|G` and `A|T`
    - `K-M`, i.e. transitions between `G|T` and `A|C`
      - **`wavelength`**: in bases from one occurence to the next.
      - `power`: the proportion of the total.
  - `references`: array of objects containing:
    - **`name`**: the name of the reference.
    - `chunks`: array of objects containing:
      - **`start`**: the starting position of the chunk (1 based).
      - `last`: the last position of the chunk.
      - `alignments`: starting position is within the chunk.
        - `total`.
        - `5'`.
        - `3'`.
    - `alignments`:
      - `total`.
      - `5'`.
      - `3'`.
    - `position`: checksum of the starting positions and strand.
    - `sequence`: checksum of the sequences.
    - `cigar`: checksum of the CIGARs.
- `groups`, an array of objects containing:
  - **`group`**: name of the spot group.
  - plus all of the nodes that `total` contains.

Counts based on base values (like `bases` and `spectra`) are binned so that
complementary bases end up in the same bin. For alignments, the sequence is
always given in reference orientation, i.e. the reverse complement of the
sequence is irrelevant.

### Testing `qa-stat`

There is a script `make-test-input.pl` that will generate test records. This
script also adds comments to the file with some stats. These could be used to
verify some of the stats generated by `qa-stats`. There is a script `reverse.pl`
that will reverse complement all sequence reads (alignment reads are unchanged).
You can reorder with `sort --random-sort`. The result of `reverse.pl` or of
reordering (or any combination) should not cause any differences in the
statistics.

It is easy to get real differences in the statistics. For example, use different
parts of the input.
