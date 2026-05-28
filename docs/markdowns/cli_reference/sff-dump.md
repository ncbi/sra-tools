# sff-dump

## Usage

```sh
[OPTIONS]
./sff-dump [options] <path> [<path>...]
./sff-dump [options] <accession>
-A|--accession <accession>       Replaces accession derived from <path> in
filename(s) and deflines (only for single
table dump)
-O|--outdir <path>               Output directory, default is working
directory '.' )
-Z|--stdout                      Output to stdout, all split data become
joined into single stream
--ngc <path>                     <path> to ngc file
-N|--minSpotId <rowid>           Minimum spot id
-X|--maxSpotId <rowid>           Maximum spot id
-G|--spot-group                  Split into files by SPOT_GROUP (member name)
--spot-groups <[list]>           Filter by SPOT_GROUP (member): name[,...]
-R|--read-filter <[filter]>      Split into files by READ_FILTER value
optionally filter by value:
pass|reject|criteria|redacted
-T|--group-in-dirs               Split into subdirectories instead of files
-K|--keep-empty-files            Do not delete empty files
--table <table-name>             Table name within cSRA object, default is
"SEQUENCE"
--disable-multithreading         disable multithreading
-h|--help                        Output brief explanation of program usage
-V|--version                     Display the version of the program
-L|--log-level <level>           Logging level as number or enum string One
of (fatal|sys|int|err|warn|info) or (0-5)
Current/default is warn
-v|--verbose                     Increase the verbosity level of the program
Use multiple times for more verbosity
--ncbi_error_report              Control program execution environment
report generation (if implemented). One of
(never|error|always). Default is error
--legacy-report                  use legacy style 'Written spots' for tool
```

## Options

| Option | Description |
|---|---|
| -h\|--help | Output brief explanation for the program. |
| -V\|--version | Display the version of the program then&lt;br&gt;quit. |
| -L\|--log-level &lt;level&gt; | Logging level as number or enum string. One&lt;br&gt;of (fatal\|sys\|int\|err\|warn\|info\|debug) or&lt;br&gt;(0-6) Current/default is warn. |
| -v\|--verbose | Increase the verbosity of the program&lt;br&gt;status messages. Use multiple times for more&lt;br&gt;verbosity. Negates quiet. |
| -q\|--quiet | Turn off all status messages for the&lt;br&gt;program. Negated by verbose. |
| --option-file &lt;file&gt; | Read more options and parameters from the&lt;br&gt;file. |
