# fasterq-dump

## Summary

A faster implementation of the fastq-dump. Extracts fastq from an SRA data file, only faster!

## Usage

```text
fasterq-dump <path> [options]
fasterq-dump <accession> [options]
```

## Options

| Option | Description |
|---|---|
| `-F`\|`--format` | format (special, fastq, default=fastq) |
| `-o`\|`--outfile` | output-file |
| `-O`\|`--outdir` | output-dir |
| `-b`\|`--bufsize` | size of file-buffer dflt=1MB |
| `-c`\|`--curcache` | size of cursor-cache dflt=10MB |
| `-m`\|`--mem` | memory limit for sorting dflt=100MB |
| `-t`\|`--temp` | where to put temp. files dflt=curr dir |
| `-e`\|`--threads` | how many thread dflt=6 |
| `-p`\|`--progress` | show progress |
| `-x`\|`--details` | print details |
| `-s`\|`--split-spot` | split spots into reads |
| `-S`\|`--split-files` | write reads into different files |
| `-3`\|`--split-3` | writes single reads in special file |
| `--concatenate-reads` | writes whole spots into one file |
| `-Z`\|`--stdout` | print output to stdout |
| `-f`\|`--force` | force to overwrite existing file(s) |
| `--skip-technical` | skip technical reads |
| `--include-technical` | include technical reads |
| `-M`\|`--min-read-len` | filter by sequence-len |
| `--table` | which seq-table to use in case of pacbio |
| `-B`\|`--bases` | filter by bases |
| `-A`\|`--append` | append to output-file |
| `--fasta` | produce FASTA output |
| `--fasta-unsorted` | produce FASTA output, unsorted |
| `--fasta-ref-tbl` | produce FASTA output from REFERENCE tbl |
| `--fasta-concat-all` | concatenate all rows and produce FASTA |
| `--internal-ref` | extract only internal REFERENCEs |
| `--external-ref` | extract only external REFERENCEs |
| `--ref-name` | extract only these REFERENCEs |
| `--ref-report` | enumerate references |
| `--use-name` | print name instead of seq-id |
| `--seq-defline` | custom defline for sequence:  $ac=accession, $sn=spot-name,  $sg=spot-group, $si=spot-id, $ri=read-id, $rl=read-length |
| `--qual-defline` | custom defline for qualities:  same as seq-defline |
| `-U`\|`--only-unaligned` | process only unaligned reads |
| `-a`\|`--only-aligned` | process only aligned reads |
| `--disk-limit` | explicitly set disk-limit |
| `--disk-limit-tmp` | explicitly set disk-limit for temp. files |
| `--size-check` | switch to control: on=perform size-check (default),  off=do not perform size-check, only=perform size-check only |
| `--ngc <PATH>` | PATH to ngc file |
| `-h`\|`--help` | Output brief explanation for the program. |
| `-V`\|`--version` | Display the version of the program then quit. |
| `-L`\|`--log-level <level>` | Logging level as number or enum string. One of (fatal\|sys\|int\|err\|warn\|info\|debug) or (0-6) Current/default is warn. |
| `-v`\|`--verbose` | Increase the verbosity of the program status messages. Use multiple times for more verbosity. Negates quiet. |
| `-q`\|`--quiet` | Turn off all status messages for the program. Negated by verbose. |
| `--option-file <file>` | Read more options and parameters from the file. |

## For More Information

https://github.com/ncbi/sra-tools/wiki/HowTo:-fasterq-dump
https://github.com/ncbi/sra-tools/wiki/08.-prefetch-and-fasterq-dump
