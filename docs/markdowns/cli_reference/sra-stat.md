# sra-stat

## Summary

Sorts an SRA archive by reference position - similar to samtools sort.

Display table statistics

## Usage

```text
sra-stat [options] table
```

## Options

| Option | Description |
|---|---|
| `-x`\|`--xml` | Output as XML, default is text. |
| `-b`\|`--start <row-id>` | Starting spot id, default is 1. |
| `-e`\|`--stop <row-id>` | Ending spot id, default is max. |
| `-m`\|`--meta` | Print load metadata. |
| `-q`\|`--quick` | Quick mode: get statistics from metadata; do not scan the table. |
| `--member-stats <on `\|` off>` | Print member stats, default is on. |
| `--archive-info` | Output archive info, default is off. |
| `-s`\|`--statistics` | Calculate READ_LEN average and standard deviation. |
| `-a`\|`--alignment <on `\|` off>` | Print alignment info, default is on. |
| `-l`\|`--local-info` | Print the date, path, size and md5 of local run. |
| `-p`\|`--show_progress` | Show the percentage of completion. |
| `--ngc <path>` | Path to ngc file. |
| `-z`\|`--xml-log <logfile>` | Produce XML-formatted log file. |
| `--repair-data` | Generate data for repair tool. |
| `--info` | Print report for all fields examined for mismatch even if the old value is correct. |
| `-h`\|`--help` | Output brief explanation for the program. |
| `-V`\|`--version` | Display the version of the program then quit. |
| `-L`\|`--log-level <level>` | Logging level as number or enum string. One of (fatal\|sys\|int\|err\|warn\|info\|debug) or (0-6) Current/default is warn. |
| `-v`\|`--verbose` | Increase the verbosity of the program status messages. Use multiple times for more verbosity. Negates quiet. |
| `-q`\|`--quiet` | Turn off all status messages for the program. Negated by verbose. |
| `--option-file <file>` | Read more options and parameters from the file. |
