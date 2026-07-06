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
| `-o`\|`--output <path>` | Path and Name of the output database. |
| `-q`\|`--quality` | Quality encoding (PHRED_33, PHRED_64, LOGODDS) |
| `-t`\|`--tmpfs <path-to-file>` | Path to be used for scratch files. |
| `-Q`\|`--qual-quant <phred-score>` | Quality scores quantization level, can be number (0: none default, 1: 2bit, 2: 1bit), or string like '1:10,10:20,20:30,30:-' (which is equivalent to 1). |
| `--cache-size <mbytes>` | Set the cache size in MB for the temporary indices |
| `--max-rec-count <count>` | Set the maximum number of records to process from the FASTQ file |
| `-E`\|`--max-err-count <count>` | Set the maximum number of errors to ignore from the FASTQ file |
| `-p`\|`--platform` | Platform (ILLUMINA, LS454, SOLID, COMPLETE_GENOMICS, HELICOS, PACBIO, IONTORRENT, CAPILLARY) |
| `--max-err-pct` | acceptable percentage of spots creation errors, default is 5 |
| `--ignore-illumina-tags` | ignore barcodes contained in Illumina-formatted names |
| `--no-readnames` | drop original read names |
| `-a`\|`--allow_duplicates` | allow duplicate read names in the same file |
| `-1`\|`--read1PairFiles <path-to-file>` | Default read number for this file is 1. Processing will be interleaved with the file specified in --read2PairFile\|-r2 |
| `-2`\|`--read2PairFiles <path-to-file>` | Default read number for this file is 2. Processing will be interleaved with the file specified in --read1PairFile\|-r1 |
| `-z`\|`--xml-log <logfile>` | Produce XML-formatted log file. |
| `-h`\|`--help` | Output brief explanation for the program. |
| `-V`\|`--version` | Display the version of the program then quit. |
| `-L`\|`--log-level <level>` | Logging level as number or enum string. One of (fatal\|sys\|int\|err\|warn\|info\|debug) or (0-6) Current/default is warn. |
| `-v`\|`--verbose` | Increase the verbosity of the program status messages. Use multiple times for more verbosity. Negates quiet. |
| `-q`\|`--quiet` | Turn off all status messages for the program. Negated by verbose. |
| `--option-file <file>` | Read more options and parameters from the file. |

## Example

```sh
latf-load -p 454 -o /tmp/SRZ123456 123456-1.fastq 123456-2.fastq
```
