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
| -o\|--output &lt;path&gt; | Path and Name of the output database. |
| -i\|--input &lt;path&gt; | Path where to get fasta files from. |
| -k\|--config &lt;path-to-file&gt; | Path to configuration file: maps the input<br>BAM file's reference names to the<br>equivalent GenBank accession. It is<br>tab-delimited text file with unix line<br>endings (\n LF) with the following fields<br>in this order: #1 reference name as it<br>occurs in the BAM file's SN field of @SQ<br>header record; #2 INSDC reference ID |
| --header &lt;path-to-file&gt; | path to file containing the SAM header to<br>store in the resulting cSRA, recommended in<br>case of multiple input BAMs |
| -t\|--tmpfs &lt;path&gt; | Path to be used for scratch files. |
| -u\|--unaligned &lt;path-to-file&gt; | Specify file without aligned reads |
| -d\|--accept-dups | Accept spots inconsistent PCR duplicate<br>flags |
| --accept-nomatch | Accept alignments with no matching bases |
| --nomatch-log &lt;path-to-file&gt; | Where to write info for alignments with no<br>matching bases |
| -Q\|--qual-quant &lt;level&gt; | Quality scores quantization level, can be<br>number (0: none, 1: 2bit, 2: 1bit), or<br>string like '1:10,10:20,20:30,30:-' (which<br>is equivalent to 1) (nb. the endpoint is<br>exclusive). |
| -q\|--min-mapq &lt;phred-score&gt; | Filter secondary alignments by minimum<br>mapping quality. |
| --cache-size &lt;mbytes&gt; | Set the cache size in MB for the temporary<br>indices |
| --no-cs | turn off awareness of colorspace |
| --minimum-match &lt;count&gt; | minimum number of matches for an alignment |
| -P\|--no-secondary | ignore alignments marked as secondary |
| --force-sorted | Disable sort order checking. |
| --unsorted | Tell the loader to expect unsorted input<br>(requires more memory) |
| --sorted | Tell the loader to require sorted input |
| --no-verify | Skip verify existence of references from<br>the BAM file |
| --only-verify | Exit after verifying existence of<br>references from the BAM file |
| --use-QUAL | use QUAL column for quality values (default<br>is to use OQ if it is available) |
| --ref-config | Only process alignments to references in<br>the config file |
| --ref-filter &lt;name&gt; | Only process alignments to the given<br>reference |
| --edit-aligned-qual &lt;new-value&gt; | Convert quality at aligned positions to<br>this value |
| --keep-mismatch-qual | Don't quantized quality at mismatched<br>positions |
| --max-rec-count &lt;number&gt; | Set the maximum number of records to<br>process from the BAM file |
| -E\|--max-err-count &lt;number&gt; | Set the maximum number of errors to ignore<br>from the BAM file |
| -r\|--ref-file &lt;path-to-file&gt; | path to fasta file with references |
| --TI | for trace alignments |
| --max-warning-dup-flag &lt;count&gt; | set limit for number of duplicate flag<br>mismatch warnings |
| --accept-hard-clip | accept hard clipping in CIGAR |
| --allow-multi-map | allow the same reference to be mapped to<br>multiple names in the input files (default<br>is disallow, old behaviors was to allow it) |
| --make-spots-with-secondary | use secondary alignments for constructing<br>spots |
| --defer-secondary | defer processing of secondary alignments<br>until the end of the file |
| --batch-size | optional maximum size of the spot batches<br>(default: 240e6) |
| --threads | number of threads (3 or greater; can be 0,<br>means default: 8) |
| --extra-logging | extra_logging |
| --min-batch-size &lt;count&gt; | Set the minimum batch size for spot<br>assembly (default: 10,000,000 spots) |
| --telemetry &lt;file-name&gt; | Path and Name of the telemetry file. |
| -z\|--xml-log &lt;logfile&gt; | Produce XML-formatted log file. |
| -h\|--help | Output brief explanation for the program. |
| -V\|--version | Display the version of the program then<br>quit. |
| -L\|--log-level &lt;level&gt; | Logging level as number or enum string. One<br>of (fatal\|sys\|int\|err\|warn\|info\|debug) or<br>(0-6) Current/default is warn. |
| -v\|--verbose | Increase the verbosity of the program<br>status messages. Use multiple times for more<br>verbosity. Negates quiet. |
| -q\|--quiet | Turn off all status messages for the<br>program. Negated by verbose. |
| --option-file &lt;file&gt; | Read more options and parameters from the<br>file. |

## Example

```sh
bam-load -o /tmp/SRZ123456 -k analysis.bam.cfg 123456.bam
```
