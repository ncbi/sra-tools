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
| -t\|--temp &lt;path&gt; | temp directory to use for scratch space,<br>default: $TMPDIR or $TEMPDIR or $TEMP or<br>$TMP or /tmp |
| --vdbcache &lt;path&gt; | location of .vdbcache to update |
| -h\|--help | Output brief explanation for the program. |
| -V\|--version | Display the version of the program then<br>quit. |
| -L\|--log-level &lt;level&gt; | Logging level as number or enum string. One<br>of (fatal\|sys\|int\|err\|warn\|info\|debug) or<br>(0-6) Current/default is warn. |
| -v\|--verbose | Increase the verbosity of the program<br>status messages. Use multiple times for more<br>verbosity. Negates quiet. |
| -q\|--quiet | Turn off all status messages for the<br>program. Negated by verbose. |
| --option-file &lt;file&gt; | Read more options and parameters from the<br>file. |
