# ngs-pileup

## Usage

```text
ngs-pileup <path> [options]
```

## Options

| Option | Description |
|---|---|
| `-r`\|`--aligned-region <region>` | Filter by position on genome. Name can either be file specific or canonical (ex: "chr1" or "1"). "from" and "to" are 1-based coordinates |
| `--ngc <PATH>` | PATH to ngc file |
| `-h`\|`--help` | Output brief explanation for the program. |
| `-V`\|`--version` | Display the version of the program then quit. |
| `-L`\|`--log-level <level>` | Logging level as number or enum string. One of (fatal\|sys\|int\|err\|warn\|info\|debug) or (0-6) Current/default is warn. |
| `-v`\|`--verbose` | Increase the verbosity of the program status messages. Use multiple times for more verbosity. Negates quiet. |
| `-q`\|`--quiet` | Turn off all status messages for the program. Negated by verbose. |
| `--option-file <file>` | Read more options and parameters from the file. |
