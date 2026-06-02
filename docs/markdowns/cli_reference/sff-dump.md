# sff-dump

## Usage

```text
sff-dump [options] <path> [<path>...]
sff-dump [options] <accession>
```

## Options

| Option | Description |
|---|---|
| -A\|--accession &lt;accession&gt; | Replaces accession derived from &lt;path&gt; in<br>filename(s) and deflines (only for single<br>table dump) |
| -O\|--outdir &lt;path&gt; | Output directory, default is working<br>directory '.' ) |
| -Z\|--stdout | Output to stdout, all split data become<br>joined into single stream |
| --ngc &lt;path&gt; | &lt;path&gt; to ngc file |
| -N\|--minSpotId &lt;rowid&gt; | Minimum spot id |
| -X\|--maxSpotId &lt;rowid&gt; | Maximum spot id |
| -G\|--spot-group | Split into files by SPOT_GROUP (member name) |
| --spot-groups &lt;[list]&gt; | Filter by SPOT_GROUP (member): name[,...] |
| -R\|--read-filter &lt;[filter]&gt; | Split into files by READ_FILTER value<br>optionally filter by value:<br>pass\|reject\|criteria\|redacted |
| -T\|--group-in-dirs | Split into subdirectories instead of files |
| -K\|--keep-empty-files | Do not delete empty files |
| --table &lt;table-name&gt; | Table name within cSRA object, default is<br>"SEQUENCE" |
| --disable-multithreading | disable multithreading |
| -h\|--help | Output brief explanation of program usage |
| -V\|--version | Display the version of the program |
| -L\|--log-level &lt;level&gt; | Logging level as number or enum string One<br>of (fatal\|sys\|int\|err\|warn\|info) or (0-5)<br>Current/default is warn |
| -v\|--verbose | Increase the verbosity level of the program<br>Use multiple times for more verbosity |
| --ncbi_error_report | Control program execution environment<br>report generation (if implemented). One of<br>(never\|error\|always). Default is error |
| --legacy-report | use legacy style 'Written spots' for tool |
