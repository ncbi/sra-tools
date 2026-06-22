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
| `-r`\|`--aligned-region <name[:from-to]>` | Filter by position on genome. Name can either be file specific name (ex: "chr1" or "1"). "from" and "to" are 1-based coordinates |
| `-o`\|`--outfile <output-file>` | Output will be written to this file instead of std-out |
| `-t`\|`--table <shortcut>` | Which alignment table(s) to use (p\|s\|e): primary, secondary, evidence-interval (default p) |
| `-n`\|`--noqual` | Omit qualities in output |
| `--bzip2` | Compress output using bzip2 |
| `--gzip` | Compress output using gzip |
| `--disable-multithreading` | disable multithreading |
| `--timing` | write timing log-file |
| `--ngc <path>` | path to ngc file |
| `-q`\|`--minmapq <min. mapq>` | Minimum mapq-value,  alignments with lower mapq will be ignored (default=0) |
| `-d`\|`--duplicates <dup-mode>` | process duplicates 0..off/1..on |
| `-p`\|`--spotgroups` | divide by spotgroups |
| `--depth-per-spotgroup` | print depth per spotgroup |
| `-e`\|`--seqname` | use original seq-name |
| `--minmismatch` | min percent of mismatches used in function mismatch, default is 5% |
| `--merge-dist` | If adjacent slices are closer than this, they are merged and skiplist is created. a value of zero disables the feature, default is 10000 |
| `--function ref` | list references |
| `--function ref-ex` | list references coverage |
| `--function count` | sort pileup with counters |
| `--function stat` | strand/tlen statistic |
| `--function mismatch` | only lines with mismatch |
| `--function index` | list deletion counts |
| `--function varcount` | variation counters:  ref-name, ref-pos, ref-base, coverage,  mismatch A, mismatch C, mismatch G, mismatch T, deletes, inserts, ins after A, ins after C, ins after G, ins after T |
| `--function deletes` | list deletions greater then 20 |
| `--function indels` | list only inserts/deletions |
| `-h`\|`--help` | Output brief explanation for the program. |
| `-V`\|`--version` | Display the version of the program then quit. |
| `-L`\|`--log-level <level>` | Logging level as number or enum string. One of (fatal\|sys\|int\|err\|warn\|info\|debug) or (0-6) Current/default is warn. |
| `-v`\|`--verbose` | Increase the verbosity of the program status messages. Use multiple times for more verbosity. Negates quiet. |
| `-q`\|`--quiet` | Turn off all status messages for the program. Negated by verbose. |
| `--option-file <file>` | Read more options and parameters from the file. |

## Grouping of accessions into artificial spotgroups

sra-pileup SRRXXXXXX=a SRRYYYYYY=b SRRZZZZZZ=a
