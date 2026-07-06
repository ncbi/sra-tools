# bam-load

## Summary

Load a BAM formatted data file

## Usage

```text
bam-load [options] <bam-file>
```

## Options

| Option | Description |
|---|---|
| `-o`\|`--output <path>` | Path and Name of the output database. |
| `-i`\|`--input <path>` | Path where to get fasta files from. |
| `-k`\|`--config <path-to-file>` | Path to configuration file: maps the input BAM file's reference names to the equivalent GenBank accession. It is tab-delimited text file with unix line endings (\n LF) with the following fields in this order: #1 reference name as it occurs in the BAM file's SN field of @SQ header record; #2 INSDC reference ID |
| `--header <path-to-file>` | path to file containing the SAM header to store in the resulting cSRA, recommended in case of multiple input BAMs |
| `-t`\|`--tmpfs <path>` | Path to be used for scratch files. |
| `-u`\|`--unaligned <path-to-file>` | Specify file without aligned reads |
| `-d`\|`--accept-dups` | Accept spots inconsistent PCR duplicate flags |
| `--accept-nomatch` | Accept alignments with no matching bases |
| `--nomatch-log <path-to-file>` | Where to write info for alignments with no matching bases |
| `-Q`\|`--qual-quant <level>` | Quality scores quantization level, can be number (0: none, 1: 2bit, 2: 1bit), or string like '1:10,10:20,20:30,30:-' (which is equivalent to 1) (nb. the endpoint is exclusive). |
| `-q`\|`--min-mapq <phred-score>` | Filter secondary alignments by minimum mapping quality. |
| `--cache-size <mbytes>` | Set the cache size in MB for the temporary indices |
| `--no-cs` | turn off awareness of colorspace |
| `--minimum-match <count>` | minimum number of matches for an alignment |
| `-P`\|`--no-secondary` | ignore alignments marked as secondary |
| `--force-sorted` | Disable sort order checking. |
| `--unsorted` | Tell the loader to expect unsorted input (requires more memory) |
| `--sorted` | Tell the loader to require sorted input |
| `--no-verify` | Skip verify existence of references from the BAM file |
| `--only-verify` | Exit after verifying existence of references from the BAM file |
| `--use-QUAL` | use QUAL column for quality values (default is to use OQ if it is available) |
| `--ref-config` | Only process alignments to references in the config file |
| `--ref-filter <name>` | Only process alignments to the given reference |
| `--edit-aligned-qual <new-value>` | Convert quality at aligned positions to this value |
| `--keep-mismatch-qual` | Don't quantized quality at mismatched positions |
| `--max-rec-count <number>` | Set the maximum number of records to process from the BAM file |
| `-E`\|`--max-err-count <number>` | Set the maximum number of errors to ignore from the BAM file |
| `-r`\|`--ref-file <path-to-file>` | path to fasta file with references |
| `--TI` | for trace alignments |
| `--max-warning-dup-flag <count>` | set limit for number of duplicate flag mismatch warnings |
| `--accept-hard-clip` | accept hard clipping in CIGAR |
| `--allow-multi-map` | allow the same reference to be mapped to multiple names in the input files (default is disallow, old behaviors was to allow it) |
| `--make-spots-with-secondary` | use secondary alignments for constructing spots |
| `--defer-secondary` | defer processing of secondary alignments until the end of the file |
| `--batch-size` | optional maximum size of the spot batches (default: 240e6) |
| `--threads` | number of threads (3 or greater; can be 0, means default: 8) |
| `--extra-logging` | extra_logging |
| `--min-batch-size <count>` | Set the minimum batch size for spot assembly (default: 10,000,000 spots) |
| `--telemetry <file-name>` | Path and Name of the telemetry file. |
| `-z`\|`--xml-log <logfile>` | Produce XML-formatted log file. |
| `-h`\|`--help` | Output brief explanation for the program. |
| `-V`\|`--version` | Display the version of the program then quit. |
| `-L`\|`--log-level <level>` | Logging level as number or enum string. One of (fatal\|sys\|int\|err\|warn\|info\|debug) or (0-6) Current/default is warn. |
| `-v`\|`--verbose` | Increase the verbosity of the program status messages. Use multiple times for more verbosity. Negates quiet. |
| `-q`\|`--quiet` | Turn off all status messages for the program. Negated by verbose. |
| `--option-file <file>` | Read more options and parameters from the file. |

## Example

```sh
bam-load -o /tmp/SRZ123456 -k analysis.bam.cfg 123456.bam
```
