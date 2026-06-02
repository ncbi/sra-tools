# sra-pileup

## Summary

Extracts pileups from aligned SRA data file.

## Usage

```text
sra-pileup <path> [options]
```

## Options

| Option | Description |
|---|---|
| -r\|--aligned-region &lt;name[:from-to]&gt; | Filter by position on genome. Name can<br>either be file specific name (ex: "chr1" or<br>"1"). "from" and "to" are 1-based coordinates |
| -o\|--outfile &lt;output-file&gt; | Output will be written to this file instead<br>of std-out |
| -t\|--table &lt;shortcut&gt; | Which alignment table(s) to use (p\|s\|e):<br>primary, secondary,<br>evidence-interval (default p) |
| -n\|--noqual | Omit qualities in output |
| --bzip2 | Compress output using bzip2 |
| --gzip | Compress output using gzip |
| --disable-multithreading | disable multithreading |
| --timing | write timing log-file |
| --ngc &lt;path&gt; | path to ngc file |
| -q\|--minmapq &lt;min. mapq&gt; | Minimum mapq-value,  alignments with lower<br>mapq will be ignored (default=0) |
| -d\|--duplicates &lt;dup-mode&gt; | process duplicates 0..off/1..on |
| -p\|--spotgroups | divide by spotgroups |
| --depth-per-spotgroup | print depth per spotgroup |
| -e\|--seqname | use original seq-name |
| --minmismatch | min percent of mismatches used in function<br>mismatch, default is 5% |
| --merge-dist | If adjacent slices are closer than this,<br>they are merged and skiplist is created.<br>a value of zero disables the feature,<br>default is 10000 |
| --function ref | list references |
| --function ref-ex | list references coverage |
| --function count | sort pileup with counters |
| --function stat | strand/tlen statistic |
| --function mismatch | only lines with mismatch |
| --function index | list deletion counts |
| --function varcount | variation counters:  ref-name, ref-pos,<br>ref-base, coverage,  mismatch A, mismatch C,<br>mismatch G, mismatch T, deletes, inserts, ins<br>after A, ins after C, ins after G, ins<br>after T |
| --function deletes | list deletions greater then 20 |
| --function indels | list only inserts/deletions |
| -h\|--help | Output brief explanation for the program. |
| -V\|--version | Display the version of the program then<br>quit. |
| -L\|--log-level &lt;level&gt; | Logging level as number or enum string. One<br>of (fatal\|sys\|int\|err\|warn\|info\|debug) or<br>(0-6) Current/default is warn. |
| -v\|--verbose | Increase the verbosity of the program<br>status messages. Use multiple times for more<br>verbosity. Negates quiet. |
| -q\|--quiet | Turn off all status messages for the<br>program. Negated by verbose. |
| --option-file &lt;file&gt; | Read more options and parameters from the<br>file. |

## Grouping of accessions into artificial spotgroups

sra-pileup SRRXXXXXX=a SRRYYYYYY=b SRRZZZZZZ=a
