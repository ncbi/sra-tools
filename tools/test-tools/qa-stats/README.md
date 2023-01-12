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
