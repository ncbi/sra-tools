# var-expand

## Summary

For each pair (key, variation spec) in input produces the expanded variation spec

## Options

| Option | Description |
|---|---|
| `--algorithm <value>` | the algorithm to use for searching. "sw" means Smith-Waterman. "ra" means Rolling bulldozer algorithm |
| `-z`\|`--xml-log <logfile>` | Produce XML-formatted log file. |
| `-h`\|`--help` | Output brief explanation for the program. |
| `-V`\|`--version` | Display the version of the program then quit. |
| `-L`\|`--log-level <level>` | Logging level as number or enum string. One of (fatal\|sys\|int\|err\|warn\|info\|debug) or (0-6) Current/default is warn. |
| `-v`\|`--verbose` | Increase the verbosity of the program status messages. Use multiple times for more verbosity. Negates quiet. |
| `-q`\|`--quiet` | Turn off all status messages for the program. Negated by verbose. |
| `--option-file <file>` | Read more options and parameters from the file. |

## Input

the stream of lines in the format: < key > < tab > < input variation >
