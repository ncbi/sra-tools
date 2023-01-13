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

E.g.
```
sh make-input.sh original | qa-stats > original.stats

...
# later
...

sh make-input.sh new-load | qa-stats > new-load.stats
python3 diff-tool.py original.stats new-load.stats
```
