# pacbio-loadxml

## Usage

```text
pacbio-loadxml [options] -r run.xml -e experiment.xml -o output-path
```

## Options

| Option | Description |
|---|---|
| -r\|--run-xml | path to run.xml describing input files |
| -e\|--experiment | path to experiment.xml |
| -o\|--output-path | target location |
| -i\|--input-path | input files location, default '.' |
| -u\|--input-unpacked | input files are unpacked |
| -t\|--input-no-threads | disable input files threaded caching |
| -f\|--force | force target overwrite |
| -n\|--spots-number | process only given number of spots from<br>input |
| -bE\|--bad-spot-number | acceptable number of spot creation errors,<br>default is 50 |
| -p\|--bad-spot-percentage | acceptable percentage of spots creation<br>errors, default is 5 |
| -x\|--expected | path to expected.xml |
| -s\|--intensities | [on off] load intensity data, default is<br>off. For  Illumina: signal, intensity,<br>noise; AB SOLiD: signal(s); LS454:<br>signal, position (for SFF files this option<br>is ON by default). |
| -z\|--xml-log &lt;logfile&gt; | Produce XML-formatted log file. |
| -h\|--help | Output brief explanation for the program. |
| -V\|--version | Display the version of the program then<br>quit. |
| -L\|--log-level &lt;level&gt; | Logging level as number or enum string. One<br>of (fatal\|sys\|int\|err\|warn\|info\|debug) or<br>(0-6) Current/default is warn. |
| -v\|--verbose | Increase the verbosity of the program<br>status messages. Use multiple times for more<br>verbosity. Negates quiet. |
| -q\|--quiet | Turn off all status messages for the<br>program. Negated by verbose. |
| --option-file &lt;file&gt; | Read more options and parameters from the<br>file. |
