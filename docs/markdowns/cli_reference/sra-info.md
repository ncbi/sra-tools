# sra-info

## Usage

```text
sra-info <accession> [options]
```

## Options

| Option | Description |
|---|---|
| `-P`\|`--platform` | print platform(s) |
| `-Q`\|`--quality` | are quality scores stored or generated |
| `-A`\|`--is-aligned` | is data aligned |
| `-C`\|`--schema` | print schema version and dependencies |
| `-S`\|`--spot-layout` | print spot layout(s). Uses CONSENSUS table if present, SEQUENCE table otherwise |
| `-T`\|`--contents` | list the contents of the run: databases, tables, columns etc. |
| `-F`\|`--fingerprint` | show the fingerprint information. Detail level < 0 > (default) shows only the current run fingerprint. Description of fingerprint method available here: < LINK TBD > |
| `-f`\|`--format <format>` | output format: csv ..... comma separated values on one line xml ..... xml-style json .... json-style tab ..... tab-separated values on one line csv and tab formats can only be used with a single query |
| `--schema` | does not support csv and tab |
| `-l`\|`--limit <N>` | limit output to < N > elements, e.g. < N > most popular spot layouts; < N > must be positive |
| `-D`\|`--detail <N>` | detail level, < 0 > the least detailed output; < N > must be zero or greater; default 3 |
| `-R`\|`--rows <N>` | report spot layouts for the first < N > rows of the table |
| `-h`\|`--help` | Output brief explanation for the program. |
| `-V`\|`--version` | Display the version of the program then quit. |
| `-L`\|`--log-level <level>` | Logging level as number or enum string. One of (fatal\|sys\|int\|err\|warn\|info\|debug) or (0-6) Current/default is warn. |
| `-v`\|`--verbose` | Increase the verbosity of the program status messages. Use multiple times for more verbosity. Negates quiet. |
| `-q`\|`--quiet` | Turn off all status messages for the program. Negated by verbose. |
| `--option-file <file>` | Read more options and parameters from the file. |
