# Purpose:

This table contains all the `SRA_PLATFORM` values. Its purpose is to
test that `vdb-dump` can output all of the values, and undefined values
as 'undefined'.

When a new platform is added, it is usually only necessary to update the
expected results file in `../expected/7.0.stdout`. But occasionally, we
will run out of values, and need to add more columns to this table. After
updating the expected results file, remember to inspect it and verify
its contents before checking it in.

See the example usage section below.

## To update this table:

Run `make-platform-test` which is build by
`tools/test-tools/make-platform-test`. 

`make-platform-test` can optionally take the path to the schema files
from the environment variable `VDB_INCDIR`. It can optionally take the
path as an argument.

## NB.
`make-platform-test` expects to be run in the parent directory of
this file.

## Output
If successful, `make-platform-test` will output to stderr a line like:
```
Wrote 27 platform values + 5 undefined.
```
If it does not output this, it failed, mostly likely because it could
not locate the schema files.

## A complete example usage:
```
> git rm -rf platforms                          
> cd .. # run in parent dir.
> make-platform-test /path/to/ncbi-vdb/interfaces
Will add include path '/path/to/ncbi-vdb/interfaces'.
Wrote 27 platform values + 5 undefined.
> kar -d input/platforms -c input/platforms.kar
> rm -rf input/platforms
> mv input/platforms.kar input/platforms
> vdb-dump input/platforms > expected/7.0.stdout
```

