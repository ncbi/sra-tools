# fastq-load

## Usage

```text
fastq-load [options] -r run.xml -e experiment.xml -o output-path
```

## Options

| Option | Description |
|---|---|
| `-r`\|`--run-xml` | path to run.xml describing input files |
| `-e`\|`--experiment` | path to experiment.xml |
| `-o`\|`--output-path` | target location |
| `-i`\|`--input-path` | input files location, default '.' |
| `-u`\|`--input-unpacked` | input files are unpacked |
| `-t`\|`--input-no-threads` | disable input files threaded caching |
| `-f`\|`--force` | force target overwrite |
| `-n`\|`--spots-number` | process only given number of spots from input |
| `-bE`\|`--bad-spot-number` | acceptable number of spot creation errors, default is 50 |
| `-p`\|`--bad-spot-percentage` | acceptable percentage of spots creation errors, default is 5 |
| `-x`\|`--expected` | path to expected.xml |
| `-s`\|`--intensities` | [on off] load intensity data, default is off. For  Illumina: signal, intensity, noise; AB SOLiD: signal(s); LS454: signal, position (for SFF files this option is ON by default). |
| `-z`\|`--xml-log <logfile>` | Produce XML-formatted log file. |
| `-h`\|`--help` | Output brief explanation for the program. |
| `-V`\|`--version` | Display the version of the program then quit. |
| `-L`\|`--log-level <level>` | Logging level as number or enum string. One of (fatal\|sys\|int\|err\|warn\|info\|debug) or (0-6) Current/default is warn. |
| `-v`\|`--verbose` | Increase the verbosity of the program status messages. Use multiple times for more verbosity. Negates quiet. |
| `-q`\|`--quiet` | Turn off all status messages for the program. Negated by verbose. |
| `--option-file <file>` | Read more options and parameters from the file. |
