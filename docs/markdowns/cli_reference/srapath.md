# srapath

## Usage

```sh
srapath [options] <accession> ...
```

## Summary

Tool to produce a list of full paths to files
(SRA and WGS runs, refseqs: reference sequences)
from list of NCBI accessions.

Output paths are ordered according to accession list.

The accession search path will be determined according to the
configuration. It will attempt to find files in local and site
repositories, and will also check remote repositories for run
location.
This tool produces a path that is 'likely' to be a run, in that
an entry exists in the file system at the location predicted.
It is possible that this path will fail to produce success upon
opening a run if the path does not point to a valid object.

## Options

| Option | Description |
|---|---|
| -f\|--function | function to perform (resolve, names,&lt;br&gt;search) default=resolve or names if&lt;br&gt;protocol is specified |
| --location | location of data |
| -t\|--timeout | timeout-value for request |
| -a\|--protocol | protocol (fasp; http; https; fasp,http;&lt;br&gt;...) default=https |
| -e\|--vers | version-string for cgi-calls |
| -u\|--url | url to be used for cgi-calls |
| -p\|--param | param to be added to cgi-call (tic=XXXXX):&lt;br&gt;raw-only |
| -r\|--raw | print the raw reply (instead of parsing it) |
| -j\|--json | print the reply in JSON |
| -d\|--project | use numeric [dbGaP] project-id in&lt;br&gt;names-cgi-call |
| -c\|--cache | resolve cache location along with remote&lt;br&gt;when performing names function |
| -P\|--path | print path of object: names function-only |
| --perm &lt;PATH&gt; | PATH to jwt cart file |
| --ngc &lt;PATH&gt; | PATH to ngc file |
| -h\|--help | Output brief explanation for the program. |
| -V\|--version | Display the version of the program then&lt;br&gt;quit. |
| -L\|--log-level &lt;level&gt; | Logging level as number or enum string. One&lt;br&gt;of (fatal\|sys\|int\|err\|warn\|info\|debug) or&lt;br&gt;(0-6) Current/default is warn. |
| -v\|--verbose | Increase the verbosity of the program&lt;br&gt;status messages. Use multiple times for more&lt;br&gt;verbosity. Negates quiet. |
| -q\|--quiet | Turn off all status messages for the&lt;br&gt;program. Negated by verbose. |
| --option-file &lt;file&gt; | Read more options and parameters from the&lt;br&gt;file. |
