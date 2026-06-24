# pacbio-load

## Summary

Loads pacbio hd5 data into an SRA archive.

## Usage

```text
pacbio-load <hdf5-file> -o<target>
```

## Options

| Option | Description |
|---|---|
| `-o`\|`--output <output>` | target to be created |
| `-S`\|`--schema <schema>` | schema-name to be used |
| `-f`\|`--force <force>` | forces an existing target to be overwritten |
| `-t`\|`--tabs <tabs>` | load only these tabs (SCPM), dflt=all S...Sequence C...Consensus P...Passes M...Metrics |
| `-p`\|`--with_progressbar <load-progress>` | show load-progress |
| `-z`\|`--xml-log <logfile>` | Produce XML-formatted log file. |
| `-h`\|`--help` | Output brief explanation for the program. |
| `-V`\|`--version` | Display the version of the program then quit. |
| `-L`\|`--log-level <level>` | Logging level as number or enum string. One of (fatal\|sys\|int\|err\|warn\|info\|debug) or (0-6) Current/default is warn. |
| `-v`\|`--verbose` | Increase the verbosity of the program status messages. Use multiple times for more verbosity. Negates quiet. |
| `-q`\|`--quiet` | Turn off all status messages for the program. Negated by verbose. |
| `--option-file <file>` | Read more options and parameters from the file. |
