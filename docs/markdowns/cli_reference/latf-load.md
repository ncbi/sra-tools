# latf-load

## Summary

Load FASTQ formatted data files

## Usage

```text
latf-load [options] <fastq-file> ...
```

## Options

| Option | Description |
|---|---|
| -o\|--output &lt;path&gt; | Path and Name of the output database. |
| -q\|--quality | Quality encoding (PHRED_33, PHRED_64,<br>LOGODDS) |
| -t\|--tmpfs &lt;path-to-file&gt; | Path to be used for scratch files. |
| -Q\|--qual-quant &lt;phred-score&gt; | Quality scores quantization level, can be<br>number (0: none default, 1: 2bit, 2:<br>1bit), or string like<br>'1:10,10:20,20:30,30:-' (which is<br>equivalent to 1). |
| --cache-size &lt;mbytes&gt; | Set the cache size in MB for the temporary<br>indices |
| --max-rec-count &lt;count&gt; | Set the maximum number of records to<br>process from the FASTQ file |
| -E\|--max-err-count &lt;count&gt; | Set the maximum number of errors to ignore<br>from the FASTQ file |
| -p\|--platform | Platform (ILLUMINA, LS454, SOLID,<br>COMPLETE_GENOMICS, HELICOS, PACBIO,<br>IONTORRENT, CAPILLARY) |
| --max-err-pct | acceptable percentage of spots creation<br>errors, default is 5 |
| --ignore-illumina-tags | ignore barcodes contained in<br>Illumina-formatted names |
| --no-readnames | drop original read names |
| -a\|--allow_duplicates | allow duplicate read names in the same file |
| -1\|--read1PairFiles &lt;path-to-file&gt; | Default read number for this file is 1.<br>Processing will be interleaved with the<br>file specified in --read2PairFile\|-r2 |
| -2\|--read2PairFiles &lt;path-to-file&gt; | Default read number for this file is 2.<br>Processing will be interleaved with the<br>file specified in --read1PairFile\|-r1 |
| -z\|--xml-log &lt;logfile&gt; | Produce XML-formatted log file. |
| -h\|--help | Output brief explanation for the program. |
| -V\|--version | Display the version of the program then<br>quit. |
| -L\|--log-level &lt;level&gt; | Logging level as number or enum string. One<br>of (fatal\|sys\|int\|err\|warn\|info\|debug) or<br>(0-6) Current/default is warn. |
| -v\|--verbose | Increase the verbosity of the program<br>status messages. Use multiple times for more<br>verbosity. Negates quiet. |
| -q\|--quiet | Turn off all status messages for the<br>program. Negated by verbose. |
| --option-file &lt;file&gt; | Read more options and parameters from the<br>file. |

## Example

```sh
latf-load -p 454 -o /tmp/SRZ123456 123456-1.fastq 123456-2.fastq
```
