# illumina-load

## Usage

```sh
illumina-load [options] -r run.xml -e experiment.xml -o output-path
-r|--run-xml                     path to run.xml describing input files
-e|--experiment                  path to experiment.xml
-o|--output-path                 target location
```

## Options

| Option | Description |
|---|---|
| -i\|--input-path | input files location, default '.' |
| -u\|--input-unpacked | input files are unpacked |
| -t\|--input-no-threads | disable input files threaded caching |
| -f\|--force | force target overwrite |
| -n\|--spots-number | process only given number of spots from&lt;br&gt;input |
| -bE\|--bad-spot-number | acceptable number of spot creation errors,&lt;br&gt;default is 50 |
| -p\|--bad-spot-percentage | acceptable percentage of spots creation&lt;br&gt;errors, default is 5 |
| -x\|--expected | path to expected.xml |
| -s\|--intensities | [on off] load intensity data, default is&lt;br&gt;off. For  Illumina: signal, intensity,&lt;br&gt;noise; AB SOLiD: signal(s); LS454:&lt;br&gt;signal, position (for SFF files this option&lt;br&gt;is ON by default). |
| -z\|--xml-log &lt;logfile&gt; | Produce XML-formatted log file. |
| -h\|--help | Output brief explanation for the program. |
| -V\|--version | Display the version of the program then&lt;br&gt;quit. |
| -L\|--log-level &lt;level&gt; | Logging level as number or enum string. One&lt;br&gt;of (fatal\|sys\|int\|err\|warn\|info\|debug) or&lt;br&gt;(0-6) Current/default is warn. |
| -v\|--verbose | Increase the verbosity of the program&lt;br&gt;status messages. Use multiple times for more&lt;br&gt;verbosity. Negates quiet. |
| -q\|--quiet | Turn off all status messages for the&lt;br&gt;program. Negated by verbose. |
| --option-file &lt;file&gt; | Read more options and parameters from the&lt;br&gt;file. |
