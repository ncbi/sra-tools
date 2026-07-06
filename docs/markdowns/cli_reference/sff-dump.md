# sff-dump

## Usage

```text
sff-dump [options] <path> [<path>...]
sff-dump [options] <accession>
```

## Options

| Option | Description |
|---|---|
| `-A`\|`--accession <accession>` | Replaces accession derived from < path > in filename(s) and deflines (only for single table dump) |
| `-O`\|`--outdir <path>` | Output directory, default is working directory '.' ) |
| `-Z`\|`--stdout` | Output to stdout, all split data become joined into single stream |
| `--ngc <path>` | < path > to ngc file |
| `-N`\|`--minSpotId <rowid>` | Minimum spot id |
| `-X`\|`--maxSpotId <rowid>` | Maximum spot id |
| `-G`\|`--spot-group` | Split into files by SPOT_GROUP (member name) |
| `--spot-groups <[list]>` | Filter by SPOT_GROUP (member): name[,...] |
| `-R`\|`--read-filter <[filter]>` | Split into files by READ_FILTER value optionally filter by value: pass\|reject\|criteria\|redacted |
| `-T`\|`--group-in-dirs` | Split into subdirectories instead of files |
| `-K`\|`--keep-empty-files` | Do not delete empty files |
| `--table <table-name>` | Table name within cSRA object, default is "SEQUENCE" |
| `--disable-multithreading` | disable multithreading |
| `-h`\|`--help` | Output brief explanation of program usage |
| `-V`\|`--version` | Display the version of the program |
| `-L`\|`--log-level <level>` | Logging level as number or enum string One of (fatal\|sys\|int\|err\|warn\|info) or (0-5) Current/default is warn |
| `-v`\|`--verbose` | Increase the verbosity level of the program Use multiple times for more verbosity |
| `--ncbi_error_report` | Control program execution environment report generation (if implemented). One of (never\|error\|always). Default is error |
| `--legacy-report` | use legacy style 'Written spots' for tool |
