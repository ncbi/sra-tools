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

## Format of statistics file.

The statistics file is JSON text. For arrays of objects, the distinguishing member is highlighted. The top level is an object containing:

* `total`:
    * `reads`:
        * `total`:
            * `count`: the total number of reads.
            * `hash`:
                * `bases`: hash of actual bases (info only, **DO NOT COMPARE**).
                * `unstranded`: hash of base value or complement.
                * `sequence`: hash of the sequence (and its complement).
        * `biological`: same content as `total`, optional if `count` is 0.
        * `technical`: same content as `total`, optional if `count` is 0.
    * `bases`, an array of objects containing:
        * **`base`**: a string describing which bases are counted.
        * `biological`: the count, optional if 0.
        * `technical`: the count, optional if 0.
    * `spots`, an array of objects containing:
        * **`nreads`**: the number of reads in the spot.
        * `total`: the number of spots.
        * `biological`: the number of spots with a biological read.
        * `technical`: the number of spots with a technical read.
    * `distances`: e.g. the sequence `ATA` (or its complement) will give `A|T` at distance 2.
        * `A|T`, array of objects containing (an entry is optional if its `count` is 0):
            * **`distance`**: the distance from one occurence of a base to the next.
            * `count`: the number of times this distance occurs.
        * `C|G`: same content as the `A|T` node.
    * `references`: array of objects containing:
        * **`name`**: the name of the reference.
        * `chunks`: array of objects containing:
            * **`start`**: the starting position of the chunk (1 based).
            * `last`: the last position of the chunk.
            * `alignments`: the alignment's whose starting position >= `start` and <= `last`
                * `total`.
                * `5'`.
                * `3'`.
        * `alignments`:
            * `total`.
            * `5'`.
            * `3'`.
        * `position`: checksum of the starting positions (order insensitive).
        * `sequence`: checksum of the sequences (order insensitive).
        * `cigar`: checksum of the CIGARs (order insensitive).
* `groups`, an array of objects containing:
    * **`group`**: name of the spot group.
    * all of the nodes that `total` contains.

## Procedure to `diff` statistics files.

Run `diff-tool.py` on two putatively equivalent statistics files. The output
will be a list of the statistics that differ. No output means no differences.
The exit code will be 0 if there are no differences, otherwise it will be 1.

E.g.
```
sh make-input.sh original | qa-stats > original.stats

...
# later
...

sh make-input.sh new-load | qa-stats > new-load.stats
python3 diff-tool.py original.stats new-load.stats
```

### Testing `qa-stat`

There is a Perl script `make-test-input.pl` that will shuffle the input records
and randomly swap reads or reverse-complement some sequences. The result should
not cause any differences in the statistics.

It is easy to get real differences in the statistics. For example, use different
parts of the input.

https://maps.google.com/maps?cid=2752778480423147732