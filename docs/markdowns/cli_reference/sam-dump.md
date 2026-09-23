# sam-dump

## Summary

Outputs an SRA run in SAM format.

## Usage

```text
sam-dump [options] path-to-run[ path-to-run ...]
```

## Options

| Option | Description |
|---|---|
| `-u`\|`--unaligned` | Output unaligned reads along with aligned reads |
| `-1`\|`--primary` | Output only primary alignments |
| `-c`\|`--cigar-long` | Output long version of CIGAR |
| `--cigar-CG` | Output CG version of CIGAR |
| `-r`\|`--header` | Always reconstruct header |
| `--header-file <filename>` | take all headers from this file |
| `-n`\|`--no-header` | Do not output headers |
| `--header-comment <text>` | Add comment to header. Use multiple times for several lines. Use quotes |
| `--aligned-region <name[:from-to]>` | Filter by position on genome. Name can either be file specific name (ex: "chr1" or "1"). "from" and "to" (inclusive) are 1-based coordinates |
| `--matepair-distance <from-to`\|`'unknown'>` | Filter by distance between matepairs. Use "unknown" to find matepairs split between the references. Use from-to (inclusive) to limit matepair distance on the same reference |
| `-s`\|`--seqid` | Print reference SEQ_ID in RNAME instead of NAME |
| `-=`\|`--hide-identical` | Output '=' if base is identical to reference |
| `--gzip` | Compress output using gzip |
| `--bzip2` | Compress output using bzip2 |
| `-g`\|`--spot-group` | Add .SPOT_GROUP to QNAME |
| `--fastq` | Produce FastQ formatted output |
| `--fasta` | Produce Fasta formatted output |
| `-p`\|`--prefix <prefix>` | Prefix QNAME: prefix.QNAME |
| `--reverse` | Reverse unaligned reads according to read type |
| `--cigar-CG-merge` | Apply CG fixups to CIGAR/SEQ/QUAL and outputs CG-specific columns |
| `--XI` | Output cSRA alignment id in XI column |
| `-Q`\|`--qual-quant <quantization string>` | Quality scores quantization level string like '1:10,10:20,20:30,30:-' |
| `--CG-evidence` | Output CG evidence aligned to reference |
| `--CG-ev-dnb` | Output CG evidence DNB's aligned to evidence |
| `--CG-mappings` | Output CG sequences aligned to reference |
| `--CG-SAM` | Output CG evidence DNB's aligned to reference |
| `--report` | report options instead of executing |
| `--output-file` | print output into this file (instead of STDOUT) |
| `--output-buffer-size` | size of output-buffer(dflt:32k, 0...off) |
| `--cachereport` | print report about mate-pair-cache |
| `--unaligned-spots-only` | output reads for spots with no aligned reads |
| `--CG-names` | prints cg-style spotgroup.spotid formed names |
| `--cursor-cache` | open cached cursor with this size |
| `--min-mapq` | min. mapq an alignment has to have, to be printed |
| `--no-mate-cache` | do not use mate-cache, slower but less memory usage |
| `--rna-splicing` | modify cigar-string (replace .D. with .N.) and add output flags (XS:A:+/-)  when rna-splicing is detected by match to spliceosome recognition sites |
| `--rna-splice-level` | level of rna-splicing detection (0,1,2) when testing for spliceosome recognition sites 0=perfect match, 1=one mismatch, 2=two mismatches  one on each site |
| `--rna-splice-log` | file, into which rna-splice events are written |
| `--disable-multithreading` | disable multithreading |
| `-o`\|`--omit-quality` | omit qualities |
| `--with-md-flag` | print MD-flag |
| `--ngc <PATH>` | PATH to ngc file |
| `-h`\|`--help` | Output brief explanation for the program. |
| `-V`\|`--version` | Display the version of the program then quit. |
| `-L`\|`--log-level <level>` | Logging level as number or enum string. One of (fatal\|sys\|int\|err\|warn\|info\|debug) or (0-6) Current/default is warn. |
| `-v`\|`--verbose` | Increase the verbosity of the program status messages. Use multiple times for more verbosity. Negates quiet. |
| `-q`\|`--quiet` | Turn off all status messages for the program. Negated by verbose. |
| `--option-file <file>` | Read more options and parameters from the file. |