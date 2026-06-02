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
| -o\|--output &lt;output&gt; | target to be created |
| -S\|--schema &lt;schema&gt; | schema-name to be used |
| -f\|--force &lt;force&gt; | forces an existing target to be overwritten |
| -t\|--tabs &lt;tabs&gt; | load only these tabs (SCPM), dflt=all<br>S...Sequence C...Consensus P...Passes<br>M...Metrics |
| -p\|--with_progressbar &lt;load-progress&gt; | show load-progress |
| -z\|--xml-log &lt;logfile&gt; | Produce XML-formatted log file. |
| -h\|--help | Output brief explanation for the program. |
| -V\|--version | Display the version of the program then<br>quit. |
| -L\|--log-level &lt;level&gt; | Logging level as number or enum string. One<br>of (fatal\|sys\|int\|err\|warn\|info\|debug) or<br>(0-6) Current/default is warn. |
| -v\|--verbose | Increase the verbosity of the program<br>status messages. Use multiple times for more<br>verbosity. Negates quiet. |
| -q\|--quiet | Turn off all status messages for the<br>program. Negated by verbose. |
| --option-file &lt;file&gt; | Read more options and parameters from the<br>file. |
