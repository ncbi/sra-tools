# vdb-diff

## Usage

```text
vdb-diff <src1_path> <src2_path> [options]
```

## Options

| Option | Description |
|---|---|
| -R\|--rows &lt;row-range&gt; | set of rows to be comparend (default all) |
| -C\|--columns &lt;column-set&gt; | set of columns to be compared (default<br>all) |
| -T\|--table &lt;table-name&gt; | name of table (in case of database to be<br>compared |
| -p\|--progress | show progress in percent |
| -e\|--maxerr &lt;max value&gt; | max errors im comparing (default 1) |
| -i\|--intersect | intersect column-set from both runs |
| -x\|--exclude &lt;column-set&gt; | exclude these columns from comapring |
| -c\|--col-by-col | exclude these columns from comapring |
| -h\|--help | Output brief explanation for the program. |
| -V\|--version | Display the version of the program then<br>quit. |
| -L\|--log-level &lt;level&gt; | Logging level as number or enum string. One<br>of (fatal\|sys\|int\|err\|warn\|info\|debug) or<br>(0-6) Current/default is warn. |
| -v\|--verbose | Increase the verbosity of the program<br>status messages. Use multiple times for more<br>verbosity. Negates quiet. |
| -q\|--quiet | Turn off all status messages for the<br>program. Negated by verbose. |
| --option-file &lt;file&gt; | Read more options and parameters from the<br>file. |
