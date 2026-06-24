# vdb-diff

## Usage

```text
vdb-diff <src1_path> <src2_path> [options]
```

## Options

| Option | Description |
|---|---|
| `-R`\|`--rows <row-range>` | set of rows to be comparend (default all) |
| `-C`\|`--columns <column-set>` | set of columns to be compared (default all) |
| `-T`\|`--table <table-name>` | name of table (in case of database to be compared |
| `-p`\|`--progress` | show progress in percent |
| `-e`\|`--maxerr <max value>` | max errors im comparing (default 1) |
| `-i`\|`--intersect` | intersect column-set from both runs |
| `-x`\|`--exclude <column-set>` | exclude these columns from comapring |
| `-c`\|`--col-by-col` | exclude these columns from comapring |
| `-h`\|`--help` | Output brief explanation for the program. |
| `-V`\|`--version` | Display the version of the program then quit. |
| `-L`\|`--log-level <level>` | Logging level as number or enum string. One of (fatal\|sys\|int\|err\|warn\|info\|debug) or (0-6) Current/default is warn. |
| `-v`\|`--verbose` | Increase the verbosity of the program status messages. Use multiple times for more verbosity. Negates quiet. |
| `-q`\|`--quiet` | Turn off all status messages for the program. Negated by verbose. |
| `--option-file <file>` | Read more options and parameters from the file. |
