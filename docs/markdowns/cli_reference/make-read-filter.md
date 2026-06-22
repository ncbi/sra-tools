# make-read-filter

## Summary

Make/Update RD_FILTER from QUALITY.

## Usage

```text
make-read-filter [options] <input>
```

## Options

| Option | Description |
|---|---|
| `-t`\|`--temp <path>` | temp directory to use for scratch space, default: $TMPDIR or $TEMPDIR or $TEMP or $TMP or /tmp |
| `--vdbcache <path>` | location of .vdbcache to update |
| `-h`\|`--help` | Output brief explanation for the program. |
| `-V`\|`--version` | Display the version of the program then quit. |
| `-L`\|`--log-level <level>` | Logging level as number or enum string. One of (fatal\|sys\|int\|err\|warn\|info\|debug) or (0-6) Current/default is warn. |
| `-v`\|`--verbose` | Increase the verbosity of the program status messages. Use multiple times for more verbosity. Negates quiet. |
| `-q`\|`--quiet` | Turn off all status messages for the program. Negated by verbose. |
| `--option-file <file>` | Read more options and parameters from the file. |
