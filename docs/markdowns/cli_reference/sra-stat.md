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
| -x\|--xml | Output as XML, default is text. |
| -b\|--start &lt;row-id&gt; | Starting spot id, default is 1. |
| -e\|--stop &lt;row-id&gt; | Ending spot id, default is max. |
| -m\|--meta | Print load metadata. |
| -q\|--quick | Quick mode: get statistics from metadata; do<br>not scan the table. |
| --member-stats &lt;on \| off&gt; | Print member stats, default is on. |
| --archive-info | Output archive info, default is off. |
| -s\|--statistics | Calculate READ_LEN average and standard<br>deviation. |
| -a\|--alignment &lt;on \| off&gt; | Print alignment info, default is on. |
| -l\|--local-info | Print the date, path, size and md5 of local<br>run. |
| -p\|--show_progress | Show the percentage of completion. |
| --ngc &lt;path&gt; | Path to ngc file. |
| -z\|--xml-log &lt;logfile&gt; | Produce XML-formatted log file. |
| --repair-data | Generate data for repair tool. |
| --info | Print report for all fields examined for<br>mismatch even if the old value is correct. |
| -h\|--help | Output brief explanation for the program. |
| -V\|--version | Display the version of the program then<br>quit. |
| -L\|--log-level &lt;level&gt; | Logging level as number or enum string. One<br>of (fatal\|sys\|int\|err\|warn\|info\|debug) or<br>(0-6) Current/default is warn. |
| -v\|--verbose | Increase the verbosity of the program<br>status messages. Use multiple times for more<br>verbosity. Negates quiet. |
| -q\|--quiet | Turn off all status messages for the<br>program. Negated by verbose. |
| --option-file &lt;file&gt; | Read more options and parameters from the<br>file. |
