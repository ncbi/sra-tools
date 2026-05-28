# pacbio-load

## Usage

```sh
pacbio-load <hdf5-file> -o<target>
```

## Options

| Option | Description |
|---|---|
| -o\|--output &lt;output&gt; | target to be created |
| -S\|--schema &lt;schema&gt; | schema-name to be used |
| -f\|--force &lt;force&gt; | forces an existing target to be overwritten |
| -t\|--tabs &lt;tabs&gt; | load only these tabs (SCPM), dflt=all&lt;br&gt;S...Sequence C...Consensus P...Passes&lt;br&gt;M...Metrics |
| -p\|--with_progressbar &lt;load-progress&gt; | show load-progress |
| -z\|--xml-log &lt;logfile&gt; | Produce XML-formatted log file. |
| -h\|--help | Output brief explanation for the program. |
| -V\|--version | Display the version of the program then&lt;br&gt;quit. |
| -L\|--log-level &lt;level&gt; | Logging level as number or enum string. One&lt;br&gt;of (fatal\|sys\|int\|err\|warn\|info\|debug) or&lt;br&gt;(0-6) Current/default is warn. |
| -v\|--verbose | Increase the verbosity of the program&lt;br&gt;status messages. Use multiple times for more&lt;br&gt;verbosity. Negates quiet. |
| -q\|--quiet | Turn off all status messages for the&lt;br&gt;program. Negated by verbose. |
| --option-file &lt;file&gt; | Read more options and parameters from the&lt;br&gt;file. |
