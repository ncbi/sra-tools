# cg-load

## Usage

```sh
cg-load [options] -m map-dir -o path-to-run
```

## Summary

Load a Complete Genomics formatted data files

## Example

cg-load -m build36/MAP -o /tmp/SRZ123456

-m|--map                         MAP input directory path containing files
-o|--output                      output database path

## Options

| Option | Description |
|---|---|
| -a\|--asm | ASM input directory path containing files |
| --load-extra-evidence | load extra evidence files |
| -s\|--schema | database schema file name |
| -k\|--refseq-config | path to file with reference-to-accession&lt;br&gt;list |
| -i\|--refseq-path | path to directory with reference&lt;br&gt;sequences in fasta |
| -r\|--ref-file | path to fasta file with references |
| -f\|--force | force output overwrite |
| -g\|--write-reference | force reference sequence write |
| -w\|--write-read | force reads write |
| -Q\|--qual-quant | quality scores quantization level, can be&lt;br&gt;number (0: none, 1: 2bit, 2: 1bit), or&lt;br&gt;string like '1:10,10:20,20:30,30:-' (which&lt;br&gt;is equivalent to 1) |
| -G\|--no-spotgroup | do not write source file key to SPOT_GROUP&lt;br&gt;column |
| -q\|--min-mapq | filter secondary mappings by minimum weight&lt;br&gt;(phred) |
| -P\|--no-secondary | preserve only one mapping per half-DNB&lt;br&gt;based on weight |
| --single-mate | if secondary mates have duplicates preserve&lt;br&gt;only one in each pair based on weight |
| --cluster-size | defines cluster window on the reference,&lt;br&gt;records only placement from given cluster&lt;br&gt;size; default is zero which means ignore |
| -t\|--input-no-threads | disable input files threaded caching |
| -l\|--library | copy extra file/directory into output |
| -z\|--xml-log &lt;logfile&gt; | Produce XML-formatted log file. |
| -h\|--help | Output brief explanation for the program. |
| -V\|--version | Display the version of the program then&lt;br&gt;quit. |
| -L\|--log-level &lt;level&gt; | Logging level as number or enum string. One&lt;br&gt;of (fatal\|sys\|int\|err\|warn\|info\|debug) or&lt;br&gt;(0-6) Current/default is warn. |
| -v\|--verbose | Increase the verbosity of the program&lt;br&gt;status messages. Use multiple times for more&lt;br&gt;verbosity. Negates quiet. |
| -q\|--quiet | Turn off all status messages for the&lt;br&gt;program. Negated by verbose. |
| --option-file &lt;file&gt; | Read more options and parameters from the&lt;br&gt;file. |
